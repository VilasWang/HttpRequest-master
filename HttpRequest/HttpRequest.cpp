//#include "stdafx.h"
#include <curl/curl.h>
#include "HttpRequest.h"
#include "HttpTask.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <process.h>
#include "ThreadPool/mutex.h"
#include "ClassMemoryTracer.h"
#include "httprequestdef.h"


#define DEFAULT_RETRY_COUNT					3
#define DEFAULT_DOWNLOAD_THREAD_COUNT		5
#define DEFAULT_CONNECT_TIMEOUT				10		//连接最长时间
#define DEFAULT_TIMEOUT						600		//传输最长时间
#define MINIMAL_PROGRESS_INTERVAL			3


class CURLWrapper : public CURLInterface
{
public:
	CURLWrapper();
	~CURLWrapper();

	friend class HttpRequest;

public:
	int	setRetryTimes(int retry_times);
	int	setRequestTimeout(long time_out = 0);
	int	setRequestUrl(const std::string& url);
	int	setFollowLocation(bool follow_location);
	int	setPostData(const char* data, unsigned int size);
	int	setRequestHeader(const std::string& header);
	int	setRequestProxy(const std::string& proxy, long proxy_port);
	int setDownloadFile(const std::string& file_name);
	int setDownloadThreadCount(int thread_count);
	int setUploadFile(const std::string& file_name, const std::string& target_name, const std::string& target_path);
	void setResultCallback(ResultCallback rc);
	void setProgressCallback(ProgressCallback pc);
	void setRequestType(HttpRequest::RequestType);

public:
	int	perform() override;
	void cancel() override;
	int	requestId() override { return m_id; }

	bool isRunning() const override;
	bool isCanceled() const override;
	bool isFailed() const override;
	bool isMultiDownload() const override;

	INT64 totalBytes() const { return m_total_size; }
	INT64 currentBytes() const override;
	void setCurrentBytes(INT64) override;

	void setRunning(bool bRunning);
	void setCanceled(bool bCancel);
	void setFailed(bool bFail);

	void reset();

private:
	CURLcode publicSetoptMethod(CURL* curl_handle, curl_slist* http_headers);
	int doPostGet();
	int doDownload();
	int doUpload();
	int doUpload2();//实际上是httppost的方式

	int	download(ThreadChunk* thread_chunk);
	int	splitDownloadCount(INT64 file_size);
	INT64 getDownloadFileSize();

	static UINT WINAPI downloadProc(LPVOID param);

private:
	static std::atomic<int> s_id;
	std::shared_ptr<TPLock> m_lock;

	int m_id;
	HttpRequest::RequestType m_type;
	std::atomic<bool> m_is_running;
	std::atomic<bool> m_is_cancel;
	std::atomic<bool> m_is_failed;
	bool m_follow_location;
	int m_retry_times;
	long m_time_out;
	long m_proxy_port;
	std::string	m_http_proxy;
	std::string	m_post_data;
	std::string	m_url;
	std::list<std::string> m_list_header;
	ResultCallback  m_result_callback;
	ProgressCallback  m_progress_callback;

	//下载
	std::string	m_file_path;
	int	m_thread_count;
	std::atomic<bool> m_multi_download;
	std::atomic<INT64> m_total_size;
	std::atomic<INT64> m_current_size;

	//上传
	std::string m_strTargetName;
	std::string m_strTargetPath;
	std::string m_strUploadFile;
};
std::atomic<int> CURLWrapper::s_id = 0;

//////////////////////////////////////////////////////////////////////////
size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t retcode;
	curl_off_t nread;

	/* in real-world cases, this would probably get this data differently
	as this fread() stuff is exactly what the library already would do
	by default internally */
	retcode = fread(ptr, size, nmemb, (FILE *)stream);

	nread = (curl_off_t)retcode;

	fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T
			" bytes from file\n", nread);

	return retcode;
}

size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata)
{
	std::string* receive_header = reinterpret_cast<std::string*>(userdata);
	if (nullptr == receive_header || nullptr == buffer)
		return 0;

	receive_header->append(reinterpret_cast<const char*>(buffer), size * nitems);
	return nitems * size;
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	std::string* receive_content = reinterpret_cast<std::string*>(userdata);
	if (nullptr == receive_content || nullptr == ptr)
		return 0;

	receive_content->append(reinterpret_cast<const char*>(ptr), size * nmemb);
	return nmemb * size;
}

size_t write_file_callback(char* buffer, size_t size, size_t nmemb, void* userdata)
{
	CURLInterface* helper = nullptr;
	ThreadChunk* thread_chunk = reinterpret_cast<ThreadChunk*>(userdata);
	if (thread_chunk)
	{
		helper = thread_chunk->_helper;
	}

	if (!thread_chunk || !helper || helper->isCanceled() || helper->isFailed())
	{
		return 0;
	}

	size_t written = 0;
	int seek_error = fseek(thread_chunk->_fp, thread_chunk->_startidx, SEEK_SET);
	if (seek_error != 0)
	{
		OutputDebugStringA("fseek error!");
	}
	else
	{
		written = fwrite(buffer, size, nmemb, thread_chunk->_fp);
		if (written > 0)
		{
			thread_chunk->_startidx += written;
			helper->setCurrentBytes(helper->currentBytes() + written);
		}
	}

	return written;
}

int progress_download_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	CURLInterface* helper = nullptr;
	ThreadChunk* thread_chunk = reinterpret_cast<ThreadChunk*>(clientp);
	if (thread_chunk)
	{
		helper = thread_chunk->_helper;
	}

	if (!thread_chunk || !helper || helper->isCanceled() || helper->isFailed())
	{
		return -1;
	}

	std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(helper->requestId());
	if (dltotal > 0 && dlnow > 0 && reply.get())
	{
		if (helper->isMultiDownload())
		{
			reply->replyProgress(helper->requestId(), true, helper->totalBytes(), helper->currentBytes());
		}
		else
		{
			reply->replyProgress(helper->requestId(), true, dltotal, dlnow);
		}
	}

	return 0;
}

int progress_upload_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	CURLInterface* helper = reinterpret_cast<CURLInterface*>(clientp);
	if (!helper || helper->isCanceled() || helper->isFailed())
	{
		return -1;
	}

	std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(helper->requestId());
	if (ultotal > 0 && ulnow > 0 && reply.get())
	{
		reply->replyProgress(helper->requestId(), false, ultotal, ulnow);
	}

	return 0;
}

CURLWrapper::CURLWrapper()
	: m_type(HttpRequest::Unkonwn)
	, m_id(++s_id)
	, m_is_running(false)
	, m_is_cancel(false)
	, m_follow_location(true)
	, m_retry_times(DEFAULT_RETRY_COUNT)
	, m_time_out(0)
	, m_proxy_port(0)
	, m_thread_count(0)
	, m_multi_download(true)
	, m_is_failed(false)
	, m_total_size(0)
	, m_current_size(0)
	, m_lock(new TPLock)
	, m_progress_callback(nullptr)
	, m_result_callback(nullptr)
{
	TRACE_CLASS_CONSTRUCTOR(CURLWrapper);
}

CURLWrapper::~CURLWrapper()
{
	/*char ch[64];
	sprintf_s(ch, "%s id:%d\n", __FUNCTION__, m_id);
	OutputDebugStringA(ch);*/
	TRACE_CLASS_DESTRUCTOR(CURLWrapper);
	cancel();
}

int	CURLWrapper::setRetryTimes(int retry_times)
{
	m_retry_times = retry_times;
	return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestTimeout(long time_out)
{
	if (time_out < 0)
	{
		return HttpRequest::REQUEST_INVALID_OPT;
	}
	m_time_out = time_out;
	return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestUrl(const std::string& url)
{
	m_url = url;
	return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setFollowLocation(bool follow_location)
{
	m_follow_location = follow_location;
	return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setPostData(const char* data, unsigned int size)
{
	m_post_data = std::string(data, size);
	return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestHeader(const std::string& header)
{
	if (header.empty())
	{
		return HttpRequest::REQUEST_INVALID_OPT;
	}

	m_list_header.push_back(header);
	return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setRequestProxy(const std::string& proxy, long proxy_port)
{
	m_http_proxy = proxy;
	m_proxy_port = proxy_port;
	return HttpRequest::REQUEST_OK;
}

int CURLWrapper::setDownloadFile(const std::string& file_name)
{
	m_file_path = file_name;
	return CURLE_OK;
}

int CURLWrapper::setDownloadThreadCount(int thread_count)
{
	m_thread_count = thread_count;
	return CURLE_OK;
}

int CURLWrapper::setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path)
{
	m_strUploadFile = file_path;
	m_strTargetName = target_name;
	m_strTargetPath = target_path;
	return CURLE_OK;
}

void CURLWrapper::setResultCallback(ResultCallback rc)
{
	m_result_callback = rc;
}

void CURLWrapper::setProgressCallback(ProgressCallback pc)
{
	m_progress_callback = pc;
}

void CURLWrapper::setRequestType(HttpRequest::RequestType type)
{
	m_type = type;
}

CURLcode CURLWrapper::publicSetoptMethod(CURL* curl_handle, curl_slist* http_headers)
{
	CURLcode curl_code = CURLE_FAILED_INIT;
	if (curl_handle)
	{
		if (m_url.substr(0, 5) == "https" || m_url.substr(0, 5) == "HTTPS")
		{
			curl_code = curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_code = curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
		}
		curl_code = curl_easy_setopt(curl_handle, CURLOPT_URL, m_url.c_str());

		if (m_follow_location)
		{
			curl_code = curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5);
			curl_code = curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
			curl_code = curl_easy_setopt(curl_handle, CURLOPT_AUTOREFERER, 1L);
		}
		else
		{
			curl_code = curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 0L);
		}

		curl_code = curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
		curl_code = curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, DEFAULT_CONNECT_TIMEOUT);
		curl_code = curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, m_time_out);
	}

	if (!m_list_header.empty())
	{
		for (auto iter = m_list_header.cbegin(); iter != m_list_header.cend(); ++iter)
		{
			http_headers = curl_slist_append(http_headers, (*iter).c_str());
		}

		if (http_headers)
		{
			curl_code = curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, http_headers);
		}
	}

	return curl_code;
}

bool CURLWrapper::isRunning() const
{
	return m_is_running;
}

void CURLWrapper::setRunning(bool bRunning)
{
	m_is_running = bRunning;
}

bool CURLWrapper::isCanceled() const
{
	return m_is_cancel;
}

void CURLWrapper::setCanceled(bool bCancel)
{
	m_is_cancel = bCancel;
}

bool CURLWrapper::isFailed() const
{
	return m_is_failed;
}

void CURLWrapper::setFailed(bool bFailed)
{
	m_is_failed = bFailed;
}

INT64 CURLWrapper::currentBytes() const
{
	return m_current_size;
}

void CURLWrapper::setCurrentBytes(INT64 current_size)
{
	m_current_size = current_size;
}

bool CURLWrapper::isMultiDownload() const
{
	return m_multi_download;
}

int CURLWrapper::perform()
{
	int curl_code = CURLE_UNSUPPORTED_PROTOCOL;
	if (isCanceled())
	{
		return curl_code;
	}

	setRunning(true);

	std::shared_ptr<HttpReply> rly = HttpManager::globalInstance()->getReply(m_id);
	if (rly.get())
	{
		rly->setProgressCallback(m_progress_callback);
		rly->setResultCallback(m_result_callback);
	}

	if (m_type == HttpRequest::Post || m_type == HttpRequest::Get)
	{
		curl_code = doPostGet();
	}
	else if (m_type == HttpRequest::Download)
	{
		curl_code = doDownload();
	}
	else if (m_type == HttpRequest::Upload)
	{
		curl_code = doUpload();
	}

	std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->takeReply(m_id);
	if (!isCanceled() && reply.get())
	{
		bool success = (curl_code == CURLE_OK);
		reply->replyResult(success);
		reply.reset();
	}

	setRunning(true);
	return curl_code;
}

void CURLWrapper::cancel()
{
	setCanceled(true);
}

int CURLWrapper::doPostGet()
{
	CURL* curl = curl_easy_init();
	if (curl)
	{
		HttpManager::set_share_handle(curl);

		curl_slist*	http_headers = nullptr;
		CURLcode curl_code = publicSetoptMethod(curl, http_headers);

		std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
		if (reply.get())
		{
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, &reply->m_receive_header);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->m_receive_content);
		}
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
		if (m_type == HttpRequest::Post)
		{
			curl_code = curl_easy_setopt(curl, CURLOPT_POST, 1);
			if (curl_code == CURLE_OK)
			{
				curl_code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, m_post_data.c_str());
				curl_code = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, m_post_data.size());
			}
		}

		if (curl_code != CURLE_OK)
		{
			curl_slist_free_all(http_headers);
			curl_easy_cleanup(curl);
			return curl_code;
		}

		curl_code = curl_easy_perform(curl);
		if (curl_code == CURLE_OPERATION_TIMEDOUT)
		{
			int retry_count = m_retry_times;
			while (!isCanceled() && retry_count > 0)
			{
				curl_code = curl_easy_perform(curl);
				if (curl_code != CURLE_OPERATION_TIMEDOUT)
				{
					break;
				}
				retry_count--;
			}
		}

		if (reply.get())
		{
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->m_http_code);
			if (curl_code != CURLE_OK || reply->m_http_code != 200)
			{
				const char* err_string = curl_easy_strerror(curl_code);
				reply->m_error_string = err_string;
			}
		}

		curl_slist_free_all(http_headers);
		curl_easy_cleanup(curl);
		return curl_code;
	}

	return CURLE_FAILED_INIT;
}

int CURLWrapper::doDownload()
{
	m_total_size = getDownloadFileSize();
	if (m_total_size < 0)
	{
		m_multi_download = false;
	}

	std::string file_name = m_file_path;
	std::string out_file_name = file_name + ".dl";
	DeleteFileA(out_file_name.c_str());

	FILE* fp = nullptr;
	fopen_s(&fp, out_file_name.c_str(), "wb+");
	if (nullptr == fp)
	{
		std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
		if (reply.get())
		{
			reply->m_error_string = "Open local file error!";
		}
		return HttpRequest::REQUEST_OPENFILE_ERROR;
	}

	int ret_code = HttpRequest::REQUEST_PERFORM_ERROR;
	if (m_multi_download)
	{
		int thread_count = splitDownloadCount(m_total_size);
		m_thread_count = thread_count > m_thread_count ? m_thread_count : thread_count;
		if (m_thread_count <= 1)
		{
			m_multi_download = false;
		}
	}
	char ch[64];
	sprintf_s(ch, "multi-download: %d; thread-count: %d\n", (int)m_multi_download, m_thread_count);
	OutputDebugStringA(ch);

	//文件大小有分开下载的必要并且服务器支持多线程下载时，启用多线程下载
	if (m_multi_download)
	{
		long gap = static_cast<long>(m_total_size) / m_thread_count;
		std::vector<HANDLE> threads;

		for (int i = 0; i < m_thread_count; i++)
		{
			ThreadChunk* thread_chunk = new ThreadChunk;
			thread_chunk->_fp = fp;
			thread_chunk->_helper = this;

			if (i < m_thread_count - 1)
			{
				thread_chunk->_startidx = i * gap;
				thread_chunk->_endidx = thread_chunk->_startidx + gap - 1;
			}
			else
			{
				thread_chunk->_startidx = i * gap;
				thread_chunk->_endidx = -1;
			}

			UINT thread_id;
			HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, &CURLWrapper::downloadProc, thread_chunk, 0, &thread_id);
			threads.push_back(hThread);
		}

		WaitForMultipleObjects(threads.size(), &threads[0], TRUE, INFINITE);
		for (int i = 0; i < threads.size(); ++i)
		{
			HANDLE handle = threads[i];
			CloseHandle(handle);
		}
	}
	else
	{
		ThreadChunk* thread_chunk = new ThreadChunk;
		thread_chunk->_fp = fp;
		thread_chunk->_helper = this;
		thread_chunk->_startidx = 0;
		thread_chunk->_endidx = 0;
		download(thread_chunk);
	}

	fclose(fp);

	if (!isFailed())
	{
		ret_code = HttpRequest::REQUEST_OK;
		MoveFileExA(out_file_name.c_str(), file_name.c_str(), MOVEFILE_REPLACE_EXISTING);
	}
	else
	{
		DeleteFileA(out_file_name.c_str());
	}

	return ret_code;
}

int CURLWrapper::doUpload2()
{
	CURL* curl = curl_easy_init();
	if (curl)
	{
		std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
		HttpManager::set_share_handle(curl);

		curl_slist*	http_headers = nullptr;
		CURLcode curl_code = publicSetoptMethod(curl, http_headers);
		if (curl_code != CURLE_OK)
		{
			if (reply.get())
			{
				const char* err_string = curl_easy_strerror(curl_code);
				reply->m_error_string = err_string;
			}
			setFailed(true);
			curl_slist_free_all(http_headers);
			curl_easy_cleanup(curl);
			return curl_code;
		}

		if (reply.get())
		{
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->m_receive_content);
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_upload_callback);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		}

		struct curl_httppost* httppost = NULL;
		struct curl_httppost* lastpost = NULL;
		/*
		Fill in the file upload field. This makes libcurl load data from
		the given file name when curl_easy_perform() is called.
		*/
		curl_formadd(&httppost,
					 &lastpost,
					 CURLFORM_COPYNAME, "sendfile",
					 CURLFORM_FILE, m_strUploadFile.c_str(),
					 CURLFORM_END);

		/* Fill in the filename field */
		curl_formadd(&httppost,
					 &lastpost,
					 CURLFORM_COPYNAME, "filename",
					 CURLFORM_COPYCONTENTS, m_strTargetName.c_str(),
					 CURLFORM_END);

		/* Fill in the path field */
		curl_formadd(&httppost,
					 &lastpost,
					 CURLFORM_COPYNAME, "path",
					 CURLFORM_COPYCONTENTS, m_strTargetPath.c_str(),
					 CURLFORM_END);

		/* Fill in the submit field too, even if this is rarely needed */
		curl_formadd(&httppost,
					 &lastpost,
					 CURLFORM_COPYNAME, "submit",
					 CURLFORM_COPYCONTENTS, "send",
					 CURLFORM_END);

		curl_easy_setopt(curl, CURLOPT_HTTPPOST, httppost);

		curl_code = curl_easy_perform(curl);
		if (curl_code == CURLE_OPERATION_TIMEDOUT)
		{
			int retry_count = m_retry_times;
			while (!isCanceled() && retry_count > 0)
			{
				curl_code = curl_easy_perform(curl);
				if (curl_code != CURLE_OPERATION_TIMEDOUT)
				{
					break;
				}
				retry_count--;
			}
		}
		
		if (reply.get())
		{
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->m_http_code);
			if (curl_code != CURLE_OK || reply->m_http_code != 200)
			{
				const char* err_string = curl_easy_strerror(curl_code);
				reply->m_error_string = err_string;
			}
		}
		
		if (httppost)
		{
			curl_formfree(httppost);
		}
		if (http_headers)
		{
			curl_slist_free_all(http_headers);
		}
		curl_easy_cleanup(curl);
		return curl_code;
	}

	return CURLE_FAILED_INIT;
}

int CURLWrapper::doUpload()
{
	CURL* curl = curl_easy_init();
	if (curl)
	{
		std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
		HttpManager::set_share_handle(curl);

		curl_slist*	http_headers = nullptr;
		CURLcode curl_code = publicSetoptMethod(curl, http_headers);
		if (curl_code != CURLE_OK)
		{
			if (reply.get())
			{
				const char* err_string = curl_easy_strerror(curl_code);
				reply->m_error_string = err_string;
			}
			setFailed(true);
			curl_slist_free_all(http_headers);
			curl_easy_cleanup(curl);
			return curl_code;
		}

		FILE * file;
		errno_t err = fopen_s(&file, m_strUploadFile.c_str(), "rb");
		if (err != 0 || nullptr == file)
		{
			std::cout << "open file error!" << std::endl;
			curl_code = CURLE_READ_ERROR;
			setFailed(true);
			curl_slist_free_all(http_headers);
			curl_easy_cleanup(curl);
			return curl_code;
		}

		if (reply.get())
		{
			/* get the file size of the local file */
			struct stat file_info = {0};
			stat(m_strUploadFile.c_str(), &file_info);

			curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
			curl_easy_setopt(curl, CURLOPT_READDATA, file);
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
				(curl_off_t)file_info.st_size);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->m_receive_content);
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_upload_callback);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		}

		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		/* HTTP PUT please */
		curl_easy_setopt(curl, CURLOPT_PUT, 1L);

		curl_code = curl_easy_perform(curl);
		if (curl_code == CURLE_OPERATION_TIMEDOUT)
		{
			int retry_count = m_retry_times;
			while (!isCanceled() && retry_count > 0)
			{
				curl_code = curl_easy_perform(curl);
				if (curl_code != CURLE_OPERATION_TIMEDOUT)
				{
					break;
				}
				retry_count--;
			}
		}

		if (reply.get())
		{
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->m_http_code);
			if (curl_code != CURLE_OK || reply->m_http_code != 200)
			{
				const char* err_string = curl_easy_strerror(curl_code);
				reply->m_error_string = err_string;
			}
		}

		fclose(file);
		if (http_headers)
		{
			curl_slist_free_all(http_headers);
		}
		curl_easy_cleanup(curl);
		return curl_code;
	}

	return CURLE_FAILED_INIT;
}

int CURLWrapper::download(ThreadChunk* thread_chunk)
{
	CURLcode curl_code = CURLE_FAILED_INIT;
	CURL* curl = curl_easy_init();
	if (curl)
	{
		std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
		HttpManager::set_share_handle(curl);

		curl_slist*	http_headers = nullptr;
		CURLcode curl_code = publicSetoptMethod(curl, http_headers);
		if (curl_code != CURLE_OK)
		{
			if (reply.get())
			{
				const char* err_string = curl_easy_strerror(curl_code);
				reply->m_error_string = err_string;
			}
			setFailed(true);
			curl_slist_free_all(http_headers);
			curl_easy_cleanup(curl);
			return curl_code;
		}

		curl_easy_setopt(curl, CURLOPT_POST, 0L);
		if (reply.get())
		{
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, thread_chunk);
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_download_callback);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, thread_chunk);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
			curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 5L);
		}

		//const char* user_agent = ("Mozilla/5.0 (Windows NT 5.1; rv:5.0) Gecko/20100101 Firefox/5.0");
		//curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, user_agent);

		if (thread_chunk && thread_chunk->_endidx != 0)
		{
			std::string range;
			std::ostringstream ostr;
			if (thread_chunk->_endidx > 0)
			{
				ostr << thread_chunk->_startidx << "-" << thread_chunk->_endidx;
			}
			else
			{
				ostr << thread_chunk->_startidx << "-";
			}

			range = ostr.str();
			curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
		}

		curl_code = curl_easy_perform(curl);
		if (curl_code == CURLE_OPERATION_TIMEDOUT)
		{
			int retry_count = m_retry_times;
			while (!isFailed() && retry_count > 0)
			{
				curl_code = curl_easy_perform(curl);
				if (curl_code != CURLE_OPERATION_TIMEDOUT)
				{
					break;
				}
				retry_count--;
			}
		}

		bool chunk_failed = false;
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (curl_code == CURLE_OK && (http_code >= 200 && http_code <= 300))
		{
			chunk_failed = false;
		}
		else
		{
			chunk_failed = true;
		}

		if (chunk_failed)
		{
			setFailed(true);
			if (reply.get())
			{
				if (curl_code != CURLE_OK)
				{
					const char* err_string = curl_easy_strerror(curl_code);
					reply->m_error_string = err_string;
				}
				else
				{
					char ch[32];
					sprintf_s(ch, "http code[%d]", http_code);
					reply->m_error_string = ch;
				}
			}
		}
		if (reply.get())
		{
			reply->m_http_code = http_code;
		}

		curl_slist_free_all(http_headers);
		curl_easy_cleanup(curl);
	}

	if (thread_chunk)
	{
		delete thread_chunk;
	}

	return curl_code;
}

INT64 CURLWrapper::getDownloadFileSize()
{
	if (m_url.empty())
	{
		return -1;
	}
	else
	{
		INT64 file_size = -1;
		CURL* curl_handle = curl_easy_init();
		if (curl_handle)
		{
			std::shared_ptr<HttpReply> reply = HttpManager::globalInstance()->getReply(m_id);
			HttpManager::set_share_handle(curl_handle);

			curl_slist*	http_headers = nullptr;
			CURLcode curl_code = publicSetoptMethod(curl_handle, http_headers);
			if (curl_code != CURLE_OK)
			{
				if (reply.get())
				{
					const char* err_string = curl_easy_strerror(curl_code);
					reply->m_error_string = err_string;
				}
				setFailed(true);
				curl_slist_free_all(http_headers);
				curl_easy_cleanup(curl_handle);
				return -1;
			}

			curl_easy_setopt(curl_handle, CURLOPT_HEADER, 1L);
			curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
			//curl_easy_setopt(curl_handle, CURLOPT_RANGE, "0-1");
			if (reply.get())
			{
				curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_callback);
				curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &reply->m_receive_header);
			}

			long http_code = 0;
			curl_code = curl_easy_perform(curl_handle);
			curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
			if (curl_code == CURLE_OK && http_code == 200)
			{
				double size = -1.0;
				curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
				file_size = size;

				if (reply.get())
				{
					//匹配"Accept-Ranges: bytes" 则证明支持多线程下载
					std::regex pattern("Accept-Ranges: bytes", std::regex::icase);
					m_multi_download = std::regex_search(reply->m_receive_header, pattern);
				}
			}
			else if (reply.get())
			{
				const char* err_string = curl_easy_strerror(curl_code);
				reply->m_error_string = err_string;
			}
			if (reply.get())
			{
				reply->m_http_code = http_code;
			}

			curl_easy_cleanup(curl_handle);
		}

		return file_size;
	}
}

int CURLWrapper::splitDownloadCount(INT64 total_size)
{
	const INT64 size_2MB = 2 * 1024 * 1024;
	const INT64 size_10MB = 10 * 1024 * 1024;
	const INT64 size_50MB = 50 * 1024 * 1024;

	int max_count = DEFAULT_DOWNLOAD_THREAD_COUNT * 2;

	if (total_size <= size_2MB)
	{
		return 1;
	}
	else if (total_size > size_2MB && total_size <= size_10MB)
	{
		return static_cast<int>(total_size / (size_2MB));
	}
	else if (total_size > size_10MB && total_size <= size_50MB)
	{
		return DEFAULT_DOWNLOAD_THREAD_COUNT;
	}
	else
	{
		int count = static_cast<int>(total_size / size_10MB);
		return count > max_count ? max_count : count;
	}

	return 1;
}

UINT WINAPI CURLWrapper::downloadProc(LPVOID param)
{
	ThreadChunk* thread_chunk = reinterpret_cast<ThreadChunk*>(param);
	if (thread_chunk)
	{
		CURLInterface* helper = thread_chunk->_helper;
		if (helper)
		{
			CURLWrapper *pcurl = dynamic_cast<CURLWrapper *>(helper);
			if (pcurl)
			{
				return pcurl->download(thread_chunk);
			}
		}
	}
	return -1;
}

void CURLWrapper::reset()
{
	setRunning(false);
	setCanceled(false);
	setFailed(false);
	m_multi_download = false;
	m_thread_count = 1;
	m_total_size = 0.0;
	m_current_size = 0.0;
}

//////////////////////////////////////////////////////////////////////////
HttpRequest::HttpRequest()
	: m_helper(new CURLWrapper)
{
	TRACE_CLASS_CONSTRUCTOR(HttpRequest);
	HttpManager::globalInstance();
}

HttpRequest::~HttpRequest()
{
	TRACE_CLASS_DESTRUCTOR(HttpRequest);
	m_helper = nullptr;
}

int HttpRequest::setRetryTimes(int retry_times)
{
	if (m_helper.get())
	{
		return m_helper->setRetryTimes(retry_times);
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestTimeout(long time_out)
{
	if (m_helper.get())
	{
		return m_helper->setRequestTimeout(time_out);
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestUrl(const std::string& url)
{
	if (m_helper.get())
	{
		return m_helper->setRequestUrl(url);
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setFollowLocation(bool follow_location)
{
	if (m_helper.get())
	{
		return m_helper->setFollowLocation(follow_location);
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setPostData(const std::string& message)
{
	return setPostData(message.c_str(), message.size());
}

int HttpRequest::setPostData(const char* data, unsigned int size)
{
	if (m_helper.get())
	{
		return m_helper->setPostData(data, size);
	}
	return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestHeader(const std::map<std::string, std::string>& headers)
{
	if (m_helper.get())
	{
		for (auto it = headers.begin(); it != headers.end(); ++it)
		{
			std::string header = it->first;
			header += ": ";
			header += it->second;
			m_helper->setRequestHeader(header);
		}
		return REQUEST_OK;
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestHeader(const std::string& header)
{
	if (m_helper.get())
	{
		return m_helper->setRequestHeader(header);
	}
	return REQUEST_INIT_ERROR;
}

int HttpRequest::setRequestProxy(const std::string& proxy, long proxy_port)
{
	if (m_helper.get())
	{
		return m_helper->setRequestProxy(proxy, proxy_port);
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setDownloadFile(const std::string& file_path, int thread_count /* = 5 */)
{
	if (m_helper.get())
	{
		m_helper->setDownloadFile(file_path);
		m_helper->setDownloadThreadCount(thread_count);
		return REQUEST_OK;
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path)
{
	if (m_helper.get())
	{
		m_helper->setUploadFile(file_path, target_name, target_path);
		return REQUEST_OK;
	}

	return REQUEST_INIT_ERROR;
}

int HttpRequest::setResultCallback(ResultCallback rc)
{
	if (m_helper.get())
	{
		m_helper->setResultCallback(rc);
		return REQUEST_OK;
	}

	return REQUEST_INIT_ERROR;
}

int	HttpRequest::setProgressCallback(ProgressCallback pc)
{
	if (m_helper.get())
	{
		m_helper->setProgressCallback(pc);
		return REQUEST_OK;
	}

	return REQUEST_INIT_ERROR;
}

std::shared_ptr<HttpReply> HttpRequest::perform(RequestType rtype, CallType ctype)
{
	std::shared_ptr<HttpReply> reply = nullptr;
	if (m_helper.get())
	{
		if (m_helper->isRunning())
			m_helper->cancel();

		int nId = m_helper->requestId();
		m_helper->setRequestType(rtype);

		reply = std::make_shared<HttpReply>(nId);
		HttpManager::globalInstance()->addReply(reply);

		if (ctype == Sync)
		{
			m_helper->perform();
		}
		else if (ctype == Async)
		{
			std::shared_ptr<HttpTask> task = std::make_shared<HttpTask>(true);
			task->attach(m_helper);
			HttpManager::addTask(task);
		}
	}

	return reply;
}

bool HttpRequest::cancel(int requestId)
{
	return HttpManager::abortTask(requestId);
}

bool HttpRequest::cancelAll()
{
	return HttpManager::abortAllTask();
}

void HttpRequest::globalCleanup()
{
	return HttpManager::globalCleanup();
}