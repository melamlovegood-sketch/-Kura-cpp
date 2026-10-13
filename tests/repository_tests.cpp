#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "ExpenseSummary.h"
#include "BudgetRepository.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class RepositoryTests : public QObject
{
    Q_OBJECT
private slots:
    void saveAndReopen();
    void validation_data();
    void validation();
    void monetaryPrecisionAndQuotedNames();
    void databaseFailuresAreExplicit();
    void readOnlyDatabaseCannotSave();
    void failedReadKeepsExistingList();
    void independentConnections();
    void survivesNewProcess();
    void updateAndDeletePersist();
    void missingAndInvalidMutations();
    void lockedAndReadOnlyMutationsKeepData();
    void notesAndTimesPersist();
    void timeOrderingAndDailyBoundaries();
    void legacyDetailsMigration();
    void detailsValidationAndMigrationFailure();
};

void RepositoryTests::saveAndReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("数据/消费.sqlite"));
    qint64 savedId = 0;
    const QStringList before = QSqlDatabase::connectionNames();
    {
        DatabaseManager database(path);
        QVERIFY2(database.initialize(), qPrintable(database.lastError()));
        QVERIFY(database.initialize());
        TransactionRepository repository(database);
        Transaction transaction(0, QStringLiteral("  午饭  "), 2800,
                                QStringLiteral("餐饮"), QDate(2026, 10, 13));
        QVERIFY2(repository.add(transaction), qPrintable(repository.lastError()));
        savedId = transaction.id();
        QVERIFY(savedId > 0);
        QVERIFY(!repository.add(transaction));
    }
    QCOMPARE(QSqlDatabase::connectionNames(), before);
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository repository(database);
        QList<Transaction> transactions;
        QVERIFY(repository.findAll(transactions));
        QCOMPARE(transactions.size(), 1);
        QCOMPARE(transactions.first().id(), savedId);
        QCOMPARE(transactions.first().description(), QStringLiteral("午饭"));
        QCOMPARE(transactions.first().amountCents(), 2800);
        QCOMPARE(transactions.first().date(), QDate(2026, 10, 13));
    }
    QCOMPARE(QSqlDatabase::connectionNames(), before);
}

void RepositoryTests::validation_data()
{
    QTest::addColumn<QString>("description");
    QTest::addColumn<qint64>("cents");
    QTest::addColumn<QString>("category");
    QTest::addColumn<QDate>("date");
    const QDate date(2026, 10, 13);
    QTest::newRow("blank name") << QStringLiteral("  ") << qint64(100) << QStringLiteral("餐饮") << date;
    QTest::newRow("long name") << QString(101, QLatin1Char('a')) << qint64(100) << QStringLiteral("餐饮") << date;
    QTest::newRow("zero") << QStringLiteral("午饭") << qint64(0) << QStringLiteral("餐饮") << date;
    QTest::newRow("negative") << QStringLiteral("午饭") << qint64(-1) << QStringLiteral("餐饮") << date;
    QTest::newRow("above maximum") << QStringLiteral("午饭") << (Transaction::MaximumAmountCents + 1)
                                  << QStringLiteral("餐饮") << date;
    QTest::newRow("invalid category") << QStringLiteral("午饭") << qint64(100) << QStringLiteral("未知") << date;
    QTest::newRow("invalid date") << QStringLiteral("午饭") << qint64(100) << QStringLiteral("餐饮") << QDate();
}

void RepositoryTests::validation()
{
    QFETCH(QString, description);
    QFETCH(qint64, cents);
    QFETCH(QString, category);
    QFETCH(QDate, date);
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    Transaction invalid(0, description, cents, category, date);
    QVERIFY(!repository.add(invalid));
    QVERIFY(!repository.lastError().isEmpty());
    QCOMPARE(invalid.id(), 0);
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QVERIFY(rows.isEmpty());
}

void RepositoryTests::monetaryPrecisionAndQuotedNames()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    const QList<qint64> amounts = {1, 10, 20, 29, 1234, Transaction::MaximumAmountCents};
    const QString description = QStringLiteral("奶茶 '); DROP TABLE transactions; --");
    for (qint64 cents : amounts) {
        Transaction transaction(0, description, cents, QStringLiteral("餐饮"), QDate(2026, 10, 13));
        QVERIFY(repository.add(transaction));
    }
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), amounts.size());
    for (qsizetype i = 0; i < rows.size(); ++i) {
        QCOMPARE(rows[i].amountCents(), amounts[amounts.size() - 1 - i]);
        QCOMPARE(rows[i].description(), description);
        if (i > 0)
            QVERIFY(rows[i - 1].id() > rows[i].id());
    }
}

void RepositoryTests::databaseFailuresAreExplicit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blocker = directory.filePath(QStringLiteral("not-a-directory"));
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("blocker");
    file.close();
    DatabaseManager blocked(blocker + QStringLiteral("/kura.sqlite"));
    QVERIFY(!blocked.initialize());
    QVERIFY(!blocked.lastError().isEmpty());
    const QString corruptPath = directory.filePath(QStringLiteral("corrupt.sqlite"));
    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    corrupt.write("not a sqlite database");
    corrupt.close();
    DatabaseManager bad(corruptPath);
    QVERIFY(!bad.initialize());
    QVERIFY(!bad.lastError().isEmpty());

    DatabaseManager good(QStringLiteral(":memory:"));
    QVERIFY(good.initialize());
    TransactionRepository repository(good);
    good.database().close();
    Transaction transaction(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate::currentDate());
    QVERIFY(!repository.add(transaction));
    QVERIFY(!repository.lastError().isEmpty());
    QCOMPARE(transaction.id(), 0);
}

void RepositoryTests::failedReadKeepsExistingList()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    QList<Transaction> rows{Transaction(7, QStringLiteral("已有记录"), 123,
                                        QStringLiteral("其他"), QDate::currentDate())};
    QSqlQuery query(database.database());
    QVERIFY(query.exec(QStringLiteral("DROP TABLE transactions")));
    QVERIFY(!repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().id(), 7);
    QVERIFY(!repository.lastError().isEmpty());
}

void RepositoryTests::readOnlyDatabaseCannotSave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    DatabaseManager database(directory.filePath(QStringLiteral("readonly.sqlite")));
    QVERIFY(database.initialize());
    auto connection = database.database();
    connection.close();
    connection.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    QVERIFY(connection.open());
    TransactionRepository repository(database);
    Transaction transaction(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate::currentDate());
    QVERIFY(!repository.add(transaction));
    QVERIFY(repository.lastError().contains(QStringLiteral("只读")));
    QCOMPARE(transaction.id(), 0);
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QVERIFY(rows.isEmpty());
}

void RepositoryTests::independentConnections()
{
    DatabaseManager first(QStringLiteral(":memory:"));
    DatabaseManager second(QStringLiteral(":memory:"));
    QVERIFY(first.initialize());
    QVERIFY(second.initialize());
    QVERIFY(first.database().connectionName() != second.database().connectionName());
    TransactionRepository a(first), b(second);
    Transaction transaction(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate::currentDate());
    QVERIFY(a.add(transaction));
    QList<Transaction> rows;
    QVERIFY(b.findAll(rows));
    QVERIFY(rows.isEmpty());
}

void RepositoryTests::survivesNewProcess()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("process.sqlite"));
    for (const auto &mode : {QStringLiteral("--probe-write"), QStringLiteral("--probe-read"),
                            QStringLiteral("--probe-mutate"), QStringLiteral("--probe-read-mutated")}) {
        QProcess process;
        process.start(QCoreApplication::applicationFilePath(), {mode, path});
        QVERIFY(process.waitForStarted(5000));
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    }
}

void RepositoryTests::updateAndDeletePersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("crud.sqlite"));
    qint64 keptId = 0, deletedId = 0;
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository repository(database);
        Transaction a(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate(2026, 10, 13));
        Transaction b(0, QStringLiteral("地铁"), 400, QStringLiteral("交通"), QDate(2026, 10, 12));
        QVERIFY(repository.add(a));
        QVERIFY(repository.add(b));
        keptId = a.id();
        deletedId = b.id();
        Transaction edited(a.id(), QStringLiteral("  教材 ' SQL  "), 1234,
                           QStringLiteral("学习"), QDate(2026, 10, 1));
        QVERIFY(repository.update(edited));
        QVERIFY(repository.update(edited)); // Unchanged values are still a successful edit.
        QVERIFY(repository.remove(b.id()));
    }
    DatabaseManager database(path);
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().id(), keptId);
    QCOMPARE(rows.first().description(), QStringLiteral("教材 ' SQL"));
    QCOMPARE(rows.first().amountCents(), 1234);
    QCOMPARE(rows.first().category(), QStringLiteral("学习"));
    QCOMPARE(rows.first().date(), QDate(2026, 10, 1));
    Transaction next(0, QStringLiteral("下一条"), 100, QStringLiteral("其他"), QDate::currentDate());
    QVERIFY(repository.add(next));
    QVERIFY(next.id() > deletedId);
}

void RepositoryTests::missingAndInvalidMutations()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    Transaction saved(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate::currentDate());
    QVERIFY(repository.add(saved));
    QVERIFY(!repository.update(Transaction(saved.id(), QString(), 2800,
                                           QStringLiteral("餐饮"), QDate::currentDate())));
    QVERIFY(!repository.update(Transaction(saved.id(), QStringLiteral("午饭"), 0,
                                           QStringLiteral("餐饮"), QDate::currentDate())));
    QVERIFY(!repository.update(Transaction(0, QStringLiteral("午饭"), 2800,
                                           QStringLiteral("餐饮"), QDate::currentDate())));
    QVERIFY(!repository.update(Transaction(999, QStringLiteral("午饭"), 2800,
                                           QStringLiteral("餐饮"), QDate::currentDate())));
    QVERIFY(!repository.remove(0));
    QVERIFY(!repository.remove(-1));
    QVERIFY(!repository.remove(999));
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().amountCents(), 2800);
    QVERIFY(repository.remove(saved.id()));
    QVERIFY(!repository.remove(saved.id()));
    QVERIFY(!repository.lastError().isEmpty());
}

void RepositoryTests::lockedAndReadOnlyMutationsKeepData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("locked-mutations.sqlite"));
    DatabaseManager database(path), other(path);
    QVERIFY(database.initialize());
    QVERIFY(other.initialize());
    TransactionRepository repository(database);
    Transaction saved(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate::currentDate());
    QVERIFY(repository.add(saved));
    const Transaction edited(saved.id(), QStringLiteral("教材"), 1234, QStringLiteral("学习"), QDate::currentDate());
    auto connection = database.database();
    connection.close();
    connection.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=20"));
    QVERIFY(connection.open());
    QSqlQuery lock(other.database());
    QVERIFY(lock.exec(QStringLiteral("BEGIN IMMEDIATE")));
    QVERIFY(!repository.update(edited));
    QVERIFY(repository.lastError().contains(QStringLiteral("占用")));
    QVERIFY(!repository.remove(saved.id()));
    QVERIFY(repository.lastError().contains(QStringLiteral("占用")));
    QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
    connection.close();
    connection.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    QVERIFY(connection.open());
    QVERIFY(!repository.update(edited));
    QVERIFY(repository.lastError().contains(QStringLiteral("只读")));
    QVERIFY(!repository.remove(saved.id()));
    QVERIFY(repository.lastError().contains(QStringLiteral("只读")));
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().description(), QStringLiteral("午饭"));
    QCOMPARE(rows.first().amountCents(), 2800);
}

void RepositoryTests::notesAndTimesPersist()
{
    QTemporaryDir directory;
    const QString path = directory.filePath(QStringLiteral("details.sqlite"));
    const QString note = QStringLiteral("食堂午饭\n与同学 AA，'引号'；<b>纯文本</b>，🍜");
    qint64 id = 0;
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository repository(database);
        Transaction item(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"),
                         QDate(2026, 10, 13), QTime(12, 34, 56), note);
        QVERIFY(repository.add(item));
        id = item.id();
        QCOMPARE(item.time(), QTime(12, 34, 56));
        QCOMPARE(item.note(), note);
    }
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository repository(database);
        QList<Transaction> rows;
        QVERIFY(repository.findAll(rows));
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].id(), id);
        QCOMPARE(rows[0].time(), QTime(12, 34, 56));
        QCOMPARE(rows[0].note(), note);
        Transaction edited(id, QStringLiteral("晚饭"), 3250, QStringLiteral("餐饮"),
                           QDate(2026, 10, 14), QTime(23, 59, 59), QString());
        QVERIFY(repository.update(edited));
    }
    DatabaseManager database(path);
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows[0].id(), id);
    QCOMPARE(rows[0].date(), QDate(2026, 10, 14));
    QCOMPARE(rows[0].time(), QTime(23, 59, 59));
    QVERIFY(rows[0].note().isEmpty());
}

void RepositoryTests::timeOrderingAndDailyBoundaries()
{
    DatabaseManager database(QStringLiteral(":memory:"));
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    const QDate day(2026, 10, 31);
    QList<Transaction> inputs{
        Transaction(0, QStringLiteral("晚饭"), 100, QStringLiteral("餐饮"), day, QTime(23, 59, 59)),
        Transaction(0, QStringLiteral("午夜"), 200, QStringLiteral("餐饮"), day, QTime(0, 0, 0)),
        Transaction(0, QStringLiteral("午饭"), 300, QStringLiteral("餐饮"), day, QTime(12, 0, 1)),
        Transaction(0, QStringLiteral("旧记录"), 400, QStringLiteral("其他"), day),
        Transaction(0, QStringLiteral("下月"), 500, QStringLiteral("其他"), day.addDays(1), QTime(0, 0, 0))};
    for (auto &item : inputs)
        QVERIFY(repository.add(item));
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows[0].description(), QStringLiteral("下月"));
    QCOMPARE(rows[1].description(), QStringLiteral("晚饭"));
    QCOMPARE(rows[2].description(), QStringLiteral("午饭"));
    QCOMPARE(rows[3].description(), QStringLiteral("午夜"));
    QCOMPARE(rows[4].description(), QStringLiteral("旧记录"));
    TransactionFilter filter;
    filter.useDateRange = true;
    filter.startDate = filter.endDate = day;
    const auto summary = ExpenseSummary::calculate(rows, filter, day);
    QCOMPARE(summary.filteredCount, 4);
    QCOMPARE(summary.filteredCents, 1000);
    QCOMPARE(summary.todayCents, 1000);
    QCOMPARE(summary.monthCents, 1000);
    BudgetRepository budgets(database);
    MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(day, status));
    QCOMPARE(status.spentCents, 1000);
}

void RepositoryTests::legacyDetailsMigration()
{
    QTemporaryDir directory;
    for (int fields = 0; fields < 4; ++fields) {
        const QString path = directory.filePath(QStringLiteral("legacy-%1.sqlite").arg(fields));
        {
            DatabaseManager database(path);
            QVERIFY(database.initialize());
            QSqlQuery query(database.database());
            QVERIFY(query.exec(QStringLiteral("DROP TABLE transactions")));
            QString schema = QStringLiteral("CREATE TABLE transactions (id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "description TEXT NOT NULL, amount_cents INTEGER NOT NULL, category TEXT NOT NULL, date TEXT NOT NULL");
            if (fields & 1) schema += QStringLiteral(", occurred_time TEXT NOT NULL DEFAULT ''");
            if (fields & 2) schema += QStringLiteral(", note TEXT NOT NULL DEFAULT ''");
            QVERIFY(query.exec(schema + QLatin1Char(')')));
            QVERIFY(query.exec(QStringLiteral("INSERT INTO transactions(id,description,amount_cents,category,date) "
                "VALUES (42,'原有消费',1234,'餐饮','2026-10-13')")));
            if (fields & 1) QVERIFY(query.exec(QStringLiteral("UPDATE transactions SET occurred_time='08:09:10'")));
            if (fields & 2) QVERIFY(query.exec(QStringLiteral("UPDATE transactions SET note='已有备注'")));
            BudgetRepository budgets(database);
            QVERIFY(budgets.save(MonthlyBudget(QDate(2026, 10, 1), 2000)));
        }
        DatabaseManager upgraded(path);
        QVERIFY2(upgraded.initialize(), qPrintable(upgraded.lastError()));
        QVERIFY(upgraded.initialize()); // Repeated startup must not overwrite migrated fields.
        TransactionRepository repository(upgraded);
        QList<Transaction> rows;
        QVERIFY(repository.findAll(rows));
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].id(), 42);
        QCOMPARE(rows[0].date(), QDate(2026, 10, 13));
        QCOMPARE(rows[0].amountCents(), 1234);
        QCOMPARE(rows[0].time(), (fields & 1) ? QTime(8, 9, 10) : QTime());
        QCOMPARE(rows[0].note(), (fields & 2) ? QStringLiteral("已有备注") : QStringLiteral(""));
        BudgetRepository budgets(upgraded);
        MonthlyBudgetStatus status;
        QVERIFY(budgets.loadStatus(QDate(2026, 10, 1), status));
        QCOMPARE(status.remainingCents(), 766);
        Transaction next(0, QStringLiteral("新记录"), 100, QStringLiteral("其他"),
                         QDate(2026, 10, 13), QTime(10, 11, 12), QStringLiteral("新备注"));
        QVERIFY(repository.add(next));
        QVERIFY(next.id() > 42);
    }
}

void RepositoryTests::detailsValidationAndMigrationFailure()
{
    QTemporaryDir directory;
    DatabaseManager database(directory.filePath(QStringLiteral("failure.sqlite")));
    QVERIFY(database.initialize());
    TransactionRepository repository(database);
    Transaction maximum(0, QStringLiteral("备注边界"), 100, QStringLiteral("其他"),
        QDate(2026, 10, 13), QTime(0, 0), QString(1000, QChar(0x4e2d)));
    QVERIFY(repository.add(maximum));
    Transaction tooLong(0, QStringLiteral("过长备注"), 100, QStringLiteral("其他"),
        maximum.date(), QTime(0, 0), QString(1001, QLatin1Char('x')));
    QVERIFY(!repository.add(tooLong));
    QCOMPARE(tooLong.id(), 0);
    QVERIFY(!repository.update(Transaction(maximum.id(), maximum.description(), maximum.amountCents(),
        maximum.category(), maximum.date(), maximum.time(), tooLong.note())));
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].note(), maximum.note());
    QSqlQuery query(database.database());
    QVERIFY(query.exec(QStringLiteral("UPDATE transactions SET occurred_time='99:99:99'")));
    QVERIFY(!repository.findAll(rows));
    QCOMPARE(rows[0].time(), QTime(0, 0)); // Keep the last good snapshot on corrupt stored data.
    QVERIFY(query.exec(QStringLiteral("DROP TABLE transactions")));
    QVERIFY(query.exec(QStringLiteral("CREATE TABLE transactions(id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "description TEXT NOT NULL,amount_cents INTEGER NOT NULL,category TEXT NOT NULL,date TEXT NOT NULL)")));
    QVERIFY(query.exec(QStringLiteral("INSERT INTO transactions VALUES(42,'旧记录',100,'其他','2026-10-13')")));
    query.finish();
    auto connection = database.database();
    connection.close();
    connection.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    QVERIFY(connection.open());
    QVERIFY(!database.initialize());
    QVERIFY(!database.lastError().isEmpty());
    connection.close();
    connection.setConnectOptions(QString());
    QVERIFY(connection.open());
    QVERIFY(database.initialize());
    QVERIFY(repository.findAll(rows));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].id(), 42);
    QVERIFY(!rows[0].time().isValid());
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("KuraTests"));
    app.setApplicationName(QStringLiteral("RepositoryTests"));
    QStandardPaths::setTestModeEnabled(true);
    const auto arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1].startsWith(QStringLiteral("--probe-"))) {
        DatabaseManager database(arguments[2]);
        if (!database.initialize())
            return 1;
        TransactionRepository repository(database);
        if (arguments[1] == QStringLiteral("--probe-write")) {
            Transaction a(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate(2026, 10, 13));
            Transaction b(0, QStringLiteral("地铁"), 400, QStringLiteral("交通"), QDate(2026, 10, 12));
            return repository.add(a) && repository.add(b) ? 0 : 2;
        }
        QList<Transaction> rows;
        if (arguments[1] == QStringLiteral("--probe-mutate")) {
            if (!repository.findAll(rows) || rows.size() != 2)
                return 4;
            Transaction edited(rows[0].id(), QStringLiteral("咖啡"), 3250,
                               QStringLiteral("餐饮"), QDate(2026, 10, 14));
            return repository.update(edited) && repository.remove(rows[1].id()) ? 0 : 5;
        }
        if (arguments[1] == QStringLiteral("--probe-read-mutated")) {
            return repository.findAll(rows) && rows.size() == 1
                && rows[0].id() > 0 && rows[0].description() == QStringLiteral("咖啡")
                && rows[0].amountCents() == 3250 && rows[0].date() == QDate(2026, 10, 14) ? 0 : 6;
        }
        return repository.findAll(rows) && rows.size() == 2
            && rows[0].description() == QStringLiteral("午饭") && rows[0].amountCents() == 2800
            && rows[1].description() == QStringLiteral("地铁") && rows[1].amountCents() == 400
            && rows[0].id() > 0 && rows[1].id() > 0 && rows[0].id() != rows[1].id() ? 0 : 3;
    }
    RepositoryTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "repository_tests.moc"
