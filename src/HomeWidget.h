#pragma once
#include "BudgetRepository.h"
#include "WishlistRepository.h"
#include <QWidget>
class QLabel;
class QProgressBar;
class QPushButton;
class WishPoolWidget;
class WorkflowOverview;

class HomeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HomeWidget(DatabaseManager &database, QWidget *parent = nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
signals:
    void budgetRequested();
    void wishlistRequested();
    void reviewRequested();
    void pricesRequested();
    void attentionChanged(qint64 dueReviews);
private:
    BudgetRepository m_budgets;
    WishlistRepository m_wishes;
    bool m_available = false;
    QLabel *m_month;
    QLabel *m_remaining;
    QLabel *m_budgetDetail;
    QLabel *m_wishCount;
    QLabel *m_wishDetail;
    QLabel *m_error;
    QLabel *m_basicBudget;
    QLabel *m_discretionaryBudget;
    QProgressBar *m_basicProgress;
    QProgressBar *m_discretionaryProgress;
    WishPoolWidget *m_pool;
    WorkflowOverview *m_overview;
    QProgressBar *m_progress;
    QPushButton *m_budgetButton;
};
