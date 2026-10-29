#pragma once

#include <QMainWindow>
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "TransactionFilter.h"

class QPushButton;
class QTableWidget;
class QLabel;
class QComboBox;
class QCheckBox;
class QDateEdit;
class BudgetWidget;
class QStackedWidget;
class QToolButton;
class HomeWidget;
class AppearanceWidget;
class ExecutionWidget;
class MonthlyReviewWidget;
class PriceWidget;
class WishlistWidget;
class DataManagementWidget;
class QBoxLayout;
class QGroupBox;
class QResizeEvent;
class BillingWorkspace;
class QDialog;
class BatchSelectionBar;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class Page { Billing = 0, Review, Wishlist, Data, Home, Execution, Settings };
    explicit MainWindow(QWidget *parent = nullptr);
    // Explicit paths are useful for isolated tests; normal startup uses app data.
    explicit MainWindow(const QString &databasePath, QWidget *parent = nullptr);

    bool reloadTransactions(qint64 selectedId = 0);
    void showPage(Page page);
    Page currentPage() const;
    void startFirstRunGuide();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void showOnboarding(bool resume=false);
    bool m_guideShown = false;
    void buildShell();
    void applyTheme(const QString &theme, bool save);
    void updateNavigation();
    QWidget *buildExecutionPage();
    void openDecisionDialog();
    void choosePoolGoal(qint64 wishId);
    void showError(const QString &message);
    void openTransactionDialog(const Transaction *transaction = nullptr,bool purchase=false,qint64 sessionId=0);
    void editSelectedTransaction();
    void deleteSelectedTransaction();
    void applyFilters();
    void resetFilters();
    void showRecords(const QDate &first, const QDate &last);
    bool selectRecord(qint64 id);
    void updateActionState();
    qint64 selectedTransactionId() const;
    const Transaction *selectedTransaction() const;
    DatabaseManager m_database;
    TransactionRepository m_repository;
    QList<Transaction> m_transactions;
    TransactionFilter m_filter;
    bool m_databaseReady = false;
    BudgetWidget *m_budgetWidget = nullptr;
    BillingWorkspace *m_billingWorkspace = nullptr;
    QStackedWidget *m_billingPages = nullptr;
    QDialog *m_budgetDialog = nullptr;
    QStackedWidget *m_pages = nullptr;
    HomeWidget *m_homeWidget = nullptr;
    AppearanceWidget *m_appearanceWidget = nullptr;
    QList<QToolButton *> m_navigation;
    QToolButton *m_settingsButton = nullptr;
    QLabel *m_brandMark = nullptr;
    QWidget *m_homeActionBar = nullptr;
    QPushButton *m_homeDecision = nullptr;
    ExecutionWidget *m_executionWidget = nullptr;
    MonthlyReviewWidget *m_monthlyReviewWidget = nullptr;
    QString m_theme;
    QString m_themePreference;
    QString m_preferencesPath;
    WishlistWidget *m_wishlistWidget = nullptr;
    DataManagementWidget *m_dataWidget = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_applyFilterButton = nullptr;
    QPushButton *m_resetFilterButton = nullptr;
    QComboBox *m_categoryFilter = nullptr;
    QCheckBox *m_useDateRange = nullptr;
    QDateEdit *m_startDate = nullptr;
    QDateEdit *m_endDate = nullptr;
    QTableWidget *m_transactionTable = nullptr;
    BatchSelectionBar *m_recordSelection = nullptr;
    QTableWidget *m_categorySummaryTable = nullptr;
    QBoxLayout *m_recordsLayout = nullptr;
    QGroupBox *m_categoryBox = nullptr;
    QLabel *m_emptyHint = nullptr;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLabel *m_monthLabel = nullptr;
    QLabel *m_todayLabel = nullptr;
    QLabel *m_filteredSummaryLabel = nullptr;
    QLabel *m_filterDescriptionLabel = nullptr;
};
