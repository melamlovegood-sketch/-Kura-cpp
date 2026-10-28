#pragma once
#include "ReviewRepository.h"
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
    void reviewRequested();
    void pricesRequested();
    void attentionChanged(qint64 dueReviews);
private:
    DatabaseManager &m_database;
    ReviewRepository m_reviews;
    PriceRepository m_prices;
    bool m_available=false;
    qint64 m_trackId=0,m_recordId=0;
    QLabel *m_reviewText,*m_priceText,*m_error;
    QPushButton *m_review,*m_price,*m_dismiss;
};
