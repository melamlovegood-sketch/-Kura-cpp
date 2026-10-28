#pragma once
#include <QLabel>
class PricePerspectiveHint:public QLabel {
    Q_OBJECT
public:
    explicit PricePerspectiveHint(const QString &settingsPath,QWidget *parent=nullptr);
    void setAmount(qint64 cents);
    void refresh();
private:
    QString m_settingsPath;
    qint64 m_cents=0;
};
