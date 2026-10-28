#include "mainwindow.h"
#include "AppTheme.h"
#include "WishlistWidget.h"
#include "WishPriceDialog.h"
#include "PriceRecordDialog.h"
#include "UnifiedWishlistRepository.h"
#include "DatabaseBackupService.h"
#include "ProductRepository.h"
#include "MonthlyReviewRepository.h"
#include "WishPoolRepository.h"
#include "ProductDetailsDialog.h"
#include <QMessageBox>
#include <algorithm>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QApplication::processEvents(); }
void click(QObject &owner,const char *name) { auto *b=owner.findChild<QPushButton *>(name); QVERIFY2(b,name); b->click(); settle(); }
bool screenshot(QWidget &widget,const QString &name) {
    settle(); const auto dir=qEnvironmentVariable("KURA_PREVIEW_DIR");
    return dir.isEmpty() || (QDir().mkpath(dir) && widget.grab().save(dir+"/planning-"+name+".png"));
}
QList<qint64> ids(QTableWidget *table) { QList<qint64> result; for (int row=0;row<table->rowCount();++row) result.append(table->item(row,0)->data(Qt::UserRole).toLongLong()); return result; }
bool populate(DatabaseManager &database,int count=3) {
    WishlistRepository wishes(database); const auto now=QDateTime::currentDateTimeUtc();
    const QStringList names={QStringLiteral("iPhone 18 Pro Max"),QStringLiteral("优衣库 舒芙蕾圆领针织衫"),QStringLiteral("耳机")};
    for (int i=0;i<count;++i) {
        WishItem wish; wish.name=i<3?names[i]:QStringLiteral("长名称商品 %1：日常使用的无线耳机与学习用品").arg(i+1);
        wish.estimatedCents=i==0?1088401:i==1?9700:20000; wish.coolingDays=3; wish.reason=i<3?QStringLiteral("考虑一下实际需求。"):QString(500,QChar(0x7528));
        if (!wishes.add(wish,now.addSecs(-i*60))) return false;
    }
    PriceRepository prices(database); qint64 id=0;
    if (!prices.intake(QStringLiteral("MUJI 女式柔软针织衫"),"online_shopping",22800,QStringLiteral("淘宝 · M 码"),"planning-first",id,now.addDays(-2))
        || !prices.record(id,25300,QStringLiteral("京东 · M 码"),"planning-next",now.addDays(-1)) || !prices.configure(id,20000,QString())) return false;
    QSqlQuery query(database.database()); query.prepare("UPDATE price_tracks SET created_ms=:time WHERE id=:id"); query.bindValue(":time",now.addDays(-2).toMSecsSinceEpoch()); query.bindValue(":id",id); return query.exec();
}
}
class PlanningLayoutTests:public QObject {
    Q_OBJECT
private slots:
    void unifiedListAndPerProductPrices();
    void directGoalAndAbandonActions();
    void coolingBadgeOnlyRestrictsPurchasing();
    void manualOrderPersistsAndNewItemsAppend();
    void historyAndSmallThemesFit();
    void migrationLinksWithoutLosingHistoryAndBackup();
    void duplicateNamesDoNotSharePricesAndDeleteDoesNotReimport();
    void archivedPricesDoNotInventAbandonmentDecisions();
};

void PlanningLayoutTests::directGoalAndAbandonActions()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("actions.sqlite")); QVERIFY(db.initialize()); QVERIFY(populate(db,2));
    MainWindow window(db.databasePath()); window.resize(1000,800); window.show(); window.showPage(MainWindow::Page::Wishlist); settle();
    auto *table=window.findChild<QTableWidget *>("wishlistTable");
    const auto original=ids(table); QVERIFY(original.size()>1);
    const auto goal=[table](int row) { return table->cellWidget(row,0)->findChild<QToolButton *>("wishGoalButton"); };
    const auto abandon=[table](int row) { return table->cellWidget(row,0)->findChild<QToolButton *>("wishAbandonButton"); };
    QVERIFY(!window.findChild<QWidget *>("wishProductMoreButton"));
    QVERIFY(goal(0)->isVisible()); QVERIFY(!goal(0)->isChecked());
    QVERIFY(goal(0)->toolTip().contains(QStringLiteral("点击设为许愿目标")));
    QTest::mouseClick(goal(0),Qt::LeftButton); settle();
    QCOMPARE(window.currentPage(),MainWindow::Page::Wishlist); QVERIFY(goal(0)->isChecked()); QVERIFY(!goal(1)->isChecked());
    QVERIFY(screenshot(window,"goal-lit"));
    QVERIFY(!table->item(0,0)->isSelected());
    WishPoolRepository pool(db); WishPoolSnapshot snapshot; QVERIFY(pool.load(snapshot)); QCOMPARE(snapshot.wishId,original[0]);
    goal(1)->click(); settle(); auto *replace=window.findChild<QMessageBox *>("replaceWishPoolDialog"); QVERIFY(replace);
    QVERIFY(!goal(1)->isChecked()); replace->button(QMessageBox::No)->click(); settle();
    QVERIFY(goal(0)->isChecked()); QVERIFY(!goal(1)->isChecked());
    goal(1)->click(); settle(); replace=window.findChild<QMessageBox *>("replaceWishPoolDialog"); QVERIFY(replace);
    replace->button(QMessageBox::Yes)->click(); settle(); QVERIFY(!goal(0)->isChecked()); QVERIFY(goal(1)->isChecked());
    window.findChild<QAction *>("wishlistSortOldestAction")->trigger(); settle();
    auto *wishlist=window.findChild<WishlistWidget *>(); QVERIFY(wishlist->refresh());
    const int targetRow=ids(table).indexOf(original[1]); QVERIFY(targetRow>=0); QVERIFY(goal(targetRow)->isChecked());
    table->selectRow(targetRow); settle();
    abandon(targetRow)->click(); settle(); auto *confirmation=window.findChild<QMessageBox *>("wishConfirmationDialog"); QVERIFY(confirmation);
    QVERIFY(confirmation->text().contains(QStringLiteral("确定删除")));
    QCOMPARE(confirmation->defaultButton(),qobject_cast<QPushButton *>(confirmation->button(QMessageBox::No)));
    confirmation->button(QMessageBox::No)->click(); settle(); QCOMPARE(ids(table).size(),original.size()); QVERIFY(goal(targetRow)->isChecked());
    abandon(targetRow)->click(); settle(); confirmation=window.findChild<QMessageBox *>("wishConfirmationDialog"); QVERIFY(confirmation);
    confirmation->button(QMessageBox::Yes)->click(); settle(); QVERIFY(!ids(table).contains(original[1]));
    QVERIFY(pool.load(snapshot)); QCOMPARE(snapshot.wishId,0);
    QList<WishItem> wishes; WishlistRepository repository(db); QVERIFY(repository.findAll(wishes));
    const auto found=std::find_if(wishes.cbegin(),wishes.cend(),[&](const WishItem &wish) { return wish.id==original[1]; });
    QVERIFY(found!=wishes.cend()); QCOMPARE(found->status,WishStatus::Abandoned);
    window.resize(780,720); settle(); table->selectRow(0); settle();
    QVERIFY(goal(0)->geometry().width()>=36); QVERIFY(abandon(0)->isVisible()); QVERIFY(screenshot(window,"direct-actions"));
    click(window,"wishProductInfoButton"); auto *details=window.findChild<ProductDetailsDialog *>(); QVERIFY(details);
    QVERIFY(details->findChild<QPushButton *>("editWishDetailsButton")->isVisible()); details->reject(); settle();
}

void PlanningLayoutTests::unifiedListAndPerProductPrices()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("unified.sqlite")); QVERIFY(db.initialize()); QVERIFY(populate(db));
    MainWindow window(db.databasePath()); window.resize(1440,960); window.show(); window.showPage(MainWindow::Page::Wishlist); settle();
    auto *table=window.findChild<QTableWidget *>("wishlistTable"); QCOMPARE(table->rowCount(),4); QVERIFY(table->horizontalHeader()->isHidden());
    QVERIFY(!window.findChild<QPushButton *>("planningPriceTabButton")); QVERIFY(!window.findChild<QWidget *>("planningTabs"));
    QCOMPARE(window.findChild<QLabel *>("wishlistSummaryLabel")->text(),QStringLiteral("4 件待购"));
    QVERIFY(screenshot(window,"unified")); table->selectRow(0); settle();
    QVERIFY(window.findChild<QWidget *>("wishProductDetails")->isVisible()); QVERIFY(!window.findChild<QPushButton *>("purchaseWishButton")->isEnabled());
    QVERIFY(window.findChild<QPushButton *>("wishPriceButton")->isEnabled());
    click(window,"wishPriceButton"); auto *prices=window.findChild<WishPriceDialog *>(); QVERIFY(prices); click(*prices,"recordWishPriceButton");
    auto *record=window.findChild<PriceRecordDialog *>(); QVERIFY(record);
    QCOMPARE(record->findChild<QLineEdit *>("priceNameEdit")->text(),QStringLiteral("iPhone 18 Pro Max")); QVERIFY(record->findChild<QLineEdit *>("priceNameEdit")->isReadOnly());
    record->findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(9999); click(*record,"savePriceRecordButton");
    QCOMPARE(prices->findChild<QTableWidget *>("wishPriceHistoryTable")->rowCount(),1); click(*prices,"closeWishPriceButton");
    QCOMPARE(table->rowCount(),4); QCOMPARE(table->cellWidget(0,0)->findChild<QLabel *>("productPriceNoteLabel")->text(),QStringLiteral("最近价格"));
    QCOMPARE(table->cellWidget(0,0)->findChild<QLabel *>("productAmountLabel")->text(),QStringLiteral("¥9999.00"));
    QVERIFY(!window.findChild<QPushButton *>("purchaseWishButton")->isEnabled()); QVERIFY(screenshot(window,"unified-expanded"));
    table->selectRow(3); settle(); click(window,"wishPriceButton"); prices=window.findChild<WishPriceDialog *>(); QVERIFY(prices);
    QCOMPARE(prices->findChild<QTableWidget *>("wishPriceHistoryTable")->rowCount(),2); QVERIFY(prices->findChild<QWidget *>("priceTrendChart")->isVisible());
    QVERIFY(screenshot(*prices,"product-prices")); click(*prices,"wishPriceTargetButton"); auto *target=window.findChild<QDialog *>("wishPriceTargetDialog"); QVERIFY(target);
    target->findChild<QDoubleSpinBox *>("wishTargetAmountSpinBox")->setValue(210); click(*target,"saveWishPriceTargetButton");
    QVERIFY(prices->findChild<QLabel *>("wishPriceSummaryLabel")->text().contains("210.00"));
}

void PlanningLayoutTests::coolingBadgeOnlyRestrictsPurchasing()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("cooling.sqlite")); QVERIFY(db.initialize()); WishlistRepository wishes(db);
    const auto now=QDateTime::currentDateTimeUtc(); WishItem wish; wish.name=QStringLiteral("通勤耳机"); wish.estimatedCents=30000; wish.coolingDays=1; QVERIFY(wishes.add(wish,now));
    WishlistWidget widget(db); AppTheme::apply(&widget,"warm"); widget.resize(740,520); widget.setDatabaseAvailable(true); widget.show(); settle();
    auto *table=widget.findChild<QTableWidget *>("wishlistTable"); table->selectRow(0); settle(); auto *buy=widget.findChild<QPushButton *>("purchaseWishButton");
    const auto order=ids(table); widget.refreshCountdowns(wish.readyAt.addMSecs(-1)); settle();
    QVERIFY(buy->isVisible() && !buy->isEnabled()); QVERIFY(table->cellWidget(0,0)->findChild<QWidget *>("productStateBadge")->property("cooling").toBool());
    QVERIFY(widget.findChild<QPushButton *>("wishPriceButton")->isEnabled()); QVERIFY(!widget.findChild<QLabel *>("wishProductDetailsLabel")->text().contains(QStringLiteral("剩余")));
    widget.refreshCountdowns(wish.readyAt); settle(); QVERIFY(buy->isEnabled()); QCOMPARE(ids(table),order);
    QVERIFY(!table->cellWidget(0,0)->findChild<QWidget *>("productStateBadge")->property("cooling").toBool());
    QCOMPARE(table->cellWidget(0,0)->findChild<QLabel *>("productStatusLabel")->text(),QStringLiteral("中优先 · 待决定"));
    QVERIFY(screenshot(widget,"ready")); widget.refreshCountdowns(now); QVERIFY(!buy->isEnabled());
}

void PlanningLayoutTests::manualOrderPersistsAndNewItemsAppend()
{
    QTemporaryDir dir; const auto path=dir.filePath("order.sqlite"); QList<qint64> expected;
    {
        DatabaseManager db(path); QVERIFY(db.initialize()); QVERIFY(populate(db));
        MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Wishlist); settle(); auto *table=window.findChild<QTableWidget *>("wishlistTable");
        const auto original=ids(table); QCOMPARE(original.size(),4); window.findChild<QAction *>("wishlistSortOldestAction")->trigger(); settle(); QCOMPARE(ids(table).first(),original.last());
        window.findChild<QAction *>("wishlistSortNewestAction")->trigger(); settle(); QCOMPARE(ids(table),original);
        window.findChild<QAction *>("wishlistAdjustOrderAction")->trigger(); settle(); auto *dialog=window.findChild<QDialog *>("wishlistOrderDialog"); QVERIFY(dialog);
        auto *list=dialog->findChild<QListWidget *>("wishlistOrderList"); QCOMPARE(list->dragDropMode(),QAbstractItemView::InternalMove); list->setCurrentRow(1); click(*dialog,"wishlistMoveUpButton");
        expected=original; expected.swapItemsAt(0,1); QVERIFY(screenshot(*dialog,"order-editor")); click(*dialog,"saveWishlistOrderButton"); QCOMPARE(ids(table),expected);
        const auto order=ids(table); window.findChild<WishlistWidget *>()->refreshCountdowns(QDateTime::currentDateTimeUtc().addDays(7)); settle(); QCOMPARE(ids(table),order);
        window.findChild<QAction *>("wishlistAdjustOrderAction")->trigger(); settle(); dialog=window.findChild<QDialog *>("wishlistOrderDialog"); click(*dialog,"wishlistMoveDownButton"); dialog->reject(); settle(); QCOMPARE(ids(table),expected);
    }
    MainWindow restarted(path); restarted.show(); restarted.showPage(MainWindow::Page::Wishlist); settle(); auto *table=restarted.findChild<QTableWidget *>("wishlistTable"); QCOMPARE(ids(table),expected);
    DatabaseManager db(path); QVERIFY(db.initialize()); WishlistRepository wishes(db); WishItem added; added.name=QStringLiteral("新加入的书包"); added.estimatedCents=15000; QVERIFY(wishes.add(added));
    QVERIFY(restarted.findChild<WishlistWidget *>()->refresh()); settle(); QCOMPARE(ids(table).last(),added.id);
}

void PlanningLayoutTests::historyAndSmallThemesFit()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("themes.sqlite")); QVERIFY(db.initialize()); QVERIFY(populate(db,12));
    MainWindow window(db.databasePath()); window.resize(800,600); window.show(); window.showPage(MainWindow::Page::Wishlist); settle();
    auto *table=window.findChild<QTableWidget *>("wishlistTable"); auto *scroll=window.findChild<QScrollArea *>("planningScrollArea"); table->selectRow(0); settle();
    for (const auto &theme:{"warm","cool","dark"}) { AppTheme::apply(&window,theme); settle(); QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); QVERIFY(scroll->verticalScrollBar()->maximum()>0); QVERIFY(screenshot(window,QStringLiteral("small-")+theme)); }
    click(window,"wishPriceButton"); auto *prices=window.findChild<WishPriceDialog *>(); QVERIFY(prices); QVERIFY(screenshot(*prices,"empty-price")); click(*prices,"closeWishPriceButton");
    WishlistRepository wishes(db); QList<WishItem> all; QVERIFY(wishes.findAll(all)); QVERIFY(wishes.abandon(all.first().id)); QVERIFY(window.findChild<WishlistWidget *>()->refresh());
    auto *filter=window.findChild<QComboBox *>("wishlistStatusFilter"); filter->setCurrentIndex(4); settle(); QCOMPARE(table->rowCount(),1); table->selectRow(0); settle();
    QVERIFY(!window.findChild<QPushButton *>("purchaseWishButton")->isVisible()); click(window,"wishPriceButton"); prices=window.findChild<WishPriceDialog *>(); QVERIFY(prices); QVERIFY(!prices->findChild<QPushButton *>("recordWishPriceButton")->isEnabled());
}

void PlanningLayoutTests::migrationLinksWithoutLosingHistoryAndBackup()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("migration.sqlite")); QVERIFY(db.initialize()); WishlistRepository wishes(db); WishItem wish; wish.name=QStringLiteral("同一件耳机"); wish.estimatedCents=30000; QVERIFY(wishes.add(wish));
    PriceRepository prices(db); qint64 track=0; QVERIFY(prices.intake(wish.name,"online_shopping",28000,"manual","legacy-price",track));
    UnifiedWishlistRepository catalog(db); QVERIFY(catalog.synchronize()); QVERIFY(catalog.synchronize()); QList<WishItem> all; QVERIFY(wishes.findAll(all)); QCOMPARE(all.size(),1);
    QHash<qint64,qint64> links,positions; QVERIFY(catalog.state(links,positions)); QCOMPARE(links.value(wish.id),track); QVERIFY(catalog.saveOrder({wish.id}));
    const auto deadline=all.first().readyAt; qint64 saved=0; QVERIFY(catalog.recordPrice(wish.id,25000,"manual","new-price",QDateTime::currentDateTimeUtc(),saved)); QCOMPARE(saved,track);
    QVERIFY(wishes.findAll(all)); QCOMPARE(all.first().readyAt,deadline); QList<PriceRecord> records; QVERIFY(prices.history(track,records)); QCOMPARE(records.size(),2);
    DatabaseBackupService backup(db); QString error,before; const auto path=dir.filePath("complete.sqlite"); QVERIFY2(backup.backup(path,error),qPrintable(error));
    QVERIFY(wishes.remove(wish.id)); QVERIFY(catalog.synchronize()); QVERIFY(wishes.findAll(all)); QVERIFY(all.isEmpty());
    BackupInfo info; QVERIFY2(backup.prepareRestore(path,info,error),qPrintable(error)); QVERIFY2(backup.restorePrepared(before,error),qPrintable(error));
    QVERIFY(catalog.state(links,positions)); QCOMPARE(links.value(wish.id),track); QCOMPARE(positions.value(wish.id),1); QVERIFY(prices.history(track,records)); QCOMPARE(records.size(),2);
    // A previous-version backup has the original three product tables, without the new list state tables.
    QSqlQuery query(db.database()); QVERIFY(query.exec("DROP TABLE wishlist_order")); QVERIFY(query.exec("DROP TABLE wishlist_price_links")); query.finish();
    const auto legacy=dir.filePath("legacy.sqlite"); QVERIFY2(backup.backup(legacy,error),qPrintable(error)); QVERIFY(db.initialize());
    QVERIFY2(backup.prepareRestore(legacy,info,error),qPrintable(error)); QVERIFY2(backup.restorePrepared(before,error),qPrintable(error)); QVERIFY(catalog.synchronize());
    QVERIFY(catalog.state(links,positions)); QCOMPARE(links.value(wish.id),track); QVERIFY(prices.history(track,records)); QCOMPARE(records.size(),2);
}

void PlanningLayoutTests::duplicateNamesDoNotSharePricesAndDeleteDoesNotReimport()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("duplicates.sqlite")); QVERIFY(db.initialize()); WishlistRepository wishes(db);
    WishItem a,b; a.name=b.name=QStringLiteral("同名但不同规格的鞋"); a.estimatedCents=b.estimatedCents=20000; QVERIFY(wishes.add(a)); QVERIFY(wishes.add(b));
    UnifiedWishlistRepository catalog(db); qint64 at=0,bt=0; const auto now=QDateTime::currentDateTimeUtc();
    QVERIFY(catalog.recordPrice(a.id,18000,"size 40","a-price",now,at)); QVERIFY(catalog.recordPrice(b.id,19000,"size 42","b-price",now,bt)); QVERIFY(at!=bt);
    QVERIFY(catalog.recordPrice(a.id,18000,"size 40","a-price",now,at)); PriceRepository prices(db); QList<PriceRecord> records; QVERIFY(prices.history(at,records)); QCOMPARE(records.size(),1);
    QVERIFY(!catalog.recordPrice(b.id,18000,"wrong retry","a-price",now,bt)); QVERIFY(prices.history(bt,records)); QCOMPARE(records.size(),1);
    QVERIFY(!catalog.saveOrder({a.id,a.id}));
    QVERIFY(wishes.remove(a.id)); QVERIFY(catalog.synchronize()); QList<WishItem> all; QVERIFY(wishes.findAll(all)); QCOMPARE(all.size(),1); QCOMPARE(all.first().id,b.id);
    QVERIFY(prices.history(at,records)); QCOMPARE(records.size(),1); QString error; QVERIFY2(ProductSchema::validate(db.database(),error),qPrintable(error));
}
void PlanningLayoutTests::archivedPricesDoNotInventAbandonmentDecisions()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("archived.sqlite")); QVERIFY(db.initialize());
    PriceRepository prices(db); qint64 track=0; QVERIFY(prices.intake(QStringLiteral("以前结束观察的相机"),"online_shopping",90000,"manual","archived-price",track)); QVERIFY(prices.archive(track));
    MonthlyReviewRepository review(db); MonthlyReviewData before,after; QVERIFY(review.load(QDate::currentDate(),before));
    UnifiedWishlistRepository catalog(db); QVERIFY(catalog.synchronize()); QVERIFY(review.load(QDate::currentDate(),after));
    QCOMPARE(after.abandonedCount,before.abandonedCount); QCOMPARE(after.factsHash,before.factsHash);
    WishlistWidget widget(db); widget.setDatabaseAvailable(true); widget.show(); widget.findChild<QComboBox *>("wishlistStatusFilter")->setCurrentIndex(4); settle();
    auto *table=widget.findChild<QTableWidget *>("wishlistTable"); QCOMPARE(table->rowCount(),1); QCOMPARE(table->cellWidget(0,0)->findChild<QLabel *>("productStatusLabel")->text(),QStringLiteral("已归档"));
}
QTEST_MAIN(PlanningLayoutTests)
#include "planning_layout_tests.moc"
