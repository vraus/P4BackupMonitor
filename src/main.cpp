#include "ui/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QStyleFactory>
#include <QTimer>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("HadrienBelleville");
    QApplication::setApplicationName("P4BackupMonitor");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QFile qss(":/style.qss");
    if(qss.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    app.setFont(QFont("Segoe UI", 10));
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    MainWindow w;
    w.show();
    return app.exec();
}
