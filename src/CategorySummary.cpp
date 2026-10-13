#include "CategorySummary.h"

QList<CategoryExpense> CategorySummary::calculate(const QList<Transaction> &transactions,
                                                const QDate &today)
{
    QList<CategoryExpense> result;
    for (const auto &category : Transaction::categories())
        result.append({category, 0});

    for (const auto &transaction : transactions) {
        if (transaction.date().year() != today.year()
            || transaction.date().month() != today.month())
            continue;
        for (auto &entry : result) {
            if (entry.category == transaction.category()) {
                entry.amountCents += transaction.amountCents();
                break;
            }
        }
    }
    return result;
}
