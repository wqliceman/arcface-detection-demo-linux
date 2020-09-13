#include "ArcFaceDetection.h"
#include <QtWidgets/QApplication>
#include "UiStyle.h"

#include <QDebug>
#include <QThread>

#include <QFileDialog>

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    auto const filename = [](char const * name)
    {
        if (!name) { return ""; }
        for (auto p = name; *p; ++p)
        {
            name = *p == '/' || *p == '\\'? (p+1): name;
        }
        return name;
    }(context.file);
    char const * levels[] = {
        "debug", "warning", "critical", "fatal", "info"
    };

    fprintf(stderr, "%-8s %p %s (%s:%d)\n",
            levels[type], QThread::currentThreadId(), msg.toLocal8Bit().constData(), filename, context.line
    );
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageOutput);

    qDebug() << "enter main";

    QApplication a(argc, argv);

    CommonHelper::setStyle("://resources/style.qss");

	ArcFaceDetection w;
    w.show();
	return a.exec();
}
