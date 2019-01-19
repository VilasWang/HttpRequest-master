#ifndef INTERNET_H
#define INTERNET_H

#include <QtWidgets/QMainWindow>
#include <QTime>
#include <memory>
#include "ui_curltool.h"
#include "HttpRequest.h"
#include "HttpReply.h"

class CurlTool : public QMainWindow
{
	Q_OBJECT

public:
	CurlTool(QWidget* parent = 0);
	~CurlTool();

	static CurlTool *instance() { return ms_instance; }
	static bool isInstantiated() { return (ms_instance != nullptr); }

	bool event(QEvent* event) Q_DECL_OVERRIDE;

	public Q_SLOTS:
	void onStartTask();
	void onAbortTask();

	private Q_SLOTS:
	void onDownload();
	void onUpload();
	void onFormPost();
	void onGetRequest();
	void onPostRequest();

	void onUpdateDefaultInfos();
	void onGetSaveDirectory();
	void onGetUploadFile();

	static void onRequestResultCallback(int id, bool success, const std::string& data, const std::string& error_string);
	static void onProgressCallback(int id, bool bDownload, qint64 total_size, qint64 downloaded_size);

private:
	void initialize();
	void unIntialize();
	QString bytes2String(qint64 bytes);
	void appendMsg(const QString& strMsg, bool bQDebug = true);
	void reset();
	//获取系统默认下载目录
	QString getDefaultDownloadDir();

	public Q_SLOTS:
	void onProgress(quint64 dltotal, quint64 dlnow, quint64 ultotal, quint64 ulnow);

private:
	Ui::networkClass ui;

	qint64 m_nbytesReceived;
	qint64 m_nbytesTotalDownload;
	QString m_strTotalDownload;
	qint64 m_nbytesSent;
	qint64 m_nbytesTotalUpload;
	QString m_strTotalUpload;

private:
	static CurlTool *ms_instance;

	static int m_nTotalNum;
	static int m_nFailedNum;
	static int m_nSuccessNum;

	static QTime m_timeStart;
	static QMap<int, std::shared_ptr<HttpReply>> m_mapReplys;
};

#endif // INTERNET_H
