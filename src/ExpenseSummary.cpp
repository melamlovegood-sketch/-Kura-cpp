#include "ExpenseSummary.h"

ExpenseSummary ExpenseSummary::calculate(const QList<Transaction> &transactions,
                                        const TransactionFilter &filter, const QDate &today)
{
    ExpenseSummary result;
    for (const auto &transaction : transactions) {
        result.totalCents += transaction.amountCents();
        if (transaction.date() == today)
            result.todayCents += transaction.amountCents();
        if (transaction.date().year() == today.year() && transaction.date().month() == today.month())
            result.monthCents += transaction.amountCents();
        if (filter.matches(transaction)) {
            ++result.filteredCount;
            result.filteredCents += transaction.amountCents();
        }
    }
    return result;
}
