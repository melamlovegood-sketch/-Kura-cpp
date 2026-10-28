#include "AppTheme.h"
#include "BudgetRepository.h"
#include "ExecutionRepository.h"
#include "ExecutionWidget.h"
#include "DecisionChatWidget.h"
#include "DecisionDialog.h"
#include "ShoppingProfileRepository.h"
#include "TransactionRepository.h"
#include "WishlistRepository.h"
#include "ProductDetailsDialog.h"
#include "DatabaseBackupService.h"
#include "PurchaseRepository.h"
#include "UnifiedWishlistRepository.h"
#include "WishPoolRepository.h"
#include "WorkflowSchema.h"
#include "AddTransactionDialog.h"
#include "WishDialog.h"
#include "mainwindow.h"
#include <QCheckBox>
#include <QBuffer>
#include <QImage>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
template<class T> T *field(QObject &root,const char *name) { auto *value=root.findChild<T *>(QLatin1String(name)); Q_ASSERT(value); return value; }
qint64 count(DatabaseManager &db,const QString &table) { QSqlQuery q(db.database()); return q.exec("SELECT COUNT(*) FROM "+table)&&q.next()?q.value(0).toLongLong():-1; }
void shot(MainWindow &window,const QString &name) {
    settle(); const auto dir=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!dir.isEmpty()) { QVERIFY(QDir().mkpath(dir)); QVERIFY(window.grab().save(dir+"/execution-guided-"+name+".png")); }
}
void fill(MainWindow &window,const QString &name=QStringLiteral("苹果手机"),double amount=10006) {
    field<QLineEdit>(window,"executionNameEdit")->setText(name);
    field<QDoubleSpinBox>(window,"executionEstimatedAmountSpinBox")->setValue(amount);
    field<QLineEdit>(window,"executionPurposeEdit")->setText(QStringLiteral("拍照，记录生活"));
}
void next(MainWindow &window) {
    if (field<QWidget>(window,"executionInputCard")->isVisible()) field<QPushButton>(window,"executionNextButton")->click();
    else QTest::keyClick(field<QLineEdit>(window,"executionReviewNameEdit"),Qt::Key_Return);
    settle();
}
void start(MainWindow &window) { next(window); field<QPushButton>(window,"startExecutionButton")->click(); settle(); }
}

class ExecutionUiTests:public QObject {
    Q_OBJECT
private slots:
    void durationStepperKeepsTimerAndPreferences();
    void stepsAndRealBudget();
    void editsAndNotesSurviveRestart();
    void wishlistCancelAndSave();
    void wishlistCommitIsAtomic();
    void missingAndBasicBudgetsAndSmallWindow();
    void productOnlyEntryAndPrinciples();
    void modesSwitchWithoutRestartingSession();
    void expiredTimerNeverBuysAutomatically();
    void focusedInputSurvivesPurchaseFlowAndKeepsSharedEntry();
    void abandonPurchaseFromDecisionActions_data();
    void abandonPurchaseFromDecisionActions();
    void wishlistAmountCanBeTypedFromUnknownPrice_data();
    void wishlistAmountCanBeTypedFromUnknownPrice();
    void entryWorkspaceUsesRealWishesAndReflows();
    void readyWishDecision_data();
    void readyWishDecision();
    void wishDecisionRejectsStaleItemsAndRestoresLinks();
};

void ExecutionUiTests::entryWorkspaceUsesRealWishesAndReflows()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("workspace.sqlite")); QVERIFY(db.initialize());
    ExecutionWidget execution(db,QString()); execution.setProperty("page",true); execution.resize(1040,1000); AppTheme::apply(&execution,"warm"); execution.setDatabaseAvailable(true); execution.show(); settle();
    QVERIFY(field<QLabel>(execution,"executionPendingEmpty")->isVisible());
    auto examples=execution.findChildren<QPushButton *>();
    for (auto *button:examples) if (button->property("executionExample").toBool() && button->text()==QStringLiteral("衣服")) { button->click(); break; }
    QCOMPARE(field<QLineEdit>(execution,"executionNameEdit")->text(),QStringLiteral("一件针织衫"));
    WishlistRepository wishes(db); QList<WishItem> added; const auto now=QDateTime::currentDateTimeUtc();
    for (int i=0;i<5;++i) {
        WishItem wish; wish.name=QStringLiteral("想了很久的通勤针织开衫 %1").arg(i); wish.estimatedCents=29900+i*100; wish.coolingDays=3;
        QVERIFY(wishes.add(wish,now.addDays(-6+i))); added.append(wish);
    }
    QVERIFY(wishes.abandon(added[0].id));
    ShoppingProfileRepository profile(db); const QStringList rules={QStringLiteral("月生活费2000元，必要生活与自由消费各1000元，不挪用吃饭钱。"),QStringLiteral("超过200元先冷静3天；重复款先不买，先看看衣柜。"),QStringLiteral("喜欢不等于必须现在买，想一想会用多少次。"),QStringLiteral("符号 < > & 保留原样，不作为富文本。")}; QVERIFY(profile.saveRules(rules));
    QVERIFY(execution.refreshReferences()); settle();
    QCOMPARE(field<QLineEdit>(execution,"executionNameEdit")->text(),QStringLiteral("一件针织衫"));
    QVERIFY(!field<QLabel>(execution,"executionPendingEmpty")->isVisible());
    QCOMPARE(field<QLabel>(execution,"executionPendingSummary")->text(),QStringLiteral("3 件已结束冷静期"));
    auto rows=execution.findChildren<QWidget *>("executionPendingRow"); QCOMPARE(rows.size(),3);
    QCOMPARE(rows[0]->findChild<QLabel *>("executionPendingName")->text(),added[3].name);
    QVERIFY(rows[0]->findChild<QLabel *>("executionPendingDetail")->text().contains(QStringLiteral("冷静期已结束")));
    QVERIFY(rows[2]->findChild<QLabel *>("executionPendingDetail")->text().contains(QStringLiteral("冷静期已结束")));
    QSignalSpy navigation(&execution,&ExecutionWidget::wishlistRequested); field<QPushButton>(execution,"executionAllPendingButton")->click(); QCOMPARE(navigation.count(),1);
    QCOMPARE(rows[0]->findChild<QPushButton *>("executionPendingOpenButton")->text(),QStringLiteral("去决策 →"));
    QVERIFY(!execution.findChild<ProductDetailsDialog *>());
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    for (const auto &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) {
        execution.setProperty("fontScale",100); execution.resize(1040,1000); AppTheme::apply(&execution,theme); settle();
        auto *steps=field<QWidget>(execution,"executionSteps"),*rulesCard=field<QWidget>(execution,"executionRulesCard");
        QVERIFY(rulesCard->x()>steps->geometry().right());
        for (auto *label:execution.findChildren<QLabel *>()) if (label->isVisible() && label->wordWrap()) QVERIFY2(label->height()>=label->heightForWidth(label->width()),qPrintable(label->objectName()));
        if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(execution.grab().save(preview+"/execution-workspace-"+theme+".png")); }
    }
    execution.setProperty("fontScale",130); execution.resize(500,1600); AppTheme::apply(&execution,"warm"); settle();
    auto *steps=field<QWidget>(execution,"executionSteps"),*rulesCard=field<QWidget>(execution,"executionRulesCard"); QVERIFY(rulesCard->y()>steps->geometry().bottom());
    QVERIFY(execution.minimumSizeHint().width()<=500);
    if (!preview.isEmpty()) QVERIFY(execution.grab().save(preview+"/execution-workspace-compact.png"));
    execution.resize(1040,1000); execution.setProperty("fontScale",100); AppTheme::apply(&execution,"warm"); settle();
    field<QPushButton>(execution,"executionNextButton")->click(); settle(); QVERIFY(!field<QWidget>(execution,"executionPendingSection")->isVisible());
    field<QPushButton>(execution,"executionBackButton")->click(); settle(); QVERIFY(field<QWidget>(execution,"executionPendingSection")->isVisible());
    for (int i=1;i<added.size();++i) QVERIFY(wishes.abandon(added[i].id));
    QVERIFY(execution.refreshReferences()); settle(); QVERIFY(field<QLabel>(execution,"executionPendingEmpty")->isVisible());
    for (auto *row:rows) QVERIFY(!row->isVisible());
    QCOMPARE(count(db,"execution_sessions"),0); QCOMPARE(count(db,"transactions"),0);
    execution.setDatabaseAvailable(false); QVERIFY(!field<QPushButton>(execution,"executionAllPendingButton")->isEnabled());
}

void ExecutionUiTests::readyWishDecision_data()
{
    QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("outcome");
    for (const auto &mode:QStringList{"fast","research"}) for (const auto &outcome:QStringList{"wait","buy","skip"})
        QTest::newRow(qPrintable(mode+"-"+outcome)) << mode << outcome;
}

void ExecutionUiTests::readyWishDecision()
{
    QFETCH(QString,mode); QFETCH(QString,outcome);
    QTemporaryDir dir; const auto path=dir.filePath("ready-wish.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    WishlistRepository wishes(db); const auto now=QDateTime::currentDateTimeUtc();
    WishItem ready; ready.name=QStringLiteral("通勤耳机"); ready.estimatedCents=19900; ready.coolingDays=3; ready.priority=1; ready.reason=QStringLiteral("通勤听课"); ready.detailCategory="online_shopping";
    QImage image(20,20,QImage::Format_RGB32); image.fill(Qt::green); QBuffer buffer(&ready.sourceImage); buffer.open(QIODevice::WriteOnly); QVERIFY(image.save(&buffer,"PNG"));
    QVERIFY(wishes.add(ready,now.addDays(-5)));
    auto cooling=ready; cooling.id=0; cooling.name=QStringLiteral("还在冷静的耳机"); QVERIFY(wishes.add(cooling,now));
    WishPoolRepository pool(db); QVERIFY(pool.setGoal(ready.id));
    UnifiedWishlistRepository catalog(db); qint64 track=0; QVERIFY(catalog.recordPrice(ready.id,18900,"manual","ready-price",now,track));
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Execution); settle();
    auto *execution=field<ExecutionWidget>(window,"executionWidget");
    QCOMPARE(field<QLabel>(*execution,"executionPendingSummary")->text(),QStringLiteral("1 件已结束冷静期"));
    auto rows=execution->findChildren<QWidget *>("executionPendingRow"); QVERIFY(rows[0]->isVisible()); QVERIFY(!rows[1]->isVisible());
    rows[0]->findChild<QPushButton *>("executionPendingOpenButton")->click(); settle();
    QVERIFY(!field<QWidget>(*execution,"executionInputCard")->isVisible());
    QVERIFY(field<QPushButton>(*execution,"startExecutionButton")->isVisible());
    QCOMPARE(field<QLineEdit>(*execution,"executionNameEdit")->text(),ready.name);
    QCOMPARE(field<QDoubleSpinBox>(*execution,"executionEstimatedAmountSpinBox")->value(),199.0);
    QCOMPARE(field<QLineEdit>(*execution,"executionPurposeEdit")->text(),ready.reason);
    QCOMPARE(count(db,"execution_sessions"),0); QCOMPARE(count(db,"transactions"),0); QVERIFY(!execution->findChild<ProductDetailsDialog *>());
    field<QComboBox>(*execution,"executionModeComboBox")->setCurrentIndex(mode=="fast"?0:1);
    field<QPushButton>(*execution,"startExecutionButton")->click(); settle();
    ExecutionRepository sessions(db); ExecutionSession active; QVERIFY(sessions.loadActive(active)); QCOMPARE(active.wishId,ready.id); QVERIFY(active.id>0);
    rows[0]->findChild<QPushButton *>("executionPendingOpenButton")->click(); QCOMPARE(execution->activeSessionId(),active.id);
    // The original item association and evidence must survive a restart.
    QVERIFY(execution->reloadState()); QCOMPARE(execution->activeSessionId(),active.id);
    QVERIFY(field<QPushButton>(*execution,"executionWishButton")->text().contains(QStringLiteral("继续等等")));
    if (mode=="fast") field<QPushButton>(*execution,"executionFastDecideButton")->click();
    if (outcome=="wait") {
        field<QPushButton>(*execution,"executionWishButton")->click(); settle(); QVERIFY(!window.findChild<WishDialog *>());
    } else if (outcome=="skip") {
        field<QPushButton>(*execution,"skipExecutionButton")->click(); settle();
    } else {
        field<QPushButton>(*execution,"executionBuyButton")->click(); settle();
        auto *confirmation=window.findChild<AddTransactionDialog *>(); QVERIFY(confirmation);
        QCOMPARE(confirmation->transaction().sourceImage(),ready.sourceImage);
        confirmation->findChild<QPushButton *>("saveButton")->click(); settle();
        QVERIFY(!window.findChild<AddTransactionDialog *>());
    }
    QCOMPARE(count(db,"wishlist"),2); QCOMPARE(count(db,"transactions"),outcome=="buy"?1:0);
    QVERIFY(sessions.loadActive(active)); QCOMPARE(active.id,0);
    QList<WishItem> saved; QVERIFY(wishes.findAll(saved));
    for (const auto &wish:saved) if (wish.id==ready.id) {
        QCOMPARE(wish.status,outcome=="buy"?WishStatus::Purchased:outcome=="skip"?WishStatus::Abandoned:WishStatus::Waiting);
        QCOMPARE(wish.readyAt,ready.readyAt); QCOMPARE(wish.priority,1);
        if (outcome=="buy") QVERIFY(wish.transactionId>0);
    }
    WishPoolSnapshot goal; QVERIFY(pool.load(goal)); QCOMPARE(goal.wishId,outcome=="wait"?ready.id:0);
    QString error; QVERIFY2(WorkflowSchema::validate(db.database(),error),qPrintable(error));
}

void ExecutionUiTests::wishDecisionRejectsStaleItemsAndRestoresLinks()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("stale-wish.sqlite")); QVERIFY(db.initialize()); WishlistRepository wishes(db);
    WishItem wish; wish.name=QStringLiteral("到期的背包"); wish.estimatedCents=5000; wish.coolingDays=1;
    QVERIFY(wishes.add(wish,QDateTime::currentDateTimeUtc().addDays(-3)));
    ExecutionWidget execution(db,QString()); execution.resize(1040,1000); execution.setDatabaseAvailable(true); execution.show(); settle();
    auto *open=field<QPushButton>(execution,"executionPendingOpenButton"); open->click(); settle();
    QVERIFY(wishes.update(wish)); // Re-starting cooling after entry must prevent Start.
    field<QPushButton>(execution,"startExecutionButton")->click(); QCOMPARE(count(db,"execution_sessions"),0);
    field<QPushButton>(execution,"executionBackButton")->click(); settle();
    open->setProperty("wishId",wish.id); open->click(); QCOMPARE(count(db,"execution_sessions"),0);
    QVERIFY(field<QWidget>(execution,"executionInputCard")->isVisible());
    QVERIFY(field<QLabel>(execution,"executionPendingEmpty")->isVisible());
    ExecutionRepository sessions(db); ExecutionSession cold; cold.name=wish.name; cold.category="online_shopping"; cold.wishId=wish.id;
    QVERIFY(!sessions.start(cold)); QCOMPARE(cold.id,0);
    QSqlQuery query(db.database()); QVERIFY(query.exec("UPDATE wishlist SET ready_ms=created_ms"));
    QVERIFY(execution.refreshReferences()); open->click(); field<QComboBox>(execution,"executionModeComboBox")->setCurrentIndex(1);
    field<QPushButton>(execution,"startExecutionButton")->click();
    ExecutionSession active; QVERIFY(sessions.loadActive(active)); QCOMPARE(active.wishId,wish.id);
    ExecutionSession conflict; conflict.name="other"; conflict.category="other"; QVERIFY(!sessions.start(conflict));
    auto duplicate=wish; duplicate.id=0; QVERIFY(!sessions.deferToWishlist(active.id,duplicate)); QCOMPARE(count(db,"wishlist"),1);
    DatabaseBackupService backups(db); QString error,safety; BackupInfo info; const auto copy=dir.filePath("with-link.sqlite");
    QVERIFY2(backups.backup(copy,error),qPrintable(error)); QVERIFY(sessions.finish(active.id,"undecided"));
    QVERIFY2(backups.prepareRestore(copy,info,error),qPrintable(error)); QVERIFY2(backups.restorePrepared(safety,error),qPrintable(error));
    QVERIFY(sessions.loadActive(active)); QCOMPARE(active.wishId,wish.id);
    QVERIFY(query.exec("CREATE TRIGGER fail_linked_skip BEFORE UPDATE OF status ON wishlist WHEN NEW.status=2 BEGIN SELECT RAISE(ABORT,'test'); END"));
    QVERIFY(!sessions.finish(active.id,"skipped")); ExecutionSession stillActive; QVERIFY(sessions.loadActive(stillActive)); QCOMPARE(stillActive.id,active.id);
    QVERIFY(query.exec("DROP TRIGGER fail_linked_skip"));
    QVERIFY(query.exec("CREATE TRIGGER fail_linked_purchase BEFORE INSERT ON transactions BEGIN SELECT RAISE(ABORT,'test'); END"));
    PurchaseRepository purchases(db); Transaction expense(0,wish.name,4500,QStringLiteral("购物"),QDate::currentDate());
    QVERIFY(!purchases.record(expense,0,active.id)); QCOMPARE(expense.id(),0); QCOMPARE(count(db,"transactions"),0);
    ExecutionSession retry; QVERIFY(sessions.loadActive(retry)); QCOMPARE(retry.id,active.id);
    QList<WishItem> saved; QVERIFY(wishes.findAll(saved)); QCOMPARE(saved.first().status,WishStatus::Waiting);
    QVERIFY(query.exec("DROP TRIGGER fail_linked_purchase")); QVERIFY(purchases.record(expense,0,active.id));
    auto second=Transaction(0,wish.name,4500,QStringLiteral("购物"),QDate::currentDate()); QVERIFY(!purchases.record(second,0,active.id)); QCOMPARE(count(db,"transactions"),1);
    QVERIFY(wishes.findAll(saved)); QCOMPARE(saved.first().status,WishStatus::Purchased);
    // Old backups without the new association table remain restorable.
    QVERIFY(query.exec("DROP TABLE execution_wishes")); const auto legacy=dir.filePath("legacy.sqlite"); QVERIFY(backups.backup(legacy,error)); QVERIFY(db.initialize());
    QVERIFY2(backups.prepareRestore(legacy,info,error),qPrintable(error)); QVERIFY2(backups.restorePrepared(safety,error),qPrintable(error)); QCOMPARE(count(db,"execution_wishes"),0);
}

void ExecutionUiTests::durationStepperKeepsTimerAndPreferences()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("duration.sqlite")); QVERIFY(db.initialize());
    const auto settings=dir.filePath("preferences.ini");
    ExecutionWidget execution(db,settings); execution.setDatabaseAvailable(true); execution.resize(620,1000);
    AppTheme::apply(&execution,"warm"); execution.show(); settle();
    field<QLineEdit>(execution,"executionNameEdit")->setText(QStringLiteral("围巾"));
    field<QPushButton>(execution,"executionNextButton")->click(); settle();
    auto *minutes=field<QSpinBox>(execution,"executionMinutesSpinBox");
    auto *less=field<QToolButton>(execution,"executionMinutesDecreaseButton");
    auto *more=field<QToolButton>(execution,"executionMinutesIncreaseButton");
    QVERIFY(minutes->isVisible()); const int initial=minutes->value();
    QTest::mouseClick(more,Qt::LeftButton); QCOMPARE(minutes->value(),initial+1);
    QTest::mouseClick(less,Qt::LeftButton); QCOMPARE(minutes->value(),initial);
    minutes->setValue(1); QVERIFY(!less->isEnabled()); QVERIFY(more->isEnabled());
    QTest::mouseClick(more,Qt::LeftButton); QCOMPARE(minutes->value(),2); QVERIFY(less->isEnabled());
    minutes->setValue(120); QVERIFY(!more->isEnabled());
    QTest::mouseClick(less,Qt::LeftButton); QCOMPARE(minutes->value(),119); QVERIFY(more->isEnabled());
    minutes->setFocus(); minutes->selectAll(); QTest::keyClicks(minutes,"3"); QTest::keyClick(minutes,Qt::Key_Tab);
    QCOMPARE(minutes->value(),3);
    QVERIFY(field<QPushButton>(execution,"executionFastChoiceButton")->text().contains(QStringLiteral("3 分钟")));
    if (auto *focused=QApplication::focusWidget()) focused->clearFocus();
    for (const auto &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) {
        AppTheme::apply(&execution,theme); settle();
        auto *card=field<QWidget>(execution,"executionModeCard");
        const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
        if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(card->grab().save(preview+"/duration-stepper-"+theme+".png")); }
    }
    field<QPushButton>(execution,"executionResearchChoiceButton")->click(); QVERIFY(!minutes->isVisible());
    field<QPushButton>(execution,"executionFastChoiceButton")->click(); QVERIFY(minutes->isVisible()); QCOMPARE(minutes->value(),3);
    execution.setDatabaseAvailable(false); QVERIFY(!less->isEnabled()); QVERIFY(!more->isEnabled());
    execution.setDatabaseAvailable(true);
    field<QLineEdit>(execution,"executionNameEdit")->setText(QStringLiteral("围巾"));
    field<QPushButton>(execution,"executionNextButton")->click();
    field<QPushButton>(execution,"startExecutionButton")->click(); settle();
    QVERIFY(execution.activeSessionId()>0); QVERIFY(!less->isEnabled()); QVERIFY(!more->isEnabled());
    ExecutionRepository repository(db); ExecutionSession active; QVERIFY(repository.loadActive(active)); QCOMPARE(active.durationSeconds,180);
    ExecutionWidget restored(db,settings); QCOMPARE(field<QSpinBox>(restored,"executionMinutesSpinBox")->value(),3);
}

void ExecutionUiTests::wishlistAmountCanBeTypedFromUnknownPrice_data()
{
    QTest::addColumn<QString>("mode"); QTest::addColumn<bool>("submitEmpty");
    QTest::newRow("fast-first-input") << QStringLiteral("fast") << false;
    QTest::newRow("fast-after-validation") << QStringLiteral("fast") << true;
    QTest::newRow("research-first-input") << QStringLiteral("research") << false;
    QTest::newRow("research-after-validation") << QStringLiteral("research") << true;
}

void ExecutionUiTests::wishlistAmountCanBeTypedFromUnknownPrice()
{
    QFETCH(QString,mode); QFETCH(bool,submitEmpty);
    QTemporaryDir dir; const auto path=dir.filePath("typed-wish.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Execution);
    field<QLineEdit>(window,"executionNameEdit")->setText(QStringLiteral("沐浴露")); next(window);
    field<QComboBox>(window,"executionModeComboBox")->setCurrentIndex(mode=="fast"?0:1);
    field<QPushButton>(window,"startExecutionButton")->click(); settle();
    if (mode=="fast") field<QPushButton>(window,"executionFastDecideButton")->click();
    field<QPushButton>(window,"executionWishButton")->click(); settle();
    auto *dialog=field<WishDialog>(window,"wishDialog");
    auto *amount=field<QDoubleSpinBox>(*dialog,"wishAmountSpinBox"); QCOMPARE(amount->value(),0.0);
    if (submitEmpty) {
        field<QPushButton>(*dialog,"saveWishButton")->click(); settle();
        QVERIFY(dialog->isVisible()); QCOMPARE(count(db,"wishlist"),0);
    }
    // Reproduce an ordinary click and typing, without setValue or selecting the hint.
    auto *editor=amount->findChild<QLineEdit *>(); QVERIFY(editor);
    QTest::mouseClick(editor,Qt::LeftButton); settle(); QTest::keyClicks(editor,"29.90");
    QTest::keyClick(editor,Qt::Key_Tab); settle(); QCOMPARE(amount->value(),29.90);
    field<QPushButton>(*dialog,"wishPriority1Button")->click();
    field<QPushButton>(*dialog,"saveWishButton")->click(); settle();
    QCOMPARE(count(db,"wishlist"),1); QCOMPARE(count(db,"transactions"),0);
    QCOMPARE(field<ExecutionWidget>(window,"executionWidget")->activeSessionId(),0);
    QSqlQuery query(db.database()); QVERIFY(query.exec("SELECT name,estimated_cents FROM wishlist")); QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(),QStringLiteral("沐浴露")); QCOMPARE(query.value(1).toLongLong(),2990);
}

void ExecutionUiTests::abandonPurchaseFromDecisionActions_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<double>("amount");
    QTest::addColumn<int>("width");
    QTest::newRow("research-known-wide") << QStringLiteral("research") << 99.0 << 1000;
    QTest::newRow("research-unknown-narrow") << QStringLiteral("research") << 0.0 << 500;
    QTest::newRow("fast-known-narrow") << QStringLiteral("fast") << 99.0 << 500;
    QTest::newRow("fast-unknown-wide") << QStringLiteral("fast") << 0.0 << 1000;
}

void ExecutionUiTests::abandonPurchaseFromDecisionActions()
{
    QFETCH(QString,mode); QFETCH(double,amount); QFETCH(int,width);
    QTemporaryDir dir; DatabaseManager db(dir.filePath("abandon.sqlite")); QVERIFY(db.initialize());
    ExecutionWidget execution(db,QString()); AppTheme::apply(&execution,"warm"); execution.setDatabaseAvailable(true);
    execution.resize(width,1200); execution.show(); settle();
    field<QLineEdit>(execution,"executionNameEdit")->setText(QStringLiteral("沐浴露"));
    field<QDoubleSpinBox>(execution,"executionEstimatedAmountSpinBox")->setValue(amount);
    field<QPushButton>(execution,"executionNextButton")->click();
    field<QComboBox>(execution,"executionModeComboBox")->setCurrentIndex(mode=="fast"?0:1);
    field<QPushButton>(execution,"startExecutionButton")->click(); settle();
    const auto sessionId=execution.activeSessionId(); QVERIFY(sessionId>0);
    auto *abandon=field<QPushButton>(execution,"skipExecutionButton");
    auto *wish=field<QPushButton>(execution,"executionWishButton");
    auto *buy=field<QPushButton>(execution,"executionBuyButton");
    if (mode=="fast") {
        QVERIFY(!abandon->isVisible());
        field<QPushButton>(execution,"executionFastDecideButton")->click(); settle();
    }
    for (auto *button:{wish,buy,abandon}) { QVERIFY(button->isVisible()); QVERIFY(button->isEnabled()); }
    // All three outcomes must be in the same action group, above the references.
    QCOMPARE(abandon->parentWidget(),wish->parentWidget()); QCOMPARE(abandon->parentWidget(),buy->parentWidget());
    if (width<540) {
        QVERIFY(wish->geometry().bottom()<buy->geometry().top());
        QVERIFY(buy->geometry().bottom()<abandon->geometry().top());
    } else {
        QCOMPARE(abandon->geometry().top(),buy->geometry().top());
        QVERIFY(buy->geometry().right()<abandon->geometry().left());
    }
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        QVERIFY(QDir().mkpath(preview));
        QVERIFY(abandon->parentWidget()->grab().save(preview+"/decision-actions-"+QLatin1String(QTest::currentDataTag())+".png"));
    }
    field<QPlainTextEdit>(execution,"executionNotesEdit")->setPlainText(QStringLiteral("家里还有，决定不买。"));
    abandon->click(); settle();
    QCOMPARE(execution.activeSessionId(),0);
    QVERIFY(field<QLabel>(execution,"executionStatusLabel")->text().contains(QStringLiteral("已记录放弃购买")));
    QCOMPARE(count(db,"transactions"),0); QCOMPARE(count(db,"wishlist"),0);
    QCOMPARE(count(db,"execution_sessions"),1);
    QSqlQuery query(db.database()); query.prepare("SELECT state,notes,closed_ms FROM execution_sessions WHERE id=:id"); query.bindValue(":id",sessionId);
    QVERIFY(query.exec()); QVERIFY(query.next()); QCOMPARE(query.value(0).toString(),QStringLiteral("skipped"));
    QVERIFY(QJsonDocument::fromJson(query.value(1).toString().toUtf8())["text"].toString().contains(QStringLiteral("决定不买")));
    QVERIFY(query.value(2).toLongLong()>0); query.finish();
    QVERIFY(execution.reloadState()); QCOMPARE(execution.activeSessionId(),0);
    QVERIFY(!abandon->isVisible());
}

void ExecutionUiTests::focusedInputSurvivesPurchaseFlowAndKeepsSharedEntry()
{
    QTemporaryDir dir; const auto path=dir.filePath("focused-input.sqlite");
    DatabaseManager db(path); QVERIFY(db.initialize());
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Execution); settle();
    auto *name=field<QLineEdit>(window,"executionNameEdit");
    QVERIFY2(name->isVisible(),"The refined execution entry must ask what to buy directly, before opening a purchase dialog.");
    auto *nextButton=field<QPushButton>(window,"executionNextButton");
    QVERIFY(!nextButton->isEnabled());
    name->setText(QStringLiteral("耳机")); QVERIFY(nextButton->isEnabled());
    QTest::keyClick(name,Qt::Key_Return); settle();
    QVERIFY(field<QWidget>(window,"executionModeCard")->isVisible());
    QVERIFY(field<QPushButton>(window,"executionResearchChoiceButton")->isChecked());
    QVERIFY(!window.findChild<DecisionDialog *>());
    QCOMPARE(count(db,"execution_sessions"),0); QCOMPARE(count(db,"transactions"),0);
    auto *reviewName=field<QLineEdit>(window,"executionReviewNameEdit");
    QCOMPARE(reviewName->text(),name->text()); reviewName->setText(QStringLiteral("通勤耳机"));
    field<QPushButton>(window,"executionBackButton")->click(); settle();
    QVERIFY(name->isVisible()); QCOMPARE(name->text(),QStringLiteral("通勤耳机"));
    field<QPushButton>(window,"executionInputButton")->click(); settle();
    auto *dialog=window.findChild<DecisionDialog *>(); QVERIFY(dialog && dialog->isVisible());
    QCOMPARE(field<QPlainTextEdit>(*dialog,"decisionInput")->toPlainText(),QStringLiteral("通勤耳机"));
    dialog->reject(); settle();
    QCOMPARE(name->text(),QStringLiteral("通勤耳机"));
    QCOMPARE(count(db,"execution_sessions"),0); QCOMPARE(count(db,"transactions"),0);
}

void ExecutionUiTests::stepsAndRealBudget()
{
    QTemporaryDir dir; const auto path=dir.filePath("steps.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    BudgetRepository budget(db); QVERIFY(budget.saveSplit(QDate::currentDate(),50000,100000));
    MainWindow window(path); window.resize(1440,1040); window.show(); window.showPage(MainWindow::Page::Execution); settle();
    auto *steps=field<QWidget>(window,"executionSteps"); QVERIFY(field<QWidget>(window,"executionInputCard")->isVisible());
    QVERIFY(!window.findChild<QPushButton *>("executionStep0Button"));
    QVERIFY(field<QPushButton>(window,"executionInputButton")->isEnabled());
    QVERIFY(field<QLabel>(window,"executionRulesLabel")->isVisible());
    QVERIFY(!field<QDoubleSpinBox>(window,"executionEstimatedAmountSpinBox")->isVisible());
    QVERIFY(!field<QLabel>(window,"executionTimerLabel")->isVisible());
    QVERIFY(!field<QWidget>(window,"executionActiveModes")->isVisible());
    shot(window,"setup-empty");
    fill(window); settle(); QVERIFY2(steps->geometry().top()<190,"Hidden steps must not add space above the input card");
    shot(window,"input"); next(window); QVERIFY(field<QWidget>(window,"executionModeCard")->isVisible()); shot(window,"mode");
    field<QPushButton>(window,"startExecutionButton")->click(); settle(); QVERIFY(field<QWidget>(window,"executionDecisionCard")->isVisible());
    auto *execution=field<ExecutionWidget>(window,"executionWidget"); QVERIFY(execution->activeSessionId()>0);
    QCOMPARE(field<QLabel>(window,"executionBudgetRemainingLabel")->text(),QStringLiteral("¥1,000.00"));
    QCOMPARE(field<QLabel>(window,"executionBudgetAfterLabel")->text(),QStringLiteral("−¥9,006.00"));
    QVERIFY(field<QLabel>(window,"executionBudgetTitleLabel")->text().contains(QStringLiteral("预算不足")));
    QVERIFY(!field<QPlainTextEdit>(window,"executionNotesEdit")->isVisible());
    QVERIFY(field<DecisionChatWidget>(window,"decisionChatWidget")->plainText().isEmpty());
    QVERIFY(field<QPushButton>(window,"executionWishButton")->isVisible());
    QVERIFY(field<QPushButton>(window,"executionBuyButton")->isVisible());
    field<QCheckBox>(window,"researchCheck0")->setChecked(true);
    QCOMPARE(field<QLabel>(window,"executionCheckCountLabel")->text(),QStringLiteral("1 / 3"));
    shot(window,"decision");
    field<QPushButton>(window,"executionEditProductButton")->click(); QVERIFY(field<QWidget>(window,"executionBriefPage")->isVisible());
    field<QDoubleSpinBox>(window,"executionEstimatedAmountSpinBox")->setValue(800); next(window);
    field<QPushButton>(window,"startExecutionButton")->click(); settle();
    QVERIFY(field<QWidget>(window,"executionDecisionCard")->isVisible()); QCOMPARE(field<QLabel>(window,"executionBudgetAfterLabel")->text(),QStringLiteral("¥200.00"));
    QVERIFY(field<QLabel>(window,"executionBudgetTitleLabel")->text().contains(QStringLiteral("预算内")));
    QCOMPARE(count(db,"transactions"),0); QCOMPARE(count(db,"execution_sessions"),1);
}

void ExecutionUiTests::editsAndNotesSurviveRestart()
{
    QTemporaryDir dir; const auto path=dir.filePath("resume.sqlite"); qint64 sessionId=0;
    {
        MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Execution); fill(window); start(window);
        sessionId=field<ExecutionWidget>(window,"executionWidget")->activeSessionId();
        field<QCheckBox>(window,"researchCheck1")->setChecked(true);
        field<QToolButton>(window,"executionNotesToggle")->click();
        QVERIFY(field<QPlainTextEdit>(window,"executionNotesEdit")->isVisible());
        field<QPlainTextEdit>(window,"executionNotesEdit")->setPlainText(QStringLiteral("已比较上一代机型。"));
        field<QPushButton>(window,"saveExecutionNotesButton")->click();
        field<QPushButton>(window,"executionEditProductButton")->click();
        fill(window,QStringLiteral("上一代苹果手机"),6000); start(window);
    }
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Execution); settle();
    QCOMPARE(field<ExecutionWidget>(window,"executionWidget")->activeSessionId(),sessionId);
    QVERIFY(field<QWidget>(window,"executionDecisionCard")->isVisible());
    QCOMPARE(field<QLabel>(window,"executionProductTitle")->text(),QStringLiteral("上一代苹果手机"));
    QCOMPARE(field<QLabel>(window,"executionProductPurposeLabel")->text(),QStringLiteral("拍照，记录生活"));
    QCOMPARE(field<QLabel>(window,"executionProductPrice")->text(),QStringLiteral("¥6,000.00"));
    QVERIFY(field<QCheckBox>(window,"researchCheck1")->isChecked());
    QCOMPARE(field<QPlainTextEdit>(window,"executionNotesEdit")->toPlainText(),QStringLiteral("已比较上一代机型。"));
    QVERIFY(!field<QToolButton>(window,"executionNotesToggle")->isChecked());
}

void ExecutionUiTests::wishlistCancelAndSave()
{
    QTemporaryDir dir; const auto path=dir.filePath("wish.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Execution); fill(window); start(window);
    auto *execution=field<ExecutionWidget>(window,"executionWidget"); const auto sessionId=execution->activeSessionId(); QVERIFY(sessionId>0);
    field<QPushButton>(window,"executionWishButton")->click(); settle(); auto *dialog=field<WishDialog>(window,"wishDialog");
    QCOMPARE(field<QLineEdit>(*dialog,"wishNameEdit")->text(),QStringLiteral("苹果手机"));
    QCOMPARE(field<QDoubleSpinBox>(*dialog,"wishAmountSpinBox")->value(),10006.0);
    QCOMPARE(field<QLineEdit>(*dialog,"wishReasonEdit")->text(),QStringLiteral("拍照，记录生活"));
    dialog->reject(); settle(); QCOMPARE(execution->activeSessionId(),sessionId); QCOMPARE(count(db,"wishlist"),0);
    field<QPushButton>(window,"executionWishButton")->click(); settle(); dialog=field<WishDialog>(window,"wishDialog");
    field<QPushButton>(*dialog,"saveWishButton")->click(); settle();
    QCOMPARE(count(db,"wishlist"),0); QCOMPARE(execution->activeSessionId(),sessionId);
    field<QPushButton>(*dialog,"wishPriority3Button")->click();
    field<QPushButton>(*dialog,"saveWishButton")->click(); settle();
    QCOMPARE(count(db,"wishlist"),1); QCOMPARE(count(db,"transactions"),0); QCOMPARE(execution->activeSessionId(),0);
    QCOMPARE(window.currentPage(),MainWindow::Page::Wishlist);
    QSqlQuery q(db.database()); QVERIFY(q.exec("SELECT state FROM execution_sessions")); QVERIFY(q.next()); QCOMPARE(q.value(0).toString(),QStringLiteral("undecided"));
    QVERIFY(q.exec("SELECT priority FROM wishlist")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(),3);
}

void ExecutionUiTests::wishlistCommitIsAtomic()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("atomic.sqlite")); QVERIFY(db.initialize());
    ExecutionRepository executions(db); ExecutionSession session; session.name="headphones"; session.category="online_shopping"; QVERIFY(executions.start(session));
    QSqlQuery q(db.database()); QVERIFY(q.exec("CREATE TRIGGER reject_finish BEFORE UPDATE OF state ON execution_sessions BEGIN SELECT RAISE(FAIL,'fixture'); END"));
    WishItem item; item.name="headphones"; item.estimatedCents=89900;
    QVERIFY(!executions.deferToWishlist(session.id,item)); QCOMPARE(item.id,0); QCOMPARE(count(db,"wishlist"),0);
    ExecutionSession active; QVERIFY(executions.loadActive(active)); QCOMPARE(active.id,session.id);
    QVERIFY(q.exec("DROP TRIGGER reject_finish")); QVERIFY(executions.deferToWishlist(session.id,item)); QVERIFY(item.id>0); QCOMPARE(count(db,"wishlist"),1);
    WishItem duplicate; duplicate.name="headphones"; duplicate.estimatedCents=89900;
    QVERIFY(!executions.deferToWishlist(session.id,duplicate)); QCOMPARE(count(db,"wishlist"),1); QCOMPARE(count(db,"transactions"),0);
    QVERIFY(!executions.updateProduct(session.id,"changed","online_shopping",100));
}

void ExecutionUiTests::missingAndBasicBudgetsAndSmallWindow()
{
    QTemporaryDir dir; const auto path=dir.filePath("small.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    MainWindow window(path); window.resize(1180,900); window.show(); window.showPage(MainWindow::Page::Execution); fill(window); start(window);
    QCOMPARE(field<QLabel>(window,"executionBudgetAfterLabel")->text(),QStringLiteral("—"));
    QVERIFY(field<QLabel>(window,"executionBudgetTitleLabel")->text().contains(QStringLiteral("尚未设置")));
    BudgetRepository budgets(db); QVERIFY(budgets.saveSplit(QDate::currentDate(),50000,100000));
    field<QPushButton>(window,"executionEditProductButton")->click();
    auto *category=field<QComboBox>(window,"executionCategoryComboBox"); category->setCurrentIndex(category->findData("canteen"));
    field<QDoubleSpinBox>(window,"executionEstimatedAmountSpinBox")->setValue(20); start(window);
    auto *execution=field<ExecutionWidget>(window,"executionWidget"); QVERIFY(execution->refreshReferences());
    QCOMPARE(field<QLabel>(window,"executionBudgetAfterLabel")->text(),QStringLiteral("¥480.00"));
    for (const auto &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) {
        AppTheme::apply(&window,theme); window.resize(800,600); settle();
        for (auto *scroll:window.findChildren<QScrollArea *>()) if (scroll->isVisible()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        shot(window,theme+"-small");
    }
    AppTheme::apply(&window,QStringLiteral("dark")); window.resize(1440,1040); settle(); shot(window,"dark");
}

void ExecutionUiTests::productOnlyEntryAndPrinciples()
{
    QTemporaryDir dir; const auto path=dir.filePath("product-only.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Execution); settle();
    QCOMPARE(field<QLabel>(window,"executionHeading")->text(),QStringLiteral("执行"));
    auto *name=field<QLineEdit>(window,"executionNameEdit"); name->setText(QStringLiteral("   "));
    QVERIFY(field<QPushButton>(window,"executionInputButton")->isEnabled());
    field<QPushButton>(window,"executionAddRuleButton")->click(); settle(); auto *editor=field<QDialog>(window,"executionRulesEditor");
    field<QPlainTextEdit>(*editor,"executionRuleEditorText")->setPlainText(QStringLiteral("用途不明确，先不买。"));
    field<QPushButton>(*editor,"executionSaveRuleButton")->click(); settle();
    ShoppingProfileRepository profile(db); QStringList rules; QVERIFY(profile.loadRules(rules)); QCOMPARE(rules,QStringList{QStringLiteral("用途不明确，先不买。")});
    QVERIFY(field<QLabel>(window,"executionRulesLabel")->text().contains(rules[0]));
    field<QPushButton>(window,"executionEditRulesButton")->click(); settle(); editor=field<QDialog>(window,"executionRulesEditor");
    field<QPlainTextEdit>(*editor,"executionRuleEditorText")->setPlainText(QStringLiteral("检查家里是否已有。")); editor->reject(); settle();
    QVERIFY(profile.loadRules(rules)); QCOMPARE(rules[0],QStringLiteral("用途不明确，先不买。"));
    field<QPushButton>(window,"executionEditRulesButton")->click(); settle(); editor=field<QDialog>(window,"executionRulesEditor");
    field<QPlainTextEdit>(*editor,"executionRuleEditorText")->setPlainText(QStringLiteral("检查家里是否已有。"));
    field<QPushButton>(*editor,"executionSaveRuleButton")->click(); settle(); QVERIFY(profile.loadRules(rules)); QCOMPARE(rules[0],QStringLiteral("检查家里是否已有。"));
    name->setText(QStringLiteral("耳机")); next(window);
    QVERIFY(field<QWidget>(window,"executionModeCard")->isVisible()); QVERIFY(!field<QWidget>(window,"executionInputCard")->isVisible());
    QVERIFY(!field<QWidget>(window,"executionRulesCard")->isVisible());
    QVERIFY(field<QPushButton>(window,"executionResearchChoiceButton")->isChecked());
    QCOMPARE(field<QDoubleSpinBox>(window,"executionEstimatedAmountSpinBox")->value(),0.0);
    QVERIFY(field<QLabel>(window,"executionModeProductLabel")->text().contains(QStringLiteral("你要买：耳机")));
    shot(window,"product-only-brief");
    field<QPushButton>(window,"executionBackButton")->click(); settle(); QCOMPARE(name->text(),QStringLiteral("耳机"));
    QVERIFY(field<QLabel>(window,"executionRulesLabel")->isVisible()); shot(window,"principles");
    name->setText(QStringLiteral("袜子")); next(window); QVERIFY(field<QPushButton>(window,"executionFastChoiceButton")->isChecked());
    QCOMPARE(count(db,"execution_sessions"),0); QCOMPARE(count(db,"transactions"),0);
}

void ExecutionUiTests::modesSwitchWithoutRestartingSession()
{
    QTemporaryDir dir; const auto path=dir.filePath("mode-switch.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    qint64 sessionId=0; QDateTime deadline;
    {
        MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Execution);
        field<QLineEdit>(window,"executionNameEdit")->setText(QStringLiteral("袜子")); start(window);
        auto *execution=field<ExecutionWidget>(window,"executionWidget"); sessionId=execution->activeSessionId(); QVERIFY(sessionId>0);
        QVERIFY(field<QLabel>(window,"executionTimerLabel")->isVisible()); QVERIFY(!field<QPushButton>(window,"executionBuyButton")->isVisible()); shot(window,"fast-countdown");
        ExecutionRepository sessions(db); ExecutionSession active; QVERIFY(sessions.loadActive(active)); deadline=active.deadline;
        field<QPushButton>(window,"executionFastDecideButton")->click(); settle(); QVERIFY(field<QPushButton>(window,"executionBuyButton")->isVisible());
        field<QPushButton>(window,"executionActiveResearchButton")->click(); settle();
        QVERIFY(field<QCheckBox>(window,"researchCheck0")->isVisible()); QVERIFY(!field<QLabel>(window,"executionTimerLabel")->isVisible());
        field<QCheckBox>(window,"researchCheck0")->setChecked(true);
        field<QPushButton>(window,"executionActiveFastButton")->click(); settle();
        QVERIFY(sessions.loadActive(active)); QCOMPARE(active.id,sessionId); QCOMPARE(active.deadline,deadline); QCOMPARE(active.mode,QStringLiteral("fast"));
        QCOMPARE(count(db,"transactions"),0);
    }
    MainWindow restarted(path); restarted.show(); restarted.showPage(MainWindow::Page::Execution); settle();
    QCOMPARE(field<ExecutionWidget>(restarted,"executionWidget")->activeSessionId(),sessionId);
    QVERIFY(field<QPushButton>(restarted,"executionActiveFastButton")->isChecked());
    QVERIFY(field<QCheckBox>(restarted,"researchCheck0")->isChecked());
    ExecutionRepository sessions(db); ExecutionSession active; QVERIFY(sessions.loadActive(active)); QCOMPARE(active.deadline,deadline);
}

void ExecutionUiTests::expiredTimerNeverBuysAutomatically()
{
    QTemporaryDir dir; const auto path=dir.filePath("expired.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    ExecutionRepository sessions(db); ExecutionSession active; active.name=QStringLiteral("袜子"); active.category="online_shopping"; active.durationSeconds=60;
    QVERIFY(sessions.start(active,QDateTime::currentDateTimeUtc().addSecs(-120)));
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Execution); settle();
    QCOMPARE(field<QLabel>(window,"executionTimerLabel")->text(),QStringLiteral("时间到了"));
    QVERIFY(field<QPushButton>(window,"executionBuyButton")->isVisible());
    QCOMPARE(count(db,"transactions"),0); QVERIFY(sessions.loadActive(active)); QVERIFY(active.id>0);
}

QTEST_MAIN(ExecutionUiTests)
#include "execution_ui_tests.moc"
