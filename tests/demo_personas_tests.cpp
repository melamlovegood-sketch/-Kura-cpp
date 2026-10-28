#include "DemoPersonas.h"
#include "DemoMode.h"
#include "BillImportRepository.h"
#include "BudgetRepository.h"
#include "DatabaseBackupService.h"
#include "ExecutionRepository.h"
#include "MonthlyReviewRepository.h"
#include "PricePerspective.h"
#include "PriceRepository.h"
#include "ProductRepository.h"
#include "ShoppingProfileRepository.h"
#include "SupplementSchema.h"
#include "TransactionRepository.h"
#include "WishPoolRepository.h"
#include "WishlistRepository.h"
#include "WorkflowSchema.h"
#include "BillingWorkspace.h"
#include "MonthlyReviewWidget.h"
#include "mainwindow.h"
#include <QApplication>
#include <QDateEdit>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

namespace {
using Persona=DemoPersonas::Persona;
const auto fixedNow=QDateTime::fromString(QStringLiteral("2026-10-28T08:00:00Z"),Qt::ISODate);
const QDate october(2026,10,1);
qint64 scalar(DatabaseManager &db,const QString &statement) {
    QSqlQuery q(db.database()); return q.exec(statement)&&q.next()?q.value(0).toLongLong():-1;
}
QString bill() { return QFINDTESTDATA("../examples/student-october-demo.xlsx"); }
bool importStudentBill(DatabaseManager &db,QString &error) {
    BillParseResult preview; BillImportRepository imports(db);
    if (!BillParser::parseFile(bill(),preview,error) || !imports.prepare(preview)) { if(error.isEmpty()) error=imports.lastError(); return false; }
    for(auto &row:preview.rows) {row.confirmed=true; if(row.state==BillDraft::NeedsReview) row.state=BillDraft::Ready;}
    BillImportOutcome outcome;
    if (!imports.commit(preview,outcome)) {error=imports.lastError(); return false;}
    return outcome.inserted==116;
}
}

class DemoPersonasTests:public QObject {
    Q_OBJECT
private slots:
    void studentHistoryAndLiveImport();
    void workerAndRelatedRecords();
    void emptyGuardAndRollback();
    void switchingAndCleanup();
    void calendarBoundaries();
    void actualDemoSwitchLoop();
    void profilesAndPreviews();
};

void DemoPersonasTests::studentHistoryAndLiveImport() {
    DatabaseManager db(":memory:"); QVERIFY(db.initialize()); QString error;
    QVERIFY2(DemoPersonas::populate(db,Persona::Student,error,fixedNow),qPrintable(error));
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM transactions WHERE date>='2026-10-01'"),0);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM bill_import_keys"),0);
    QCOMPARE(scalar(db,"SELECT COUNT(DISTINCT substr(date,1,7)) FROM transactions"),6);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM transactions WHERE date<'2026-04-01' OR date>'2026-09-30'"),0);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM (SELECT substr(date,1,7) m,COUNT(*) n FROM transactions WHERE purpose_category='clothing' GROUP BY m HAVING n BETWEEN 2 AND 3)"),6);
    QVERIFY(scalar(db,"SELECT COUNT(*) FROM transactions WHERE description LIKE 'MUJI%' OR description LIKE '优衣库%' ")>=17);
    QCOMPARE(scalar(db,"SELECT COUNT(DISTINCT budget_scope) FROM transactions WHERE purpose_category='food'"),2);
    QCOMPARE(scalar(db,"SELECT COUNT(DISTINCT budget_scope) FROM transactions WHERE purpose_category='travel'"),2);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM transactions t WHERE description LIKE '校园食堂%' AND EXISTS(SELECT 1 FROM transactions p WHERE p.description IN ('周边两日游往返车票','青旅住宿个人份额','旅行餐饮','展馆与景区门票') AND p.date=t.date)"),0);
    BudgetRepository budgets(db); MonthlyBudgetStatus status;
    QVERIFY(budgets.loadStatus(october,status)); QCOMPARE(status.limitCents,200000); QCOMPARE(status.spentCents,0);
    QCOMPARE(status.basicLimitCents,100000); QCOMPARE(status.discretionaryLimitCents,100000);
    QVERIFY2(importStudentBill(db,error),qPrintable(error));
    QVERIFY(budgets.loadStatus(october,status)); QCOMPARE(status.spentCents,169080);
    QCOMPARE(status.basicSpentCents,92370); QCOMPARE(status.discretionarySpentCents,76710); QCOMPARE(status.remainingCents(),30920);
    BillParseResult again; QVERIFY(BillParser::parseFile(bill(),again,error)); BillImportRepository imports(db);
    QVERIFY(imports.prepare(again));
    for(const auto &row:again.rows) {QCOMPARE(row.state,BillDraft::Duplicate); QVERIFY(!row.selected);}
    BillImportOutcome outcome; QVERIFY(imports.commit(again,outcome)); QCOMPARE(outcome.inserted,0);
    MonthlyReviewRepository monthly(db); MonthlyReviewData data; QVERIFY(monthly.load(october,data)); QCOMPARE(data.totalCents,169080);
    QVERIFY2(WorkflowSchema::validate(db.database(),error),qPrintable(error));
    QVERIFY2(SupplementSchema::validate(db.database(),error),qPrintable(error));
}

void DemoPersonasTests::workerAndRelatedRecords() {
    QTemporaryDir directory; DatabaseManager db(directory.filePath("worker.sqlite")); QVERIFY(db.initialize()); QString error;
    QVERIFY2(DemoPersonas::populate(db,Persona::Worker,error,fixedNow),qPrintable(error));
    const auto perspective=PricePerspectiveStore::load(PricePerspectiveStore::settingsPath(db.databasePath()));
    QCOMPARE(perspective.mode,PricePerspective::Worker); QCOMPARE(perspective.monthlyIncomeCents,1000000); QVERIFY(perspective.ready());
    QCOMPARE(scalar(db,"SELECT COUNT(DISTINCT substr(date,1,7)) FROM transactions"),7);
    QVERIFY(scalar(db,"SELECT COUNT(*) FROM transactions")>650);
    QVERIFY(scalar(db,"SELECT COUNT(*) FROM transactions WHERE description LIKE 'COS%' OR description LIKE 'ZARA%' ")>=16);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM transactions WHERE date>'2026-10-27'"),0);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM transactions t WHERE description='地铁通勤往返' AND EXISTS(SELECT 1 FROM transactions p WHERE p.description LIKE '周末旅行%' AND p.date=t.date)"),0);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM wishlist WHERE status=1 AND transaction_id NOT IN (SELECT id FROM transactions)"),0);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM execution_sessions WHERE state='active'"),0);
    QCOMPARE(scalar(db,"SELECT COUNT(DISTINCT state) FROM execution_sessions"),3);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM wishlist WHERE status=1"),2);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM wishlist WHERE status=2"),2);
    QVERIFY(scalar(db,"SELECT COUNT(*) FROM transaction_reviews WHERE tag=1")>=12);
    QVERIFY(scalar(db,"SELECT COUNT(*) FROM transaction_reviews WHERE tag=-1")>=4);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM monthly_stories"),7);
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM brands"),0);
    BudgetRepository budgets(db); MonthlyBudgetStatus status; QVERIFY(budgets.loadStatus(october,status));
    QCOMPARE(status.limitCents,800000); QCOMPARE(status.basicLimitCents,520000); QCOMPARE(status.discretionaryLimitCents,280000);
    QVERIFY(status.spentCents>400000); QVERIFY(status.spentCents<800000);
    WishlistRepository wishes(db); QList<WishItem> items; QVERIFY(wishes.findAll(items));
    int cooling=0,ready=0;
    for(const auto &item:items) if(item.status==WishStatus::Waiting) {if(item.canPurchase(fixedNow)) ++ready; else ++cooling;}
    QCOMPARE(cooling,1); QCOMPARE(ready,4);
    PriceRepository prices(db); QList<PriceTrack> tracks; QVERIFY(prices.load(tracks)); QCOMPARE(tracks.size(),3);
    int hits=0,drops=0; for(const auto &track:tracks) {hits+=track.targetHit; drops+=track.priceDrop;}
    QVERIFY(hits>=1); QVERIFY(drops>=2);
    WishPoolRepository pool(db); WishPoolSnapshot snapshot; QVERIFY(pool.load(snapshot));
    QCOMPARE(snapshot.savedCents,87900); QCOMPARE(snapshot.targetCents,89900); QCOMPARE(snapshot.pendingCents,0);
    ProductRepository products(db); QList<ProductProfile> profiles; QVERIFY(products.list(profiles)); QVERIFY(profiles.size()>=10);
    QList<ProductEvidence> evidence; QVERIFY(products.evidence(profiles[0].id,evidence)); QVERIFY(!evidence.isEmpty());
    QVERIFY2(ProductSchema::validate(db.database(),error),qPrintable(error));
    DatabaseBackupService backup(db); const auto copy=directory.filePath("worker-backup.sqlite"); QVERIFY2(backup.backup(copy,error),qPrintable(error));
    BackupInfo info; QVERIFY2(backup.prepareRestore(copy,info,error),qPrintable(error));
    QCOMPARE(info.expenseCount,scalar(db,"SELECT COUNT(*) FROM transactions")); QVERIFY(info.tagCount>0); QVERIFY(info.storyCount>=6);
}

void DemoPersonasTests::emptyGuardAndRollback() {
    DatabaseManager db(":memory:"); QVERIFY(db.initialize()); QString error;
    ProductRepository products(db); ProductProfile profile;
    QVERIFY(products.ensure({QString(),QStringLiteral("已存在的资料"),"other",0,1000,{}},profile));
    const auto before=scalar(db,"SELECT COUNT(*) FROM product_profiles");
    QVERIFY(!DemoPersonas::populate(db,Persona::Student,error,fixedNow));
    QCOMPARE(scalar(db,"SELECT COUNT(*) FROM product_profiles"),before); QCOMPARE(scalar(db,"SELECT COUNT(*) FROM transactions"),0);
    DatabaseManager fresh(":memory:"); QVERIFY(fresh.initialize());
    QSqlQuery trigger(fresh.database());
    QVERIFY(trigger.exec("CREATE TRIGGER reject_demo BEFORE INSERT ON transactions WHEN NEW.description LIKE 'MUJI%' BEGIN SELECT RAISE(ABORT,'seed fault'); END"));
    QVERIFY(!DemoPersonas::populate(fresh,Persona::Student,error,fixedNow));
    QCOMPARE(scalar(fresh,"SELECT COUNT(*) FROM transactions"),0); QCOMPARE(scalar(fresh,"SELECT COUNT(*) FROM monthly_budgets"),0);
    QCOMPARE(scalar(fresh,"SELECT COUNT(*) FROM transaction_reviews"),0);
    QVERIFY(trigger.exec("DROP TRIGGER reject_demo"));
    QVERIFY2(DemoPersonas::populate(fresh,Persona::Student,error,fixedNow),qPrintable(error));
    const auto count=scalar(fresh,"SELECT COUNT(*) FROM transactions");
    QVERIFY(!DemoPersonas::populate(fresh,Persona::Worker,error,fixedNow)); QCOMPARE(scalar(fresh,"SELECT COUNT(*) FROM transactions"),count);
}

void DemoPersonasTests::switchingAndCleanup() {
    QString student,worker,error;
    {
        DemoPersonas::Session session; student=session.open(Persona::Student,error,fixedNow); QVERIFY2(!student.isEmpty(),qPrintable(error));
        {
            DatabaseManager db(student); QVERIFY(db.initialize()); QVERIFY2(importStudentBill(db,error),qPrintable(error));
        }
        worker=session.open(Persona::Worker,error,fixedNow); QVERIFY2(!worker.isEmpty(),qPrintable(error)); QVERIFY(worker!=student);
        QCOMPARE(session.open(Persona::Student,error,fixedNow),student);
        {
            DatabaseManager db(student); QVERIFY(db.initialize()); QCOMPARE(scalar(db,"SELECT COUNT(*) FROM transactions WHERE date>='2026-10-01'"),116);
        }
        {
            DatabaseManager db(worker); QVERIFY(db.initialize()); QCOMPARE(scalar(db,"SELECT COUNT(*) FROM bill_import_keys"),0);
        }
    }
    QVERIFY(!QFile::exists(student)); QVERIFY(!QFile::exists(worker)); QVERIFY(!QFile::exists(student+".ui.ini"));
}

void DemoPersonasTests::calendarBoundaries() {
    for(const auto &iso:{QStringLiteral("2027-01-01T04:00:00Z"),QStringLiteral("2028-03-01T04:00:00Z")}) {
        DatabaseManager db(":memory:"); QVERIFY(db.initialize()); QString error;
        const auto now=QDateTime::fromString(iso,Qt::ISODate); const auto date=now.toLocalTime().date();
        QVERIFY2(DemoPersonas::populate(db,Persona::Student,error,now),qPrintable(error));
        QCOMPARE(scalar(db,"SELECT COUNT(DISTINCT substr(date,1,7)) FROM transactions"),6);
        QCOMPARE(scalar(db,"SELECT COUNT(*) FROM monthly_budgets"),7);
        QCOMPARE(scalar(db,QStringLiteral("SELECT COUNT(*) FROM transactions WHERE date>='%1'").arg(QDate(date.year(),date.month(),1).toString(Qt::ISODate))),0);
    }
}

void DemoPersonasTests::actualDemoSwitchLoop() {
    QStringList observed;
    QTimer probe,watchdog; watchdog.setSingleShot(true);
    connect(&watchdog,&QTimer::timeout,qApp,[] {qApp->exit(91);});
    connect(&probe,&QTimer::timeout,qApp,[&] {
        MainWindow *window=nullptr;
        for(auto *top:QApplication::topLevelWidgets()) if(top->isVisible()) if(auto *candidate=qobject_cast<MainWindow *>(top)) window=candidate;
        if(!window || !window->findChild<QPushButton *>("switchDemoProfileButton")) return;
        const bool student=window->windowTitle().contains(QStringLiteral("小满"));
        if((observed.size()==0 && student) || (observed.size()==1 && !student)) {
            observed<<window->windowTitle();
            const auto button=student?"demoWorkerButton":"demoStudentButton";
            QTimer::singleShot(0,[button] {
                for(auto *top:QApplication::topLevelWidgets()) if(top->objectName()=="demoProfileDialog")
                    if(auto *choose=top->findChild<QPushButton *>(button)) choose->click();
            });
            window->findChild<QPushButton *>("switchDemoProfileButton")->click();
        } else if(observed.size()==2 && student) {
            observed<<window->windowTitle(); window->close();
        }
    });
    watchdog.start(15000); probe.start(60);
    QCOMPARE(DemoMode::run(*qApp,"student"),0);
    probe.stop(); watchdog.stop(); QCOMPARE(observed.size(),3);
}

void DemoPersonasTests::profilesAndPreviews() {
    const QString output=qEnvironmentVariable("KURA_PERSONA_PREVIEW_DIR");
    if(!output.isEmpty()) QVERIFY(QDir().mkpath(output));
    DemoProfileDialog chooser; chooser.show(); QTest::qWait(20);
    if(!output.isEmpty()) QVERIFY(chooser.grab().save(output+"/choose-persona.png"));
    chooser.findChild<QPushButton *>("demoWorkerButton")->click(); QCOMPARE(chooser.result(),int(QDialog::Accepted)); QCOMPARE(chooser.persona(),Persona::Worker);
    DemoPersonas::Session session; QString error;
    for(auto persona:{Persona::Student,Persona::Worker}) {
        const auto path=session.open(persona,error,fixedNow); QVERIFY2(!path.isEmpty(),qPrintable(error));
        MainWindow window(path); window.resize(1180,860); window.show(); QTest::qWait(30);
        const auto name=DemoPersonas::key(persona);
        if(!output.isEmpty()) {
            QVERIFY(window.grab().save(output+"/"+name+"-home.png"));
            window.showPage(MainWindow::Page::Wishlist); QTest::qWait(20); QVERIFY(window.grab().save(output+"/"+name+"-wishlist.png"));
            window.showPage(MainWindow::Page::Billing); auto *billing=window.findChild<BillingWorkspace *>(); QVERIFY(billing); QVERIFY(billing->setMonth(october.addMonths(-1))); QTest::qWait(20);
            QVERIFY(window.grab().save(output+"/"+name+"-history.png"));
            window.showPage(MainWindow::Page::Review); auto *review=window.findChild<MonthlyReviewWidget *>(); QVERIFY(review);
            const auto dates=review->findChildren<QDateEdit *>(); QVERIFY(!dates.isEmpty()); dates.first()->setDate(october.addMonths(-1)); QTest::qWait(20);
            QVERIFY(window.grab().save(output+"/"+name+"-review.png"));
            DatabaseManager db(path); QVERIFY(db.initialize()); DatabaseBackupService backup(db);
            QVERIFY2(backup.backup(output+"/"+name+"-demo.sqlite",error),qPrintable(error));
            QJsonArray months; BudgetRepository budgets(db);
            for(int i=-6;i<=0;++i) {MonthlyBudgetStatus b; QVERIFY(budgets.loadStatus(october.addMonths(i),b)); months.append(QJsonObject{{"month",b.month.toString("yyyy-MM")},{"spentCents",b.spentCents},{"basicCents",b.basicSpentCents},{"freeCents",b.discretionarySpentCents}});}
            QFile summary(output+"/"+name+"-summary.json"); QVERIFY(summary.open(QIODevice::WriteOnly)); summary.write(QJsonDocument(months).toJson());
        }
    }
}
QTEST_MAIN(DemoPersonasTests)
#include "demo_personas_tests.moc"
