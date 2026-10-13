#include "WishItem.h"

QString WishItem::inputError() const
{
    const QString error = Transaction(id, name, estimatedCents, category, QDate(2026, 1, 1)).validationError();
    if (!error.isEmpty())
        return error;
    if (coolingDays < 1 || coolingDays > 30)
        return QStringLiteral("冷静期须为 1 至 30 天。");
    if (priority < 1 || priority > 3)
        return QStringLiteral("请选择高、中或低优先级。");
    if (reason.size() > 500)
        return QStringLiteral("购买理由最多 500 个字符。");
    return {};
}

QString WishItem::validationError() const
{
    const QString error = inputError();
    if (!error.isEmpty())
        return error;
    if (!createdAt.isValid() || !readyAt.isValid() || readyAt < createdAt)
        return QStringLiteral("愿望的创建时间或冷静期截止时间无效。");
    switch (status) {
    case WishStatus::Waiting:
        if (closedAt.isValid() || transactionId != 0)
            return QStringLiteral("待决定的愿望不能包含完成记录。");
        break;
    case WishStatus::Purchased:
        if (!closedAt.isValid() || closedAt < readyAt || transactionId <= 0)
            return QStringLiteral("购买记录无效。");
        break;
    case WishStatus::Abandoned:
        if (!closedAt.isValid() || closedAt < createdAt || transactionId != 0)
            return QStringLiteral("放弃记录无效。");
        break;
    default:
        return QStringLiteral("愿望状态无效。");
    }
    return {};
}

bool WishItem::canPurchase(const QDateTime &now) const
{
    return status == WishStatus::Waiting && now.isValid() && readyAt.isValid() && now >= readyAt;
}

QString WishItem::priorityText() const
{
    return priority == 1 ? QStringLiteral("高") : priority == 3 ? QStringLiteral("低") : QStringLiteral("中");
}

QString WishItem::statusText(const QDateTime &now) const
{
    if (status == WishStatus::Purchased)
        return QStringLiteral("已购买");
    if (status == WishStatus::Abandoned)
        return QStringLiteral("已放弃");
    if (canPurchase(now))
        return QStringLiteral("冷静期结束，可购买");
    if (!now.isValid() || !readyAt.isValid())
        return QStringLiteral("时间不可用");
    const qint64 milliseconds = now.msecsTo(readyAt);
    const qint64 minutes = milliseconds / 60000 + (milliseconds % 60000 > 0 ? 1 : 0);
    return QStringLiteral("剩余 %1 天 %2 小时 %3 分")
        .arg(minutes / 1440).arg(minutes / 60 % 24).arg(minutes % 60);
}
