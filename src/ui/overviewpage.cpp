#include "overviewpage.h"
#include "src/ui/ui_overviewpage.h"
#include "ui/overviewpage.h"

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
