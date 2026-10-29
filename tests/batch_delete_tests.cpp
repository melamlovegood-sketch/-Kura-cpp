#include "mainwindow.h"
#include "AppTheme.h"
#include "BatchSelectionBar.h"
#include "WishlistWidget.h"
#include "WishlistRepository.h"
#include "UnifiedWishlistRepository.h"
#include "WishPoolRepository.h"
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QApplication::sendPostedEvents(nullptr,QEvent::LayoutRequest); QApplication::processEvents(); }
void click(QObject &owner,const char *name) { auto *b=owner.findChild<QPushButton *>(name); QVERIFY2(b,name); b->click(); settle(); }
void answer(QObject &owner,const char *name,QMessageBox::StandardButton button) {
    auto *box=owner.findChild<QMessageBox *>(name); QVERIFY(box); box->button(button)->click(); settle();
}
int count(QTableWidget *table) { return table->selectionModel()->selectedRows().size(); }
void checkRecord(QTableWidget *table,int row) {
    const auto index=table->model()->index(row,0);
    QStyleOptionViewItem option; option.initFrom(table); option.rect=table->visualRect(index); option.features=QStyleOptionViewItem::HasCheckIndicator;
    const auto rect=table->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator,&option,table);
    QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center()); settle();
}
WishItem wish(const QString &name) { WishItem w; w.name=name; w.estimatedCents=1000; w.coolingDays=3; return w; }
}
class BatchDeleteTests:public QObject {
    Q_OBJECT
private slots:
    void recordsMouseKeyboardFiltersCancelAndRestart();
    void recordDeletionRollsBackRelatedData();
    void wishesChecksTimerFiltersAndMixedDeletion();
    void wishDeletionRollsBackAndDoesNotReimportPrices();
};

void BatchDeleteTests::recordsMouseKeyboardFiltersCancelAndRestart()
{
    QTemporaryDir dir; const auto path=dir.filePath("records.sqlite");
    DatabaseManager db(path); QVERIFY(db.initialize()); TransactionRepository repo(db);
    for (int i=0;i<4;++i) {
        Transaction t(0,QStringLiteral("虚构消费%1").arg(i),100*(i+1),QStringLiteral("餐饮"),QDate::currentDate().addMonths(i==3?-1:0));
        QVERIFY(repo.add(t));
    }
    {
        MainWindow window(path); window.resize(1180,860); window.showPage(MainWindow::Page::Billing); window.show(); settle();
        click(window,"billingAllRecordsButton");
        auto *table=window.findChild<QTableWidget *>("transactionTable"); QCOMPARE(table->rowCount(),3);
        checkRecord(table,0); checkRecord(table,2); QCOMPARE(count(table),2);
        QVERIFY(!window.findChild<QPushButton *>("editTransactionButton")->isEnabled());
        QCOMPARE(window.findChild<QCheckBox *>("transactionSelectAllCheckBox")->checkState(),Qt::PartiallyChecked);
        click(window,"deleteTransactionButton");
        auto *confirmation=window.findChild<QMessageBox *>("deleteConfirmationDialog"); QVERIFY(confirmation);
        QVERIFY(confirmation->text().contains(QStringLiteral("2 笔")));
        answer(window,"deleteConfirmationDialog",QMessageBox::No); QCOMPARE(table->rowCount(),3); QCOMPARE(count(table),2);
        window.findChild<QCheckBox *>("transactionSelectAllCheckBox")->click(); QCOMPARE(count(table),3);
        click(window,"transactionClearSelectionButton"); QCOMPARE(count(table),0);
        table->setFocus(); QTest::keyClick(table,Qt::Key_A,Qt::ControlModifier); QCOMPARE(count(table),3);
        QSqlQuery failure(db.database());
        QVERIFY(failure.exec("CREATE TRIGGER fail_ui BEFORE DELETE ON transactions BEGIN SELECT RAISE(ABORT,'simulated failure'); END"));
        click(window,"deleteTransactionButton"); answer(window,"deleteConfirmationDialog",QMessageBox::Yes);
        QCOMPARE(table->rowCount(),3); QCOMPARE(count(table),3);
        QVERIFY(!window.findChild<QLabel *>("databaseErrorLabel")->isHidden());
        QVERIFY(failure.exec("DROP TRIGGER fail_ui"));
        // Clear the deliberately induced error before capturing normal use.
        QVERIFY(window.reloadTransactions()); table->selectAll();
        for (const auto &theme:QStringList{"warm","cool","dark"}) {
            AppTheme::apply(&window,theme); window.resize(800,650); settle();
            const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
            if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(window.grab().save(preview+"/batch-records-"+theme+".png")); }
        }
        click(window,"deleteTransactionButton");
        // Confirmation owns the original selection, even if the underlying model changes.
        table->clearSelection();
        answer(window,"deleteConfirmationDialog",QMessageBox::Yes); QCOMPARE(table->rowCount(),0);
        QList<Transaction> left; QVERIFY(repo.findAll(left)); QCOMPARE(left.size(),1);
        QCOMPARE(window.findChild<QLabel *>("billingMonthTotalLabel")->text(),QStringLiteral("¥0.00"));
        click(window,"resetFilterButton"); QCOMPARE(table->rowCount(),1); QCOMPARE(count(table),0);
        window.findChild<QCheckBox *>("transactionSelectAllCheckBox")->click();
        click(window,"deleteTransactionButton"); answer(window,"deleteConfirmationDialog",QMessageBox::Yes);
        QCOMPARE(table->rowCount(),0); QVERIFY(!window.findChild<QPushButton *>("deleteTransactionButton")->isEnabled());
    }
    MainWindow restarted(path); QCOMPARE(restarted.findChild<QTableWidget *>("transactionTable")->rowCount(),0);
}

void BatchDeleteTests::recordDeletionRollsBackRelatedData()
{
    DatabaseManager db(":memory:"); QVERIFY(db.initialize()); TransactionRepository repo(db);
    Transaction a(0,"first",100,QStringLiteral("餐饮"),QDate::currentDate()),b(0,"second",200,QStringLiteral("餐饮"),QDate::currentDate());
    QVERIFY(repo.add(a)); QVERIFY(repo.add(b));
    QSqlQuery q(db.database());
    QVERIFY(q.exec(QStringLiteral("INSERT INTO transaction_reviews(transaction_id,tag,note,updated_ms) VALUES(%1,1,'test',1)").arg(a.id())));
    QVERIFY(q.exec(QStringLiteral("CREATE TRIGGER fail_second BEFORE DELETE ON transactions WHEN OLD.id=%1 BEGIN SELECT RAISE(ABORT,'simulated failure'); END").arg(b.id())));
    QVERIFY(!repo.removeMany({a.id(),b.id()}));
    QList<Transaction> records; QVERIFY(repo.findAll(records)); QCOMPARE(records.size(),2);
    QVERIFY(q.exec("SELECT COUNT(*) FROM transaction_reviews")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(),1); q.finish();
    QVERIFY(q.exec("DROP TRIGGER fail_second"));
    QVERIFY(!repo.removeMany({a.id(),99999})); QVERIFY(repo.findAll(records)); QCOMPARE(records.size(),2);
    QVERIFY(!repo.removeMany({a.id(),a.id()})); QVERIFY(repo.removeMany({a.id(),b.id()}));
    QVERIFY(repo.findAll(records)); QVERIFY(records.isEmpty());
}

void BatchDeleteTests::wishesChecksTimerFiltersAndMixedDeletion()
{
    QTemporaryDir dir; const auto path=dir.filePath("wishes.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    WishlistRepository repo(db); auto a=wish("A"),b=wish("B"),c=wish("C"),history=wish("history");
    QVERIFY(repo.add(a)); QVERIFY(repo.add(b)); QVERIFY(repo.add(c)); QVERIFY(repo.add(history)); QVERIFY(repo.abandon(history.id));
    MainWindow window(path); window.resize(1100,900); window.showPage(MainWindow::Page::Wishlist); window.show(); settle();
    auto *table=window.findChild<QTableWidget *>("wishlistTable"); QCOMPARE(table->rowCount(),3);
    const auto check=[table](int row) { QTest::mouseClick(table->cellWidget(row,0)->findChild<QCheckBox *>("wishSelectCheckBox"),Qt::LeftButton); settle(); };
    check(0); check(2); QCOMPARE(count(table),2);
    window.findChild<WishlistWidget *>()->refreshCountdowns(); QCOMPARE(count(table),2);
    click(window,"deleteSelectedWishesButton"); answer(window,"wishConfirmationDialog",QMessageBox::No); QCOMPARE(count(table),2);
    auto *filter=window.findChild<QComboBox *>("wishlistStatusFilter"); filter->setCurrentIndex(4); settle();
    QCOMPARE(table->rowCount(),1); QCOMPARE(count(table),0);
    filter->setCurrentIndex(0); settle(); QCOMPARE(table->rowCount(),4);
    window.findChild<QCheckBox *>("wishlistSelectAllCheckBox")->click(); QCOMPARE(count(table),4);
    settle();
    auto *remove=window.findChild<QPushButton *>("deleteSelectedWishesButton");
    QVERIFY(remove->parentWidget()->rect().contains(remove->geometry()));
    QVERIFY(remove->width()>=remove->fontMetrics().horizontalAdvance(remove->text())+24);
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(window.grab().save(preview+"/batch-wishes.png")); }
    click(window,"deleteSelectedWishesButton");
    auto *box=window.findChild<QMessageBox *>("wishConfirmationDialog"); QVERIFY(box);
    QVERIFY(box->text().contains(QStringLiteral("放弃 3 件"))); QVERIFY(box->text().contains(QStringLiteral("删除 1 条")));
    answer(window,"wishConfirmationDialog",QMessageBox::Yes);
    QList<WishItem> items; QVERIFY(repo.findAll(items)); QCOMPARE(items.size(),3);
    for (const auto &item:items) QCOMPARE(item.status,WishStatus::Abandoned);
    QCOMPARE(count(table),0); window.findChild<QCheckBox *>("wishlistSelectAllCheckBox")->click();
    click(window,"deleteSelectedWishesButton"); answer(window,"wishConfirmationDialog",QMessageBox::Yes);
    QCOMPARE(table->rowCount(),0); QVERIFY(repo.findAll(items)); QVERIFY(items.isEmpty());
    MainWindow restarted(path); QCOMPARE(restarted.findChild<QTableWidget *>("wishlistTable")->rowCount(),0);
}

void BatchDeleteTests::wishDeletionRollsBackAndDoesNotReimportPrices()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("related.sqlite")); QVERIFY(db.initialize());
    WishlistRepository repo(db); UnifiedWishlistRepository catalog(db); auto a=wish("first"),b=wish("second");
    QVERIFY(repo.add(a)); QVERIFY(repo.add(b));
    WishPoolRepository pool(db); QVERIFY(pool.setGoal(b.id));
    qint64 trackId=0; QVERIFY(catalog.recordPrice(a.id,900,"manual","test",QDateTime::currentDateTimeUtc(),trackId));
    QVERIFY(repo.abandon(a.id)); a.status=WishStatus::Abandoned;
    QSqlQuery q(db.database());
    QVERIFY(q.exec(QStringLiteral("CREATE TRIGGER fail_wish BEFORE UPDATE ON wishlist WHEN OLD.id=%1 BEGIN SELECT RAISE(ABORT,'simulated failure'); END").arg(b.id)));
    QVERIFY(!repo.removeMany({a,b}));
    QList<WishItem> items; QVERIFY(repo.findAll(items)); QCOMPARE(items.size(),2);
    WishPoolSnapshot goal; QVERIFY(pool.load(goal)); QCOMPARE(goal.wishId,b.id);
    QHash<qint64,qint64> links,positions; QVERIFY(catalog.state(links,positions)); QCOMPARE(links.value(a.id),trackId);
    QVERIFY(q.exec("DROP TRIGGER fail_wish"));
    auto stale=b; QVERIFY(repo.abandon(b.id)); QVERIFY(!repo.removeMany({a,stale}));
    QVERIFY(repo.findAll(items)); QCOMPARE(items.size(),2);
    b.status=WishStatus::Abandoned; QVERIFY(repo.removeMany({a,b}));
    QVERIFY(pool.load(goal)); QCOMPARE(goal.wishId,0);
    QVERIFY(catalog.synchronize()); QVERIFY(repo.findAll(items)); QVERIFY(items.isEmpty());
    QVERIFY(catalog.state(links,positions)); QVERIFY(!links.contains(a.id));
    TransactionRepository transactions(db); QList<Transaction> expenses; QVERIFY(transactions.findAll(expenses)); QVERIFY(expenses.isEmpty());
}

QTEST_MAIN(BatchDeleteTests)
#include "batch_delete_tests.moc"
