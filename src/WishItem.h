#pragma once

#include "Transaction.h"
#include <QDateTime>

enum class WishStatus { Waiting = 0, Purchased = 1, Abandoned = 2 };

struct WishItem
{
    qint64 id = 0;
    QString name;
    qint64 estimatedCents = 0;
    QString category = QStringLiteral("购物");
    int coolingDays = 3;
    int priority = 2; // 1 high, 2 medium, 3 low.
    QString reason;
    QDateTime createdAt;
    QDateTime readyAt;
    WishStatus status = WishStatus::Waiting;
    QDateTime closedAt;
    qint64 transactionId = 0;

    QString inputError() const;
    QString validationError() const;
    bool canPurchase(const QDateTime &now) const;
    QString statusText(const QDateTime &now) const;
    QString priorityText() const;
};
