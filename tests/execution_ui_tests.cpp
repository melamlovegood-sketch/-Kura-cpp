#include "AppTheme.h"
#include "BudgetRepository.h"
#include "ExecutionRepository.h"
#include "ExecutionWidget.h"
#include "DecisionChatWidget.h"
#include "DecisionDialog.h"
#include "ShoppingProfileRepository.h"
#include "TransactionRepository.h"
#include "WishDialog.h"
#include "mainwindow.h"
#include <QCheckBox>
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
    void stepsAndRealBudget();
    void editsAndNotesSurviveRestart();
    void wishlistCancelAndSave();
    void wishlistCommitIsAtomic();
    void missingAndBasicBudgetsAndSmallWindow();
    void productOnlyEntryAndPrinciples();
    void modesSwitchWithoutRestartingSession();
    void expiredTimerNeverBuysAutomatically();
    void focusedInputSurvivesPurchaseFlowAndKeepsSharedEntry();
};

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
    QCOMPARE(count(db,"wishlist"),1); QCOMPARE(count(db,"transactions"),0); QCOMPARE(execution->activeSessionId(),0);
    QCOMPARE(window.currentPage(),MainWindow::Page::Wishlist);
    QSqlQuery q(db.database()); QVERIFY(q.exec("SELECT state FROM execution_sessions")); QVERIFY(q.next()); QCOMPARE(q.value(0).toString(),QStringLiteral("undecided"));
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
    QCOMPARE(field<QLabel>(window,"executionHeading")->text(),QStringLiteral("执行层"));
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
