#include "MonthlyBudget.h"

MonthlyBudget::MonthlyBudget(const QDate &month, qint64 limitCents)
    : m_month(firstDay(month)), m_limitCents(limitCents)
{
}

QDate MonthlyBudget::firstDay(const QDate &date)
{
    return date.isValid() ? QDate(date.year(), date.month(), 1) : QDate();
}

QString MonthlyBudget::monthKey(const QDate &date)
{
    return firstDay(date).toString(QStringLiteral("yyyy-MM"));
}

QDate MonthlyBudget::month() const { return m_month; }
qint64 MonthlyBudget::limitCents() const { return m_limitCents; }

QString MonthlyBudget::validationError() const
{
    if (!m_month.isValid() || m_month.year() < 1 || m_month.year() > 9999)
        return QStringLiteral("请选择有效的预算月份。");
    if (m_limitCents < 0 || m_limitCents > MaximumLimitCents)
        return QStringLiteral("预算须在 0 至 99,999,999.99 元之间。");
    return {};
}

qint64 MonthlyBudgetStatus::remainingCents() const
{
    return hasBudget ? limitCents - spentCents : 0;
}

bool MonthlyBudgetStatus::isOverBudget() const
{
    return hasBudget && spentCents > limitCents;
}

qint64 MonthlyBudgetStatus::overspentCents() const
{
    return isOverBudget() ? spentCents - limitCents : 0;
}
