#ifndef __HTTP_REQUEST_H
#define __HTTP_REQUEST_H

#include <string>
#include <map>
#include <vector>

#include "HttpManager.h"
#include "HttpReply.h"
#include "HttpRequest_global.h"


class CURLWrapper;
// class HttpRequest - Http�����ࣨlibcurl��
class HTTP_REQUEST_EXPORT HttpRequest
{
public:
	enum RequestType
	{
		Post,
		Get,
		Download,
		Upload,		//HTTP put ��ʽ�ϴ��ļ� (�ļ�����·������url��ָ��)
		Upload2,	//HTTP Multipart formpost ��ʽ�ϴ��ļ�(��С�����ڷ�����post���ݵĴ�С)
		Unkonwn = -1
	};

	enum CallType
	{
		Sync,
		Async,
	};

	enum RequestResult
	{
		REQUEST_OK,
		REQUEST_INVALID_OPT,
		REQUEST_PERFORM_ERROR,
		REQUEST_OPENFILE_ERROR,
		REQUEST_INIT_ERROR,
	};

public:
	explicit HttpRequest();
	~HttpRequest();

	// ��ʼ���󣬲�����HttpReply. 
	// 1��ͬ���������ֱ�ӵ���HttpReply�Ľӿڻ�ȡ���
	// 2���첽������������첽�ص��ӿڣ��������ʱ�Զ��ص���ȡ���
	std::shared_ptr<HttpReply> perform(RequestType rtype, CallType ctype = Async);
	// ȡ������
	static bool cancel(int requestId);
	// ȡ����������
	static bool cancelAll();
	// ȡ������������������curl��Դ�����̳߳������߳�ȡ�������˳�
	static void globalCleanup();

	// �첽�ص�api
	// ��ò�Ҫ����ķǾ�̬����������ص�ʱ��������
	int setResultCallback(ResultCallback rc);
	int	setProgressCallback(ProgressCallback pc);

	int setRetryTimes(int retry_times);
	int setRequestTimeout(long time_out = 0); // ����ʱ��second��
	int setRequestUrl(const std::string& url);
	int setRequestProxy(const std::string& proxy, long proxy_port);
	// set http redirect follow location
	int setFollowLocation(bool follow_location);
	// set http request header, for example : Range:bytes=554554-
	int setRequestHeader(const std::map<std::string, std::string>& headers);
	int setRequestHeader(const std::string& header);

	// �����ø÷�������post��ʽ���󣬷�����curlĬ��get
	int setPostData(const std::string& data);
	int setPostData(const char* data, unsigned int size);

	int setDownloadFile(const std::string& file_path, int thread_count = 5);
	//HTTP put ��ʽ�ϴ��ļ� (�ļ�����·����url����Ҫָ��)
	int setUploadFile(const std::string& file_path);
	//HTTP Multipart formpost ��ʽ�ϴ��ļ�
	int setUploadFile(const std::string& file_path, const std::string& target_name, const std::string& target_path);

private:
	std::shared_ptr<CURLWrapper> m_helper;
};

#endif  /*__HTTP_REQUEST_H*/
