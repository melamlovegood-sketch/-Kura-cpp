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
class StatisticsWidget;
class QTabWidget;
class WishlistWidget;
class DataManagementWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    // Explicit paths are useful for isolated tests; normal startup uses app data.
    explicit MainWindow(const QString &databasePath, QWidget *parent = nullptr);

    bool reloadTransactions(qint64 selectedId = 0);

private:
    void showError(const QString &message);
    void openTransactionDialog(const Transaction *transaction = nullptr);
    void editSelectedTransaction();
    void deleteSelectedTransaction();
    void applyFilters();
    void resetFilters();
    void updateActionState();
    qint64 selectedTransactionId() const;
    const Transaction *selectedTransaction() const;
    DatabaseManager m_database;
    TransactionRepository m_repository;
    QList<Transaction> m_transactions;
    TransactionFilter m_filter;
    bool m_databaseReady = false;
    BudgetWidget *m_budgetWidget = nullptr;
    StatisticsWidget *m_statisticsWidget = nullptr;
    QTabWidget *m_pages = nullptr;
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
    QTableWidget *m_categorySummaryTable = nullptr;
    QLabel *m_emptyHint = nullptr;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLabel *m_monthLabel = nullptr;
    QLabel *m_todayLabel = nullptr;
    QLabel *m_filteredSummaryLabel = nullptr;
    QLabel *m_filterDescriptionLabel = nullptr;
};
