#include "CategoryCatalog.h"
#include "CategorySummary.h"
#include "ExpenseSummary.h"
#include "AddTransactionDialog.h"
#include "mainwindow.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace {
void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void fill(AddTransactionDialog *dialog, const QString &category, double amount, const QDate &date)
{
    dialog->findChild<QLineEdit *>("descriptionEdit")->setText(QStringLiteral("测试消费"));
    dialog->findChild<QDoubleSpinBox *>("amountSpinBox")->setValue(amount);
    dialog->findChild<QComboBox *>("categoryComboBox")->setCurrentText(CategoryCatalog::purposeLabel(CategoryCatalog::purposeFromDetail(CategoryCatalog::fromLegacy(category))));
    dialog->findChild<QDateEdit *>("dateEdit")->setDate(date);
}
}

class CategorySummaryTests : public QObject
{
    Q_OBJECT
private slots:
    void emptyAndMonthBoundaries();
    void allCategoriesAndExactCents();
    void countsSharesAndChanges();
    void zeroMonthAndSingleCategory();
    void arbitraryMonthAndPercentageFormatting();
    void refreshWithCrudAndFilters();
    void loadFailurePreservesSummary();
    void preview();
};

void CategorySummaryTests::emptyAndMonthBoundaries()
{
    const auto empty = CategorySummary::calculate({}, QDate(2024, 2, 15));
    QCOMPARE(empty.size(), 6);
    for (int row = 0; row < empty.size(); ++row) {
        QCOMPARE(empty[row].category, Transaction::categories()[row]);
        QCOMPARE(empty[row].amountCents, 0);
        QCOMPARE(empty[row].transactionCount, 0);
        QCOMPARE(empty[row].percentage, 0.0);
        QCOMPARE(empty[row].previousMonthAmountCents, 0);
        QCOMPARE(empty[row].changeCents, 0);
        QCOMPARE(empty[row].formattedPercentage(), QStringLiteral("0.0%"));
    }
    const QList<Transaction> rows{
        {1, "A", 10, QStringLiteral("餐饮"), QDate(2024, 2, 1)},
        {2, "B", 20, QStringLiteral("餐饮"), QDate(2024, 2, 29)},
        {3, "C", 40, QStringLiteral("交通"), QDate(2024, 1, 31)},
        {4, "D", 80, QStringLiteral("购物"), QDate(2024, 3, 1)},
        {5, "E", 160, QStringLiteral("学习"), QDate(2023, 2, 15)}};
    const auto result = CategorySummary::calculate(rows, QDate(2024, 2, 15));
    QCOMPARE(result[0].amountCents, 30); // Includes the last day, even after today's date.
    for (int row = 1; row < result.size(); ++row)
        QCOMPARE(result[row].amountCents, 0);
    QCOMPARE(CategorySummary::calculate(rows, QDate(2023, 2, 1))[4].amountCents, 160);
    const QList<Transaction> newYear{
        {1, "A", 1, QStringLiteral("其他"), QDate(2025, 12, 31)},
        {2, "B", 2, QStringLiteral("其他"), QDate(2026, 1, 1)}};
    QCOMPARE(CategorySummary::calculate(newYear, QDate(2026, 1, 31))[5].amountCents, 2);
}

void CategorySummaryTests::allCategoriesAndExactCents()
{
    QList<Transaction> rows;
    const QDate today(2026, 10, 13);
    qint64 expected = 0;
    for (const auto &category : Transaction::categories()) {
        rows.append({1, "A", Transaction::MaximumAmountCents, category, today});
        rows.append({2, "B", 1, category, today});
        expected += Transaction::MaximumAmountCents + 1;
    }
    qint64 sum = 0;
    for (const auto &entry : CategorySummary::calculate(rows, today)) {
        QCOMPARE(entry.amountCents, Transaction::MaximumAmountCents + 1);
        QCOMPARE(entry.transactionCount, 2);
        QCOMPARE(entry.changeCents, entry.amountCents);
        QCOMPARE(entry.formattedPercentage(), QStringLiteral("16.7%"));
        sum += entry.amountCents;
    }
    QCOMPARE(sum, expected);
    TransactionFilter filter;
    filter.category = QStringLiteral("学习");
    QCOMPARE(sum, ExpenseSummary::calculate(rows, filter, today).monthCents);
}

void CategorySummaryTests::countsSharesAndChanges()
{
    const auto categories = Transaction::categories();
    const QList<Transaction> rows{
        {1, "A", 10, categories[0], QDate(2026, 1, 1)},
        {2, "B", 20, categories[0], QDate(2026, 1, 31)},
        {3, "C", 20, categories[1], QDate(2026, 1, 15)},
        {4, "D", 50, categories[2], QDate(2026, 1, 20)},
        {5, "E", 5, categories[0], QDate(2025, 12, 1)},
        {6, "F", 5, categories[0], QDate(2025, 12, 31)},
        {7, "G", 40, categories[1], QDate(2025, 12, 15)},
        {8, "H", 50, categories[2], QDate(2025, 12, 20)},
        {9, "I", 123, categories[3], QDate(2025, 12, 20)},
        {10, "excluded", 999, categories[0], QDate(2025, 11, 30)},
        {11, "excluded", 999, categories[0], QDate(2026, 2, 1)},
        {12, "excluded", 999, categories[0], QDate(2025, 1, 1)}};
    const auto result = CategorySummary::calculate(rows, 2026, 1);
    const QList<qint64> amounts{30, 20, 50, 0, 0, 0};
    const QList<qint64> counts{2, 1, 1, 0, 0, 0};
    const QList<qint64> previous{10, 40, 50, 123, 0, 0};
    const QList<qint64> changes{20, -20, 0, -123, 0, 0};
    qint64 total = 0, count = 0;
    double percentage = 0;
    for (int row = 0; row < result.size(); ++row) {
        QCOMPARE(result[row].category, categories[row]);
        QCOMPARE(result[row].amountCents, amounts[row]);
        QCOMPARE(result[row].transactionCount, counts[row]);
        QCOMPARE(result[row].previousMonthAmountCents, previous[row]);
        QCOMPARE(result[row].changeCents, changes[row]);
        QCOMPARE(result[row].percentage, static_cast<double>(amounts[row]));
        total += result[row].amountCents;
        count += result[row].transactionCount;
        percentage += result[row].percentage;
    }
    TransactionFilter monthFilter;
    monthFilter.useDateRange = true;
    monthFilter.startDate = QDate(2026, 1, 1);
    monthFilter.endDate = QDate(2026, 1, 31);
    const auto summary = ExpenseSummary::calculate(rows, monthFilter, monthFilter.startDate);
    QCOMPARE(total, summary.monthCents);
    QCOMPARE(count, summary.filteredCount);
    QCOMPARE(total, 100);
    QCOMPARE(count, 4);
    QCOMPARE(percentage, 100.0);
}

void CategorySummaryTests::zeroMonthAndSingleCategory()
{
    const QList<Transaction> rows{
        {1, "A", 1, QStringLiteral("餐饮"), QDate(2030, 5, 1)},
        {2, "B", 2, QStringLiteral("餐饮"), QDate(2030, 5, 31)}};
    const auto current = CategorySummary::calculate(rows, 2030, 5);
    QCOMPARE(current[0].amountCents, 3);
    QCOMPARE(current[0].transactionCount, 2);
    QCOMPARE(current[0].percentage, 100.0);
    QCOMPARE(current[0].formattedPercentage(), QStringLiteral("100.0%"));
    QCOMPARE(current[0].previousMonthAmountCents, 0);
    QCOMPARE(current[0].changeCents, 3);
    // No June spending: previous month's spending must not enter count/share.
    const auto next = CategorySummary::calculate(rows, 2030, 6);
    for (const auto &entry : next) {
        QCOMPARE(entry.amountCents, 0);
        QCOMPARE(entry.transactionCount, 0);
        QCOMPARE(entry.percentage, 0.0);
        QCOMPARE(entry.formattedPercentage(), QStringLiteral("0.0%"));
    }
    QCOMPARE(next[0].previousMonthAmountCents, 3);
    QCOMPARE(next[0].changeCents, -3);
    for (int row = 1; row < current.size(); ++row) {
        QCOMPARE(current[row].amountCents, 0);
        QCOMPARE(current[row].transactionCount, 0);
        QCOMPARE(current[row].percentage, 0.0);
        QCOMPARE(current[row].changeCents, 0);
    }
}

void CategorySummaryTests::arbitraryMonthAndPercentageFormatting()
{
    const QList<Transaction> rows{
        {1, "A", 1, QStringLiteral("学习"), QDate(2024, 2, 29)},
        {2, "B", 2, QStringLiteral("其他"), QDate(2024, 2, 1)},
        {3, "C", 1, QStringLiteral("学习"), QDate(2024, 1, 31)}};
    const auto result = CategorySummary::calculate(rows, 2024, 2);
    QCOMPARE(result[4].formattedPercentage(), QStringLiteral("33.3%"));
    QCOMPARE(result[5].formattedPercentage(), QStringLiteral("66.7%"));
    QVERIFY(qAbs(result[4].percentage - 100.0 / 3) < 1e-10);
    QCOMPARE(result[4].changeCents, 0);
    QCOMPARE(result[5].changeCents, 2);
    // Keep the QDate overload compatible with the existing main window.
    const auto dated = CategorySummary::calculate(rows, QDate(2024, 2, 29));
    for (int row = 0; row < result.size(); ++row) {
        QCOMPARE(dated[row].amountCents, result[row].amountCents);
        QCOMPARE(dated[row].transactionCount, result[row].transactionCount);
        QCOMPARE(dated[row].percentage, result[row].percentage);
        QCOMPARE(dated[row].changeCents, result[row].changeCents);
    }
    for (const auto &entry : CategorySummary::calculate(rows, 2024, 13)) {
        QCOMPARE(entry.amountCents, 0);
        QCOMPARE(entry.transactionCount, 0);
        QCOMPARE(entry.percentage, 0.0);
        QCOMPARE(entry.changeCents, 0);
    }
}

void CategorySummaryTests::refreshWithCrudAndFilters()
{
    MainWindow window(QStringLiteral(":memory:")); window.showPage(MainWindow::Page::Billing);
    window.show();
    settle();
    auto *summary = window.findChild<QTableWidget *>("categorySummaryTable");
    auto *records = window.findChild<QTableWidget *>("transactionTable");
    QVERIFY(summary && records);
    QCOMPARE(summary->rowCount(), 6);
    auto check = [&](const QList<qint64> &expected) {
        const QList<qint64> purposes={expected[0],expected[3],0,expected[1],expected[4]+expected[5],expected[2]};
        qint64 sum = 0;
        for (int row = 0; row < 6; ++row) {
            QCOMPARE(summary->item(row, 0)->text(), CategoryCatalog::purposeLabel(CategoryCatalog::purposeKeys()[row]));
            QCOMPARE(summary->item(row, 1)->text(), Transaction::formatAmount(purposes[row]));
            sum += purposes[row];
        }
        QCOMPARE(window.findChild<QLabel *>("monthExpenseLabel")->text(),
                 QStringLiteral("本月支出：%1").arg(Transaction::formatAmount(sum)));
    };
    check({0, 0, 0, 0, 0, 0});
    auto *category = window.findChild<QComboBox *>("categoryFilter");
    category->setCurrentText(QStringLiteral("出行"));
    window.findChild<QCheckBox *>("useDateRangeCheckBox")->setChecked(true);
    window.findChild<QDateEdit *>("startDateFilter")->setDate(QDate::currentDate().addMonths(-1));
    window.findChild<QDateEdit *>("endDateFilter")->setDate(QDate::currentDate().addMonths(-1));
    window.findChild<QPushButton *>("applyFilterButton")->click();
    window.findChild<QPushButton *>("addTransactionButton")->click();
    auto *dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    fill(dialog, QStringLiteral("餐饮"), 28.01, QDate::currentDate());
    dialog->findChild<QPushButton *>("saveButton")->click();
    settle();
    QCOMPARE(records->rowCount(), 0);
    check({2801, 0, 0, 0, 0, 0});
    window.findChild<QPushButton *>("resetFilterButton")->click();
    QCOMPARE(records->rowCount(), 1);
    records->selectRow(0);
    window.findChild<QPushButton *>("editTransactionButton")->click();
    dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    fill(dialog, QStringLiteral("学习"), 32.50, QDate::currentDate());
    dialog->findChild<QPushButton *>("saveButton")->click();
    settle();
    check({0, 0, 0, 0, 3250, 0});
    // Editing a record out of this month removes its previous contribution.
    window.findChild<QPushButton *>("editTransactionButton")->click();
    dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    fill(dialog, QStringLiteral("学习"), 32.50, QDate::currentDate().addMonths(-1));
    dialog->findChild<QPushButton *>("saveButton")->click();
    settle();
    check({0, 0, 0, 0, 0, 0});
    window.findChild<QPushButton *>("editTransactionButton")->click();
    dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    fill(dialog, QStringLiteral("其他"), 0.01, QDate::currentDate());
    dialog->findChild<QPushButton *>("saveButton")->click();
    settle();
    check({0, 0, 0, 0, 0, 1});
    window.findChild<QPushButton *>("deleteTransactionButton")->click();
    auto *confirmation = window.findChild<QMessageBox *>();
    QVERIFY(confirmation);
    confirmation->button(QMessageBox::Yes)->click();
    settle();
    QCOMPARE(records->rowCount(), 0);
    check({0, 0, 0, 0, 0, 0});
}

void CategorySummaryTests::loadFailurePreservesSummary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("test.sqlite");
    DatabaseManager database(path);
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    Transaction record(0, "A", 100, QStringLiteral("餐饮"), QDate::currentDate());
    QVERIFY(repository.add(record));
    MainWindow window(path); window.showPage(MainWindow::Page::Billing);
    auto *summary = window.findChild<QTableWidget *>("categorySummaryTable");
    QCOMPARE(summary->item(0, 1)->text(), QStringLiteral("¥1.00"));
    QSqlQuery query(database.database());
    QVERIFY(query.exec("DROP TABLE transactions"));
    QVERIFY(!window.reloadTransactions());
    QCOMPARE(summary->item(0, 1)->text(), QStringLiteral("¥1.00"));
    QCOMPARE(window.findChild<QLabel *>("monthExpenseLabel")->text(), QStringLiteral("本月支出：¥1.00"));
    MainWindow failed(directory.path()); failed.showPage(MainWindow::Page::Billing);
    QCOMPARE(failed.findChild<QTableWidget *>("categorySummaryTable")->item(0, 1)->text(), QStringLiteral("—"));
}

void CategorySummaryTests::preview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("preview.sqlite");
    DatabaseManager database(path);
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    const auto today = QDate::currentDate();
    QList<Transaction> rows{
        {0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), today},
        {0, QStringLiteral("地铁"), 400, QStringLiteral("交通"), today},
        {0, QStringLiteral("教材"), 3250, QStringLiteral("学习"), today},
        {0, QStringLiteral("上月购物"), 9900, QStringLiteral("购物"), today.addMonths(-1)}};
    for (auto &row : rows)
        QVERIFY(repository.add(row));
    MainWindow window(path); window.showPage(MainWindow::Page::Billing);
    window.show();
    settle();
    const auto previewDir = qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!previewDir.isEmpty()) {
        QVERIFY(QDir().mkpath(previewDir));
        QVERIFY(window.grab().save(previewDir + "/category-summary.png"));
    }
    window.findChild<QComboBox *>("categoryFilter")->setCurrentText(QStringLiteral("吃喝"));
    window.findChild<QPushButton *>("applyFilterButton")->click();
    settle();
    QCOMPARE(window.findChild<QTableWidget *>("transactionTable")->rowCount(), 1);
    QCOMPARE(window.findChild<QLabel *>("monthExpenseLabel")->text(), QStringLiteral("本月支出：¥64.50"));
    if (!previewDir.isEmpty())
        QVERIFY(window.grab().save(previewDir + "/category-summary-filtered.png"));
}

QTEST_MAIN(CategorySummaryTests)
#include "category_summary_tests.moc"
