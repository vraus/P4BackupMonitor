#include "ui/mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName("HadrienBelleville");
    QApplication::setApplicationName("P4BackupMonitor");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));

    MainWindow w;
    w.show();
    return app.exec();
}
