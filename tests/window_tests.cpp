#include "AddTransactionDialog.h"
#include "DatabaseManager.h"
#include "mainwindow.h"

#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QStatusBar>
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
    void editAndDeleteThenRestart();
    void filtersAndTotalsUpdate();
    void lockedEditsAndDeletes();
    void missingEditedRecordNotRecreated();
    void editedRecordMovesOutOfFilter();
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
    const QString previousSummary = window.findChild<QLabel *>(QStringLiteral("filteredExpenseLabel"))->text();
    const QString previousRange = window.findChild<QLabel *>(QStringLiteral("appliedFilterLabel"))->text();
    window.findChild<QComboBox *>(QStringLiteral("categoryFilter"))->setCurrentText(QStringLiteral("交通"));
    window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("午饭"));
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("databaseErrorLabel"))->isVisible());
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("filteredExpenseLabel"))->text(), previousSummary);
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("appliedFilterLabel"))->text(), previousRange);
}

void WindowTests::editAndDeleteThenRestart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("edit-delete.sqlite"));
    qint64 keptId = 0;
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository repository(database);
        Transaction a(0, QStringLiteral("午饭"), 2800, QStringLiteral("餐饮"), QDate::currentDate());
        Transaction b(0, QStringLiteral("地铁"), 400, QStringLiteral("交通"), QDate::currentDate().addDays(-1));
        QVERIFY(repository.add(a));
        QVERIFY(repository.add(b));
        keptId = a.id();
    }
    {
        MainWindow window(path);
        window.show();
        settle();
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
        auto *edit = window.findChild<QPushButton *>(QStringLiteral("editTransactionButton"));
        auto *remove = window.findChild<QPushButton *>(QStringLiteral("deleteTransactionButton"));
        QVERIFY(!edit->isEnabled());
        QVERIFY(!remove->isEnabled());
        table->selectRow(0);
        QVERIFY(edit->isEnabled());
        edit->click();
        settle();
        auto *dialog = window.findChild<AddTransactionDialog *>();
        QVERIFY(dialog);
        QCOMPARE(dialog->windowTitle(), QStringLiteral("修改消费"));
        QCOMPARE(dialog->description(), QStringLiteral("午饭"));
        QCOMPARE(dialog->amount(), 28.0);
        QCOMPARE(dialog->transaction().id(), keptId);
        fill(*dialog, QStringLiteral("取消修改"), 99, QStringLiteral("其他"), QDate::currentDate());
        dialog->findChild<QPushButton *>(QStringLiteral("cancelButton"))->click();
        settle();
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("午饭"));
        edit->click();
        settle();
        dialog = window.findChild<AddTransactionDialog *>();
        QVERIFY(dialog);
        fill(*dialog, QStringLiteral("教材"), 32.50, QStringLiteral("学习"), QDate::currentDate().addDays(-1));
        const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
        if (!preview.isEmpty())
            QVERIFY(dialog->grab().save(preview + QStringLiteral("/bookkeeping-edit.png")));
        dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
        settle();
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->currentRow(), 1); // Date change reordered rows, selection follows the id.
        QCOMPARE(table->item(1, 0)->data(Qt::UserRole).toLongLong(), keptId);
        QCOMPARE(table->item(1, 1)->text(), QStringLiteral("教材"));
        QCOMPARE(table->item(1, 2)->text(), QStringLiteral("学习"));
        QCOMPARE(table->item(1, 3)->text(), QStringLiteral("¥32.50"));
        QCOMPARE(window.findChild<QLabel *>(QStringLiteral("totalExpenseLabel"))->text(),
                 QStringLiteral("全部支出：¥36.50"));
        table->selectRow(0); // Delete the other record, whose id is not its row number.
        remove->click();
        settle();
        auto *confirmation = window.findChild<QMessageBox *>();
        QVERIFY(confirmation && confirmation->isVisible());
        QVERIFY(confirmation->text().contains(QStringLiteral("地铁")));
        QCOMPARE(confirmation->textFormat(), Qt::PlainText);
        QCOMPARE(static_cast<QAbstractButton *>(confirmation->defaultButton()), confirmation->button(QMessageBox::No));
        confirmation->button(QMessageBox::No)->click();
        settle();
        QCOMPARE(table->rowCount(), 2);
        remove->click();
        settle();
        confirmation = window.findChild<QMessageBox *>();
        QVERIFY(confirmation);
        QTest::keyClick(confirmation, Qt::Key_Escape);
        settle();
        QCOMPARE(table->rowCount(), 2);
        remove->click();
        settle();
        confirmation = window.findChild<QMessageBox *>();
        QVERIFY(confirmation);
        confirmation->button(QMessageBox::Yes)->click();
        settle();
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), keptId);
        QCOMPARE(window.findChild<QLabel *>(QStringLiteral("totalExpenseLabel"))->text(),
                 QStringLiteral("全部支出：¥32.50"));
    }
    {
        MainWindow restarted(path);
        restarted.show();
        settle();
        auto *table = restarted.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
        QCOMPARE(table->rowCount(), 1);
        QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), keptId);
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("教材"));
        table->selectRow(0);
        restarted.findChild<QPushButton *>(QStringLiteral("deleteTransactionButton"))->click();
        settle();
        auto *confirmation = restarted.findChild<QMessageBox *>();
        QVERIFY(confirmation);
        confirmation->button(QMessageBox::Yes)->click();
        settle();
        QCOMPARE(table->rowCount(), 0);
        QCOMPARE(restarted.findChild<QLabel *>(QStringLiteral("totalExpenseLabel"))->text(),
                 QStringLiteral("全部支出：¥0.00"));
        QVERIFY(!restarted.findChild<QPushButton *>(QStringLiteral("deleteTransactionButton"))->isEnabled());
        QVERIFY(!restarted.findChild<QPushButton *>(QStringLiteral("editTransactionButton"))->isEnabled());
    }
    MainWindow empty(path);
    QCOMPARE(empty.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(), 0);
}

void WindowTests::filtersAndTotalsUpdate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("filters.sqlite"));
    const QDate today = QDate::currentDate();
    {
        DatabaseManager database(path);
        QVERIFY(database.initialize());
        TransactionRepository repository(database);
        QList<Transaction> fixtures{
            Transaction(0, QStringLiteral("早餐"), 1000, QStringLiteral("餐饮"), today),
            Transaction(0, QStringLiteral("地铁"), 400, QStringLiteral("交通"), today),
            Transaction(0, QStringLiteral("午饭"), 2000, QStringLiteral("餐饮"), today.addDays(-1)),
            Transaction(0, QStringLiteral("奶茶"), 750, QStringLiteral("餐饮"), today.addDays(-2))};
        for (auto &record : fixtures)
            QVERIFY(repository.add(record));
    }
    MainWindow window(path);
    window.show();
    settle();
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    auto *category = window.findChild<QComboBox *>(QStringLiteral("categoryFilter"));
    auto *useDate = window.findChild<QCheckBox *>(QStringLiteral("useDateRangeCheckBox"));
    auto *start = window.findChild<QDateEdit *>(QStringLiteral("startDateFilter"));
    auto *end = window.findChild<QDateEdit *>(QStringLiteral("endDateFilter"));
    auto *apply = window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"));
    auto *filtered = window.findChild<QLabel *>(QStringLiteral("filteredExpenseLabel"));
    auto *total = window.findChild<QLabel *>(QStringLiteral("totalExpenseLabel"));
    QCOMPARE(table->rowCount(), 4);
    QCOMPARE(total->text(), QStringLiteral("全部支出：¥41.50"));
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("todayExpenseLabel"))->text(),
             QStringLiteral("今日支出：¥14.00"));
    const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty())
        QVERIFY(window.grab().save(preview + QStringLiteral("/bookkeeping-main.png")));
    category->setCurrentText(QStringLiteral("餐饮"));
    apply->click();
    QCOMPARE(table->rowCount(), 3);
    QCOMPARE(filtered->text(), QStringLiteral("当前结果：3 笔 · ¥37.50"));
    QCOMPARE(total->text(), QStringLiteral("全部支出：¥41.50"));
    useDate->setChecked(true);
    QVERIFY(start->isEnabled() && end->isEnabled());
    start->setDate(today.addDays(-1));
    end->setDate(today);
    apply->click();
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(filtered->text(), QStringLiteral("当前结果：2 笔 · ¥30.00"));
    if (!preview.isEmpty()) {
        settle();
        QVERIFY(window.grab().save(preview + QStringLiteral("/bookkeeping-filtered.png")));
    }
    start->setDate(today);
    apply->click();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(filtered->text(), QStringLiteral("当前结果：1 笔 · ¥10.00"));
    const QString appliedRange = window.findChild<QLabel *>(QStringLiteral("appliedFilterLabel"))->text();
    end->setDate(today.addDays(-1));
    apply->click();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(filtered->text(), QStringLiteral("当前结果：1 笔 · ¥10.00"));
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("appliedFilterLabel"))->text(), appliedRange);
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("databaseErrorLabel"))->text()
                .contains(QStringLiteral("开始日期")));
    end->setDate(today);
    category->setCurrentText(QStringLiteral("购物"));
    apply->click();
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(filtered->text(), QStringLiteral("当前结果：0 笔 · ¥0.00"));
    QCOMPARE(total->text(), QStringLiteral("全部支出：¥41.50"));
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("emptyHint"))->isVisible());
    category->setCurrentText(QStringLiteral("交通"));
    apply->click();
    QCOMPARE(table->rowCount(), 1);
    auto *dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("新早餐"), 12.34, QStringLiteral("餐饮"), today);
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(filtered->text(), QStringLiteral("当前结果：1 笔 · ¥4.00"));
    QCOMPARE(total->text(), QStringLiteral("全部支出：¥53.84"));
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("不符合")));
    window.findChild<QPushButton *>(QStringLiteral("resetFilterButton"))->click();
    QCOMPARE(table->rowCount(), 5);
    QCOMPARE(category->currentIndex(), 0);
    QVERIFY(!useDate->isChecked());
    QVERIFY(!start->isEnabled() && !end->isEnabled());
    QCOMPARE(filtered->text(), QStringLiteral("当前结果：5 笔 · ¥53.84"));
}

void WindowTests::lockedEditsAndDeletes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("locked-crud.sqlite"));
    MainWindow window(path);
    window.show();
    settle();
    auto *dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("午饭"), 28, QStringLiteral("餐饮"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    const qint64 id = table->item(0, 0)->data(Qt::UserRole).toLongLong();
    DatabaseManager blocker(path);
    QVERIFY(blocker.initialize());
    QSqlQuery lock(blocker.database());
    QVERIFY(lock.exec(QStringLiteral("BEGIN IMMEDIATE")));
    window.findChild<QPushButton *>(QStringLiteral("editTransactionButton"))->click();
    settle();
    dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("教材"), 12.34, QStringLiteral("学习"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QVERIFY(dialog->isVisible());
    QCOMPARE(dialog->transaction().id(), id);
    QCOMPARE(dialog->description(), QStringLiteral("教材"));
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("validationErrorLabel"))->text().contains(QStringLiteral("占用")));
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("午饭"));
    QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), id);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("教材"));
    QVERIFY(lock.exec(QStringLiteral("BEGIN IMMEDIATE")));
    auto *remove = window.findChild<QPushButton *>(QStringLiteral("deleteTransactionButton"));
    remove->click();
    settle();
    auto *confirmation = window.findChild<QMessageBox *>();
    QVERIFY(confirmation);
    confirmation->button(QMessageBox::Yes)->click();
    settle();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("totalExpenseLabel"))->text(), QStringLiteral("全部支出：¥12.34"));
    QVERIFY(window.findChild<QLabel *>(QStringLiteral("databaseErrorLabel"))->text().contains(QStringLiteral("占用")));
    QVERIFY(lock.exec(QStringLiteral("ROLLBACK")));
    remove->click();
    settle();
    confirmation = window.findChild<QMessageBox *>();
    QVERIFY(confirmation);
    confirmation->button(QMessageBox::Yes)->click();
    settle();
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("totalExpenseLabel"))->text(), QStringLiteral("全部支出：¥0.00"));
}

void WindowTests::missingEditedRecordNotRecreated()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("missing.sqlite"));
    MainWindow window(path);
    window.show();
    settle();
    auto *dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("午饭"), 28, QStringLiteral("餐饮"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    const qint64 id = table->item(0, 0)->data(Qt::UserRole).toLongLong();
    window.findChild<QPushButton *>(QStringLiteral("editTransactionButton"))->click();
    settle();
    dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    DatabaseManager external(path);
    QVERIFY(external.initialize());
    TransactionRepository repository(external);
    QVERIFY(repository.remove(id));
    fill(*dialog, QStringLiteral("修改草稿"), 99, QStringLiteral("学习"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QVERIFY(dialog->isVisible());
    QCOMPARE(dialog->description(), QStringLiteral("修改草稿"));
    QVERIFY(dialog->findChild<QLabel *>(QStringLiteral("validationErrorLabel"))->text().contains(QStringLiteral("不存在")));
    QList<Transaction> rows;
    QVERIFY(repository.findAll(rows));
    QVERIFY(rows.isEmpty());
    dialog->close();
    settle();
    window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
    QCOMPARE(table->rowCount(), 0);
}

void WindowTests::editedRecordMovesOutOfFilter()
{
    MainWindow window(QStringLiteral(":memory:"));
    window.show();
    settle();
    auto *dialog = openDialog(window);
    QVERIFY(dialog);
    fill(*dialog, QStringLiteral("午饭"), 28, QStringLiteral("餐饮"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    auto *category = window.findChild<QComboBox *>(QStringLiteral("categoryFilter"));
    category->setCurrentText(QStringLiteral("餐饮"));
    window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
    auto *table = window.findChild<QTableWidget *>(QStringLiteral("transactionTable"));
    table->selectRow(0);
    window.findChild<QPushButton *>(QStringLiteral("editTransactionButton"))->click();
    settle();
    dialog = window.findChild<AddTransactionDialog *>();
    QVERIFY(dialog);
    const qint64 id = dialog->transaction().id();
    fill(*dialog, QStringLiteral("教材"), 12.34, QStringLiteral("学习"), QDate::currentDate());
    dialog->findChild<QPushButton *>(QStringLiteral("saveButton"))->click();
    settle();
    QCOMPARE(table->rowCount(), 0);
    QCOMPARE(category->currentText(), QStringLiteral("餐饮"));
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("filteredExpenseLabel"))->text(), QStringLiteral("当前结果：0 笔 · ¥0.00"));
    QCOMPARE(window.findChild<QLabel *>(QStringLiteral("totalExpenseLabel"))->text(), QStringLiteral("全部支出：¥12.34"));
    QVERIFY(window.statusBar()->currentMessage().contains(QStringLiteral("不符合")));
    window.findChild<QPushButton *>(QStringLiteral("resetFilterButton"))->click();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toLongLong(), id);
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("教材"));
}

QTEST_MAIN(WindowTests)
#include "window_tests.moc"
