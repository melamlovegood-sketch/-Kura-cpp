#pragma once

#include "Transaction.h"
#include <QList>

struct CategoryExpense
{
    QString category;
    qint64 amountCents = 0;
};

class CategorySummary
{
public:
    // Pass the complete, validated repository snapshot, never the filtered list.
    // Includes all six categories in Transaction::categories() order, even at zero.
    static QList<CategoryExpense> calculate(const QList<Transaction> &transactions,
                                           const QDate &today = QDate::currentDate());
};
