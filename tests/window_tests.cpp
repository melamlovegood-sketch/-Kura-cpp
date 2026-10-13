#include "AddTransactionDialog.h"
#include "DatabaseManager.h"
#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
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

static AddTransactionDialog *openDialog(MainWindow &window)
{
    window.findChild<QPushButton *>(QStringLiteral("addTransactionButton"))->click();
    settle();
    return window.findChild<AddTransactionDialog *>();
}

static void fill(AddTransactionDialog &dialog, const QString &name, double amount,
                 const QString &category, const QDate &date)
{
    dialog.findChild<QLineEdit *>(QStringLiteral("descriptionEdit"))->setText(name);
    dialog.findChild<QDoubleSpinBox *>(QStringLiteral("amountSpinBox"))->setValue(amount);
    dialog.findChild<QComboBox *>(QStringLiteral("categoryComboBox"))->setCurrentText(category);
    dialog.findChild<QDateEdit *>(QStringLiteral("dateEdit"))->setDate(date);
}

class WindowTests : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void saveThenRestart();
    void invalidAndCancelledInputs();
    void lockedDatabasePreservesInputAndRetry();
    void initialDatabaseFailure();
    void refreshFailurePreservesTable();
};

void WindowTests::initTestCase()
{
    QApplication::setOrganizationName(QStringLiteral("KuraTests"));
    QApplication::setApplicationName(QStringLiteral("WindowTests"));
    QStandardPaths::setTestModeEnabled(true);
}

void WindowTests::saveThenRestart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("kura.sqlite"));
    qint64 savedId = 0;
    {
        MainWindow window(path);
        window.show();
        settle();
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
        QCOMPARE(table->rowCount(), 0);
        auto *dialog = openDialog(window);
        QVERIFY(dialog && dialog->isVisible());
        fill(*dialog, QStringLiteral("  午饭  "), 28, QStringLiteral("餐饮"), QDate(2026, 10, 13));
        const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
        if (!preview.isEmpty()) {
            QVERIFY(QDir().mkpath(preview));
            QVERIFY(dialog->grab().save(preview + QStringLiteral("/sqlite-dialog.png")));
        }
        dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
        settle();
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("午饭"));
        QCOMPARE(table->item(0, 3)->text(), QStringLiteral("¥28.00"));
        savedId = table->item(0, 0)->data(Qt::UserRole).toLongLong();
        QVERIFY(savedId > 0);
        QVERIFY(!window.findChild<AddTransactionDialog *>());
        dialog = openDialog(window);
        QVERIFY(dialog);
        QVERIFY(dialog->description().isEmpty());
        QCOMPARE(dialog->date(), QDate::currentDate());
        fill(*dialog, QStringLiteral("地铁"), 4, QStringLiteral("交通"), QDate(2026, 10, 12));
        QTest::keyClick(dialog->findChild<QLineEdit *>(QStringLiteral("descriptionEdit")), Qt::Key_Return);
        settle();
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(1, 1)->text(), QStringLiteral("地铁"));
        QCOMPARE(table->item(1, 3)->text(), QStringLiteral("¥4.00"));
    }
    {
        MainWindow restarted(path);
        restarted.show();
        settle();
        auto *table = restarted.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), savedId);
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("午饭"));
        QCOMPARE(table->item(1, 2)->text(), QStringLiteral("交通"));
        const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
        if (!preview.isEmpty())
            QVERIFY(restarted.grab().save(preview + QStringLiteral("/sqlite-reloaded.png")));
    }
}

void WindowTests::invalidAndCancelledInputs()
{
    MainWindow window(QStringLiteral(":memory:"));
    window.show();
    settle();
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    auto *dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("  "), 28, QStringLiteral("餐饮"), QDate::currentDate());
    auto *save = dialog->findChild<QPushButton *>(QStringLiteral("saveButton"));
    save->click();
    QVERIFY(dialog->isVisible());
    QCOMPARE(table->rowCount(), 0);
    fill(*dialog, QStringLiteral("午饭"), 0, QStringLiteral("餐饮"), QDate::currentDate());
    save->click();
    QVERIFY(dialog->isVisible());
    QCOMPARE(table->rowCount(), 0);
    fill(*dialog, QStringLiteral("午饭"), 28, QStringLiteral("餐饮"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("cancelButton"))->click();
    settle();
    QCOMPARE(table->rowCount(), 0);
    dialog = openDialog(window);
    QVERIFY(dialog);
    QTest::keyClick(dialog, Qt::Key_Escape);
    settle();
    QCOMPARE(table->rowCount(), 0);
    dialog = openDialog(window);
    QVERIFY(dialog);
    dialog->close();
    settle();
    QCOMPARE(table->rowCount(), 0);
    dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("一分钱"), 0.01, QStringLiteral("其他"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("¥0.01"));
}

void WindowTests::lockedDatabasePreservesInputAndRetry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("locked.sqlite"));
    MainWindow window(path);
    window.show();
    settle();
    DatabaseManager blocker(path);
    QVERIFY(blocker.initialize());
    QSqlQuery query(blocker.database());
    QVERIFY(query.exec(QStringLiteral("BEGIN IMMEDIATE")));
    auto *dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("奶茶"), 12.34, QStringLiteral("餐饮"), QDate(2026, 10, 13));
    QSignalSpy accepted(dialog, &QDialog::accepted);
    auto *save = dialog->findChild<QPushButton *>(QStringLiteral("saveButton"));
    save->click();
    settle();
    QCOMPARE(accepted.count(), 0);
    QVERIFY(dialog->isVisible());
    QVERIFY(save->isEnabled());
    QCOMPARE(dialog->description(), QStringLiteral("奶茶"));
    QCOMPARE(dialog->amount(), 12.34);
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("validationErrorLabel"))->text()
                .contains(QStringLiteral("占用")));
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    QCOMPARE(table->rowCount(), 0);
    const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty())
        QVERIFY(dialog->grab().save(preview + QStringLiteral("/sqlite-save-failure.png")));
    QVERIFY(query.exec(QStringLiteral("ROLLBACK")));
    save->click();
    QCOMPARE(accepted.count(), 1);
    settle();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("¥12.34"));
    QVERIFY(window.reloadTransactions());
    QCOMPARE(table->rowCount(), 1);
}

void WindowTests::initialDatabaseFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MainWindow window(directory.path()); // A directory cannot be opened as a database file.
    window.show();
    settle();
    QVERIFY(!window.findChild<QPushButton *>(QStringLiteral("addTransactionButton"))->isEnabled());
    auto *error = window.findChild<QLabel *>(QStringLiteral("databaseErrorLabel"));
    QVERIFY(error->isVisible());
    QVERIFY(!error->text().isEmpty());
    QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(), 0);
}

void WindowTests::refreshFailurePreservesTable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("refresh.sqlite"));
    MainWindow window(path);
    window.show();
    settle();
    auto *dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("午饭"), 28, QStringLiteral("餐饮"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    DatabaseManager external(path);
    QVERIFY(external.initialize());
    QSqlQuery query(external.database());
    QVERIFY(query.exec(QStringLiteral("DROP TABLE transactions")));
    QVERIFY(!window.reloadTransactions());
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("午饭"));
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("databaseErrorLabel"))->isVisible());
}

QTEST_MAIN(WindowTests)
#include "window_tests.moc"
