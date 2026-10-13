#pragma once

#include "Transaction.h"
#include <QList>

class DatabaseManager;

class TransactionRepository
{
public:
    explicit TransactionRepository(DatabaseManager &database);

    // A new record has id 0; successful insertion fills in its database id.
    bool add(Transaction &transaction);
    // On failure, the caller's existing list is kept unchanged.
    bool findAll(QList<Transaction> &transactions);
    QString lastError() const;

private:
    DatabaseManager &m_database;
    QString m_lastError;
};
