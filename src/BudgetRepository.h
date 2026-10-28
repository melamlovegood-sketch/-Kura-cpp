#pragma once

#include "MonthlyBudget.h"

class DatabaseManager;

class BudgetRepository
{
public:
    explicit BudgetRepository(DatabaseManager &database);
    bool save(const MonthlyBudget &budget);
    bool saveSplit(const QDate &month, qint64 basicCents, qint64 discretionaryCents);
    // A missing budget is a successful read with hasBudget=false, not an error.
    // On failure the caller's status is left unchanged.
    bool loadStatus(const QDate &month, MonthlyBudgetStatus &status);
    QString lastError() const;

private:
    DatabaseManager &m_database;
    QString m_lastError;
};
