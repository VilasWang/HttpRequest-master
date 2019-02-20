#include <QDebug>
#include <QDir>
#include <QTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextCodec>
#include <QFileDialog>
#include <QMessageBox>
#include <QButtonGroup>
#include <QListView>
#include <QStandardPaths>
#include <memory>
#include "curltool.h"
#include "HttpRequest.h"

#define POST_GET_TEST_NUMBER 100
//������Apache http������
#define HTTP_SERVER_IP "127.0.0.1"
#define HTTP_SERVER_PORT "80"

//////////////////////////////////////////////////////////////////////////
namespace
{
	const int RequestFinish = QEvent::User + 150;
	const int Rrogress = QEvent::User + 151;
}

class RequestFinishEvent : public QEvent
{
public:
	RequestFinishEvent() : QEvent(QEvent::Type(RequestFinish))
		, bFinishAll(false)
		, success(false)
		, id(0){}

	int id; 
	bool success;
	QString strMsg;
	bool bFinishAll;
};

class ProgressEvent : public QEvent
{
public:
	ProgressEvent() : QEvent(QEvent::Type(Rrogress))
		, total(0)
		, current(0)
		, isDownload(false)
	{
	}

	qint64 total;
	qint64 current;
	bool isDownload;
};
//////////////////////////////////////////////////////////////////////////

int CurlTool::m_nTotalNum = 0;
int CurlTool::m_nFailedNum = 0;
int CurlTool::m_nSuccessNum = 0;
QTime CurlTool::m_timeStart;
std::map<int, std::shared_ptr<HttpReply>> CurlTool::m_mapReplys;
CurlTool *CurlTool::ms_instance = nullptr;

CurlTool::CurlTool(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	setFixedSize(630, 610);
	setWindowTitle(QStringLiteral("Curl�������󹤾�"));

	ui.btn_abort->setEnabled(false);
	ui.textEdit_output->setReadOnly(true);
	ui.progressBar_d->setFormat("%p%(%v / %m)");
	ui.progressBar_u->setFormat("%p%(%v / %m)");

	ui.cmb_multiDownload->setView(new QListView(this));
	for (int i = 1; i <= 10; ++i)
	{
		ui.cmb_multiDownload->addItem(QString::number(i));
	}
	ui.cmb_multiDownload->setCurrentText(QString::number(5));

	QButtonGroup* bg1 = new QButtonGroup(this);
	bg1->addButton(ui.cb_download);
	bg1->addButton(ui.cb_upload);
	bg1->addButton(ui.cb_get);
	bg1->addButton(ui.cb_post);
	bg1->addButton(ui.cb_formpost);
	bg1->addButton(ui.cb_head);
	bg1->setExclusive(true);

	connect(ui.btn_start, SIGNAL(clicked()), this, SLOT(onStartTask()));
	connect(ui.btn_abort, SIGNAL(clicked()), this, SLOT(onAbortTask()));
	connect(ui.btn_browser1, SIGNAL(clicked()), this, SLOT(onGetSaveDirectory()));
	connect(ui.btn_browser2, SIGNAL(clicked()), this, SLOT(onGetUploadFile()));
	connect(ui.cb_useDefault, &QAbstractButton::toggled, this, [=](bool checked) {
		if (checked)
		{
			onUpdateDefaultInfos();
		}
	});
	connect(bg1, SIGNAL(buttonToggled(int, bool)), this, SLOT(onUpdateDefaultInfos()));

	initialize();
	onUpdateDefaultInfos();

	ms_instance = this;
}

CurlTool::~CurlTool()
{
	qDebug() << __FUNCTION__ << "(B)";
	unIntialize();
	ms_instance = nullptr;
	qDebug() << __FUNCTION__ << "(E)";
}

void CurlTool::initialize()
{
	HttpRequest::globalInit();
}

void CurlTool::unIntialize()
{
	HttpRequest::globalCleanup();
}

void CurlTool::onUpdateDefaultInfos()
{
	ui.lineEdit_url->clear();
	ui.lineEdit_arg->clear();
	ui.lineEdit_targetname->clear();
	ui.lineEdit_saveDir->clear();
	ui.lineEdit_uploadFile->clear();

	if (ui.cb_useDefault->isChecked())
	{
		if (ui.cb_download->isChecked())
		{
			QString str1 = "timgsa.baidu.com/timg?image&quality=80&size=b9999_10000&sec=1547925666567&di=52db6d8b1f0da19f118cfdbe3d969b10&imgtype=0&src=http%3A%2F%2Fimg.pconline.com.cn%2Fimages%2Fupload%2Fupc%2Ftx%2Fwallpaper%2F1207%2F02%2Fc0%2F12195456_1341200893045.jpg";
			QString str2 = "https://www.python.org/ftp/python/3.7.2/python-3.7.2.exe";
			const QString& strUrl = str2;
			ui.lineEdit_url->setText(strUrl);
			ui.lineEdit_targetname->setText("python-3.7.2.exe");
			ui.lineEdit_saveDir->setText(getDefaultDownloadDir());
		}
		else if (ui.cb_upload->isChecked())
		{
			ui.lineEdit_url->setText("http://127.0.0.1:80/_php/upload.php?filename=upload/test.rar");
			ui.lineEdit_uploadFile->setText("test.rar");
		}
		else if (ui.cb_formpost->isChecked())
		{
			ui.lineEdit_url->setText("http://127.0.0.1:80/_php/formpost.php");
			ui.lineEdit_uploadFile->setText("test.rar");
			ui.lineEdit_saveDir->setText("./upload");		//��Ӧ�ϴ��������ĸ�Ŀ¼�����·��
			ui.lineEdit_targetname->setText("test.rar");
		}
		else if (ui.cb_get->isChecked())
		{
			ui.lineEdit_url->setText("http://m.kugou.com/singer/list/88?json=true");
		}
		else if (ui.cb_post->isChecked())
		{
			const QString& strArg = "userId=121892674&userName=33CxghNmt1FhAA==&st=QQBnAEEAQQBBAEUATAB2AFEAdwBjAEEAQQBBAEEAQQBBAEEAQQBBAEEATAB2AFAANwBoAE4AcwBJ"
				"AC8AbwBWAFMAQQArAEQAVgBIADIAdgAyAHcARgBRAGYANABJAHkAOQA3AFAAYQBkAFMARwBoAEoA"
				"KwBUAEoAcAAzADkAVgBYAFYAMwBDAE4AVABiAHEAZQB3AE4AMAANAAoAOABlAHUANQBBAHMAUwBY"
				"AFEAbQAyAFUAWQBmAHEAMgA1ADkAcQBvAG4AZQBCAFEAYgB5AE8ANwAyAFQAMQB0AGwARwBIADYA"
				"dAB1AGYAYQBxAEoAMwBnAFUARwA4AGoAdQA5AGsAOQBzAFoAYQB1AHAARwBjAE8ANABnADIAegBn"
				"ADIANgB1AEcANwBoAHAAUwBHADIAVQANAAoAWQBmAHEAMgA1ADkAcQBvAG4AZQBCAFEAYgB5AE8A"
				"NwAyAFQAMAA9AA==";

			ui.lineEdit_url->setText("https://passportservice.7fgame.com/HttpService/PlatService.ashx");
			ui.lineEdit_arg->setText(strArg);
		}
		else if (ui.cb_head->isChecked())
		{
			ui.lineEdit_url->setText("http://iso.mirrors.ustc.edu.cn/qtproject/archive/qt/5.12/5.12.1/single/qt-everywhere-src-5.12.1.zip");
		}
	}
}

void CurlTool::onStartTask()
{
	m_nFailedNum = 0;
	m_nSuccessNum = 0;
	m_nTotalNum = 0;
	m_nbytesReceived = 0;
	m_nbytesTotalDownload = 0;
	m_nbytesSent = 0;
	m_nbytesTotalUpload = 0;
	ui.progressBar_d->setValue(0);
	ui.progressBar_u->setValue(0);
	ui.btn_start->setEnabled(false);
	ui.btn_abort->setEnabled(true);

	if (ui.cb_download->isChecked())
	{
		onDownload();
	}
	else if (ui.cb_upload->isChecked())
	{
		onUpload();
	}
	else if (ui.cb_formpost->isChecked())
	{
		onFormPost();
	}
	else if (ui.cb_get->isChecked())
	{
		onGetRequest();
	}
	else if (ui.cb_post->isChecked())
	{
		onPostRequest();
	}
	else if (ui.cb_head->isChecked())
	{
		onHeadRequest();
	}
}

void CurlTool::onAbortTask()
{
	appendMsg("Stop request.");

	HttpRequest::cancelAll();
	ui.btn_abort->setEnabled(false);
	reset();
}

bool CurlTool::event(QEvent* event)
{
	if (event->type() == QEvent::Type(RequestFinish))
	{
		RequestFinishEvent* e = static_cast<RequestFinishEvent*>(event);
		if (nullptr != e)
		{
			appendMsg(e->strMsg, false);
			if (e->bFinishAll)
			{
				reset();
			}
			else
			{
				auto iter = m_mapReplys.find(e->id);
				if (iter != m_mapReplys.end())
				{
					m_mapReplys.erase(iter);
				}
			}
		}

		return true;
	}
	else if (event->type() == QEvent::Type(Rrogress))
	{
		ProgressEvent* e = static_cast<ProgressEvent*>(event);
		if (nullptr != e)
		{
			if (e->isDownload)
			{
				onProgress(e->total, e->current, 0, 0);
			}
			else
			{
				onProgress(0, 0, e->total, e->current);
			}
		}
		return true;
	}
	return __super::event(event);
}

//////////////////////////////////////////////////////////////////////////
//  �����ǵ��÷���
//////////////////////////////////////////////////////////////////////////
void CurlTool::onDownload()
{
	const QString& strUrl = ui.lineEdit_url->text().trimmed();
	if (strUrl.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("url����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	const QString& strSavePath = ui.lineEdit_saveDir->text().trimmed();
	if (strSavePath.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("�ļ�����λ�ò���Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	const QString& strFileName = ui.lineEdit_targetname->text().trimmed();
	if (strFileName.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("�ļ�����λ�ò���Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	QDir dir;
	if (!dir.exists(strSavePath))
	{
		dir.mkpath(strSavePath);
	}

	m_timeStart = QTime::currentTime();
	appendMsg(m_timeStart.toString() + " - Start request[" + strUrl + "]");

	m_nTotalNum = 1;
	const QString& strFilePath = strSavePath + "/" + strFileName;

	HttpRequest request;
	request.setUrl(strUrl.toStdString());
	request.setDownloadFile(strFilePath.toStdString(), ui.cmb_multiDownload->currentText().toInt());
	request.setFollowLocation(true);
	request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	request.setProgressCallback(std::bind(&CurlTool::onProgressCallback,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	std::shared_ptr<HttpReply> reply = request.perform(HttpRequestType::Download, HttpRequest::Async);
	if (reply.get())
	{
		auto iter = m_mapReplys.find(reply->id());
		if (iter == m_mapReplys.end())
		{
			m_mapReplys[reply->id()] = reply;
		}
		else
		{
			qDebug() << "error reply! repeat reply id!";
		}
	}
}

void CurlTool::onUpload()
{
	const QString& strUrl = ui.lineEdit_url->text().trimmed();
	if (strUrl.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("url����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	const QString& strUploadFilePath = ui.lineEdit_uploadFile->text().trimmed();
	if (strUploadFilePath.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("�ϴ��ļ�����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	m_timeStart = QTime::currentTime();
	appendMsg(m_timeStart.toString() + " - Start request[" + strUrl + "]");

	m_nTotalNum = 1;

	HttpRequest request;
	request.setUrl(strUrl.toStdString());
	request.setUploadFile(strUploadFilePath.toStdString());
	request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	request.setProgressCallback(std::bind(&CurlTool::onProgressCallback,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	std::shared_ptr<HttpReply> reply = request.perform(HttpRequestType::Upload, HttpRequest::Async);
	if (reply.get())
	{
		auto iter = m_mapReplys.find(reply->id());
		if (iter == m_mapReplys.end())
		{
			m_mapReplys[reply->id()] = reply;
		}
		else
		{
			qDebug() << "error reply!!!";
		}
	}
}

void CurlTool::onFormPost()
{
	const QString& strUrl = ui.lineEdit_url->text().trimmed();
	if (strUrl.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("url����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	const QString& strUploadFilePath = ui.lineEdit_uploadFile->text().trimmed();
	if (strUploadFilePath.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("�ϴ��ļ�����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	QString strSavePath = ui.lineEdit_saveDir->text().trimmed();
	if (strSavePath.isEmpty())
	{
		strSavePath = ".";
	}

	QString strTargetName = ui.lineEdit_targetname->text().trimmed();
	if (strTargetName.isEmpty())
	{
		QFileInfo fileInfo(strUploadFilePath);
		strTargetName = fileInfo.fileName();
	}

	const QString& strTargetFilePath = strSavePath + "/" + strTargetName;

	m_timeStart = QTime::currentTime();
	appendMsg(m_timeStart.toString() + " - Start request[" + strUrl + "]");

	m_nTotalNum = 1;

	HttpRequest request;
	request.setUrl(strUrl.toStdString());
	request.setUploadFile(strUploadFilePath.toStdString(), strTargetName.toStdString(), strSavePath.toStdString());
	request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	request.setProgressCallback(std::bind(&CurlTool::onProgressCallback,
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	std::shared_ptr<HttpReply> reply = request.perform(HttpRequestType::Upload2, HttpRequest::Async);
	if (reply.get())
	{
		auto iter = m_mapReplys.find(reply->id());
		if (iter == m_mapReplys.end())
		{
			m_mapReplys[reply->id()] = reply;
		}
		else
		{
			qDebug() << "error reply!!!";
		}
	}
}

void CurlTool::onGetRequest()
{
	const QString& strUrl = ui.lineEdit_url->text().trimmed();
	if (strUrl.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("url����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	m_timeStart = QTime::currentTime();
	appendMsg(m_timeStart.toString() + " - Start request[" + strUrl + "]");

	m_nTotalNum = POST_GET_TEST_NUMBER;
	for (int i = 0; i < m_nTotalNum; ++i)
	{
		HttpRequest request;
		request.setUrl(strUrl.toStdString());
		request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback,
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

		std::shared_ptr<HttpReply> reply = request.perform(HttpRequestType::Get, HttpRequest::Async);
		if (reply.get())
		{
			auto iter = m_mapReplys.find(reply->id());
			if (iter == m_mapReplys.end())
			{
				m_mapReplys[reply->id()] = reply;
			}
			else
			{
				qDebug() << "error reply!!!";
			}
		}
	}
}

void CurlTool::onPostRequest()
{
	const QString& strUrl = ui.lineEdit_url->text().trimmed();
	if (strUrl.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("url����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	const QString& strArg = ui.lineEdit_arg->text().trimmed();
	if (strArg.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("��������Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	m_timeStart = QTime::currentTime();
	appendMsg(m_timeStart.toString() + " - Start request[" + strUrl + "]");

	m_mapReplys.clear();
	m_nTotalNum = POST_GET_TEST_NUMBER;
	for (int i = 0; i < m_nTotalNum; ++i)
	{
		HttpRequest request;
		request.setUrl(strUrl.toStdString());
		request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback,
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		std::string strSendData = strArg.toStdString();
		request.setPostData(strSendData.c_str(), strSendData.size());

		std::shared_ptr<HttpReply> reply = request.perform(HttpRequestType::Post, HttpRequest::Async);
		if (reply.get())
		{
			int id = reply->id();
			auto iter = m_mapReplys.find(reply->id());
			if (iter == m_mapReplys.end())
			{
				m_mapReplys[id] = reply;
			}
			else
			{
				qDebug() << "error reply!!!";
			}
		}
	}
}

void CurlTool::onHeadRequest()
{
	const QString& strUrl = ui.lineEdit_url->text().trimmed();
	if (strUrl.isEmpty())
	{
		QMessageBox::information(nullptr, "Tips", QStringLiteral("url����Ϊ��"), QMessageBox::Ok);
		reset();
		return;
	}

	m_timeStart = QTime::currentTime();
	appendMsg(m_timeStart.toString() + " - Start request[" + strUrl + "]");

	m_nTotalNum = 1;

	HttpRequest request;
	request.setUrl(strUrl.toStdString());
	request.setResultCallback(std::bind(&CurlTool::onRequestResultCallback,
							  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	std::shared_ptr<HttpReply> reply = request.perform(HttpRequestType::Head, HttpRequest::Async);
	if (reply.get())
	{
		int id = reply->id();
		auto iter = m_mapReplys.find(reply->id());
		if (iter == m_mapReplys.end())
		{
			m_mapReplys[id] = reply;
		}
		else
		{
			qDebug() << "error reply!!!";
		}
	}
}

void CurlTool::onRequestResultCallback(int id, bool success, const std::string& data, const std::string& error_string)
{
	QString strMsg;
	if (success)
	{
		m_nSuccessNum++;
		strMsg = QString("[async][%1] success.\n%2").arg(id).arg(QString::fromStdString(data));
	}
	else
	{
		m_nFailedNum++;
		strMsg = QString("[async][%1] failed.\n%2").arg(id).arg(QString::fromStdString(error_string));
	}
	//qDebug() << m_nTotalNum << m_nSuccessNum << m_nFailedNum;

	if (CurlTool::isInstantiated())
	{
		RequestFinishEvent* event = new RequestFinishEvent;
		event->id = id;
		event->success = success;
		event->strMsg = strMsg;
		QCoreApplication::postEvent(CurlTool::instance(), event);
	}
	
	if (m_nSuccessNum + m_nFailedNum == m_nTotalNum)
	{
		if (CurlTool::isInstantiated())
		{
			QTime time = QTime::currentTime();
			int msec = m_timeStart.msecsTo(time);
			float sec = (float)msec / 1000;
			strMsg = QString("Time elapsed: %1s.").arg(sec);

			RequestFinishEvent* event = new RequestFinishEvent;
			event->strMsg = strMsg;
			event->bFinishAll = true;
			QCoreApplication::postEvent(CurlTool::instance(), event);
		}
	}
}

void CurlTool::onProgressCallback(int id, bool bDownload, qint64 total_size, qint64 current_size)
{
	if (CurlTool::isInstantiated())
	{
		ProgressEvent* event = new ProgressEvent;
		event->isDownload = bDownload;
		event->total = total_size;
		event->current = current_size;
		QCoreApplication::postEvent(CurlTool::instance(), event);
	}
}

void CurlTool::onGetSaveDirectory()
{
	QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
		"/home",
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!dir.isNull() && !dir.isEmpty())
	{
		ui.lineEdit_saveDir->setText(dir);
	}
}

void CurlTool::onGetUploadFile()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
		"/home",
		tr("Files (*.*)"));

	if (!fileName.isNull() && !fileName.isEmpty())
	{
		ui.lineEdit_uploadFile->setText(fileName);
	}
}

void CurlTool::onProgress(quint64 dltotal, quint64 dlnow, quint64 ultotal, quint64 ulnow)
{
	if (dlnow > m_nbytesReceived)
	{
		m_nbytesReceived = dlnow;
		if (dltotal > m_nbytesTotalDownload)
		{
			m_nbytesTotalDownload = dltotal;
			m_strTotalDownload = bytes2String(dltotal);
			ui.progressBar_d->setMaximum(dltotal);
		}
		const QString& strReceived = bytes2String(dlnow);
		ui.progressBar_d->setValue(dlnow);
		//appendMsg(QStringLiteral("���أ�%2 / %3").arg(strReceived).arg(m_strTotalDownload), false);
	}

	if (ulnow > m_nbytesSent)
	{
		m_nbytesSent = ulnow;
		if (ultotal > m_nbytesTotalUpload)
		{
			m_nbytesTotalUpload = ultotal;
			m_strTotalUpload = bytes2String(ultotal);
			ui.progressBar_u->setMaximum(ultotal);
		}
		const QString& strSent = bytes2String(ulnow);
		ui.progressBar_u->setValue(ulnow);
		//appendMsg(QStringLiteral("�ϴ���%2 / %3").arg(strSent).arg(m_strTotalUpload), false);
	}
}

void CurlTool::reset()
{
	m_nFailedNum = 0;
	m_nSuccessNum = 0;
	m_nTotalNum = 0;
	m_nbytesReceived = 0;
	m_nbytesTotalDownload = 0;
	m_nbytesSent = 0;
	m_nbytesTotalUpload = 0;

	ui.progressBar_d->setMaximum(100);
	ui.progressBar_d->setValue(0);
	ui.progressBar_u->setMaximum(100);
	ui.progressBar_u->setValue(0);

	ui.btn_start->setEnabled(true);
	ui.btn_abort->setEnabled(false);
	m_mapReplys.clear();
}

QString CurlTool::bytes2String(qint64 bytes)
{
	QString str;
	if (bytes < 1024)
	{
		str = QString("%1B").arg(bytes);
	}
	else if (bytes < 1024 * 1024)
	{
		bytes = bytes / 1024;
		str = QString("%1KB").arg(bytes);
	}
	else if (bytes < 1024 * 1024 * 1024)
	{
		qreal dSize = (qreal)bytes / 1024 / 1024;
		char ch[8] = { 0 };
		sprintf_s(ch, "%.2f", dSize);
		str = QString("%1MB").arg(ch);
	}
	else
	{
		qreal dSize = (qreal)bytes / 1024 / 1024 / 1024;
		char ch[8] = { 0 };
		sprintf_s(ch, "%.2f", dSize);
		str = QString("%1GB").arg(ch);
	}
	return str;
}

void CurlTool::appendMsg(const QString& strMsg, bool bQDebug)
{
	if (!strMsg.isEmpty())
	{
		if (bQDebug)
		{
			qDebug() << strMsg;
		}

		ui.textEdit_output->append(strMsg);
		ui.textEdit_output->append("");
	}
}

QString CurlTool::getDefaultDownloadDir()
{
	const QStringList& lstDir = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
	if (!lstDir.isEmpty())
	{
		return lstDir[0];
	}
	return "download/";
}