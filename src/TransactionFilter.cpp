#include "TransactionFilter.h"

QString TransactionFilter::validationError() const
{
    if (!category.isEmpty() && !Transaction::categories().contains(category))
        return QStringLiteral("请选择有效的筛选分类。");
    if (useDateRange) {
        if (!startDate.isValid() || !endDate.isValid())
            return QStringLiteral("请选择有效的起止日期。");
        if (startDate > endDate)
            return QStringLiteral("开始日期不能晚于结束日期。");
    }
    return {};
}

bool TransactionFilter::matches(const Transaction &transaction) const
{
    return (category.isEmpty() || transaction.category() == category)
        && (!useDateRange || (transaction.date() >= startDate && transaction.date() <= endDate));
}

QString TransactionFilter::description() const
{
    const QString categoryText = category.isEmpty() ? QStringLiteral("全部分类") : category;
    const QString dateText = useDateRange
        ? QStringLiteral("%1 至 %2").arg(startDate.toString(Qt::ISODate), endDate.toString(Qt::ISODate))
        : QStringLiteral("全部日期");
    return QStringLiteral("显示范围：%1 · %2").arg(categoryText, dateText);
}
