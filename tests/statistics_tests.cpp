#include <QStackedWidget>
#include "ExpenseStatistics.h"
#include "StatisticsWidget.h"
#include "MonthlyTrendChart.h"
#include "AddTransactionDialog.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

static Transaction expense(qint64 cents, const QDate &date)
{
    return Transaction(0, QStringLiteral("消费"), cents, QStringLiteral("餐饮"), date);
}

static void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

static QString label(QWidget &widget, const char *name)
{
    return widget.findChild<QLabel *>(QString::fromLatin1(name))->text();
}

class StatisticsTests : public QObject
{
    Q_OBJECT
private slots:
    void calendarAndPrecision();
    void emptyLargeAndInvalid();
    void widgetYearsAndNavigation();
};

void StatisticsTests::calendarAndPrecision()
{
    const QList<Transaction> records = {
        expense(10, QDate(2024, 1, 1)), expense(20, QDate(2024, 1, 31)),
        expense(30, QDate(2024, 2, 29)), expense(11, QDate(2024, 12, 31)),
        expense(900, QDate(2023, 12, 31)), expense(800, QDate(2025, 1, 1))};
    YearlyExpenseStatistics result;
    QString error;
    QVERIFY(ExpenseStatistics::calculateYear(records, 2024, result, error));
    QVERIFY(error.isEmpty());
    QCOMPARE(result.year, 2024);
    QCOMPARE(result.months.size(), 12);
    QCOMPARE(result.totalCents, 71);
    QCOMPARE(result.recordCount, 4);
    QCOMPARE(result.averageMonthCents(), 6);
    QCOMPARE(result.peakMonthIndex(), 0); // Earliest month wins a tie.
    for (int i = 0; i < 12; ++i)
        QCOMPARE(result.months[i].month, QDate(2024, i + 1, 1));
    QCOMPARE(result.months[0].totalCents, 30);
    QCOMPARE(result.months[0].recordCount, 2);
    QCOMPARE(result.months[1].totalCents, 30);
    QCOMPARE(result.months[2].totalCents, 0);
    QCOMPARE(result.months[11].totalCents, 11);
    QVERIFY(ExpenseStatistics::calculateYear(records, 2025, result, error));
    QCOMPARE(result.totalCents, 800);
    QCOMPARE(result.recordCount, 1);
}

void StatisticsTests::emptyLargeAndInvalid()
{
    YearlyExpenseStatistics result;
    QString error;
    QVERIFY(ExpenseStatistics::calculateYear({}, 1, result, error));
    QCOMPARE(result.months.size(), 12);
    QCOMPARE(result.months.first().month, QDate(1, 1, 1));
    QCOMPARE(result.totalCents, 0);
    QCOMPARE(result.averageMonthCents(), 0);
    QCOMPARE(result.peakMonthIndex(), -1);
    QVERIFY(ExpenseStatistics::calculateYear({expense(5, QDate(9999, 12, 31))}, 9999, result, error));
    QCOMPARE(result.averageMonthCents(), 0);
    QVERIFY(ExpenseStatistics::calculateYear({expense(6, QDate(9999, 12, 31))}, 9999, result, error));
    QCOMPARE(result.averageMonthCents(), 1);
    QList<Transaction> large;
    for (int i = 0; i < 3; ++i)
        large.append(expense(Transaction::MaximumAmountCents, QDate(2026, 5, 1)));
    QVERIFY(ExpenseStatistics::calculateYear(large, 2026, result, error));
    QCOMPARE(result.totalCents, 3 * Transaction::MaximumAmountCents);
    QCOMPARE(result.months[4].totalCents, result.totalCents);
    QCOMPARE(result.averageMonthCents(), 2500000000LL);
    for (int year : {0, 10000}) {
        QVERIFY(!ExpenseStatistics::calculateYear({}, year, result, error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(result.totalCents, 3 * Transaction::MaximumAmountCents);
        QCOMPARE(result.year, 2026);
    }
    QVERIFY(!ExpenseStatistics::calculateYear({expense(0, QDate(2026, 1, 1))}, 2026, result, error));
    QCOMPARE(result.recordCount, 3);
    QVERIFY(!ExpenseStatistics::calculateYear({expense(1, QDate())}, 2026, result, error));
    QCOMPARE(result.recordCount, 3);
}

void StatisticsTests::widgetYearsAndNavigation()
{
    StatisticsWidget widget;
    widget.resize(980, 760);
    widget.show();
    auto *year = widget.findChild<QSpinBox *>(QStringLiteral("statisticsYearSpinBox"));
    auto *table = widget.findChild<QTableWidget *>(QStringLiteral("monthlyStatisticsTable"));
    auto *view = widget.findChild<QPushButton *>(QStringLiteral("viewStatisticsMonthButton"));
    QVERIFY(!year->isEnabled());
    QVERIFY(!view->isEnabled());
    year->setValue(2026);
    QVERIFY(widget.setTransactions({expense(1000, QDate(2026, 1, 1)),
        expense(2000, QDate(2026, 5, 1)), expense(500, QDate(2025, 12, 31))}));
    settle();
    QCOMPARE(table->rowCount(), 12);
    QCOMPARE(label(widget, "yearExpenseLabel"), QStringLiteral("全年支出：¥30.00 · 2 笔"));
    QCOMPARE(label(widget, "yearAverageLabel"), QStringLiteral("全年月均：¥2.50"));
    QVERIFY(label(widget, "peakMonthLabel").contains(QStringLiteral("2026-05 · ¥20.00")));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("¥10.00"));
    QCOMPARE(table->item(9, 2)->text(), QStringLiteral("¥0.00"));
    QSignalSpy activated(&widget, &StatisticsWidget::monthActivated);
    table->selectRow(4);
    view->click();
    QCOMPARE(activated.takeFirst().first().toDate(), QDate(2026, 5, 1));
    auto *chart = widget.findChild<MonthlyTrendChart *>();
    const int left = chart->fontMetrics().horizontalAdvance(Transaction::formatAmount(2000)) + 16;
    const QRectF plot = QRectF(chart->rect()).adjusted(left, 18, -14, -34);
    const QPoint january(qRound(plot.left() + plot.width() / 24), qRound(plot.center().y()));
    QHelpEvent help(QEvent::ToolTip, january, chart->mapToGlobal(january));
    QApplication::sendEvent(chart, &help);
    QVERIFY(chart->toolTip().contains(QStringLiteral("2026-01")));
    QVERIFY(chart->toolTip().contains(QStringLiteral("¥10.00")));
    QTest::mouseDClick(chart, Qt::LeftButton, Qt::NoModifier, january);
    QCOMPARE(activated.takeFirst().first().toDate(), QDate(2026, 1, 1));
    year->setValue(2025);
    QCOMPARE(label(widget, "yearExpenseLabel"), QStringLiteral("全年支出：¥5.00 · 1 笔"));
    year->setValue(2024);
    QCOMPARE(label(widget, "yearExpenseLabel"), QStringLiteral("全年支出：¥0.00 · 0 笔"));
    QCOMPARE(label(widget, "peakMonthLabel"), QStringLiteral("最高月份：—"));
    QTest::mouseDClick(chart, Qt::LeftButton, Qt::NoModifier, january);
    QCOMPARE(activated.count(), 0);
    table->selectRow(1);
    view->click(); // Zero months still have a navigable table row.
    QCOMPARE(activated.takeFirst().first().toDate(), QDate(2024, 2, 1));
    const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        QVERIFY(QDir().mkpath(preview));
        QVERIFY(widget.grab().save(preview + QStringLiteral("/statistics-empty.png")));
    }
    QVERIFY(!widget.setTransactions({expense(0, QDate(2024, 1, 1))}));
    QCOMPARE(label(widget, "yearExpenseLabel"), QStringLiteral("全年支出：¥0.00 · 0 笔"));
    year->setValue(2026);
    QVERIFY(label(widget, "statisticsErrorLabel").contains(QStringLiteral("仍显示上次")));
    QVERIFY(!widget.findChild<QLabel *>(QStringLiteral("statisticsErrorLabel"))->isHidden());
    QVERIFY(widget.setTransactions({}));
    QVERIFY(widget.findChild<QLabel *>(QStringLiteral("statisticsErrorLabel"))->isHidden());
    QList<Transaction> demo;
    const QList<qint64> amounts = {12800, 7800, 23500, 16900, 8900, 31200, 15300, 20100, 9400, 18200};
    for (int i = 0; i < amounts.size(); ++i)
        demo.append(expense(amounts[i], QDate(2026, i + 1, 15)));
    QVERIFY(widget.setTransactions(demo));
    settle();
    if (!preview.isEmpty())
        QVERIFY(widget.grab().save(preview + QStringLiteral("/statistics.png")));
}

QTEST_MAIN(StatisticsTests)
#include "statistics_tests.moc"
