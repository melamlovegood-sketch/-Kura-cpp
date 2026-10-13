#pragma once

#include "ExpenseStatistics.h"
#include <QWidget>

class QLabel;
class QSpinBox;
class QTableWidget;
class QPushButton;
class MonthlyTrendChart;

class StatisticsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StatisticsWidget(QWidget *parent = nullptr);
    bool setTransactions(const QList<Transaction> &records);
    void showLoadError(const QString &message);

signals:
    void monthActivated(const QDate &month);

private:
    void render(const YearlyExpenseStatistics &statistics);
    void updateSelection();
    void activateSelectedMonth();
    QList<Transaction> m_records;
    bool m_hasSnapshot = false;
    QString m_loadError;
    QSpinBox *m_yearEdit = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLabel *m_averageLabel = nullptr;
    QLabel *m_peakLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QTableWidget *m_monthTable = nullptr;
    QPushButton *m_viewMonthButton = nullptr;
    MonthlyTrendChart *m_chart = nullptr;
};
