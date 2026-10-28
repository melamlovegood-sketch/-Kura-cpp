#include <QStackedWidget>
#include "DemoData.h"
#include "DatabaseManager.h"
#include "WishlistRepository.h"
#include "BudgetRepository.h"
#include "TransactionRepository.h"
#include "DatabaseBackupService.h"
#include "CategorySummary.h"
#include "WishDialog.h"
#include "mainwindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QIcon>
#include "AppTypography.h"
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

static void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
}
class ReleaseTests : public QObject
{
    Q_OBJECT
private slots:
    void appIconSupportsWindowsSizes();
    void demoFixtureAndNoOverwrite();
    void wishPriorityAndBackupMigration();
    void responsivePagesAndPreviews();
};
void ReleaseTests::appIconSupportsWindowsSizes()
{
    AppTypography::initialize();
    const QIcon icon(QStringLiteral(":/icons/kura.ico"));
    QVERIFY(!icon.isNull());
    for (const int size:{16,20,24,32,48,64,128,256}) {
        QVERIFY(icon.availableSizes().contains(QSize(size,size)));
        const auto pixmap=icon.pixmap(size,size);
        QVERIFY(!pixmap.isNull()); QCOMPARE(pixmap.size(),QSize(size,size));
        QVERIFY(pixmap.toImage().pixelColor(size/2,size/2).alpha()>0);
    }
}
void ReleaseTests::demoFixtureAndNoOverwrite()
{
    QTemporaryDir directory;
    DatabaseManager database(directory.filePath(QStringLiteral("demo.sqlite")));
    QVERIFY(database.initialize()); QString error;
    const auto now = QDateTime::fromString(QStringLiteral("2026-10-13T10:00:00Z"),Qt::ISODate);
    QVERIFY2(DemoData::populate(database,error,now),qPrintable(error));
    BudgetRepository budgets(database); MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(now.toLocalTime().date(),status));
    QCOMPARE(status.spentCents,23700); QCOMPARE(status.limitCents,30000); QCOMPARE(status.remainingCents(),6300);
    QList<Transaction> records; TransactionRepository transactions(database);
    QVERIFY(transactions.findAll(records)); QCOMPARE(records.size(),17);
    const auto categories = CategorySummary::calculate(records,now.toLocalTime().date());
    qint64 total = 0; for (const auto &category : categories) total += category.amountCents;
    QCOMPARE(total,23700);
    WishlistRepository wishes(database); QList<WishItem> items;
    QVERIFY(wishes.findAll(items)); QCOMPARE(items.size(),4);
    int ready=0,waiting=0,purchased=0,abandoned=0;
    for (const auto &item : items) {
        if (item.status==WishStatus::Purchased) ++purchased;
        else if (item.status==WishStatus::Abandoned) ++abandoned;
        else if (item.canPurchase(now)) ++ready;
        else ++waiting;
    }
    QCOMPARE(ready,1); QCOMPARE(waiting,1); QCOMPARE(purchased,1); QCOMPARE(abandoned,1);
    QVERIFY(!DemoData::populate(database,error,now));
    QVERIFY(transactions.findAll(records)); QCOMPARE(records.size(),17);
}
void ReleaseTests::wishPriorityAndBackupMigration()
{
    QTemporaryDir directory;
    const QString path=directory.filePath(QStringLiteral("priority.sqlite"));
    DatabaseManager database(path); QVERIFY(database.initialize());
    WishlistRepository repository(database);
    WishItem item; item.name=QStringLiteral("教材"); item.estimatedCents=5000; item.priority=1;
    QVERIFY(repository.add(item));
    WishDialog dialog(item);
    QVERIFY(dialog.findChild<QPushButton *>("wishPriority1Button")->isChecked());
    dialog.findChild<QPushButton *>("wishPriority3Button")->click();
    QVERIFY(repository.update(dialog.item()));
    QList<WishItem> items; QVERIFY(repository.findAll(items)); QCOMPARE(items[0].priority,3);
    item.priority=0; QVERIFY(!repository.update(item));
    DatabaseBackupService backup(database); QString error,safety; BackupInfo info;
    const QString copy=directory.filePath(QStringLiteral("backup.sqlite"));
    QVERIFY(backup.backup(copy,error));
    QVERIFY(backup.prepareRestore(copy,info,error)); QVERIFY(backup.restorePrepared(safety,error));
    QVERIFY(repository.findAll(items)); QCOMPARE(items[0].priority,3);
    {
        DatabaseManager legacy(copy); QVERIFY(legacy.initialize()); QSqlQuery query(legacy.database());
        QVERIFY(query.exec(QStringLiteral("ALTER TABLE wishlist DROP COLUMN priority")));
    }
    QFile source(copy); QVERIFY(source.open(QIODevice::ReadOnly)); const QByteArray old=source.readAll(); source.close();
    QVERIFY(backup.prepareRestore(copy,info,error));
    QVERIFY(source.open(QIODevice::ReadOnly)); QCOMPARE(source.readAll(),old); source.close();
    QVERIFY(backup.restorePrepared(safety,error));
    QVERIFY(repository.findAll(items)); QCOMPARE(items[0].priority,2);
    { DatabaseManager restarted(path); QVERIFY(restarted.initialize()); WishlistRepository again(restarted);
      QVERIFY(again.findAll(items)); QCOMPARE(items[0].priority,2); }
}
void ReleaseTests::responsivePagesAndPreviews()
{
    QTemporaryDir directory;
    const QString path=directory.filePath(QStringLiteral("preview.sqlite"));
    { DatabaseManager database(path); QVERIFY(database.initialize()); QString error; QVERIFY(DemoData::populate(database,error)); }
    MainWindow window(path); window.showPage(MainWindow::Page::Billing); window.resize(1180,860); window.show(); settle();
    auto *pages=window.findChild<QStackedWidget *>(QStringLiteral("mainPages"));
    QCOMPARE(pages->count(),7);
    auto *categories=window.findChild<QTableWidget *>(QStringLiteral("categorySummaryTable"));
    QCOMPARE(categories->rowCount(),6);
    qint64 total=0; for (int row=0;row<6;++row) total += categories->item(row,1)->data(Qt::UserRole).toLongLong();
    QCOMPARE(total,23700);
    const QString preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) QVERIFY(QDir().mkpath(preview));
    const QStringList names={QStringLiteral("release-bookkeeping"),QStringLiteral("release-statistics"),QStringLiteral("release-wishlist"),QStringLiteral("release-data")};
    for (int page=0;page<names.size();++page) {
        pages->setCurrentIndex(page); settle();
        if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+QStringLiteral("/")+names[page]+QStringLiteral(".png")));
    }
    pages->setCurrentIndex(0); window.resize(800,600); settle();
    QCOMPARE(window.size(),QSize(800,600));
    auto *scroll=window.findChild<QScrollArea *>(QStringLiteral("recordsScrollArea"));
    QVERIFY(scroll->verticalScrollBar()->maximum()>0);
    QVERIFY(window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->isEnabled());
    if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+QStringLiteral("/release-compact-top.png")));
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); settle();
    if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+QStringLiteral("/release-compact-bottom.png")));
    window.findChild<QComboBox *>(QStringLiteral("categoryFilter"))->setCurrentText(QStringLiteral("吃喝"));
    window.findChild<QPushButton *>(QStringLiteral("applyFilterButton"))->click();
    QCOMPARE(window.findChild<QTableWidget *>(QStringLiteral("transactionTable"))->rowCount(),2);
    total=0; for(int row=0;row<6;++row) total+=categories->item(row,1)->data(Qt::UserRole).toLongLong();
    QCOMPARE(total,23700);
}
QTEST_MAIN(ReleaseTests)
#include "release_tests.moc"
