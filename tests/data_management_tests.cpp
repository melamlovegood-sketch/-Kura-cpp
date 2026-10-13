#include "CsvExporter.h"
#include "DataManagementWidget.h"
#include "DatabaseBackupService.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "BudgetRepository.h"
#include "WishlistRepository.h"
#include "AddTransactionDialog.h"
#include "mainwindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

static void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
static QByteArray contents(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
static bool write(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
static Transaction expense(const QString &name = QStringLiteral("午饭"), qint64 cents = 1234)
{
    return Transaction(0, name, cents, QStringLiteral("餐饮"), QDate::currentDate(), QTime(12, 3, 4), QStringLiteral("第一行\n第二行,\"中文\""));
}
static bool seed(DatabaseManager &database)
{
    TransactionRepository expenses(database);
    auto item = expense();
    if (!expenses.add(item)) return false;
    BudgetRepository budgets(database);
    if (!budgets.save(MonthlyBudget(QDate::currentDate(), 10000))) return false;
    WishlistRepository wishes(database);
    WishItem wish;
    wish.name = QStringLiteral("耳机"); wish.estimatedCents = 30000; wish.coolingDays = 1;
    if (!wishes.add(wish, QDateTime::currentDateTimeUtc().addDays(-2))) return false;
    const auto draft = expense(QStringLiteral("耳机"), 25000);
    Transaction purchase(0, draft.description(), draft.amountCents(), QStringLiteral("购物"), draft.date(), draft.time(), draft.note());
    return wishes.purchase(wish.id, purchase);
}

class DataManagementTests : public QObject
{
    Q_OBJECT
private slots:
    void csvPrecisionEscapingAndFailure();
    void completeBackupAndRestore();
    void rejectInvalidAndProtectedFiles();
    void restoreFailureKeepsOriginal();
    void legacyAndEmptyBackups();
    void filterExportAndRestoreUi();
};

void DataManagementTests::csvPrecisionEscapingAndFailure()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("中文.csv"));
    QList<Transaction> records = {
        Transaction(1, QStringLiteral("名称,\"中文\""), 1, QStringLiteral("餐饮"), QDate(2026, 1, 1), QTime(0, 0, 0), QStringLiteral("两行\r\n第二行,\"引号\"")),
        Transaction(2, QStringLiteral("=1+1"), Transaction::MaximumAmountCents, QStringLiteral("其他"), QDate(2026, 1, 2), {}, QStringLiteral("  @公式"))};
    QString error;
    QVERIFY2(CsvExporter::write(path, records, error), qPrintable(error));
    const QByteArray csv = contents(path);
    QVERIFY(csv.startsWith(QByteArray::fromHex("efbbbf")));
    const QString text = QString::fromUtf8(csv);
    QVERIFY(text.contains(QStringLiteral("\"名称,\"\"中文\"\"\"")));
    QVERIFY(text.contains(QStringLiteral("\"两行\r\n第二行,\"\"引号\"\"\"")));
    QVERIFY(text.contains(QStringLiteral(",0.01,")));
    QVERIFY(text.contains(QStringLiteral(",99999999.99,")));
    QVERIFY(text.contains(QStringLiteral("\"'=1+1\"")));
    QVERIFY(text.contains(QStringLiteral("\"'  @公式\"")));
    QVERIFY(text.contains(QStringLiteral("\"2026-01-02\",\"\",")));
    QVERIFY(!CsvExporter::write(path, {Transaction(1, QString(), 0, QStringLiteral("餐饮"), QDate())}, error));
    QCOMPARE(contents(path), csv);
    QVERIFY(CsvExporter::write(path, {}, error));
    QCOMPARE(contents(path).count('\n'), 1);
    QVERIFY(!CsvExporter::write(directory.path(), records, error));
}

void DataManagementTests::completeBackupAndRestore()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("current.sqlite"));
    const QString backupPath = directory.filePath(QStringLiteral("完整备份 ' 中文.sqlite"));
    DatabaseManager database(path);
    QVERIFY(database.initialize()); QVERIFY(seed(database));
    QSqlQuery wal(database.database());
    QVERIFY(wal.exec(QStringLiteral("PRAGMA journal_mode=WAL"))); wal.finish();
    TransactionRepository transactions(database);
    auto extra = expense(QStringLiteral("WAL 记录"), 10);
    QVERIFY(transactions.add(extra));
    auto deleted = expense(QStringLiteral("曾用编号"));
    QVERIFY(transactions.add(deleted)); QVERIFY(transactions.remove(deleted.id()));
    DatabaseBackupService service(database);
    QString error;
    QVERIFY2(service.backup(backupPath, error), qPrintable(error));
    QVERIFY(contents(backupPath).startsWith("SQLite format 3"));
    BackupInfo info;
    QVERIFY2(service.prepareRestore(backupPath, info, error), qPrintable(error));
    QCOMPARE(info.expenseCount, 3); QCOMPARE(info.budgetCount, 1); QCOMPARE(info.wishCount, 1);
    auto currentOnly = expense(QStringLiteral("恢复前记录"), 500);
    QVERIFY(transactions.add(currentOnly));
    QString safety;
    QVERIFY2(service.restorePrepared(safety, error), qPrintable(error));
    QVERIFY(QFileInfo::exists(safety));
    QList<Transaction> rows;
    QVERIFY(transactions.findAll(rows)); QCOMPARE(rows.size(), 3);
    QVERIFY(rows[0].time().isValid()); QVERIFY(rows[0].note().contains(QStringLiteral("第二行")));
    WishlistRepository wishes(database); QList<WishItem> wishRows;
    QVERIFY(wishes.findAll(wishRows)); QCOMPARE(wishRows.size(), 1);
    QCOMPARE(wishRows[0].status, WishStatus::Purchased);
    BudgetRepository budgets(database); MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(QDate::currentDate(), status)); QCOMPARE(status.limitCents, 10000);
    QCOMPARE(status.spentCents, 26244);
    DatabaseManager original(safety); QVERIFY(original.initialize());
    TransactionRepository originalRows(original);
    QVERIFY(originalRows.findAll(rows)); QCOMPARE(rows.size(), 4);
    // The selected source is isolated before confirmation; later changes to it
    // must not change the restore that was just reviewed.
    QVERIFY(service.prepareRestore(backupPath, info, error));
    { DatabaseManager source(backupPath); QVERIFY(source.initialize());
      TransactionRepository sourceRepo(source); auto added = expense(QStringLiteral("之后修改源文件")); QVERIFY(sourceRepo.add(added)); }
    QVERIFY(service.restorePrepared(safety, error));
    QVERIFY(transactions.findAll(rows)); QCOMPARE(rows.size(), 3);
    auto next = expense(QStringLiteral("恢复后新增")); QVERIFY(transactions.add(next));
    QVERIFY(next.id() > deleted.id());
}

void DataManagementTests::rejectInvalidAndProtectedFiles()
{
    QTemporaryDir directory;
    DatabaseManager database(directory.filePath(QStringLiteral("current.sqlite")));
    QVERIFY(database.initialize()); QVERIFY(seed(database));
    DatabaseBackupService service(database); QString error; BackupInfo info;
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")}) {
        QVERIFY(!service.backup(database.databasePath() + suffix, error));
        QVERIFY(!service.prepareRestore(database.databasePath() + suffix, info, error));
    }
    const QString invalid = directory.filePath(QStringLiteral("bad.sqlite"));
    QVERIFY(write(invalid, "not SQLite"));
    const QByteArray original = contents(invalid);
    QVERIFY(!service.prepareRestore(invalid, info, error)); QCOMPARE(contents(invalid), original);
    { DatabaseManager unrelated(invalid + QStringLiteral(".other")); QVERIFY(unrelated.initialize());
      QSqlQuery query(unrelated.database()); QVERIFY(query.exec(QStringLiteral("DROP TABLE wishlist"))); }
    QVERIFY(!service.prepareRestore(invalid + QStringLiteral(".other"), info, error));
    const QString unsupported = directory.filePath(QStringLiteral("future.sqlite"));
    QVERIFY(service.backup(unsupported, error));
    { DatabaseManager source(unsupported); QVERIFY(source.initialize()); QSqlQuery query(source.database());
      QVERIFY(query.exec(QStringLiteral("CREATE TABLE future_data(id INTEGER)"))); }
    QVERIFY(!service.prepareRestore(unsupported, info, error));
    const QString prefixTable = directory.filePath(QStringLiteral("unknown-table.sqlite"));
    QVERIFY(service.backup(prefixTable, error));
    { DatabaseManager source(prefixTable); QVERIFY(source.initialize()); QSqlQuery query(source.database());
      QVERIFY(query.exec(QStringLiteral("CREATE TABLE sqliteOtherData(id INTEGER)"))); }
    QVERIFY(!service.prepareRestore(prefixTable, info, error));
    const QString invalidRows = directory.filePath(QStringLiteral("invalid-rows.sqlite"));
    QVERIFY(service.backup(invalidRows, error));
    { DatabaseManager source(invalidRows); QVERIFY(source.initialize()); QSqlQuery query(source.database());
      QVERIFY(query.exec(QStringLiteral("UPDATE transactions SET category='无效分类'"))); }
    QVERIFY(!service.prepareRestore(invalidRows, info, error));
    const QString invalidTypes = directory.filePath(QStringLiteral("invalid-types.sqlite"));
    QVERIFY(service.backup(invalidTypes, error));
    { DatabaseManager source(invalidTypes); QVERIFY(source.initialize()); QSqlQuery query(source.database());
      QVERIFY(query.exec(QStringLiteral("PRAGMA ignore_check_constraints=ON")));
      QVERIFY(query.exec(QStringLiteral("UPDATE transactions SET amount_cents=1.5"))); }
    QVERIFY(!service.prepareRestore(invalidTypes, info, error));
    QString safety; QVERIFY(!service.restorePrepared(safety, error));
    QList<Transaction> rows; TransactionRepository repository(database);
    QVERIFY(repository.findAll(rows)); QCOMPARE(rows.size(), 2);
}

void DataManagementTests::restoreFailureKeepsOriginal()
{
    QTemporaryDir directory;
    DatabaseManager database(directory.filePath(QStringLiteral("current.sqlite")));
    QVERIFY(database.initialize()); QVERIFY(seed(database));
    DatabaseBackupService service(database); QString error; BackupInfo info; QString safety;
    const QString backup = directory.filePath(QStringLiteral("backup.sqlite"));
    QVERIFY(service.backup(backup, error));
    TransactionRepository transactions(database); auto extra = expense(QStringLiteral("原数据")); QVERIFY(transactions.add(extra));
    QVERIFY(service.prepareRestore(backup, info, error));
    const QString backupDirectory = directory.filePath(QStringLiteral("backups"));
    QVERIFY(write(backupDirectory, "block automatic backup directory"));
    QVERIFY(!service.restorePrepared(safety, error));
    QVERIFY(safety.isEmpty()); QVERIFY(QFile::remove(backupDirectory));
    QSqlQuery query(database.database());
    QVERIFY(query.exec(QStringLiteral("CREATE TRIGGER fail_restore BEFORE INSERT ON monthly_budgets BEGIN SELECT RAISE(ABORT,'test restore failure'); END")));
    QVERIFY(!service.restorePrepared(safety, error)); QVERIFY(QFileInfo::exists(safety));
    QList<Transaction> rows; QVERIFY(transactions.findAll(rows)); QCOMPARE(rows.size(), 3);
    QVERIFY(query.exec(QStringLiteral("DROP TRIGGER fail_restore")));
    DatabaseManager blocker(database.databasePath()); QVERIFY(blocker.initialize());
    QSqlQuery lock(blocker.database()); QVERIFY(lock.exec(QStringLiteral("BEGIN IMMEDIATE")));
    QVERIFY(!service.restorePrepared(safety, error)); QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
    QVERIFY(service.restorePrepared(safety, error));
    QVERIFY(transactions.findAll(rows)); QCOMPARE(rows.size(), 2);
}

void DataManagementTests::legacyAndEmptyBackups()
{
    QTemporaryDir directory;
    DatabaseManager database(directory.filePath(QStringLiteral("current.sqlite")));
    QVERIFY(database.initialize()); QVERIFY(seed(database));
    DatabaseBackupService service(database); QString error; BackupInfo info; QString safety;
    const QString legacy = directory.filePath(QStringLiteral("legacy.sqlite"));
    QVERIFY(service.backup(legacy, error));
    {
        DatabaseManager source(legacy); QVERIFY(source.initialize()); QSqlQuery query(source.database());
        QVERIFY(query.exec(QStringLiteral("ALTER TABLE transactions DROP COLUMN occurred_time")));
        QVERIFY(query.exec(QStringLiteral("ALTER TABLE transactions DROP COLUMN note")));
    }
    const QByteArray oldBytes = contents(legacy);
    QVERIFY2(service.prepareRestore(legacy, info, error), qPrintable(error));
    QCOMPARE(contents(legacy), oldBytes);
    QVERIFY(service.restorePrepared(safety, error));
    TransactionRepository transactions(database); QList<Transaction> rows;
    QVERIFY(transactions.findAll(rows)); QCOMPARE(rows.size(), 2);
    QVERIFY(!rows[0].time().isValid()); QVERIFY(rows[0].note().isEmpty());
    const QString empty = directory.filePath(QStringLiteral("empty.sqlite"));
    { DatabaseManager source(empty); QVERIFY(source.initialize()); }
    QVERIFY(service.prepareRestore(empty, info, error));
    QCOMPARE(info.expenseCount, 0); QCOMPARE(info.budgetCount, 0); QCOMPARE(info.wishCount, 0);
    QVERIFY(service.restorePrepared(safety, error));
    QVERIFY(transactions.findAll(rows)); QCOMPARE(rows.size(), 0);
    auto next = expense(); QVERIFY(transactions.add(next)); QCOMPARE(next.id(), 1);
}

void DataManagementTests::filterExportAndRestoreUi()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("ui.sqlite"));
    { DatabaseManager database(path); QVERIFY(database.initialize()); QVERIFY(seed(database)); }
    MainWindow window(path); window.show(); settle();
    auto *widget = window.findChild<DataManagementWidget *>(); QVERIFY(widget);
    auto *category = window.findChild<QComboBox *>(QStringLiteral("categoryFilter"));
    category->setCurrentText(QStringLiteral("购物"));
    const QString csv = directory.filePath(QStringLiteral("visible.csv"));
    QVERIFY(widget->exportTo(csv, false)); // Changed controls alone are not an applied filter.
    QVERIFY(contents(csv).contains(QStringLiteral("午饭").toUtf8()));
    window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
    QVERIFY(widget->exportTo(csv, false)); QVERIFY(!contents(csv).contains(QStringLiteral("午饭").toUtf8()));
    QVERIFY(contents(csv).contains(QStringLiteral("耳机").toUtf8()));
    QVERIFY(widget->exportTo(csv, true)); QVERIFY(contents(csv).contains(QStringLiteral("午饭").toUtf8()));
    category->setCurrentText(QStringLiteral("交通"));
    window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
    QVERIFY(widget->exportTo(csv, false)); QCOMPARE(contents(csv).count('\n'), 1);
    category->setCurrentText(QStringLiteral("购物"));
    window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
    const QByteArray before = contents(path);
    QVERIFY(!widget->exportTo(path, true)); QCOMPARE(contents(path), before);
    const QString backup = directory.filePath(QStringLiteral("ui-backup.sqlite"));
    QVERIFY(widget->backupTo(backup));
    QVERIFY(widget->requestRestore(backup)); settle();
    auto *confirmation = window.findChild<QMessageBox *>(QStringLiteral("restoreConfirmationDialog"));
    QVERIFY(confirmation); QVERIFY(confirmation->text().contains(QStringLiteral("消费 2 笔，预算 1 个月，愿望 1 条")));
    QCOMPARE(static_cast<QAbstractButton *>(confirmation->defaultButton()), confirmation->button(QMessageBox::No));
    confirmation->button(QMessageBox::No)->click(); settle();
    QCOMPARE(category->currentText(), QStringLiteral("购物"));
    window.findChild<QPushButton *>(QStringLiteral("addTransactionButton"))->click(); settle();
    QVERIFY(!widget->requestRestore(backup));
    auto *editor = window.findChild<AddTransactionDialog *>(); QVERIFY(editor); editor->reject(); settle();
    auto *pages = window.findChild<QTabWidget *>(QStringLiteral("mainPages")); pages->setCurrentIndex(3);
    const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(window.grab().save(preview + QStringLiteral("/data-management.png"))); }
    QVERIFY(widget->requestRestore(backup)); settle();
    confirmation = window.findChild<QMessageBox *>(QStringLiteral("restoreConfirmationDialog")); QVERIFY(confirmation);
    if (!preview.isEmpty()) QVERIFY(confirmation->grab().save(preview + QStringLiteral("/restore-confirmation.png")));
    confirmation->button(QMessageBox::Yes)->click(); settle();
    QCOMPARE(category->currentIndex(), 0);
    QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(), 2);
    QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("wishlistTable"))->rowCount(), 1);
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("dataManagementResultLabel"))->text().contains(QStringLiteral("数据已恢复")));
    MainWindow failed(directory.path());
    QVERIFY(!failed.findChild<QPushButton *>(QStringLiteral("exportCsvButton"))->isEnabled());
}

QTEST_MAIN(DataManagementTests)
#include "data_management_tests.moc"
