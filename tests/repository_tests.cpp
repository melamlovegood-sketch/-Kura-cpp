#include "DatabaseManager.h"
#include "TransactionRepository.h"

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
    for (const auto &mode : {QStringLiteral("--probe-write"), QStringLiteral("--probe-read")}) {
        QProcess process;
        process.start(QCoreApplication::applicationFilePath(), {mode, path});
        QVERIFY(process.waitForStarted(5000));
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    }
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
        return repository.findAll(rows) && rows.size() == 2
            && rows[0].description() == QStringLiteral("午饭") && rows[0].amountCents() == 2800
            && rows[1].description() == QStringLiteral("地铁") && rows[1].amountCents() == 400
            && rows[0].id() > 0 && rows[1].id() > 0 && rows[0].id() != rows[1].id() ? 0 : 3;
    }
    RepositoryTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "repository_tests.moc"
