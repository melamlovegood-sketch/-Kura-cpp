#pragma once

#include "Transaction.h"
#include <QList>

struct MonthlyExpense
{
    QDate month;
    qint64 totalCents = 0;
    qsizetype recordCount = 0;
};

struct YearlyExpenseStatistics
{
    int year = 0;
    QList<MonthlyExpense> months;
    qint64 totalCents = 0;
    qsizetype recordCount = 0;

    qint64 averageMonthCents() const; // All 12 calendar months; round to the nearest cent.
    int peakMonthIndex() const;       // -1 when all months are zero; earliest month wins ties.
};

class ExpenseStatistics
{
public:
    // Always returns twelve chronological months, including months with no records.
    // Invalid input leaves the previous output unchanged.
    static bool calculateYear(const QList<Transaction> &records, int year,
                              YearlyExpenseStatistics &result, QString &error);
};
