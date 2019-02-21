#include <QtWidgets/QApplication>
#include <QDir>
#include <QDebug>
#include "curltool.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	CurlTool::instance()->show();
	int ret = a.exec();
	CurlTool::destroyInstance();
	return ret;
}
