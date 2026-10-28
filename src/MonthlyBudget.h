#pragma once

#include <QDate>
#include <QString>

class MonthlyBudget
{
public:
    static constexpr qint64 MaximumLimitCents = 9999999999LL;

    MonthlyBudget(const QDate &month, qint64 limitCents);
    QDate month() const;
    qint64 limitCents() const;
    QString validationError() const;
    static QDate firstDay(const QDate &date);
    static QString monthKey(const QDate &date);

private:
    QDate m_month;
    qint64 m_limitCents;
};

struct MonthlyBudgetStatus
{
    QDate month;
    bool hasBudget = false;
    qint64 limitCents = 0;
    qint64 spentCents = 0;
    bool hasSplit = false;
    qint64 basicLimitCents = 0;
    qint64 discretionaryLimitCents = 0;
    qint64 basicSpentCents = 0;
    qint64 discretionarySpentCents = 0;

    qint64 remainingCents() const;
    bool isOverBudget() const;
    qint64 overspentCents() const;
};
