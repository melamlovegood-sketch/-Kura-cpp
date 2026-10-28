#include "AIStubServer.h"
#include "AppTheme.h"
#include "BudgetRepository.h"
#include "DatabaseBackupService.h"
#include "DecisionChatWidget.h"
#include "ExecutionRepository.h"
#include "ExecutionWidget.h"
#include "TransactionRepository.h"
#include "WorkflowSchema.h"
#include "mainwindow.h"
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QInputMethodEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QToolButton>
#include <QtTest>

namespace {
const QString answer=QStringLiteral("## 建议先等等，把价格和用途确认清楚。\n\n你本月的可支配预算已超支 **¥4,025**，继续购买会增加预算压力。\n\n### 先确认这两点\n\n- **准备花多少钱？** 有了预计价格，才能看清对预算的影响。\n- **现在确实需要吗？** 先看看已有的鞋能否满足日常使用。\n\n如果不是急用，可以先加入清单，过几天再看。\n\n### 补充分析\n\n目前预计金额尚未确认，因此无法计算购买后的实际余额。确认价格后，可以再比较预算影响。");
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
template<class T> T *field(QObject &root,const char *name) { auto *result=root.findChild<T *>(QLatin1String(name)); Q_ASSERT(result); return result; }
void start(MainWindow &window) {
    window.show(); window.showPage(MainWindow::Page::Execution);
    ParsedProduct p; p.name=QStringLiteral("球鞋"); QVERIFY(field<ExecutionWidget>(window,"executionWidget")->prepareDecision(p));
    field<QPushButton>(window,"executionResearchChoiceButton")->click(); field<QPushButton>(window,"startExecutionButton")->click(); settle();
}
void send(MainWindow &window,const QString &question) { auto *edit=field<QPlainTextEdit>(window,"decisionQuestionEdit"); edit->setPlainText(question); QTest::keyClick(edit,Qt::Key_Return); }
void configure(const QString &path,AIStubServer &server) { QString error; QVERIFY(AISettingsStore::save(path+".ui.ini",server.settings(),error)); }
void screenshot(MainWindow &window,const QString &name) {
    settle(); auto *chat=field<DecisionChatWidget>(window,"decisionChatWidget");
    for (auto *parent=chat->parentWidget();parent;parent=parent->parentWidget()) if (auto *scroll=qobject_cast<QScrollArea *>(parent)) { scroll->verticalScrollBar()->setValue(qMax(0,chat->mapTo(scroll->widget(),QPoint()).y()-18)); break; }
    settle(); const auto dir=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!dir.isEmpty()) { QVERIFY(QDir().mkpath(dir)); QVERIFY(window.grab().save(dir+"/decision-chat-"+name+".png")); }
}
}

class DecisionChatTests:public QObject {
    Q_OBJECT
private slots:
    void keyboardAndMarkdown();
    void legacyAndStructuredRestore();
    void realRequestsCarryConversationAndRegenerate();
    void failureRetryStopAndContextChange();
    void historySurvivesRestartAndBackup();
    void themesAndNarrowLayout();
};

void DecisionChatTests::keyboardAndMarkdown()
{
    DecisionChatWidget chat; AppTheme::apply(&chat); chat.setAvailable(true); chat.resize(600,800); chat.show(); settle();
    auto *edit=field<QPlainTextEdit>(chat,"decisionQuestionEdit"); auto *sendButton=field<QPushButton>(chat,"askDecisionAIButton");
    QSignalSpy requests(&chat,&DecisionChatWidget::requestSubmitted);
    edit->setPlainText(QStringLiteral("   ")); QVERIFY(!sendButton->isEnabled()); QTest::keyClick(edit,Qt::Key_Return); QCOMPARE(requests.size(),0);
    edit->setPlainText(QStringLiteral("预算")); QTest::keyClick(edit,Qt::Key_Return,Qt::ShiftModifier); QCOMPARE(requests.size(),0); QVERIFY(edit->toPlainText().contains('\n'));
    QInputMethodEvent preedit(QStringLiteral("用途"),{}); QApplication::sendEvent(edit,&preedit); QTest::keyClick(edit,Qt::Key_Return); QCOMPARE(requests.size(),0);
    QInputMethodEvent commit; commit.setCommitString(QStringLiteral("用途")); QApplication::sendEvent(edit,&commit);
    edit->setPlainText(QStringLiteral("现在适合买么？")); QTest::keyClick(edit,Qt::Key_Return); QCOMPARE(requests.size(),1);
    chat.beginRequest(QStringLiteral("现在适合买么？"),false); QCOMPARE(sendButton->text(),QStringLiteral("停止 □")); chat.completeRequest(answer); settle();
    auto *body=field<QTextBrowser>(chat,"decisionChatAnswerText"); QVERIFY(body->toPlainText().contains(QStringLiteral("¥4,025"))); QVERIFY(!body->toPlainText().contains("**"));
    QCOMPARE(body->verticalScrollBarPolicy(),Qt::ScrollBarAlwaysOff); QVERIFY(!body->openExternalLinks());
    auto *details=field<QTextBrowser>(chat,"decisionChatAnswerDetails"); QVERIFY(!details->isVisible());
    field<QToolButton>(chat,"decisionChatExpandAnswerButton")->click(); settle(); QVERIFY(details->isVisible()); QVERIFY(details->toPlainText().contains(QStringLiteral("目前预计金额尚未确认")));
    field<QPushButton>(chat,"decisionChatCopyButton")->click(); QVERIFY(QApplication::clipboard()->text().contains(QStringLiteral("目前预计金额尚未确认"))); QVERIFY(!QApplication::clipboard()->text().contains("**"));
}

void DecisionChatTests::legacyAndStructuredRestore()
{
    DecisionChatWidget chat; chat.setAvailable(true);
    chat.restoreConversation(QStringLiteral("你：请根据事实给出简短的决策简报、需要核实的事项和可选下一步，不替我作决定。\n\nAI：**先确认预算**，再决定。"));
    QCOMPARE(chat.findChildren<QWidget *>("decisionChatUserMessage").size(),0); QVERIFY(!chat.plainText().contains(QStringLiteral("请根据事实"))); QVERIFY(chat.plainText().contains(QStringLiteral("先确认预算")));
    chat.restoreConversation(QStringLiteral("你：哪些是替代选择？\n\nAI：先看看已有物品。"));
    QCOMPARE(chat.findChildren<QWidget *>("decisionChatUserMessage").size(),1); QVERIFY(chat.plainText().contains(QStringLiteral("哪些是替代选择")));
    field<QPlainTextEdit>(chat,"decisionQuestionEdit")->setPlainText(QStringLiteral("下一轮草稿")); chat.setExpanded(false); const auto saved=chat.serialize();
    DecisionChatWidget restored; restored.restoreConversation(saved); QCOMPARE(restored.plainText(),chat.plainText()); QVERIFY(!restored.isExpanded());
    QCOMPARE(field<QPlainTextEdit>(restored,"decisionQuestionEdit")->toPlainText(),QStringLiteral("下一轮草稿"));
}

void DecisionChatTests::realRequestsCarryConversationAndRegenerate()
{
    QTemporaryDir dir; const auto path=dir.filePath("conversation.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    BudgetRepository budgets(db); QVERIFY(budgets.saveSplit(QDate::currentDate(),50000,100000));
    TransactionRepository expenses(db); Transaction expense(0,QStringLiteral("已有支出"),502500,QStringLiteral("购物"),QDate::currentDate(),QTime(12,0),{},"online_shopping"); QVERIFY(expenses.add(expense));
    AIStubServer server; server.handler=[](const QJsonObject &body) { return body["messages"].toArray().last().toObject()["content"].toString().contains(QStringLiteral("替代"))?QStringLiteral("## 先从实际用途反推需求。\n\n可以先看看已有的鞋，再比较能承担的替代选择。"):answer; }; configure(path,server);
    MainWindow window(path); window.resize(1280,1100); start(window); auto *chat=field<DecisionChatWidget>(window,"decisionChatWidget");
    send(window,QStringLiteral("这双球鞋，现在适合买么？")); QTRY_VERIFY_WITH_TIMEOUT(chat->plainText().contains(QStringLiteral("建议先等等")),5000); QCOMPARE(server.requests.size(),1); settle();
    QTRY_VERIFY_WITH_TIMEOUT(field<QFrame>(*chat,"decisionChatComposer")->height()<190,2000);
    QTRY_VERIFY_WITH_TIMEOUT(field<QLabel>(*chat,"decisionChatTitle")->mapTo(chat,QPoint()).y()<50,2000);
    QTRY_VERIFY_WITH_TIMEOUT(chat->height()<1000,2000); screenshot(window,"conversation");
    send(window,QStringLiteral("有哪些替代选择？")); QTRY_VERIFY_WITH_TIMEOUT(chat->plainText().contains(QStringLiteral("反推需求")),5000); QCOMPARE(server.requests.size(),2);
    const auto messages=server.requests.last()["messages"].toArray(); QCOMPARE(messages.size(),4);
    QCOMPARE(messages[1]["role"].toString(),QStringLiteral("user")); QCOMPARE(messages[1]["content"].toString(),QStringLiteral("这双球鞋，现在适合买么？"));
    QCOMPARE(messages[2]["role"].toString(),QStringLiteral("assistant")); QVERIFY(messages[2]["content"].toString().contains(QStringLiteral("建议先等等")));
    QVERIFY(messages[0]["content"].toString().contains(QStringLiteral("-¥4025.00")));
    field<QPushButton>(*chat,"decisionChatRegenerateButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),3,5000); QTRY_VERIFY_WITH_TIMEOUT(!chat->isBusy(),5000);
    QCOMPARE(chat->findChildren<QWidget *>("decisionChatUserMessage").size(),2); QCOMPARE(chat->findChildren<QWidget *>("decisionChatAssistantMessage").size(),2);
    const auto regeneration=server.requests.last()["messages"].toArray(); QCOMPARE(regeneration.size(),4); QCOMPARE(regeneration.last()["content"].toString(),QStringLiteral("有哪些替代选择？"));
    QCOMPARE(regeneration[2]["content"].toString(),answer);
}

void DecisionChatTests::failureRetryStopAndContextChange()
{
    QTemporaryDir dir; const auto path=dir.filePath("request-states.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize()); BudgetRepository budgets(db); QVERIFY(budgets.saveSplit(QDate::currentDate(),50000,100000));
    AIStubServer server; server.status=401; server.handler=[](const QJsonObject &) { return QStringLiteral("迟到的答案"); }; configure(path,server);
    MainWindow window(path); start(window); auto *chat=field<DecisionChatWidget>(window,"decisionChatWidget");
    send(window,QStringLiteral("先看看预算？")); QTRY_VERIFY_WITH_TIMEOUT(field<QPushButton>(*chat,"decisionChatRetryButton")->isVisible(),5000);
    QCOMPARE(field<QPlainTextEdit>(window,"decisionQuestionEdit")->toPlainText(),QStringLiteral("先看看预算？"));
    server.status=200; field<QPushButton>(*chat,"decisionChatRetryButton")->click(); QTRY_VERIFY_WITH_TIMEOUT(chat->plainText().contains(QStringLiteral("迟到的答案")),5000);
    QCOMPARE(chat->findChildren<QWidget *>("decisionChatUserMessage").size(),1);
    server.delay=250; send(window,QStringLiteral("这次我要停止")); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),3,5000);
    field<QPushButton>(window,"askDecisionAIButton")->click(); const auto stopped=chat->plainText(); QTest::qWait(350); QCOMPARE(chat->plainText(),stopped); QVERIFY(!chat->isBusy());
    send(window,QStringLiteral("更新预算时取消")); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),4,5000);
    QVERIFY(budgets.saveSplit(QDate::currentDate(),50000,90000)); QVERIFY(field<ExecutionWidget>(window,"executionWidget")->refreshReferences());
    QTest::qWait(350); QVERIFY(chat->plainText().isEmpty()); QVERIFY(!chat->isBusy());
    ExecutionRepository repository(db); ExecutionSession active; QVERIFY(repository.loadActive(active)); QVERIFY(active.id>0);
}

void DecisionChatTests::historySurvivesRestartAndBackup()
{
    QTemporaryDir dir; const auto path=dir.filePath("chat-backup.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    AIStubServer server; server.handler=[](const QJsonObject &) { return answer; }; configure(path,server); QString saved;
    {
        MainWindow window(path); start(window); auto *chat=field<DecisionChatWidget>(window,"decisionChatWidget");
        send(window,QStringLiteral("我该先确认什么？")); QTRY_VERIFY_WITH_TIMEOUT(chat->plainText().contains(QStringLiteral("建议先等等")),5000);
        chat->setExpanded(false); field<QPlainTextEdit>(window,"decisionQuestionEdit")->setPlainText(QStringLiteral("留给下次的问题"));
        field<QCheckBox>(window,"researchCheck0")->setChecked(true); saved=chat->serialize();
    }
    MainWindow restarted(path); restarted.show(); restarted.showPage(MainWindow::Page::Execution); settle();
    auto *chat=field<DecisionChatWidget>(restarted,"decisionChatWidget"); QCOMPARE(chat->serialize(),saved); QVERIFY(!chat->isExpanded());
    DecisionChatWidget longChat; longChat.setAvailable(true);
    const auto longAnswer=QStringLiteral("完整的记录不应被覆盖。\n\n").repeated(150);
    for (int i=0;i<8;++i) { longChat.beginRequest(QStringLiteral("第 %1 轮问题").arg(i),false); longChat.completeRequest(longAnswer); }
    const auto conversation=longChat.serialize(); QVERIFY(conversation.size()>12000); QVERIFY(conversation.size()<ExecutionSession::MaximumAdviceLength);
    ExecutionRepository sessions(db); ExecutionSession active; QVERIFY(sessions.loadActive(active)); QVERIFY(sessions.saveNotes(active.id,active.notes,conversation));
    QString error; QVERIFY2(WorkflowSchema::validate(db.database(),error),qPrintable(error));
    DatabaseBackupService backup(db); const auto filename=dir.filePath("backup.sqlite"); QVERIFY(backup.backup(filename,error)); BackupInfo info; QVERIFY2(backup.prepareRestore(filename,info,error),qPrintable(error));
    QVERIFY(sessions.saveNotes(active.id,active.notes,"changed")); QString safety; QVERIFY(backup.restorePrepared(safety,error)); QVERIFY(sessions.loadActive(active)); QCOMPARE(active.advice,conversation);
    const auto history=longChat.requestHistory(); QVERIFY(history.size()<=8);
}

void DecisionChatTests::themesAndNarrowLayout()
{
    QTemporaryDir dir; const auto path=dir.filePath("theme.sqlite"); MainWindow window(path); start(window);
    auto *chat=field<DecisionChatWidget>(window,"decisionChatWidget"); chat->beginRequest(QStringLiteral("这双球鞋，现在适合买么？"),false); chat->completeRequest(answer);
    for (const auto &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) {
        AppTheme::apply(&window,theme); window.resize(1180,1000); settle(); screenshot(window,theme);
        window.resize(800,600); settle(); screenshot(window,theme+"-small");
        for (auto *scroll:window.findChildren<QScrollArea *>()) if (scroll->isVisible()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        for (auto *browser:chat->findChildren<QTextBrowser *>()) QCOMPARE(browser->verticalScrollBarPolicy(),Qt::ScrollBarAlwaysOff);
    }
}

QTEST_MAIN(DecisionChatTests)
#include "decision_chat_tests.moc"
