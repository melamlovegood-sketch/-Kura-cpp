#pragma once
#include "PriceRepository.h"
#include <QWidget>
class QLabel;
class QPushButton;
class QCheckBox;
class QAction;
class ProductList;
class PriceTrendChart:public QWidget {
public:
    explicit PriceTrendChart(QWidget *parent=nullptr);
    void setRecords(const QList<PriceRecord> &records);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QList<PriceRecord> m_records;
};
class PriceWidget:public QWidget {
    Q_OBJECT
public:
    explicit PriceWidget(DatabaseManager &database,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh(qint64 selectedId=0);
    QPushButton *takeAddButton();
signals:
    void pricesChanged();
    void settingsRequested();
private:
    const PriceTrack *selected() const;
    void updateSelection();
    void openRecord(bool existing);
    void configure();
    void showHistory();
    DatabaseManager &m_database;
    PriceRepository m_repository;
    QList<PriceTrack> m_tracks;
    bool m_available=false;
    ProductList *m_table;
    QCheckBox *m_archived;
    QLabel *m_detail,*m_error,*m_summary,*m_empty;
    QWidget *m_addBar;
    QPushButton *m_new,*m_record,*m_history;
    QAction *m_configure,*m_end,*m_link;
};
