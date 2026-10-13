#include "CategorySummary.h"

QString CategoryExpense::formattedPercentage() const
{
    return QString::number(percentage, 'f', 1) + QLatin1Char('%');
}

QList<CategoryExpense> CategorySummary::calculate(const QList<Transaction> &transactions,
                                                int year, int month)
{
    return calculate(transactions, QDate(year, month, 1));
}

QList<CategoryExpense> CategorySummary::calculate(const QList<Transaction> &transactions,
                                                const QDate &month)
{
    QList<CategoryExpense> result;
    for (const auto &category : Transaction::categories())
        result.append({category, 0});

    if (!month.isValid())
        return result;
    const QDate firstDay(month.year(), month.month(), 1);
    const QDate previousMonth = firstDay.addMonths(-1);
    qint64 totalCents = 0;
    for (const auto &transaction : transactions) {
        const QDate date = transaction.date();
        const bool current = date.year() == firstDay.year() && date.month() == firstDay.month();
        const bool previous = date.year() == previousMonth.year() && date.month() == previousMonth.month();
        if (!current && !previous)
            continue;
        for (auto &entry : result) {
            if (entry.category == transaction.category()) {
                if (current) {
                    entry.amountCents += transaction.amountCents();
                    ++entry.transactionCount;
                    totalCents += transaction.amountCents();
                } else {
                    entry.previousMonthAmountCents += transaction.amountCents();
                }
                break;
            }
        }
    }
    for (auto &entry : result) {
        entry.changeCents = entry.amountCents - entry.previousMonthAmountCents;
        entry.percentage = totalCents == 0 ? 0.0
            : (static_cast<double>(entry.amountCents) / static_cast<double>(totalCents)) * 100.0;
    }
    return result;
}
