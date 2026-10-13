#include "ExpenseSummary.h"
#include <QtTest>

class SummaryTests : public QObject
{
    Q_OBJECT
private slots:
    void globalAndFilteredTotals();
    void inclusiveLeapDayRange();
    void invalidFilters();
    void emptyResultsAndLargeAmounts();
};

void SummaryTests::globalAndFilteredTotals()
{
    const QList<Transaction> rows{
        Transaction(1, QStringLiteral("早餐"), 10, QStringLiteral("餐饮"), QDate(2026, 1, 1)),
        Transaction(2, QStringLiteral("晚饭"), 20, QStringLiteral("餐饮"), QDate(2025, 12, 31)),
        Transaction(3, QStringLiteral("午饭"), 29, QStringLiteral("餐饮"), QDate(2026, 1, 31)),
        Transaction(4, QStringLiteral("地铁"), 1, QStringLiteral("交通"), QDate(2026, 2, 1)),
        Transaction(5, QStringLiteral("教材"), Transaction::MaximumAmountCents,
                    QStringLiteral("学习"), QDate(2026, 1, 15))};
    TransactionFilter filter;
    filter.category = QStringLiteral("餐饮");
    auto summary = ExpenseSummary::calculate(rows, filter, QDate(2026, 1, 1));
    QCOMPARE(summary.totalCents, Transaction::MaximumAmountCents + 60);
    QCOMPARE(summary.monthCents, Transaction::MaximumAmountCents + 39);
    QCOMPARE(summary.todayCents, 10);
    QCOMPARE(summary.filteredCount, 3);
    QCOMPARE(summary.filteredCents, 59);
    filter.useDateRange = true;
    filter.startDate = QDate(2026, 1, 1);
    filter.endDate = QDate(2026, 1, 31);
    QVERIFY(filter.validationError().isEmpty());
    summary = ExpenseSummary::calculate(rows, filter, QDate(2026, 1, 1));
    QCOMPARE(summary.filteredCount, 2);
    QCOMPARE(summary.filteredCents, 39);
    QCOMPARE(summary.totalCents, Transaction::MaximumAmountCents + 60);
    QCOMPARE(Transaction::formatAmount(summary.filteredCents), QStringLiteral("¥0.39"));
}

void SummaryTests::inclusiveLeapDayRange()
{
    const QList<Transaction> rows{
        Transaction(1, QStringLiteral("A"), 1, QStringLiteral("餐饮"), QDate(2024, 2, 28)),
        Transaction(2, QStringLiteral("B"), 2, QStringLiteral("餐饮"), QDate(2024, 2, 29)),
        Transaction(3, QStringLiteral("C"), 3, QStringLiteral("餐饮"), QDate(2024, 3, 1)),
        Transaction(4, QStringLiteral("D"), 4, QStringLiteral("餐饮"), QDate(2024, 3, 2))};
    TransactionFilter filter;
    filter.useDateRange = true;
    filter.startDate = QDate(2024, 2, 28);
    filter.endDate = QDate(2024, 3, 1);
    const auto summary = ExpenseSummary::calculate(rows, filter, QDate(2024, 2, 29));
    QCOMPARE(summary.filteredCount, 3);
    QCOMPARE(summary.filteredCents, 6);
    QCOMPARE(summary.monthCents, 3);
    QCOMPARE(summary.todayCents, 2);
    filter.startDate = filter.endDate = QDate(2024, 2, 29);
    QCOMPARE(ExpenseSummary::calculate(rows, filter).filteredCount, 1);
}

void SummaryTests::invalidFilters()
{
    TransactionFilter filter;
    QVERIFY(filter.validationError().isEmpty());
    filter.category = QStringLiteral("未知");
    QVERIFY(!filter.validationError().isEmpty());
    filter.category.clear();
    filter.useDateRange = true;
    QVERIFY(!filter.validationError().isEmpty());
    filter.startDate = QDate(2026, 10, 14);
    filter.endDate = QDate(2026, 10, 13);
    QVERIFY(!filter.validationError().isEmpty());
    filter.useDateRange = false;
    QVERIFY(filter.validationError().isEmpty());
}

void SummaryTests::emptyResultsAndLargeAmounts()
{
    TransactionFilter filter;
    QCOMPARE(ExpenseSummary::calculate({}, filter).totalCents, 0);
    const QList<Transaction> rows{
        Transaction(1, QStringLiteral("A"), Transaction::MaximumAmountCents, QStringLiteral("餐饮"), QDate::currentDate()),
        Transaction(2, QStringLiteral("B"), Transaction::MaximumAmountCents, QStringLiteral("餐饮"), QDate::currentDate()),
        Transaction(3, QStringLiteral("C"), Transaction::MaximumAmountCents, QStringLiteral("餐饮"), QDate::currentDate())};
    auto summary = ExpenseSummary::calculate(rows, filter);
    QCOMPARE(summary.totalCents, 3 * Transaction::MaximumAmountCents);
    QCOMPARE(Transaction::formatAmount(summary.totalCents), QStringLiteral("¥299999999.97"));
    filter.category = QStringLiteral("交通");
    summary = ExpenseSummary::calculate(rows, filter);
    QCOMPARE(summary.filteredCount, 0);
    QCOMPARE(summary.filteredCents, 0);
    QCOMPARE(summary.totalCents, 3 * Transaction::MaximumAmountCents);
    QCOMPARE(Transaction::formatAmount(1), QStringLiteral("¥0.01"));
    QCOMPARE(Transaction::formatAmount(-1), QStringLiteral("-¥0.01"));
}

QTEST_GUILESS_MAIN(SummaryTests)
#include "summary_tests.moc"
