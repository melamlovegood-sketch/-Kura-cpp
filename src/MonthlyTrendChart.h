#pragma once

#include "ExpenseStatistics.h"
#include <QWidget>

class MonthlyTrendChart : public QWidget
{
    Q_OBJECT
public:
    explicit MonthlyTrendChart(QWidget *parent = nullptr);
    void setStatistics(const YearlyExpenseStatistics &statistics);
    QSize minimumSizeHint() const override;

signals:
    void monthActivated(const QDate &month);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int monthAt(const QPoint &position) const;
    QRectF plotArea() const;
    YearlyExpenseStatistics m_statistics;
    bool m_hasSnapshot = false;
};
