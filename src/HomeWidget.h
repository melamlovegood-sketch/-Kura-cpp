#pragma once
#include "BudgetRepository.h"
#include "WishlistRepository.h"
#include "ExecutionRepository.h"
#include <QWidget>
#include <array>
class QLabel;
class QProgressBar;
class QPushButton;
class WishPoolWidget;
class QHBoxLayout;
class QBoxLayout;
class QResizeEvent;

class HomeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HomeWidget(DatabaseManager &database, QWidget *parent = nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
    void setPrimaryAction(QWidget *action);
signals:
    void budgetRequested();
    void wishlistRequested();
    void readyWishesRequested(qint64 wishId = 0);
    void executionRequested();
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    QHBoxLayout *m_heading;
    QBoxLayout *m_sections;
    BudgetRepository m_budgets;
    WishlistRepository m_wishes;
    ExecutionRepository m_execution;
    bool m_available = false;
    QLabel *m_month;
    QLabel *m_date;
    QLabel *m_remaining;
    QLabel *m_budgetDetail;
    QLabel *m_error;
    QLabel *m_basicBudget;
    QLabel *m_discretionaryBudget;
    WishPoolWidget *m_pool;
    QWidget *m_decisions;
    QLabel *m_readySummary;
    QPushButton *m_allReady;
    QWidget *m_resume;
    QLabel *m_resumeName;
    QPushButton *m_continue;
    struct WishRow {
        QWidget *widget;
        QLabel *name;
        QLabel *detail;
        QPushButton *action;
    };
    std::array<WishRow, 2> m_rows;
    QProgressBar *m_progress;
    QPushButton *m_budgetButton;
};
