#include "overviewpage.h"
#include "src/ui/ui_overviewpage.h"
#include "ui/overviewpage.h"

#include <QDebug>

OverviewPage::OverviewPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OverviewPage)
{
    ui->setupUi(this);
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setStatus(const QJsonObject &status, const Health::Summary &summary){
    qDebug() << "Server :" << status.value("server").toString() << "| warnings :" << summary.findings.size();
    for (const Health::Finding &f : summary.findings) qDebug() << " -" << f.area << ":" << f.message << f.code;
}