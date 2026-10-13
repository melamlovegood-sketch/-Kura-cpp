#pragma once

#include "TransactionFilter.h"
#include <QList>

struct ExpenseSummary
{
    qint64 totalCents = 0;
    qint64 monthCents = 0;
    qint64 todayCents = 0;
    qint64 filteredCents = 0;
    qsizetype filteredCount = 0;

    // Global totals and filtered totals come from the same database snapshot.
    static ExpenseSummary calculate(const QList<Transaction> &transactions,
                                    const TransactionFilter &filter,
                                    const QDate &today = QDate::currentDate());
};
