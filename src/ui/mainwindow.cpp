#include "mainwindow.h"
#include "src/ui/ui_mainwindow.h"
#include "ui/mainwindow.h"

#include "ui/overviewpage.h"

#include <QButtonGroup>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QPushButton>

// Numéros des pages dans le QStackedWidget "pages"
enum Page { OverviewPageIndex = 0, LogsPageIndex = 1, OperationsPageIndex = 2 };

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("P4 Backup Monitor"));
    resize(1280, 800);

    // Barre latérale à gauche (largeur fixe), pages à droite (tout le reste)
    auto *row = qobject_cast<QHBoxLayout *>(ui->centralwidget->layout());
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->removeWidget(ui->sidebar);
    row->removeWidget(ui->pages);
    row->addWidget(ui->sidebar, 0);
    row->addWidget(ui->pages, 1);
    ui->sidebar->setFixedWidth(224);

    // Designer crée deux pages vides dans un Stacked Widget : on les retire
    while (ui->pages->count() > 0) {
        QWidget *w = ui->pages->widget(0);
        ui->pages->removeWidget(w);
        delete w;
    }

    // Pages, dans l'ordre de l'enum Page
    m_overview = new OverviewPage(this);
    ui->pages->addWidget(m_overview);                                         // 0
    ui->pages->addWidget(new QLabel(tr("Journaux : à venir"), this));         // 1
    ui->pages->addWidget(new QLabel(tr("Lancer un script : à venir"), this)); // 2

    buildSidebar();

    // Menu Fichier : charger un JSON déjà collecté (démo, ou état copié depuis un serveur)
    ui->menuFichiers->addAction(tr("Ouvrir un état enregistré…"), this, &MainWindow::openStatusFile);

    // Le collecteur prévient quand il a fini ou échoué
    connect(&m_collector, &Collector::finished, this, &MainWindow::showStatus);
    connect(&m_collector, &Collector::failed, this, &MainWindow::showFailure);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ---------------------------------------------------------------------------
// Barre latérale
// ---------------------------------------------------------------------------

void MainWindow::buildSidebar()
{
    m_sideLayout = new QVBoxLayout(ui->sidebar);
    m_sideLayout->setContentsMargins(12, 16, 12, 16);
    m_sideLayout->setSpacing(2);

    // Un seul bouton coché à la fois ; un clic change la page
    m_nav = new QButtonGroup(this);
    m_nav->setExclusive(true);
    connect(m_nav, &QButtonGroup::idClicked, ui->pages, &QStackedWidget::setCurrentIndex);

    addSection(tr("SURVEILLANCE"));
    addNavButton(tr("Vue d'ensemble"), OverviewPageIndex);
    addNavButton(tr("Journaux"), LogsPageIndex);

    addSection(tr("OPÉRATIONS"));
    addNavButton(tr("Lancer un script"), OperationsPageIndex);

    m_sideLayout->addStretch(); // pousse la suite en bas

    // Paramètres : ouvre une fenêtre, ce n'est pas une page
    auto *settings = new QPushButton(tr("Paramètres"), ui->sidebar);
    settings->setFlat(true);
    settings->setCursor(Qt::PointingHandCursor);
    connect(settings, &QPushButton::clicked, this, &MainWindow::openSettings);
    m_sideLayout->addWidget(settings);

    // Page de départ
    m_nav->button(OverviewPageIndex)->setChecked(true);
    ui->pages->setCurrentIndex(OverviewPageIndex);
}

void MainWindow::addSection(const QString &title)
{
    auto *label = new QLabel(title, ui->sidebar);
    label->setObjectName("sidebarSection"); // style dans style.qss
    m_sideLayout->addWidget(label);
}

void MainWindow::addNavButton(const QString &text, int pageIndex)
{
    auto *b = new QPushButton(text, ui->sidebar);
    b->setCheckable(true);
    b->setFlat(true);
    b->setCursor(Qt::PointingHandCursor);
    m_nav->addButton(b, pageIndex);
    m_sideLayout->addWidget(b);
}

void MainWindow::openSettings()
{
    // Remplacé plus tard par SettingsDialog
    QMessageBox::information(this, tr("Paramètres"), tr("La fenêtre de paramètres arrive bientôt."));
}

// ---------------------------------------------------------------------------
// Données
// ---------------------------------------------------------------------------

void MainWindow::openStatusFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Ouvrir un état enregistré"),
                                                      QString(), tr("État JSON (*.json)"));
    if (path.isEmpty())
        return;
    m_collector.setDemoFile(path);
    m_collector.collect(QString());
    m_collector.setDemoFile(QString()); // la prochaine actualisation interrogera le serveur
}

void MainWindow::showStatus(const QJsonObject &status)
{
    m_status = status;
    const Health::Summary summary = Health::evaluate(status); // calculé une seule fois

    m_overview->setStatus(status, summary);
    // plus tard : m_logs->setStatus(status, summary); etc.

    statusBar()->showMessage(tr("État chargé : %1").arg(status.value("collectedAt").toString()));
}

void MainWindow::showFailure(const QString &message)
{
    statusBar()->showMessage(message);
}