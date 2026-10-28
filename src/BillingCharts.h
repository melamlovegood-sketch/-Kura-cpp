#pragma once
#include "BillingAggregation.h"
#include <QWidget>
class BillingCalendar:public QWidget {
    Q_OBJECT
public:
    explicit BillingCalendar(QWidget *parent=nullptr);
    void setSnapshot(const BillingSnapshot &snapshot);
    QDate selectedDate() const { return m_selected; }
    QRectF cellRect(int day) const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int heightForWidth(int width) const override;
signals:
    void daySelected(const QDate &date);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    void selectDay(int day);
    BillingSnapshot m_snapshot;
    QDate m_selected;
    bool m_hasSnapshot=false;
};
class BillingTrendChart:public QWidget {
    Q_OBJECT
public:
    explicit BillingTrendChart(QWidget *parent=nullptr);
    void setSnapshot(const BillingSnapshot &snapshot);
    void setSixMonths(bool sixMonths);
    QList<BillingMonth> points() const;
    QSize minimumSizeHint() const override;
signals:
    void pointSelected(const QDate &date,qint64 cents,qint64 count);
protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    QRectF plotRect() const;
    void selectPoint(int index);
    BillingSnapshot m_snapshot;
    bool m_hasSnapshot=false,m_six=false;
    int m_hover=-1;
};
