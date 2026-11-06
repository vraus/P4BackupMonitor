#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <QJsonObject>
#include "core/health.h"

namespace Ui { class OverviewPage; }

class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = nullptr);
    ~OverviewPage();

    // called by mainwindow for each new collection
    void setStatus(const QJsonObject &status, const Health::Summary &summary);

private:
    Ui::OverviewPage *ui;
};

#endif // OVERVIEWPAGE_H
