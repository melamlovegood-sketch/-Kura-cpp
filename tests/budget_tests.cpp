#include "BudgetRepository.h"
#include "BudgetWidget.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "AddTransactionDialog.h"
#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

static void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

static void fillExpense(AddTransactionDialog &dialog, double amount, const QDate &date)
{
    dialog.findChild<QLineEdit *>(QStringLiteral("descriptionEdit"))->setText(QStringLiteral("午饭"));
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("amountSpinBox"))->setValue(amount);
    dialog.findChild<QDateEdit *>(QStringLiteral("dateEdit"))->setDate(date);
}

class BudgetTests : public QObject
{
    Q_OBJECT
private slots:
    void unsetZeroExactAndOverspent();
    void monthIsolationAndBoundaries();
    void invalidInputsLeaveDataUnchanged();
    void writeAndReadFailures();
    void legacyDatabaseUpgradeKeepsExpenses();
    void survivesNewProcess();
    void widgetCrudAndRestart();
    void widgetZeroAndPrecision();
    void widgetFailureKeepsDraftAndMonth();
    void unavailableDatabaseDisablesBudget();
};

void BudgetTests::unsetZeroExactAndOverspent()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    BudgetRepository budgets(database);
    TransactionRepository expenses(database);
    const QDate month(2026, 10, 13);
    MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(month, status));
    QVERIFY(!status.hasBudget);
    QCOMPARE(status.spentCents, 0);
    Transaction item(0, QStringLiteral("午饭"), 1, QStringLiteral("餐饮"), month);
    QVERIFY(expenses.add(item));
    QVERIFY(budgets.loadStatus(month, status));
    QVERIFY(!status.hasBudget);
    QCOMPARE(status.spentCents, 1);
    QVERIFY(!status.isOverBudget());
    QVERIFY(budgets.save(MonthlyBudget(month, 0)));
    QVERIFY(budgets.loadStatus(month, status));
    QVERIFY(status.hasBudget);
    QVERIFY(status.isOverBudget());
    QCOMPARE(status.remainingCents(), -1);
    QCOMPARE(status.overspentCents(), 1);
    QVERIFY(budgets.save(MonthlyBudget(month, 1)));
    QVERIFY(budgets.loadStatus(month, status));
    QVERIFY(!status.isOverBudget());
    QCOMPARE(status.remainingCents(), 0);
    QVERIFY(budgets.save(MonthlyBudget(month, 2)));
    QVERIFY(budgets.loadStatus(month, status));
    QCOMPARE(status.remainingCents(), 1);
    QCOMPARE(status.overspentCents(), 0);
    QSqlQuery count(database.database());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM monthly_budgets")) && count.next());
    QCOMPARE(count.value(0).toInt(), 1); // Re-saving updates the existing month.
}

void BudgetTests::monthIsolationAndBoundaries()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    BudgetRepository budgets(database);
    TransactionRepository expenses(database);
    QList<Transaction> fixtures{
        Transaction(0, QStringLiteral("去年"), 10, QStringLiteral("餐饮"), QDate(2023, 12, 31)),
        Transaction(0, QStringLiteral("新年"), 20, QStringLiteral("餐饮"), QDate(2024, 1, 1)),
        Transaction(0, QStringLiteral("一月末"), 30, QStringLiteral("餐饮"), QDate(2024, 1, 31)),
        Transaction(0, QStringLiteral("二月"), 40, QStringLiteral("交通"), QDate(2024, 2, 1)),
        Transaction(0, QStringLiteral("闰日"), 50, QStringLiteral("餐饮"), QDate(2024, 2, 29)),
        Transaction(0, QStringLiteral("三月"), 60, QStringLiteral("餐饮"), QDate(2024, 3, 1)),
        Transaction(0, QStringLiteral("最大年份"), 70, QStringLiteral("餐饮"), QDate(9999, 12, 31))};
    for (auto &item : fixtures)
        QVERIFY(expenses.add(item));
    QVERIFY(budgets.save(MonthlyBudget(QDate(2024, 1, 15), 100)));
    QVERIFY(budgets.save(MonthlyBudget(QDate(2024, 2, 29), 200)));
    MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(QDate(2024, 1, 31), status));
    QCOMPARE(status.month, QDate(2024, 1, 1));
    QCOMPARE(status.limitCents, 100);
    QCOMPARE(status.spentCents, 50);
    QVERIFY(budgets.loadStatus(QDate(2024, 2, 1), status));
    QCOMPARE(status.limitCents, 200);
    QCOMPARE(status.spentCents, 90);
    QVERIFY(budgets.loadStatus(QDate(2023, 12, 1), status));
    QVERIFY(!status.hasBudget);
    QCOMPARE(status.spentCents, 10);
    QVERIFY(budgets.loadStatus(QDate(2025, 1, 1), status));
    QVERIFY(!status.hasBudget);
    QCOMPARE(status.spentCents, 0);
    QVERIFY(budgets.loadStatus(QDate(9999, 12, 1), status));
    QCOMPARE(status.spentCents, 70);
}

void BudgetTests::invalidInputsLeaveDataUnchanged()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    BudgetRepository budgets(database);
    const QDate month(2026, 10, 1);
    QVERIFY(budgets.save(MonthlyBudget(month, 100)));
    QVERIFY(!budgets.save(MonthlyBudget(month, -1)));
    QVERIFY(!budgets.save(MonthlyBudget(month, MonthlyBudget::MaximumLimitCents + 1)));
    QVERIFY(!budgets.save(MonthlyBudget(QDate(), 100)));
    QVERIFY(!budgets.save(MonthlyBudget(QDate(10000, 1, 1), 100)));
    MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(month, status));
    QCOMPARE(status.limitCents, 100);
    QVERIFY(!budgets.loadStatus(QDate(), status));
    QCOMPARE(status.month, month);
    QCOMPARE(status.limitCents, 100);
    QVERIFY(budgets.save(MonthlyBudget(month, MonthlyBudget::MaximumLimitCents)));
    QVERIFY(budgets.loadStatus(month, status));
    QCOMPARE(status.limitCents, MonthlyBudget::MaximumLimitCents);
}

void BudgetTests::writeAndReadFailures()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("budget-lock.sqlite"));
    DatabaseManager database(path), blocker(path);
    QVERIFY(database.initialize());
    QVERIFY(blocker.initialize());
    BudgetRepository budgets(database);
    const QDate month(2026, 10, 1);
    QVERIFY(budgets.save(MonthlyBudget(month, 2000)));
    auto connection = database.database();
    connection.close();
    connection.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=20"));
    QVERIFY(connection.open());
    QSqlQuery lock(blocker.database());
    QVERIFY(lock.exec(QStringLiteral("BEGIN IMMEDIATE")));
    QVERIFY(!budgets.save(MonthlyBudget(month, 3000)));
    QVERIFY(budgets.lastError().contains(QStringLiteral("占用")));
    QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
    connection.close();
    connection.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    QVERIFY(connection.open());
    QVERIFY(!budgets.save(MonthlyBudget(month, 3000)));
    QVERIFY(budgets.lastError().contains(QStringLiteral("只读")));
    MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(month, status));
    QCOMPARE(status.limitCents, 2000);
    QVERIFY(lock.exec(QStringLiteral("DROP TABLE monthly_budgets")));
    QVERIFY(!budgets.loadStatus(month.addMonths(1), status));
    QCOMPARE(status.month, month);
    QCOMPARE(status.limitCents, 2000);
}

void BudgetTests::legacyDatabaseUpgradeKeepsExpenses()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("legacy.sqlite"));
    qint64 id = 0;
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository expenses(database);
        Transaction item(0, QStringLiteral("原有消费"), 1234, QStringLiteral("餐饮"), QDate(2026, 10, 13));
        QVERIFY(expenses.add(item));
        id = item.id();
        QSqlQuery query(database.database());
        QVERIFY(query.exec(QStringLiteral("DROP TABLE monthly_budgets"))); // Simulate the old schema.
    }
    DatabaseManager upgraded(path);
    QVERIFY(upgraded.initialize());
    TransactionRepository expenses(upgraded);
    QList<Transaction> rows;
    QVERIFY(expenses.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().id(), id);
    BudgetRepository budgets(upgraded);
    MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(QDate(2026, 10, 1), status));
    QVERIFY(!status.hasBudget);
    QCOMPARE(status.spentCents, 1234);
    QVERIFY(budgets.save(MonthlyBudget(status.month, 2000)));
    QVERIFY(budgets.loadStatus(status.month, status));
    QCOMPARE(status.remainingCents(), 766);
}

void BudgetTests::survivesNewProcess()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("process-budget.sqlite"));
    for (const auto &mode : {QStringLiteral("--budget-write"), QStringLiteral("--budget-read")}) {
        QProcess process;
        process.start(QCoreApplication::applicationFilePath(), {mode, path});
        QVERIFY(process.waitForStarted(5000));
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    }
}

void BudgetTests::widgetCrudAndRestart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("widget-budget.sqlite"));
    const QDate month = MonthlyBudget::firstDay(QDate::currentDate());
    const QDate next = month.addMonths(1);
    {
        MainWindow window(path); window.showPage(MainWindow::Page::Billing);
        window.show();
        settle();
        auto *amount = window.findChild<QDoubleSpinBox *>(QStringLiteral("budgetAmountSpinBox"));
        auto *save = window.findChild<QPushButton *>(QStringLiteral("saveBudgetButton"));
        auto *monthEdit = window.findChild<QDateEdit *>(QStringLiteral("budgetMonthEdit"));
        auto *limit = window.findChild<QLabel *>(QStringLiteral("budgetLimitLabel"));
        auto *spent = window.findChild<QLabel *>(QStringLiteral("budgetSpentLabel"));
        auto *remaining = window.findChild<QLabel *>(QStringLiteral("budgetRemainingLabel"));
        auto *status = window.findChild<QLabel *>(QStringLiteral("budgetStatusLabel"));
        QCOMPARE(limit->text(), QStringLiteral("预算：未设置"));
        QCOMPARE(remaining->text(), QStringLiteral("剩余额度：—"));
        amount->setValue(100);
        save->click();
        QCOMPARE(limit->text(), QStringLiteral("预算：¥100.00"));
        QCOMPARE(remaining->text(), QStringLiteral("剩余额度：¥100.00"));
        window.findChild<QPushButton *>(QStringLiteral("addTransactionButton"))->click();
        settle();
        auto *dialog = window.findChild<AddTransactionDialog *>();
        QVERIFY(dialog);
        fillExpense(*dialog, 50, month);
        dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
        settle();
        QCOMPARE(spent->text(), QStringLiteral("已消费：¥50.00"));
        QCOMPARE(remaining->text(), QStringLiteral("剩余额度：¥50.00"));
        window.findChild<QPushButton *>(QStringLiteral("editTransactionButton"))->click();
        settle();
        dialog = window.findChild<AddTransactionDialog *>();
        QVERIFY(dialog);
        fillExpense(*dialog, 125, month);
        dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
        settle();
        QCOMPARE(spent->text(), QStringLiteral("已消费：¥125.00"));
        QCOMPARE(remaining->text(), QStringLiteral("剩余额度：-¥25.00"));
        QVERIFY(status->text().contains(QStringLiteral("已超支 ¥25.00")));
        const auto warningColor = status->palette().color(QPalette::WindowText);
        QVERIFY(warningColor.red() > warningColor.green() * 1.5);
        const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
        if (!preview.isEmpty()) {
            QVERIFY(QDir().mkpath(preview));
            QVERIFY(window.grab().save(preview + QStringLiteral("/budget-overspent.png")));
        }
        auto *category = window.findChild<QComboBox *>(QStringLiteral("categoryFilter"));
        category->setCurrentText(QStringLiteral("交通"));
        window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
        QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(), 0);
        QCOMPARE(spent->text(), QStringLiteral("已消费：¥125.00")); // Filtering never changes budget scope.
        window.findChild<QPushButton *>(QStringLiteral("resetFilterButton"))->click();
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
        table->selectRow(0);
        window.findChild<QPushButton *>(QStringLiteral("editTransactionButton"))->click();
        settle();
        dialog = window.findChild<AddTransactionDialog *>();
        QVERIFY(dialog);
        fillExpense(*dialog, 125, next);
        dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
        settle();
        QCOMPARE(spent->text(), QStringLiteral("已消费：¥0.00"));
        QCOMPARE(remaining->text(), QStringLiteral("剩余额度：¥100.00"));
        QVERIFY(!status->text().contains(QStringLiteral("超支")));
        monthEdit->setDate(next);
        QCOMPARE(spent->text(), QStringLiteral("已消费：¥125.00"));
        QCOMPARE(limit->text(), QStringLiteral("预算：未设置"));
        amount->setValue(130);
        save->click();
        QCOMPARE(remaining->text(), QStringLiteral("剩余额度：¥5.00"));
        window.findChild<QPushButton *>(QStringLiteral("deleteTransactionButton"))->click();
        settle();
        auto *confirmation = window.findChild<QMessageBox *>();
        QVERIFY(confirmation);
        confirmation->button(QMessageBox::Yes)->click();
        settle();
        QCOMPARE(spent->text(), QStringLiteral("已消费：¥0.00"));
        QCOMPARE(remaining->text(), QStringLiteral("剩余额度：¥130.00"));
    }
    MainWindow restarted(path); restarted.showPage(MainWindow::Page::Billing);
    restarted.show();
    settle();
    QCOMPARE(restarted.findChild<QLabel *>(QStringLiteral("budgetLimitLabel"))->text(), QStringLiteral("预算：¥100.00"));
    restarted.findChild<QDateEdit *>(QStringLiteral("budgetMonthEdit"))->setDate(next);
    QCOMPARE(restarted.findChild<QLabel *>(QStringLiteral("budgetLimitLabel"))->text(), QStringLiteral("预算：¥130.00"));
    QCOMPARE(restarted.findChild<QLabel *>(QStringLiteral("budgetRemainingLabel"))->text(), QStringLiteral("剩余额度：¥130.00"));
}

void BudgetTests::widgetZeroAndPrecision()
{
    MainWindow window(QStringLiteral(":memory:")); window.showPage(MainWindow::Page::Billing);
    window.show();
    settle();
    auto *amount = window.findChild<QDoubleSpinBox *>(QStringLiteral("budgetAmountSpinBox"));
    auto *save = window.findChild<QPushButton *>(QStringLiteral("saveBudgetButton"));
    auto *status = window.findChild<QLabel *>(QStringLiteral("budgetStatusLabel"));
    save->click();
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("budgetLimitLabel"))->text(), QStringLiteral("预算：¥0.00"));
    QCOMPARE(status->text(), QStringLiteral("预算已用完。"));
    window.findChild<QPushButton *>(QStringLiteral("addTransactionButton"))->click();
    settle();
    auto *dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    fillExpense(*dialog, 0.01, QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QVERIFY(status->text().contains(QStringLiteral("已超支 ¥0.01")));
    amount->setKeyboardTracking(false);
    amount->selectAll();
    QTest::keyClicks(amount, "0.01");
    save->click();
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("budgetLimitLabel"))->text(), QStringLiteral("预算：¥0.01"));
    QCOMPARE(status->text(), QStringLiteral("预算已用完。"));
    amount->setValue(0.02);
    save->click();
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("budgetRemainingLabel"))->text(), QStringLiteral("剩余额度：¥0.01"));
}

void BudgetTests::widgetFailureKeepsDraftAndMonth()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("widget-failure.sqlite"));
    MainWindow window(path); window.showPage(MainWindow::Page::Billing);
    window.findChild<QPushButton *>("billingEditBudgetButton")->click();
    window.show();
    settle();
    auto *amount = window.findChild<QDoubleSpinBox *>(QStringLiteral("budgetAmountSpinBox"));
    auto *save = window.findChild<QPushButton *>(QStringLiteral("saveBudgetButton"));
    auto *month = window.findChild<QDateEdit *>(QStringLiteral("budgetMonthEdit"));
    auto *limit = window.findChild<QLabel *>(QStringLiteral("budgetLimitLabel"));
    auto *error = window.findChild<QLabel *>(QStringLiteral("budgetErrorLabel"));
    const QDate current = month->date();
    amount->setValue(20);
    save->click();
    DatabaseManager blocker(path);
    QVERIFY(blocker.initialize());
    QSqlQuery lock(blocker.database());
    QVERIFY(lock.exec(QStringLiteral("BEGIN IMMEDIATE")));
    amount->setValue(30);
    save->click();
    settle();
    QCOMPARE(amount->value(), 30.0);
    QCOMPARE(limit->text(), QStringLiteral("预算：¥20.00"));
    QVERIFY(error->isVisible() && error->text().contains(QStringLiteral("占用")));
    QVERIFY(save->isEnabled());
    QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
    save->click();
    QCOMPARE(limit->text(), QStringLiteral("预算：¥30.00"));
    QVERIFY(error->isHidden());
    amount->setValue(42); // A pending budget draft survives a transaction-list refresh.
    window.findChild<QPushButton *>(QStringLiteral("addTransactionButton"))->click();
    settle();
    auto *dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    fillExpense(*dialog, 12, current);
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QCOMPARE(amount->value(), 42.0);
    QCOMPARE(limit->text(), QStringLiteral("预算：¥30.00"));
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("budgetSpentLabel"))->text(), QStringLiteral("已消费：¥12.00"));
    QVERIFY(lock.exec(QStringLiteral("DROP TABLE monthly_budgets")));
    month->setDate(current.addMonths(1));
    settle();
    QCOMPARE(month->date(), current); // Failed loading must not mislabel the previous month's data.
    QCOMPARE(amount->value(), 42.0);
    QCOMPARE(limit->text(), QStringLiteral("预算：¥30.00"));
    QVERIFY(error->isVisible() && error->text().contains(QStringLiteral("加载预算失败")));
}

void BudgetTests::unavailableDatabaseDisablesBudget()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MainWindow window(directory.path()); window.showPage(MainWindow::Page::Billing);
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("saveBudgetButton"))->isEnabled());
    QVERIFY(!window.findChild<QDateEdit *>(QStringLiteral("budgetMonthEdit"))->isEnabled());
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("budgetSpentLabel"))->text(), QStringLiteral("已消费：—"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KuraTests"));
    app.setApplicationName(QStringLiteral("BudgetTests"));
    QStandardPaths::setTestModeEnabled(true);
    const auto arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1].startsWith(QStringLiteral("--budget-"))) {
        DatabaseManager database(arguments[2]);
        if (!database.initialize())
            return 1;
        BudgetRepository budgets(database);
        const QDate month(2026, 10, 1);
        if (arguments[1] == QStringLiteral("--budget-write")) {
            TransactionRepository expenses(database);
            Transaction item(0, QStringLiteral("午饭"), 3100, QStringLiteral("餐饮"), QDate(2026, 10, 13));
            return budgets.save(MonthlyBudget(month, 10000)) && expenses.add(item) ? 0 : 2;
        }
        MonthlyBudgetStatus status;
        return budgets.loadStatus(month, status) && status.hasBudget && status.limitCents == 10000
            && status.spentCents == 3100 && status.remainingCents() == 6900 ? 0 : 3;
    }
    BudgetTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "budget_tests.moc"
