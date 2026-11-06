#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>

namespace Ui {
class OverviewPage;
}

class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = nullptr);
    ~OverviewPage();

private:
    Ui::OverviewPage *ui;
};

#endif // OVERVIEWPAGE_H
