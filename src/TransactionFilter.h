#pragma once

#include "Transaction.h"

struct TransactionFilter
{
    QString category; // Empty means all categories.
    bool useDateRange = false;
    QDate startDate;
    QDate endDate;

    QString validationError() const;
    bool matches(const Transaction &transaction) const;
    QString description() const;
};
