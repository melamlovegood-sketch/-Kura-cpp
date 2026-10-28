#include "CategorySummary.h"
#include "CategoryCatalog.h"

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

QList<CategoryExpense> CategorySummary::calculatePurposes(const QList<Transaction> &transactions,const QDate &month) {
    QList<CategoryExpense> result;
    auto keys=CategoryCatalog::defaultKeys();QStringList custom;
    for(const auto &record:transactions) if(CategoryCatalog::isCustom(record.purposeCategory()) && !custom.contains(record.purposeCategory()))custom<<record.purposeCategory();
    custom.sort();keys.append(custom);keys<<"unclassified";
    for (const auto &key:keys) result.append({CategoryCatalog::purposeLabel(key),0});
    if (!month.isValid()) return result;
    const QDate first(month.year(),month.month(),1),previous=first.addMonths(-1); qint64 total=0;
    for (const auto &record:transactions) {
        const QDate period(record.date().year(),record.date().month(),1); if (period!=first && period!=previous) continue;
        const int index=keys.indexOf(record.purposeCategory()); if (index<0) continue; auto &entry=result[index];
        if (period==first) { entry.amountCents+=record.amountCents(); ++entry.transactionCount; total+=record.amountCents(); }
        else entry.previousMonthAmountCents+=record.amountCents();
    }
    for (auto &entry:result) { entry.changeCents=entry.amountCents-entry.previousMonthAmountCents; entry.percentage=total?double(entry.amountCents)/total*100:0; }
    return result;
}
