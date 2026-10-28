#pragma once
#include <QComboBox>
#include <QDateEdit>
#include <QPointer>
class QFrame;
class QPaintEvent;
class QMouseEvent;
class QKeyEvent;
namespace ModernSelectors {
void showPopup(QFrame *popup,QWidget *anchor);
}
class MonthPicker:public QDateEdit {
    Q_OBJECT
public:
    explicit MonthPicker(const QDate &month,QWidget *parent=nullptr);
    QSize sizeHint() const override;
    void openPicker();
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
};
class DatePicker:public QDateEdit {
    Q_OBJECT
public:
    explicit DatePicker(const QDate &date,QWidget *parent=nullptr);
    QSize sizeHint() const override;
    void openPicker();
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
};
class ModernComboBox:public QComboBox {
    Q_OBJECT
public:
    explicit ModernComboBox(QWidget *parent=nullptr);
    QSize sizeHint() const override;
    void showPopup() override;
    void hidePopup() override;
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QPointer<QFrame> m_popup;
};
