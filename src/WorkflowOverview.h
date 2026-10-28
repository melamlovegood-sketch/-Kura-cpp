#pragma once
#include "PriceRepository.h"
#include <QFrame>
class QLabel;
class QPushButton;
class WorkflowOverview:public QFrame {
    Q_OBJECT
public:
    explicit WorkflowOverview(DatabaseManager &database,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
signals:
    void pricesRequested();
private:
    DatabaseManager &m_database;
    PriceRepository m_prices;
    bool m_available=false;
    qint64 m_trackId=0,m_recordId=0;
    QLabel *m_priceText,*m_error;
    QPushButton *m_price,*m_dismiss;
};
