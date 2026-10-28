#pragma once
#include "DatabaseManager.h"
#include "WishItem.h"
#include "PriceRepository.h"
#include <QDialog>
class QLabel;
class QPushButton;
class QTableWidget;
class PriceTrendChart;

class WishPriceDialog:public QDialog {
    Q_OBJECT
public:
    explicit WishPriceDialog(DatabaseManager &database,qint64 wishId,QWidget *parent=nullptr);
signals:
    void pricesChanged();
    void settingsRequested();
private:
    bool refresh();
    void record();
    void configure();
    DatabaseManager &m_database;
    qint64 m_wishId;
    WishItem m_wish;
    PriceTrack m_track;
    QLabel *m_summary,*m_error;
    QWidget *m_history;
    PriceTrendChart *m_chart;
    QTableWidget *m_table;
    QPushButton *m_record,*m_target,*m_link;
};
