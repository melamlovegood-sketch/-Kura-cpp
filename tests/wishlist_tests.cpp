#include "WishlistRepository.h"
#include "WishlistWidget.h"
#include "WishDialog.h"
#include "AddTransactionDialog.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "BudgetRepository.h"
#include "mainwindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

static QDateTime referenceTime()
{
    return QDateTime::fromString(QStringLiteral("2026-10-13T10:00:00Z"), Qt::ISODate);
}
static WishItem wish(const QString &name = QStringLiteral("耳机"), qint64 cents = 30000)
{
    WishItem item;
    item.name = name;
    item.estimatedCents = cents;
    item.coolingDays = 1;
    item.reason = QStringLiteral("通勤听音乐");
    return item;
}
static Transaction expense(qint64 cents = 25000)
{
    return Transaction(0, QStringLiteral("耳机"), cents, QStringLiteral("购物"), QDate::currentDate());
}
static void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
static QPushButton *button(QWidget &widget, const char *name)
{
    return widget.findChild<QPushButton *>(QString::fromLatin1(name));
}
static QString text(QWidget &widget, const char *name)
{
    return widget.findChild<QLabel *>(QString::fromLatin1(name))->text();
}
static void fill(WishDialog &dialog, const QString &name, double amount, int days = 1)
{
    dialog.findChild<QLineEdit *>(QStringLiteral("wishNameEdit"))->setText(name);
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("wishAmountSpinBox"))->setValue(amount);
    dialog.findChild<QSpinBox *>(QStringLiteral("wishDaysSpinBox"))->setValue(days);
}

class WishlistTests : public QObject
{
    Q_OBJECT
private slots:
    void timingAndValidation();
    void repositoryLifecycle();
    void purchaseAtomicAndOnce();
    void writeFailuresKeepData();
    void upgradeAndNewProcess();
    void dialogCrudAndRestart();
    void countdownAndFilters();
    void purchaseUpdatesWindow();
    void widgetFailuresAndRetry();
};

void WishlistTests::timingAndValidation()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    WishlistRepository repository(database);
    auto item = wish();
    const auto now = referenceTime();
    QVERIFY(repository.add(item, now));
    QCOMPARE(item.readyAt, now.addSecs(86400));
    QVERIFY(!item.canPurchase(item.readyAt.addMSecs(-1)));
    QVERIFY(item.statusText(item.readyAt.addMSecs(-1)).contains(QStringLiteral("0 天 0 小时 1 分")));
    QVERIFY(item.canPurchase(item.readyAt));
    QVERIFY(item.canPurchase(item.readyAt.addSecs(100)));
    QVERIFY(!item.canPurchase(QDateTime()));
    QVERIFY(item.validationError().isEmpty());
    auto optionalReason = wish(QStringLiteral("无备注"), 1);
    optionalReason.reason = QString();
    QVERIFY(repository.add(optionalReason, now.toOffsetFromUtc(8 * 3600)));
    QCOMPARE(optionalReason.createdAt, now);
    QCOMPARE(optionalReason.readyAt.toMSecsSinceEpoch() - optionalReason.createdAt.toMSecsSinceEpoch(), 86400000LL);
    QList<WishItem> restored;
    QVERIFY(repository.findAll(restored));
    QCOMPARE(restored.size(), 2);
    QVERIFY(restored[0].reason.isEmpty());
    QVERIFY(repository.update(optionalReason, now.addSecs(1)));
    item.coolingDays = 0;
    QVERIFY(!item.inputError().isEmpty());
    item.coolingDays = 31;
    QVERIFY(!item.inputError().isEmpty());
    item.coolingDays = 1;
    item.reason = QString(501, QChar('x'));
    QVERIFY(!item.inputError().isEmpty());
    item.reason.clear();
    item.estimatedCents = 0;
    QVERIFY(!item.inputError().isEmpty());
    item.estimatedCents = 1;
    item.category = QStringLiteral("无效分类");
    QVERIFY(!item.inputError().isEmpty());
}

void WishlistTests::repositoryLifecycle()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    WishlistRepository repository(database);
    auto item = wish(QStringLiteral("  耳机 '); DROP TABLE wishlist; --  "), 1);
    const auto now = referenceTime();
    QVERIFY(repository.add(item, now));
    const qint64 id = item.id;
    QVERIFY(id > 0);
    QVERIFY(!item.name.startsWith(QChar(' ')));
    QVERIFY(!repository.add(item, now));
    item.name = QStringLiteral("新耳机");
    item.coolingDays = 3;
    QVERIFY(repository.update(item, now.addSecs(3600)));
    QList<WishItem> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].id, id);
    QCOMPARE(rows[0].createdAt, now);
    QCOMPARE(rows[0].readyAt, now.addSecs(3600 + 3 * 86400));
    QVERIFY(!repository.abandon(id, now.addSecs(-1)));
    QVERIFY(repository.abandon(id, now.addSecs(7200))); // No need to wait to abandon.
    QVERIFY(!repository.update(item, now.addSecs(8000)));
    QVERIFY(!repository.abandon(id, now.addSecs(8000)));
    auto purchase = expense();
    QVERIFY(!repository.purchase(id, purchase, now.addDays(4)));
    QCOMPARE(purchase.id(), 0);
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows[0].status, WishStatus::Abandoned);
    QVERIFY(!rows[0].canPurchase(now.addDays(100)));
    TransactionRepository transactions(database);
    QList<Transaction> expenses;
    QVERIFY(transactions.findAll(expenses));
    QVERIFY(expenses.isEmpty());
    QVERIFY(!repository.remove(0));
    QVERIFY(repository.remove(id));
    QVERIFY(!repository.remove(id));
    QVERIFY(repository.findAll(rows));
    QVERIFY(rows.isEmpty());
}

void WishlistTests::purchaseAtomicAndOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DatabaseManager database(directory.filePath(QStringLiteral("atomic.sqlite")));
    QVERIFY(database.initialize());
    WishlistRepository repository(database);
    auto item = wish();
    QVERIFY(repository.add(item, referenceTime()));
    auto purchase = expense();
    QVERIFY(!repository.purchase(item.id, purchase, item.readyAt.addMSecs(-1)));
    QSqlQuery query(database.database());
    // Simulate a failure after claiming the wish, before its expense is saved.
    QVERIFY(query.exec(QStringLiteral("CREATE TRIGGER reject_expense BEFORE INSERT ON transactions "
        "BEGIN SELECT RAISE(ABORT,'test expense failure'); END")));
    QVERIFY(!repository.purchase(item.id, purchase, item.readyAt));
    QCOMPARE(purchase.id(), 0);
    QList<WishItem> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows[0].status, WishStatus::Waiting);
    QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_expense")));
    // Also test rollback after an expense insert succeeds but its linkage fails.
    QVERIFY(query.exec(QStringLiteral("CREATE TRIGGER reject_link BEFORE UPDATE OF transaction_id ON wishlist "
        "BEGIN SELECT RAISE(ABORT,'test linkage failure'); END")));
    QVERIFY(!repository.purchase(item.id, purchase, item.readyAt));
    TransactionRepository transactions(database);
    QList<Transaction> expenses;
    QVERIFY(transactions.findAll(expenses));
    QCOMPARE(expenses.size(), 0);
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows[0].status, WishStatus::Waiting);
    QVERIFY(query.exec(QStringLiteral("DROP TRIGGER reject_link")));
    QVERIFY(repository.purchase(item.id, purchase, item.readyAt));
    QVERIFY(purchase.id() > 0);
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows[0].status, WishStatus::Purchased);
    QCOMPARE(rows[0].transactionId, purchase.id());
    QCOMPARE(rows[0].estimatedCents, 30000); // Actual expense can differ from estimate.
    QVERIFY(transactions.findAll(expenses));
    QCOMPARE(expenses.size(), 1);
    QCOMPARE(expenses[0].amountCents(), 25000);
    DatabaseManager second(database.databasePath());
    QVERIFY(second.initialize());
    WishlistRepository other(second);
    auto duplicate = expense();
    QVERIFY(!other.purchase(item.id, duplicate, item.readyAt));
    QVERIFY(!other.abandon(item.id, item.readyAt));
    QVERIFY(other.remove(item.id));
    QVERIFY(transactions.findAll(expenses));
    QCOMPARE(expenses.size(), 1); // Deleting wish history never deletes expenses.
}

void WishlistTests::writeFailuresKeepData()
{
    QTemporaryDir directory;
    DatabaseManager database(directory.filePath(QStringLiteral("locked.sqlite")));
    DatabaseManager blocker(database.databasePath());
    QVERIFY(database.initialize());
    QVERIFY(blocker.initialize());
    WishlistRepository repository(database);
    auto item = wish();
    QVERIFY(repository.add(item, referenceTime()));
    QSqlQuery lock(blocker.database());
    QVERIFY(lock.exec(QStringLiteral("BEGIN IMMEDIATE")));
    auto draft = wish(QStringLiteral("键盘"));
    QVERIFY(!repository.add(draft, referenceTime()));
    QCOMPARE(draft.id, 0);
    QVERIFY(repository.lastError().contains(QStringLiteral("占用")));
    auto purchase = expense();
    QVERIFY(!repository.purchase(item.id, purchase, item.readyAt));
    QCOMPARE(purchase.id(), 0);
    QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
    QList<WishItem> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].status, WishStatus::Waiting);
    QSqlQuery readonly(database.database());
    QVERIFY(readonly.exec(QStringLiteral("PRAGMA query_only=ON")));
    QVERIFY(!repository.update(item, referenceTime()));
    QVERIFY(!repository.abandon(item.id, referenceTime()));
    QVERIFY(!repository.remove(item.id));
    QVERIFY(!repository.purchase(item.id, purchase, item.readyAt));
    QVERIFY(readonly.exec(QStringLiteral("PRAGMA query_only=OFF")));
    QVERIFY(repository.purchase(item.id, purchase, item.readyAt));
    const auto previous = rows;
    QVERIFY(readonly.exec(QStringLiteral("DROP TABLE wishlist")));
    QVERIFY(!repository.findAll(rows));
    QCOMPARE(rows[0].id, previous[0].id);
    QCOMPARE(rows[0].status, previous[0].status);
}

void WishlistTests::upgradeAndNewProcess()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("upgrade.sqlite"));
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository transactions(database);
        auto existing = expense(1250);
        QVERIFY(transactions.add(existing));
        BudgetRepository budgets(database);
        QVERIFY(budgets.save(MonthlyBudget(QDate::currentDate(), 50000)));
        QSqlQuery query(database.database());
        QVERIFY(query.exec(QStringLiteral("DROP TABLE wishlist"))); // Emulate v0.5.
        QVERIFY(database.initialize());
        WishlistRepository repository(database);
        auto item = wish();
        QVERIFY(repository.add(item, referenceTime()));
    }
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--wish-probe"), path});
    QVERIFY(child.waitForFinished(10000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);
}

void WishlistTests::dialogCrudAndRestart()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("ui.sqlite"));
    {
        MainWindow window(path);
        window.show();
        window.findChild<QTabWidget *>(QStringLiteral("mainPages"))->setCurrentIndex(2);
        settle();
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("wishlistTable"));
        QVERIFY(!button(window, "purchaseWishButton")->isEnabled());
        button(window, "addWishButton")->click();
        settle();
        auto *dialog = window.findChild<WishDialog *>();
        QVERIFY(dialog);
        button(*dialog, "saveWishButton")->click();
        QVERIFY(!text(*dialog, "wishValidationError").isEmpty());
        fill(*dialog, QStringLiteral("  耳机  "), 300, 3);
        const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
        if (!preview.isEmpty()) {
            QVERIFY(QDir().mkpath(preview));
            QVERIFY(dialog->grab().save(preview + QStringLiteral("/wishlist-dialog.png")));
        }
        button(*dialog, "saveWishButton")->click();
        settle();
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("耳机"));
        const qint64 id = table->item(0, 0)->data(Qt::UserRole).toLongLong();
        table->selectRow(0);
        QVERIFY(!button(window, "purchaseWishButton")->isEnabled());
        button(window, "editWishButton")->click();
        settle();
        dialog = window.findChild<WishDialog *>();
        QVERIFY(dialog);
        fill(*dialog, QStringLiteral("键盘"), 180, 1);
        button(*dialog, "cancelWishButton")->click();
        settle();
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("耳机"));
        button(window, "editWishButton")->click();
        settle();
        dialog = window.findChild<WishDialog *>();
        QVERIFY(dialog);
        fill(*dialog, QStringLiteral("键盘"), 180, 1);
        button(*dialog, "saveWishButton")->click();
        settle();
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("键盘"));
        QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), id);
        button(window, "abandonWishButton")->click();
        settle();
        auto *confirmation = window.findChild<QMessageBox *>();
        QVERIFY(confirmation);
        QCOMPARE(static_cast<QAbstractButton *>(confirmation->defaultButton()), confirmation->button(QMessageBox::No));
        confirmation->button(QMessageBox::No)->click();
        settle();
        QVERIFY(button(window, "abandonWishButton")->isEnabled());
        button(window, "abandonWishButton")->click();
        settle();
        confirmation = window.findChild<QMessageBox *>();
        QVERIFY(confirmation);
        confirmation->button(QMessageBox::Yes)->click();
        settle();
        QCOMPARE(table->item(0, 3)->text(), QStringLiteral("已放弃"));
        QVERIFY(text(window, "wishlistSummaryLabel").contains(QStringLiteral("¥180.00")));
        QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(), 0);
        QVERIFY(!button(window, "editWishButton")->isEnabled());
    }
    MainWindow restarted(path);
    auto *table = restarted.findChild<QTableWidget *>(QStringLiteral("wishlistTable"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("已放弃"));
    table->selectRow(0);
    button(restarted, "deleteWishButton")->click();
    settle();
    auto *confirmation = restarted.findChild<QMessageBox *>();
    QVERIFY(confirmation);
    confirmation->button(QMessageBox::Yes)->click();
    settle();
    QCOMPARE(table->rowCount(), 0);
}

void WishlistTests::countdownAndFilters()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    WishlistRepository repository(database);
    const auto now = referenceTime();
    auto item = wish();
    QVERIFY(repository.add(item, now));
    WishlistWidget widget(database);
    widget.setDatabaseAvailable(true);
    auto *table = widget.findChild<QTableWidget *>(QStringLiteral("wishlistTable"));
    auto *filter = widget.findChild<QComboBox *>(QStringLiteral("wishlistStatusFilter"));
    widget.refreshCountdowns(item.readyAt.addMSecs(-1));
    table->selectRow(0);
    QVERIFY(!button(widget, "purchaseWishButton")->isEnabled());
    auto *cell = table->item(0, 0);
    widget.refreshCountdowns(item.readyAt);
    QVERIFY(button(widget, "purchaseWishButton")->isEnabled());
    QCOMPARE(table->item(0, 0), cell); // Timer does not rebuild unchanged rows or disrupt selection.
    QCOMPARE(table->currentRow(), 0);
    widget.refreshCountdowns(now);
    QVERIFY(!button(widget, "purchaseWishButton")->isEnabled()); // Clock moved backwards.
    filter->setCurrentIndex(1);
    widget.refreshCountdowns(now);
    QCOMPARE(table->rowCount(), 1);
    widget.refreshCountdowns(item.readyAt);
    QCOMPARE(table->rowCount(), 0); // Item automatically leaves the cooling-only filter.
    filter->setCurrentIndex(2);
    widget.refreshCountdowns(item.readyAt);
    QCOMPARE(table->rowCount(), 1);
    filter->setCurrentIndex(3);
    QCOMPARE(table->rowCount(), 0);
}

void WishlistTests::purchaseUpdatesWindow()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("purchase-ui.sqlite"));
    DatabaseManager database(path);
    QVERIFY(database.initialize());
    WishlistRepository repository(database);
    auto item = wish();
    QVERIFY(repository.add(item, QDateTime::currentDateTimeUtc().addDays(-2)));
    auto waiting = wish(QStringLiteral("新键盘"), 18000);
    waiting.coolingDays = 3;
    QVERIFY(repository.add(waiting));
    BudgetRepository budgets(database);
    QVERIFY(budgets.save(MonthlyBudget(QDate::currentDate(), 20000)));
    {
        MainWindow window(path);
        window.show();
        window.findChild<QTabWidget *>(QStringLiteral("mainPages"))->setCurrentIndex(2);
        window.findChild<QSpinBox *>(QStringLiteral("statisticsYearSpinBox"))->setValue(QDate::currentDate().year());
        settle();
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("wishlistTable"));
        table->selectRow(1);
        QVERIFY(button(window, "purchaseWishButton")->isEnabled());
        const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
        if (!preview.isEmpty())
            QVERIFY(window.grab().save(preview + QStringLiteral("/wishlist-window.png")));
        button(window, "purchaseWishButton")->click();
        settle();
        auto *dialog = window.findChild<AddTransactionDialog *>();
        QVERIFY(dialog);
        QCOMPARE(dialog->amount(), 300.0);
        button(*dialog, "cancelButton")->click();
        settle();
        QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(), 0);
        button(window, "purchaseWishButton")->click();
        settle();
        dialog = window.findChild<AddTransactionDialog *>();
        QVERIFY(dialog);
        dialog->findChild<QDoubleSpinBox *>(QStringLiteral("amountSpinBox"))->setValue(250);
        button(*dialog, "saveButton")->click();
        settle();
        QCOMPARE(table->item(1, 3)->text(), QStringLiteral("已购买"));
        QVERIFY(!button(window, "purchaseWishButton")->isEnabled());
        QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(), 1);
        QCOMPARE(text(window, "totalExpenseLabel"), QStringLiteral("全部支出：¥250.00"));
        QCOMPARE(text(window, "budgetSpentLabel"), QStringLiteral("已消费：¥250.00"));
        QCOMPARE(text(window, "budgetRemainingLabel"), QStringLiteral("剩余额度：-¥50.00"));
        QVERIFY(text(window, "yearExpenseLabel").contains(QStringLiteral("¥250.00")));
    }
    MainWindow restarted(path);
    QCOMPARE(text(restarted, "totalExpenseLabel"), QStringLiteral("全部支出：¥250.00"));
    auto *table = restarted.findChild<QTableWidget *>(QStringLiteral("wishlistTable"));
    QCOMPARE(table->item(1, 3)->text(), QStringLiteral("已购买"));
}

void WishlistTests::widgetFailuresAndRetry()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("retry.sqlite"));
    DatabaseManager database(path);
    QVERIFY(database.initialize());
    WishlistRepository repository(database);
    auto item = wish();
    QVERIFY(repository.add(item, QDateTime::currentDateTimeUtc().addDays(-2)));
    WishlistWidget widget(database);
    widget.show();
    widget.setDatabaseAvailable(true);
    auto *table = widget.findChild<QTableWidget *>(QStringLiteral("wishlistTable"));
    table->selectRow(0);
    QSqlQuery query(database.database());
    QVERIFY(query.exec(QStringLiteral("PRAGMA query_only=ON")));
    button(widget, "addWishButton")->click();
    settle();
    auto *wishDialog = widget.findChild<WishDialog *>();
    QVERIFY(wishDialog);
    fill(*wishDialog, QStringLiteral("教材"), 12.50);
    button(*wishDialog, "saveWishButton")->click();
    QVERIFY(wishDialog->isVisible());
    QCOMPARE(wishDialog->item().estimatedCents, 1250);
    QVERIFY(button(*wishDialog, "saveWishButton")->isEnabled());
    QVERIFY(query.exec(QStringLiteral("PRAGMA query_only=OFF")));
    button(*wishDialog, "saveWishButton")->click();
    settle();
    QCOMPARE(table->rowCount(), 2);
    table->selectRow(1);
    button(widget, "purchaseWishButton")->click();
    settle();
    auto *dialog = widget.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    QVERIFY(query.exec(QStringLiteral("PRAGMA query_only=ON")));
    button(*dialog, "saveButton")->click();
    QVERIFY(dialog->isVisible());
    QVERIFY(button(*dialog, "saveButton")->isEnabled());
    QCOMPARE(table->item(1, 3)->text(), QStringLiteral("冷静期结束，可购买"));
    QVERIFY(query.exec(QStringLiteral("PRAGMA query_only=OFF")));
    button(*dialog, "saveButton")->click();
    settle();
    QCOMPARE(table->item(1, 3)->text(), QStringLiteral("已购买"));
    QVERIFY(query.exec(QStringLiteral("DROP TABLE wishlist")));
    QVERIFY(!widget.refresh());
    QCOMPARE(table->rowCount(), 2);
    QVERIFY(text(widget, "wishlistErrorLabel").contains(QStringLiteral("仍显示上次")));
    widget.refreshCountdowns();
    QVERIFY(!widget.findChild<QLabel *>(QStringLiteral("wishlistErrorLabel"))->isHidden());
    QVERIFY(database.initialize());
    QVERIFY(widget.refresh());
    QCOMPARE(table->rowCount(), 0);
    QVERIFY(widget.findChild<QLabel *>(QStringLiteral("wishlistErrorLabel"))->isHidden());
    MainWindow failed(directory.path());
    QVERIFY(!button(failed, "addWishButton")->isEnabled());
    QCOMPARE(text(failed, "wishlistSummaryLabel"), QStringLiteral("愿望数据：—"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KuraTests"));
    app.setApplicationName(QStringLiteral("WishlistTests"));
    QStandardPaths::setTestModeEnabled(true);
    const auto args = app.arguments();
    if (args.size() == 3 && args[1] == QStringLiteral("--wish-probe")) {
        DatabaseManager database(args[2]);
        if (!database.initialize()) return 1;
        WishlistRepository repository(database);
        QList<WishItem> items;
        TransactionRepository transactions(database);
        QList<Transaction> expenses;
        BudgetRepository budgets(database);
        MonthlyBudgetStatus budget;
        return repository.findAll(items) && items.size() == 1
            && items[0].readyAt == referenceTime().addSecs(86400)
            && transactions.findAll(expenses) && expenses.size() == 1 && expenses[0].amountCents() == 1250
            && budgets.loadStatus(QDate::currentDate(), budget) && budget.limitCents == 50000 ? 0 : 2;
    }
    WishlistTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "wishlist_tests.moc"
