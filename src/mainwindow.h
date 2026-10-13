#pragma once

#include <QMainWindow>
#include "DatabaseManager.h"
#include "TransactionRepository.h"

class QPushButton;
class QTableWidget;
class QLabel;

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
    DatabaseManager m_database;
    TransactionRepository m_repository;
    QList<Transaction> m_transactions;
    QPushButton *m_addButton = nullptr;
    QTableWidget *m_transactionTable = nullptr;
    QLabel *m_emptyHint = nullptr;
    QLabel *m_errorLabel = nullptr;
};
