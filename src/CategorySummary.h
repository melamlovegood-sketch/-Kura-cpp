#pragma once

#include "Transaction.h"
#include <QList>

struct CategoryExpense
{
    QString category;
    qint64 amountCents = 0;
    qint64 transactionCount = 0;
    double percentage = 0.0; // 0..100, unrounded; round only for display.
    qint64 previousMonthAmountCents = 0;
    qint64 changeCents = 0; // Selected month minus previous month.

    QString formattedPercentage() const;
};

class CategorySummary
{
public:
    // Pass the complete, validated repository snapshot, never the filtered list.
    // Includes all six categories in Transaction::categories() order, even at zero.
    static QList<CategoryExpense> calculate(const QList<Transaction> &transactions,
                                           const QDate &month = QDate::currentDate());
    // Any day in month is accepted. Invalid dates/year-month return six zero rows.
    static QList<CategoryExpense> calculate(const QList<Transaction> &transactions,
                                           int year, int month);
};
