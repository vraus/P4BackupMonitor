#pragma once

#include <QMainWindow>
#include <QJsonObject>
#include "core/collector.h"
#include "core/health.h"

class QButtonGroup;
class QVBoxLayout;
class OverviewPage;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void buildSidebar();
    void addSection(const QString &title);
    void addNavButton(const QString &text, int pageIndex);
    void openSettings();

    void openStatusFile();                        // menu Fichier > Ouvrir un état enregistré
    void showStatus(const QJsonObject &status);   // le collecteur a fini
    void showFailure(const QString &message);     // le collecteur a échoué

    Ui::MainWindow *ui;
    QVBoxLayout *m_sideLayout = nullptr;
    QButtonGroup *m_nav = nullptr;
    OverviewPage *m_overview = nullptr;

    Collector m_collector;
    QJsonObject m_status; // dernier état reçu
};