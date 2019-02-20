#ifndef __HTTP_REQUEST_H
#define __HTTP_REQUEST_H

#include <string>
#include <map>
#include <vector>

#include "HttpManager.h"
#include "HttpReply.h"
#include "httprequestdef.h"
#include "HttpRequest_global.h"


class CURLWrapper;
// class HttpRequest - Http�����ࣨlibcurl��
class HTTP_REQUEST_EXPORT HttpRequest
{
public:
	enum IOMode
	{
		Sync,
		Async,
	};

	enum RequestResult
	{
		REQUEST_OK,
		REQUEST_INVALID_OPT,
		REQUEST_PERFORM_ERROR,
		REQUEST_INIT_ERROR,
	};

public:
	explicit HttpRequest();
	~HttpRequest();

	// ��ʼ��libcurl��Դ����ʼ���̳߳�(�������߳���)
	static void globalInit();

	// ȡ��������������libcurl��Դ�����̳߳������߳�ȡ�������˳�(�������߳���)
	static void globalCleanup();

public:
	// ��ʼ���󣬲�����HttpReply. 
	// 1��ͬ���������ֱ�ӵ���HttpReply�Ľӿڻ�ȡ���
	// 2���첽������������첽�ص��ӿڣ��������ʱ�Զ��ص���ȡ���
	// ע��㣺	1.�첽�����ʱ�� ��Ҫ�ѷ��ص�std::shared_ptr<HttpReply>����id����������
	//				���յ������ص���ʱ���ٰ�std::shared_ptr<HttpReply>�ÿգ���Ȼ���ղ��������ص���
	//			2.�첽����Ļص��ӿڶ�����curlִ�еĹ����̵߳��ã����Ը��ݲ�ͬ������Լ�����һЩ����
	//				����ص��ӿ��м���������Դ���߰ѻص������post�����Լ����߳��д���(�ȽϺõ��Ǻ���)
	std::shared_ptr<HttpReply> perform(HttpRequestType rtype, IOMode mode = Async);
	// ȡ������
	static bool cancel(int requestId);
	// ȡ����������
	static bool cancelAll();

	// �첽�ص�api
	// ��ò�Ҫ����ķǾ�̬��Ա����������ص�����ʱ��������
	int setResultCallback(ResultCallback rc);
	int	setProgressCallback(ProgressCallback pc);

	int setRetryTimes(int retry_times);
	int setTimeout(long time_out = 0); // ����ʱ��second��
	int setUrl(const std::string& url);
	int setProxy(const std::string& proxy, long proxy_port);
	// set http redirect follow location
	int setFollowLocation(bool follow_location);
	// set http request header, for example : Range:bytes=554554-
	int setHeader(const std::map<std::string, std::string>& headers);
	int setHeader(const std::string& header);

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
