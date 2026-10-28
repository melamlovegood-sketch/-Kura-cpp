#include "AchievementRepository.h"
#include "AchievementWall.h"
#include "AchievementCelebration.h"
#include "mainwindow.h"
#include "AppTheme.h"
#include "BudgetRepository.h"
#include "DatabaseBackupService.h"
#include "ExecutionRepository.h"
#include "MonthlyReviewRepository.h"
#include "MonthlyReviewWidget.h"
#include "ShoppingProfileRepository.h"
#include "TransactionRepository.h"
#include "WishlistRepository.h"
#include "WishPoolRepository.h"
#include <QAbstractButton>
#include <QDateEdit>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QPainter>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QtTest>

namespace {
Achievement badge(const QList<Achievement> &items,const QString &key) { for(const auto &b:items)if(b.key==key)return b;return {}; }
bool expense(DatabaseManager &db,const QDate &day,qint64 *id=nullptr) {
    Transaction t(0,QStringLiteral("课程用书"),1200,QStringLiteral("学习"),day,{}, {},"study");
    const bool ok=TransactionRepository(db).add(t);if(id)*id=t.id();return ok;
}
WishItem wish(const QString &name) { WishItem w;w.name=name;w.estimatedCents=5000;w.detailCategory="online_shopping";w.coolingDays=1;return w; }
bool capture(DatabaseManager &db,QList<Achievement> &out) {return AchievementRepository(db).load(out);}
}

class AchievementTests:public QObject {
    Q_OBJECT
private slots:
    void emptyAndUnavailable();
    void failedCaptureKeepsPreviousSnapshot();
    void cumulativeDaysAndReviewsSurviveEdits();
    void coolingAndDistinctDecisions();
    void goalsBudgetPrinciplesAndBackupCompatibility();
    void wallLayoutThemesAndMonthScope();
    void notificationWaitsGroupsAndDoesNotReplay();
    void notificationRetriesFailedCapture();
    void realSaveShowsCelebrationAndCollection();
    void celebrationThemesAndAccessibleSizing();
};

void AchievementTests::emptyAndUnavailable()
{
    DatabaseManager db(":memory:");QList<Achievement> items=AchievementRepository::definitions();
    QVERIFY(!capture(db,items));QCOMPARE(items.size(),12);
    QVERIFY(db.initialize());QVERIFY(capture(db,items));
    for(const auto &b:items){QCOMPARE(b.progress,0);QVERIFY(!b.unlocked());}
    QSqlQuery query(db.database());QVERIFY(query.exec("SELECT COUNT(*) FROM achievement_marks"));QVERIFY(query.next());QCOMPARE(query.value(0).toInt(),0);
    AchievementWall wall(db);wall.setDatabaseAvailable(true);QVERIFY(wall.refresh());
    QCOMPARE(wall.findChild<QLabel *>("achievementCountLabel")->text(),QStringLiteral("已收藏 0 / 12 枚"));
    wall.setDatabaseAvailable(false);QVERIFY(!wall.refresh());QVERIFY(!wall.findChild<QAbstractButton *>("achievementBadge_first")->isEnabled());
    db.database().close();wall.setDatabaseAvailable(true);QVERIFY(!wall.refresh());QVERIFY(!wall.findChild<QLabel *>("achievementErrorLabel")->isHidden());
}

void AchievementTests::cumulativeDaysAndReviewsSurviveEdits()
{
    QTemporaryDir dir;const auto path=dir.filePath("collection.sqlite");
    {
        DatabaseManager db(path);QVERIFY(db.initialize());QList<Achievement> items;
        qint64 first=0;QVERIFY(expense(db,QDate::currentDate().addDays(-60),&first));
        QVERIFY(expense(db,QDate::currentDate().addDays(-60))); // One date, two records.
        QVERIFY(expense(db,QDate::currentDate().addDays(1))); // Future dates do not add days.
        MonthlyReviewRepository reviews(db);QVERIFY(reviews.setTag(first,-1,""));QVERIFY(capture(db,items));
        QCOMPARE(badge(items,"seven").progress,1);QVERIFY(!badge(items,"regret").unlocked());
        QVERIFY(reviews.setTag(first,-1,QStringLiteral("没有实际用途")));QVERIFY(capture(db,items));
        QVERIFY(badge(items,"regret").unlocked());QCOMPARE(badge(items,"review").progress,1);
        const auto unlocked=badge(items,"regret").unlockedAt;
        QVERIFY(reviews.setTag(first,1,QStringLiteral("后来用上了")));QVERIFY(capture(db,items));
        QVERIFY(badge(items,"worth").unlocked());QCOMPARE(badge(items,"review").progress,1);QCOMPARE(badge(items,"regret").unlockedAt,unlocked);
        QVERIFY(TransactionRepository(db).remove(first));
        QSqlQuery cleanup(db.database());QVERIFY(cleanup.exec("DELETE FROM transactions"));
        for(int i=1;i<30;++i){qint64 id;QVERIFY(expense(db,QDate::currentDate().addDays(-60+i),&id));
            if(i<10)QVERIFY(reviews.setTag(id,i%2?1:-1,QStringLiteral("认真记下使用感受")));
        }
        QVERIFY(capture(db,items));QVERIFY(badge(items,"seven").unlocked());QVERIFY(badge(items,"thirty").unlocked());QVERIFY(badge(items,"review").unlocked());
        // A later source deletion cannot take away a collected badge or its evidence.
        QSqlQuery query(db.database());QVERIFY(query.exec("DELETE FROM transaction_reviews"));QVERIFY(query.exec("DELETE FROM transactions"));
        QVERIFY(capture(db,items));QCOMPARE(badge(items,"thirty").progress,30);
    }
    DatabaseManager reopened(path);QVERIFY(reopened.initialize());QList<Achievement> items;QVERIFY(capture(reopened,items));
    QVERIFY(badge(items,"thirty").unlocked());QVERIFY(badge(items,"regret").unlocked());
}

void AchievementTests::failedCaptureKeepsPreviousSnapshot()
{
    DatabaseManager db(":memory:");QVERIFY(db.initialize());QList<Achievement> items;QVERIFY(capture(db,items));
    QVERIFY(expense(db,QDate::currentDate()));QSqlQuery q(db.database());
    QVERIFY(q.exec("CREATE TRIGGER mock_capture_failure BEFORE INSERT ON achievement_marks WHEN NEW.badge_key='seven' BEGIN SELECT RAISE(ABORT,'local mock failure'); END"));
    QVERIFY(!capture(db,items));QVERIFY(!badge(items,"first").unlocked());
    QVERIFY(q.exec("SELECT COUNT(*) FROM achievement_marks"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);q.finish();
    QVERIFY(q.exec("DROP TRIGGER mock_capture_failure"));QVERIFY(capture(db,items));QVERIFY(badge(items,"first").unlocked());
}

void AchievementTests::coolingAndDistinctDecisions()
{
    DatabaseManager db(":memory:");QVERIFY(db.initialize());WishlistRepository wishes(db);ExecutionRepository execution(db);
    const auto now=QDateTime::currentDateTimeUtc();QList<Achievement> items;
    auto early=wish(QStringLiteral("冲动小物"));QVERIFY(wishes.add(early,now));QVERIFY(wishes.abandon(early.id,now));
    QVERIFY(capture(db,items));QVERIFY(badge(items,"stash").unlocked());QVERIFY(!badge(items,"wait").unlocked());QCOMPARE(badge(items,"letgo").progress,1);
    for(int i=0;i<3;++i){ExecutionSession s;s.name=early.name;s.category=early.detailCategory;QVERIFY(execution.start(s,now));QVERIFY(execution.finish(s.id,"skipped",now));}
    WishPoolRepository savings(db);QVERIFY(savings.addSaving(5000,early.name+QStringLiteral(" · 已经有类似的了"),"same-product-home"));
    QVERIFY(capture(db,items));QCOMPARE(badge(items,"letgo").progress,1);
    auto late=wish(QStringLiteral("另一颗栗子"));QVERIFY(wishes.add(late,now.addDays(-2)));QVERIFY(wishes.abandon(late.id,now));
    QVERIFY(capture(db,items));QVERIFY(badge(items,"wait").unlocked());QCOMPARE(badge(items,"letgo").progress,2);
    QVERIFY(savings.addSaving(3000,QStringLiteral("没买的耳机 · 家里的还能用"),"new-product-home"));
    QVERIFY(capture(db,items));QCOMPARE(badge(items,"letgo").progress,3);
    for(int i=0;i<2;++i){ExecutionSession s;s.name=QStringLiteral("不同商品%1").arg(i);s.category="online_shopping";QVERIFY(execution.start(s,now));QVERIFY(execution.finish(s.id,"skipped",now));}
    QVERIFY(capture(db,items));QVERIFY(badge(items,"letgo").unlocked());
    DatabaseManager archived(":memory:");QVERIFY(archived.initialize());auto legacy=wish(QStringLiteral("历史归档"));
    WishlistRepository old(archived);QVERIFY(old.add(legacy,now.addDays(-2)));QVERIFY(old.abandon(legacy.id,now));
    QSqlQuery q(archived.database());q.prepare("INSERT INTO wishlist_price_links(id,wish_id,legacy_archived) VALUES(100,:wish,1)");q.bindValue(":wish",legacy.id);QVERIFY(q.exec());
    QVERIFY(capture(archived,items));QVERIFY(!badge(items,"stash").unlocked());QVERIFY(!badge(items,"wait").unlocked());QCOMPARE(badge(items,"letgo").progress,0);
}

void AchievementTests::goalsBudgetPrinciplesAndBackupCompatibility()
{
    QTemporaryDir dir;DatabaseManager db(dir.filePath("data.sqlite"));QVERIFY(db.initialize());QList<Achievement> items;
    BudgetRepository budgets(db);QVERIFY(budgets.saveSplit(QDate::currentDate(),10000,0));QVERIFY(capture(db,items));QVERIFY(!badge(items,"budget").unlocked());
    QVERIFY(budgets.saveSplit(QDate::currentDate(),10000,10000));
    ShoppingProfileRepository profile(db);QVERIFY(profile.saveRules({QStringLiteral("先想好放在哪里，再决定要不要买。")}));
    auto w=wish(QStringLiteral("心愿相机"));QVERIFY(WishlistRepository(db).add(w));QVERIFY(WishPoolRepository(db).setGoal(w.id,false));
    QVERIFY(capture(db,items));QVERIFY(badge(items,"wish").unlocked());QVERIFY(badge(items,"budget").unlocked());QVERIFY(badge(items,"principle").unlocked());
    const auto original=badge(items,"wish").unlockedAt;
    DatabaseBackupService backup(db);QString error;const auto saved=dir.filePath("saved.sqlite");QVERIFY2(backup.backup(saved,error),qPrintable(error));
    QSqlQuery query(db.database());QVERIFY(query.exec("DELETE FROM achievement_marks"));
    BackupInfo info;QVERIFY2(backup.prepareRestore(saved,info,error),qPrintable(error));QString safety;
    QVERIFY2(backup.restorePrepared(safety,error),qPrintable(error));QVERIFY(capture(db,items));QCOMPARE(badge(items,"wish").unlockedAt,original);
    // Older complete backups have no collection table; upgrade must add it safely.
    const auto oldPath=dir.filePath("old.sqlite");QVERIFY2(backup.backup(oldPath,error),qPrintable(error));
    {DatabaseManager old(oldPath);QVERIFY(old.initialize());QSqlQuery q(old.database());QVERIFY(q.exec("DROP TABLE achievement_marks"));}
    QVERIFY2(backup.prepareRestore(oldPath,info,error),qPrintable(error));QVERIFY2(backup.restorePrepared(safety,error),qPrintable(error));
    QVERIFY(capture(db,items));QVERIFY(badge(items,"wish").unlocked());
    // Reject unsupported evidence before touching live data.
    const auto corrupt=dir.filePath("invalid.sqlite");QVERIFY2(backup.backup(corrupt,error),qPrintable(error));
    {DatabaseManager invalid(corrupt);QVERIFY(invalid.initialize());QSqlQuery q(invalid.database());QVERIFY(q.exec("UPDATE achievement_marks SET badge_key='unknown' WHERE id=(SELECT MIN(id) FROM achievement_marks)"));}
    QVERIFY(!backup.prepareRestore(corrupt,info,error));QVERIFY(capture(db,items));QVERIFY(badge(items,"wish").unlocked());
}

void AchievementTests::wallLayoutThemesAndMonthScope()
{
    QTemporaryDir dir;DatabaseManager db(dir.filePath("ui.sqlite"));QVERIFY(db.initialize());qint64 id;
    QVERIFY(expense(db,QDate::currentDate().addMonths(-2),&id));QVERIFY(MonthlyReviewRepository(db).setTag(id,1,QStringLiteral("每天都用得上")));
    auto w=wish(QStringLiteral("心愿相机"));QVERIFY(WishlistRepository(db).add(w));QVERIFY(WishPoolRepository(db).setGoal(w.id,false));
    MonthlyReviewWidget review(db,dir.filePath("settings.ini"));review.resize(920,1600);review.setDatabaseAvailable(true);review.show();QTest::qWait(20);
    auto *wall=review.findChild<AchievementWall *>("achievementWall");QVERIFY(wall);
    const auto *boards=review.findChild<QFrame *>("monthlyReviewBoardsCard");QVERIFY(boards);QVERIFY(wall->y()>boards->y());
    auto *month=review.findChild<QDateEdit *>("monthlyReviewMonthEdit");month->setDate(QDate::currentDate().addYears(-1));
    QCOMPARE(wall->findChild<QLabel *>("achievementCountLabel")->text(),QStringLiteral("已收藏 4 / 12 枚"));
    auto *regret=wall->findChild<QAbstractButton *>("achievementBadge_regret");QVERIFY(regret);regret->click();
    QCOMPARE(wall->findChild<QLabel *>("achievementDetailTitle")->text(),QStringLiteral("酸栗也有收获"));
    QVERIFY(wall->findChild<QLabel *>("achievementConditionLabel")->text().contains(QStringLiteral("写下原因")));
    AchievementWall preview(db);preview.setDatabaseAvailable(true);QVERIFY(preview.refresh());
    const QString directory=qEnvironmentVariable("KURA_PREVIEW_DIR");if(!directory.isEmpty())QVERIFY(QDir().mkpath(directory));
    for(const auto &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}){
        AppTheme::apply(&preview,theme);
        for(int width:{880,640,420,320}){
            preview.resize(width,preview.sizeHint().height());preview.show();QTest::qWait(20);preview.layout()->activate();
            preview.resize(width,preview.heightForWidth(width));QTest::qWait(20);
            QCOMPARE(preview.width(),width);
            const auto buttons=preview.findChildren<QAbstractButton *>();QCOMPARE(buttons.size(),12);
            int firstRowCount=0;
            for(auto *b:buttons){
                QVERIFY(preview.rect().contains(b->geometry()));QCOMPARE(b->height(),112);
                if(b->y()==buttons.first()->y())++firstRowCount;
                auto titleFont=b->font();titleFont.setPixelSize(14);
                QVERIFY(QFontMetrics(titleFont).horizontalAdvance(b->accessibleName())<=b->width()-10);
            }
            QCOMPARE(firstRowCount,width==880?6:width==640?4:width==420?3:2);
            if(width==880)QVERIFY(preview.height()<440);
            if(!directory.isEmpty())QVERIFY(preview.grab().save(QDir(directory).filePath(QStringLiteral("achievement-%1-%2.png").arg(theme).arg(width))));
        }
    }
    preview.setProperty("fontScale",130);AppTheme::apply(&preview,"warm");
    preview.resize(420,preview.heightForWidth(420));QTest::qWait(20);
    auto *selected=preview.findChild<QAbstractButton *>("achievementBadge_regret");selected->click();QTest::qWait(20);
    preview.resize(420,preview.heightForWidth(420));QTest::qWait(20);
    auto font=selected->font();font.setPixelSize(qRound(14*1.3));
    QVERIFY(QFontMetrics(font).horizontalAdvance(QStringLiteral("酸栗也有收获"))<=selected->width()-10);
    QCOMPARE(preview.findChild<QAbstractButton *>("achievementBadge_first")->y(),preview.findChild<QAbstractButton *>("achievementBadge_seven")->y());
    QVERIFY(preview.findChild<QAbstractButton *>("achievementBadge_thirty")->y()>preview.findChild<QAbstractButton *>("achievementBadge_first")->y());
    if(!directory.isEmpty())QVERIFY(preview.grab().save(QDir(directory).filePath("achievement-warm-large-text.png")));
}
void AchievementTests::notificationWaitsGroupsAndDoesNotReplay()
{
    DatabaseManager db(":memory:");QVERIFY(db.initialize());
    QVERIFY(expense(db,QDate::currentDate())); // Existing achievement is a silent baseline.
    QWidget owner;owner.resize(1000,760);AppTheme::apply(&owner,"warm");owner.show();
    AchievementNotifier notifier(db,&owner);notifier.checkNow();
    QVERIFY(!owner.findChild<AchievementCelebration *>());
    QDialog editor(&owner);editor.setWindowModality(Qt::WindowModal);editor.open();QTest::qWait(30);
    WishlistRepository wishes(db);auto item=wish(QStringLiteral("书架"));QVERIFY(wishes.add(item));
    QVERIFY(ShoppingProfileRepository(db).saveRules({QStringLiteral("想清楚用途再买")}));
    // Another consumer can refresh the collection first without swallowing the notification.
    QList<Achievement> loaded;QVERIFY(capture(db,loaded));notifier.checkNow();
    QVERIFY(!owner.findChild<AchievementCelebration *>());
    editor.accept();QTest::qWait(30);notifier.checkNow();
    auto *dialog=owner.findChild<AchievementCelebration *>();QVERIFY(dialog);QVERIFY(dialog->isVisible());
    QCOMPARE(dialog->findChild<QLabel *>("achievementCelebrationHeading")->text(),QStringLiteral("这次，收获了 2 颗栗子"));
    QCOMPARE(dialog->findChild<QLabel *>("achievementCelebrationTitle")->text(),QStringLiteral("先藏再想"));
    QCOMPARE(dialog->findChild<QLabel *>("achievementExtraTitle")->text(),QStringLiteral("我的选栗法"));
    QTest::keyClick(dialog,Qt::Key_Escape);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
    notifier.checkNow();QVERIFY(!owner.findChild<AchievementCelebration *>());
    // A backup restoration resets queued and already known achievements silently.
    for(int day=1;day<7;++day) QVERIFY(expense(db,QDate::currentDate().addDays(-day)));
    QVERIFY(notifier.resetBaseline());notifier.checkNow();QVERIFY(!owner.findChild<AchievementCelebration *>());
    AchievementNotifier restarted(db,&owner);restarted.checkNow();QVERIFY(!owner.findChild<AchievementCelebration *>());
}

void AchievementTests::notificationRetriesFailedCapture()
{
    DatabaseManager db(":memory:");QVERIFY(db.initialize());
    QWidget owner;owner.resize(1000,760);AppTheme::apply(&owner,"warm");owner.show();
    AchievementNotifier notifier(db,&owner);
    QSqlQuery q(db.database());
    QVERIFY(q.exec("CREATE TRIGGER mock_notification_failure BEFORE INSERT ON achievement_marks BEGIN SELECT RAISE(ABORT,'mock failure'); END"));
    QVERIFY(expense(db,QDate::currentDate()));notifier.checkNow();QVERIFY(!owner.findChild<AchievementCelebration *>());
    QVERIFY(q.exec("DROP TRIGGER mock_notification_failure"));notifier.checkNow();
    auto *dialog=owner.findChild<AchievementCelebration *>();QVERIFY(dialog);
    QCOMPARE(dialog->findChild<QLabel *>("achievementCelebrationTitle")->text(),QStringLiteral("第一颗栗子"));
    QTest::mouseClick(dialog,Qt::LeftButton,Qt::NoModifier,QPoint(5,5));
    QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);notifier.checkNow();
    QVERIFY(!owner.findChild<AchievementCelebration *>());
}

void AchievementTests::realSaveShowsCelebrationAndCollection()
{
    QTemporaryDir dir;const auto path=dir.filePath("save.sqlite");
    {
        MainWindow window(path);window.setProperty("reducedMotion",true);
        window.showPage(MainWindow::Page::Billing);window.show();QTest::qWait(30);
        window.findChild<QPushButton *>("bookkeepingManualModeButton")->click();
        window.findChild<QLineEdit *>("bookkeepingNameEdit")->setText(QStringLiteral("午饭"));
        window.findChild<QDoubleSpinBox *>("bookkeepingAmountSpinBox")->setValue(15);
        window.findChild<QPushButton *>("saveBookkeepingManualButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<AchievementCelebration *>(),2000);
        auto *dialog=window.findChild<AchievementCelebration *>();
        QCOMPARE(dialog->findChild<QLabel *>("achievementCelebrationTitle")->text(),QStringLiteral("第一颗栗子"));
        const auto directory=qEnvironmentVariable("KURA_PREVIEW_DIR");
        if(!directory.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));auto preview=window.grab();
            {QPainter p(&preview);p.drawPixmap(0,0,dialog->grab());}
            QVERIFY(preview.save(QDir(directory).filePath("celebration-in-app.png")));
        }
        dialog->findChild<QPushButton *>("viewAchievementCollectionButton")->click();
        QCOMPARE(window.currentPage(),MainWindow::Page::Review);
        auto *wall=window.findChild<AchievementWall *>();QVERIFY(wall);
        auto *scroll=window.findChild<QScrollArea *>("monthlyReviewScrollArea");
        QVERIFY(scroll->viewport()->rect().intersects(QRect(wall->mapTo(scroll->viewport(),QPoint()),wall->size())));
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        window.showPage(MainWindow::Page::Home);window.findChild<AchievementNotifier *>()->checkNow();
        QVERIFY(!window.findChild<AchievementCelebration *>());
    }
    MainWindow reopened(path);reopened.show();reopened.findChild<AchievementNotifier *>()->checkNow();
    QVERIFY(!reopened.findChild<AchievementCelebration *>());
}

void AchievementTests::celebrationThemesAndAccessibleSizing()
{
    const auto directory=qEnvironmentVariable("KURA_PREVIEW_DIR");if(!directory.isEmpty()) QVERIFY(QDir().mkpath(directory));
    for(const QString &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) {
        for(int scale:{100,130}) {
            QWidget owner;owner.setProperty("fontScale",scale);owner.setProperty("reducedMotion",true);
            owner.resize(scale==130?800:1000,scale==130?600:760);AppTheme::apply(&owner,theme);owner.show();
            auto *dialog=new AchievementCelebration({AchievementRepository::definitions().first()},&owner);dialog->open();QTest::qWait(30);
            auto *card=dialog->findChild<QFrame *>("achievementCelebrationCard");QVERIFY(dialog->rect().contains(card->geometry()));
            auto *keep=dialog->findChild<QPushButton *>("keepAchievementButton");
            QVERIFY(card->rect().contains(keep->geometry()));QVERIFY(keep->isVisible());
            auto *scroll=dialog->findChild<QScrollArea *>("achievementCelebrationScroll");
            QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
            QVERIFY(!card->graphicsEffect()); // Reduced motion shows a fully visible card immediately.
            if(!directory.isEmpty()) QVERIFY(dialog->grab().save(QDir(directory).filePath(QStringLiteral("celebration-%1-%2.png").arg(theme).arg(scale))));
            dialog->findChild<QPushButton *>("keepAchievementButton")->click();
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
    }
    QWidget owner;owner.resize(800,600);owner.setProperty("fontScale",130);AppTheme::apply(&owner,"warm");owner.show();
    auto *batch=new AchievementCelebration(AchievementRepository::definitions(),&owner);batch->open();QTest::qWait(350);
    QVERIFY(batch->findChild<QScrollArea *>("achievementCelebrationScroll")->verticalScrollBar()->maximum()>0);
    QVERIFY(batch->findChild<QPushButton *>("keepAchievementButton")->isVisible());
    owner.resize(950,700);QTest::qWait(30);QCOMPARE(batch->size(),owner.size());
    if(!directory.isEmpty()) QVERIFY(batch->grab().save(QDir(directory).filePath("celebration-batch-large-text.png")));
    batch->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
}
QTEST_MAIN(AchievementTests)
#include "achievement_tests.moc"
