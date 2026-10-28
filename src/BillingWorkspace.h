#pragma once
#include "BillingAggregation.h"
#include "MonthlyBudget.h"
#include "DatabaseManager.h"
#include <QWidget>
class ExpenseComposer;
class MonthPicker;
class BillingCalendar;
class BillingTrendChart;
class QStackedWidget;
class QBoxLayout;
class QVBoxLayout;
class QFrame;
class QLabel;
class QPushButton;
class QProgressBar;
class QResizeEvent;
class BillingWorkspace:public QWidget {
    Q_OBJECT
public:
    explicit BillingWorkspace(DatabaseManager &database,const QString &settingsPath,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
    bool setMonth(const QDate &month);
    QDate month() const { return m_month; }
    void showLoadError(const QString &message);
    void resetAfterRestore();
signals:
    void expenseSaved(qint64 id);
    void recordsRequested(const QDate &first,const QDate &last);
    void budgetRequested(const QDate &month);
    void importRequested();
    void settingsRequested();
    void editRequested(qint64 id);
    void deleteRequested(qint64 id);
    void monthChanged(const QDate &month);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void showView(bool charts);
    void showChart(int view);
    void renderChartSummary();
    void render();
    void renderRecent();
    void renderBudget();
    void renderDay(const QDate &date);
    void showDetails(qint64 id);
    DatabaseManager &m_database;
    QDate m_month;
    BillingSnapshot m_snapshot;
    MonthlyBudgetStatus m_budget;
    QList<Transaction> m_records;
    bool m_available=false,m_hasSnapshot=false;
    ExpenseComposer *m_composer;
    MonthPicker *m_monthPicker;
    QStackedWidget *m_views,*m_chartViews;
    QPushButton *m_recordTab,*m_chartTab,*m_calendarTab,*m_trendTab,*m_quickRecord,*m_six,*m_budgetEdit,*m_import,*m_budgetSummaryButton,*m_detailsToggle,*m_trendRecords;
    QWidget *m_headerStats;
    QLabel *m_chartAmount;
    QDate m_trendSelection;
    QLabel *m_budgetSummary;
    QLabel *m_total,*m_today,*m_count,*m_recentCount,*m_budgetState,*m_budgetBalance,*m_budgetTotal,*m_basicRemaining,*m_discRemaining,*m_basicUsed,*m_discUsed,*m_chartTitle,*m_chartHint,*m_trendReadout,*m_trendSummary,*m_error;
    QProgressBar *m_basicBar,*m_discBar;
    BillingCalendar *m_calendar;
    BillingTrendChart *m_trend;
    QBoxLayout *m_workspaceLayout,*m_sidebarLayout;
    QWidget *m_sidebar;
    QVBoxLayout *m_recentRows,*m_categoryRows,*m_dayRows;
};
