#ifndef INTERNET_H
#define INTERNET_H

#include <QtWidgets/QMainWindow>
#include <QTime>
#include <memory>
#include <QMutex>
#include "ui_curltool.h"
#include "HttpRequest.h"
#include "HttpReply.h"

class CurlTool : public QMainWindow
{
    Q_OBJECT

public:
    ~CurlTool();

    static CurlTool *getInstance()
    {
        if (nullptr == ms_instance)
        {
            ms_instance = new CurlTool;
        }
        return ms_instance;
    }

    static void destroyInstance()
    {
        if (nullptr != ms_instance)
        {
            delete ms_instance;
            ms_instance = nullptr;
        }
    }

    static bool isInstantiated() { return (ms_instance != nullptr); }

    bool event(QEvent* event) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void onStartTask();
    void onAbortTask();
    void onProgress(quint64 dltotal, quint64 dlnow, quint64 ultotal, quint64 ulnow);
    void onDownload();
    void onUpload();
    void onFormPost();
    void onGetRequest();
    void onPostRequest();
    void onHeadRequest();

    void onUpdateDefaultInfos();
    void onGetSaveDirectory();
    void onGetUploadFile();

private:
    CurlTool(QWidget* parent = 0);

    void initialize();
    void unIntialize();

    //获取系统默认下载目录
    QString getDefaultDownloadDir();
    QString bytes2String(qint64 bytes);
    void appendMsg(const QString& strMsg, bool bQDebug = true);
    void reset();

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
};

#endif // INTERNET_H
