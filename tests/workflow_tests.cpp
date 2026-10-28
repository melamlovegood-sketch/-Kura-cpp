#include <QAction>
#include "AIService.h"
#include "AddTransactionDialog.h"
#include "BudgetRepository.h"
#include "DataManagementWidget.h"
#include "DatabaseBackupService.h"
#include "DecisionDialog.h"
#include "ExecutionWidget.h"
#include "DecisionChatWidget.h"
#include "PriceWidget.h"
#include "WishPriceDialog.h"
#include "PurchaseRepository.h"
#include "ReviewRepository.h"
#include "ReviewWidget.h"
#include "ShoppingProfileRepository.h"
#include "TransactionRepository.h"
#include "WorkflowSchema.h"
#include "mainwindow.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

static void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
static qint64 scalar(DatabaseManager &database,const QString &sql) { QSqlQuery query(database.database()); if (!query.exec(sql) || !query.next()) return -1; return query.value(0).toLongLong(); }
static BrandEntry brand(const QString &name="Fixture Brand",int weight=5) { BrandEntry result; result.name=name; result.category="online_shopping"; result.weight=weight; return result; }
static Transaction purchase(const QString &name=QStringLiteral("键盘"),int daysAgo=0) { return Transaction(0,name,25000,QStringLiteral("购物"),QDate::currentDate().addDays(-daysAgo),QTime(12,0),{},"online_shopping"); }
static void screenshot(QWidget &widget,const QString &name) { settle(); const auto directory=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!directory.isEmpty()) { QVERIFY(QDir().mkpath(directory)); QVERIFY(widget.grab().save(directory+"/workflow-"+name+".png")); } }
static void reveal(QWidget *widget) { settle(); for (auto *parent=widget->parentWidget();parent;parent=parent->parentWidget()) if (auto *scroll=qobject_cast<QScrollArea *>(parent)) { scroll->verticalScrollBar()->setValue(qMax(0,widget->mapTo(scroll->widget(),QPoint()).y()-16)); settle(); return; } }

struct AdvisorServer {
    QTcpServer server;
    QList<QByteArray> requests;
    QString answer=QStringLiteral("根据已保存预算，这次购买需要先确认实际用途。品牌权重来自你的历史评价，不能代替参数比较。");
    int delay=0;
    AdvisorServer() {
        server.listen(QHostAddress::LocalHost);
        QObject::connect(&server,&QTcpServer::newConnection,&server,[this] { while (server.hasPendingConnections()) { auto *socket=server.nextPendingConnection();
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[this,socket] {
                const auto bytes=socket->property("body").toByteArray()+socket->readAll(); socket->setProperty("body",bytes); const auto end=bytes.indexOf("\r\n\r\n"); if (end<0 || socket->property("sent").toBool()) return;
                const auto length=QRegularExpression("content-length: *([0-9]+)",QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(bytes.left(end))); if (!length.hasMatch() || bytes.size()<end+4+length.captured(1).toInt()) return;
                socket->setProperty("sent",true); requests << bytes; const auto body=QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",answer}}}}}}}).toJson(QJsonDocument::Compact);
                QTimer::singleShot(delay,socket,[socket,body] { if (socket->state()!=QAbstractSocket::UnconnectedState) { socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body); socket->disconnectFromHost(); } });
            }); QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        } });
    }
    AISettings settings() const { AISettings value; value.baseUrl=QStringLiteral("http://127.0.0.1:%1/v1").arg(server.serverPort()); value.model="fixture-advisor"; value.apiKey="fixture-key"; return value; }
};

class WorkflowTests:public QObject {
    Q_OBJECT
private slots:
    void rulesAndBrandsPersistAtomically();
    void sessionPurchaseAndReviewAreAtomic();
    void reviewBoundaryAndExactlyOnceFeedback();
    void priceHistoryAlertsAndIdempotency();
    void completeBackupAndOldFiveTableRestore();
    void rejectInvalidWorkflowBackups();
    void executionUiResumesAndPurchases();
    void reviewAndPriceUiUseRealData();
    void advisorUsesContextAndRestoreCancelsResponse();
};

void WorkflowTests::rulesAndBrandsPersistAtomically()
{
    QTemporaryDir directory; const auto path=directory.filePath("profile.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize()); ShoppingProfileRepository profile(database);
    const QStringList rules={QStringLiteral("先确认需要，再比较价格"),QStringLiteral("留意退换条件")}; QVERIFY(profile.saveRules(rules)); QStringList loaded; QVERIFY(profile.loadRules(loaded)); QCOMPARE(loaded,rules);
    QSqlQuery query(database.database()); QVERIFY(query.exec("CREATE TRIGGER fail_rules BEFORE INSERT ON shopping_rules BEGIN SELECT RAISE(FAIL,'fixture'); END")); QVERIFY(!profile.saveRules({QStringLiteral("失败的新规则")})); QVERIFY(profile.loadRules(loaded)); QCOMPARE(loaded,rules); QVERIFY(query.exec("DROP TRIGGER fail_rules"));
    auto first=brand(QString::fromUtf8("Élan"),8); QVERIFY(profile.saveBrand(first)); auto duplicate=brand(QString::fromUtf8("ÉLAN")); QVERIFY(!profile.saveBrand(duplicate));
    QVERIFY(profile.archiveBrand(first.id)); QList<BrandEntry> brands; QVERIFY(profile.loadBrands(brands)); QVERIFY(brands.isEmpty());
    auto restored=brand(QString::fromUtf8("ÉLAN"),0); QVERIFY(profile.saveBrand(restored)); QCOMPARE(restored.id,first.id); QVERIFY(profile.loadBrands(brands)); QCOMPARE(brands.size(),1); QCOMPARE(brands[0].weight,8);
    DatabaseManager reopened(path); QVERIFY(reopened.initialize()); ShoppingProfileRepository again(reopened); QVERIFY(again.loadRules(loaded)); QCOMPARE(loaded,rules); QVERIFY(again.loadBrands(brands)); QCOMPARE(brands[0].id,first.id);
}
void WorkflowTests::sessionPurchaseAndReviewAreAtomic()
{
    QTemporaryDir directory; const auto path=directory.filePath("execution.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize()); ShoppingProfileRepository profile(database); auto trusted=brand(); QVERIFY(profile.saveBrand(trusted));
    ExecutionRepository sessions(database); ExecutionSession session; session.name=QStringLiteral("键盘"); session.category="online_shopping"; session.durationSeconds=60; session.estimatedCents=25000;
    const auto start=QDateTime::currentDateTimeUtc().addSecs(-65); QVERIFY(sessions.start(session,start)); const auto deadline=session.deadline;
    ExecutionSession duplicate; duplicate.name="other"; duplicate.category="online_shopping"; QVERIFY(!sessions.start(duplicate));
    QVERIFY(sessions.saveNotes(session.id,QStringLiteral("研究记录"),QStringLiteral("已核对预算"))); ExecutionSession loaded; QVERIFY(sessions.loadActive(loaded)); QCOMPARE(loaded.id,session.id); QCOMPARE(loaded.deadline,deadline); QVERIFY(loaded.deadline<QDateTime::currentDateTimeUtc()); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),0);
    { DatabaseManager reopened(path); QVERIFY(reopened.initialize()); ExecutionRepository again(reopened); QVERIFY(again.loadActive(loaded)); QCOMPARE(loaded.notes,QStringLiteral("研究记录")); QCOMPARE(loaded.deadline,deadline); }
    QSqlQuery query(database.database()); QVERIFY(query.exec("CREATE TRIGGER fail_reviews BEFORE INSERT ON review_tasks BEGIN SELECT RAISE(FAIL,'fixture'); END"));
    PurchaseRepository purchases(database); auto expense=purchase(); QVERIFY(!purchases.record(expense,trusted.id,session.id)); QCOMPARE(expense.id(),0); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),0); QVERIFY(sessions.loadActive(loaded)); QCOMPARE(loaded.id,session.id); QVERIFY(query.exec("DROP TRIGGER fail_reviews"));
    QVERIFY(purchases.record(expense,trusted.id,session.id)); QVERIFY(expense.id()>0); QVERIFY(sessions.loadActive(loaded)); QCOMPARE(loaded.id,0); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks"),2);
    auto repeated=purchase(); QVERIFY(!purchases.record(repeated,trusted.id,session.id)); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),1);
}
void WorkflowTests::reviewBoundaryAndExactlyOnceFeedback()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("review.sqlite")); QVERIFY(database.initialize()); ShoppingProfileRepository profile(database); auto trusted=brand(); QVERIFY(profile.saveBrand(trusted));
    auto expense=purchase(); TransactionRepository expenses(database); QVERIFY(expenses.add(expense)); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks"),0);
    const auto purchased=QDateTime::fromString("2026-09-01T08:00:00Z",Qt::ISODate); ReviewRepository reviews(database); QVERIFY(reviews.schedule(expense.id(),trusted.id,purchased)); QVERIFY(reviews.schedule(expense.id(),trusted.id,purchased)); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks"),2);
    QList<ReviewTask> due; QVERIFY(reviews.list(due,false,purchased.addDays(7).addMSecs(-1))); QVERIFY(due.isEmpty()); QVERIFY(reviews.list(due,false,purchased.addDays(7))); QCOMPARE(due.size(),1); const auto first=due[0].id;
    QVERIFY(!reviews.complete(first,2,1,{},purchased.addDays(7).addMSecs(-1))); QVERIFY(reviews.complete(first,2,1,QStringLiteral("每天都在用"),purchased.addDays(7))); QVERIFY(!reviews.complete(first,0,-1,{},purchased.addDays(8)));
    QList<BrandEntry> brands; QVERIFY(profile.loadBrands(brands)); QCOMPARE(brands[0].weight,5);
    QVERIFY(reviews.list(due,false,purchased.addDays(30))); QCOMPARE(due.size(),1); const auto second=due[0].id;
    QSqlQuery query(database.database()); QVERIFY(query.exec("CREATE TRIGGER fail_weight BEFORE UPDATE ON review_tasks BEGIN SELECT RAISE(FAIL,'fixture'); END")); QVERIFY(!reviews.complete(second,0,-1,{},purchased.addDays(30))); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks WHERE completed_ms IS NOT NULL"),1); QVERIFY(query.exec("DROP TRIGGER fail_weight"));
    QVERIFY(reviews.complete(second,0,-1,QStringLiteral("后来用得很少"),purchased.addDays(30))); QVERIFY(profile.loadBrands(brands)); QCOMPARE(brands[0].weight,5);
    auto another=purchase(QStringLiteral("鼠标")); QVERIFY(expenses.add(another)); QVERIFY(reviews.schedule(another.id(),trusted.id,purchased)); QVERIFY(expenses.remove(another.id())); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks WHERE transaction_id="+QString::number(another.id())),0);
    QVERIFY(expenses.remove(expense.id())); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks"),2); QString error; QVERIFY2(WorkflowSchema::validate(database.database(),error),qPrintable(error));
}
void WorkflowTests::priceHistoryAlertsAndIdempotency()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("prices.sqlite")); QVERIFY(database.initialize()); PriceRepository prices(database); qint64 id=0; const auto now=QDateTime::currentDateTimeUtc();
    QVERIFY(prices.intake(QStringLiteral("耳机"),"online_shopping",30000,QStringLiteral("京东手动记录"),"first",id,now.addDays(-5))); const auto original=id;
    QVERIFY(prices.intake(QStringLiteral("耳机"),"online_shopping",30000,QStringLiteral("京东手动记录"),"first",id)); QCOMPARE(id,original); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM price_records"),1);
    QVERIFY(prices.configure(id,25000,"https://example.com/product")); QVERIFY(prices.record(id,23000,QStringLiteral("淘宝券后价"),"second",now));
    QList<PriceTrack> tracks; QVERIFY(prices.load(tracks)); QCOMPARE(tracks.size(),1); QVERIFY(tracks[0].targetHit); QVERIFY(tracks[0].priceDrop); QCOMPARE(tracks[0].previousCents,30000); const auto last=tracks[0].latestRecordId;
    QVERIFY(prices.dismiss(id,"target",last)); QVERIFY(prices.dismiss(id,"drop",last)); QVERIFY(prices.record(id,21000,QStringLiteral("补记更早价格"),"backfill",now.addDays(-10))); QVERIFY(prices.load(tracks)); QCOMPARE(tracks[0].latestCents,23000); QVERIFY(!tracks[0].targetHit); QVERIFY(!tracks[0].priceDrop); QCOMPARE(tracks[0].lowestCents,21000);
    QVERIFY(prices.record(id,22000,QStringLiteral("新的券后价"),"third",now.addSecs(1))); QVERIFY(prices.load(tracks)); QVERIFY(tracks[0].targetHit); QVERIFY(tracks[0].priceDrop); QVERIFY(!prices.configure(id,10000,"file:///C:/example"));
    QVERIFY(prices.archive(id)); QVERIFY(prices.load(tracks)); QVERIFY(tracks.isEmpty()); QVERIFY(prices.intake(QStringLiteral("耳机"),"online_shopping",30000,QStringLiteral("京东手动记录"),"first",id)); QCOMPARE(id,original);
    qint64 fresh=0; QVERIFY(prices.intake(QStringLiteral("耳机"),"online_shopping",24000,QStringLiteral("重新记录"),"fresh",fresh)); QVERIFY(fresh!=original); QVERIFY(!prices.record(fresh,30000,QStringLiteral("京东手动记录"),"first")); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),0); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM price_tracks"),2);
}
void WorkflowTests::completeBackupAndOldFiveTableRestore()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("current.sqlite")); QVERIFY(database.initialize()); ShoppingProfileRepository profile(database); auto trusted=brand(); QVERIFY(profile.saveBrand(trusted)); QVERIFY(profile.saveRules({QStringLiteral("核对用途")}));
    PurchaseRepository purchases(database); auto expense=purchase(QStringLiteral("旧键盘"),40); QVERIFY(purchases.record(expense,trusted.id)); ReviewRepository reviews(database); QList<ReviewTask> due; QVERIFY(reviews.list(due)); QCOMPARE(due.size(),2); QVERIFY(reviews.complete(due[0].id,2,1,QStringLiteral("值得")));
    ExecutionRepository executions(database); ExecutionSession session; session.name=QStringLiteral("下一件商品"); session.category="online_shopping"; session.mode="research"; QVERIFY(executions.start(session)); QVERIFY(executions.saveNotes(session.id,"saved research","saved advice"));
    PriceRepository prices(database); qint64 id=0; QVERIFY(prices.intake("keyboard","online_shopping",10000,"manual","price-one",id)); QVERIFY(prices.configure(id,12000,{})); QList<PriceTrack> tracks; QVERIFY(prices.load(tracks)); QVERIFY(prices.dismiss(id,"target",tracks[0].latestRecordId));
    DatabaseBackupService backup(database); QString error,safety; BackupInfo info; const auto snapshot=directory.filePath("complete.sqlite"); QVERIFY(backup.backup(snapshot,error)); QVERIFY2(backup.prepareRestore(snapshot,info,error),qPrintable(error)); QCOMPARE(info.brandCount,1); QCOMPARE(info.reviewCount,2); QCOMPARE(info.sessionCount,1); QCOMPARE(info.trackCount,1); QCOMPARE(info.priceCount,1);
    QVERIFY(profile.saveRules({"changed"})); QVERIFY(executions.finish(session.id,"undecided")); QVERIFY(prices.archive(id)); QVERIFY2(backup.restorePrepared(safety,error),qPrintable(error));
    QStringList rules; QVERIFY(profile.loadRules(rules)); QCOMPARE(rules,QStringList{QStringLiteral("核对用途")}); ExecutionSession active; QVERIFY(executions.loadActive(active)); QCOMPARE(active.id,session.id); QCOMPARE(active.notes,QStringLiteral("saved research")); QVERIFY(prices.load(tracks)); QCOMPARE(tracks.size(),1); QVERIFY(!tracks[0].targetHit);
    QList<BrandEntry> brands; QVERIFY(profile.loadBrands(brands)); QCOMPARE(brands[0].weight,5);
    const auto old=directory.filePath("old-five.sqlite"); { DatabaseManager legacy(old); QVERIFY(legacy.initialize()); TransactionRepository expenses(legacy); auto record=purchase("old bill"); QVERIFY(expenses.add(record)); QSqlQuery query(legacy.database()); for (const auto &name:{"user_categories","execution_wishes","achievement_marks","bill_import_keys","transaction_reviews","monthly_stories","review_tasks","price_records","price_tracks","execution_sessions","brands","shopping_rules"}) QVERIFY(query.exec("DROP TABLE "+QLatin1String(name))); }
    QFile file(old); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes=file.readAll(); file.close(); QVERIFY2(backup.prepareRestore(old,info,error),qPrintable(error)); QCOMPARE(info.reviewCount,0); QVERIFY(backup.restorePrepared(safety,error)); QVERIFY(executions.loadActive(active)); QCOMPARE(active.id,0); QVERIFY(prices.load(tracks)); QVERIFY(tracks.isEmpty()); QVERIFY(profile.loadBrands(brands)); QVERIFY(brands.isEmpty()); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),bytes);
}
void WorkflowTests::rejectInvalidWorkflowBackups()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("base.sqlite")); QVERIFY(database.initialize()); ShoppingProfileRepository profile(database); auto trusted=brand(); QVERIFY(profile.saveBrand(trusted)); DatabaseBackupService backup(database); QString error; BackupInfo info;
    const auto missing=directory.filePath("missing.sqlite"); QVERIFY(backup.backup(missing,error)); { DatabaseManager changed(missing); QVERIFY(changed.initialize()); QSqlQuery query(changed.database()); QVERIFY(query.exec("DROP TABLE review_tasks")); } QVERIFY(!backup.prepareRestore(missing,info,error));
    const auto corrupt=directory.filePath("bad-name-index.sqlite"); QVERIFY(backup.backup(corrupt,error)); { DatabaseManager changed(corrupt); QVERIFY(changed.initialize()); QSqlQuery query(changed.database()); QVERIFY(query.exec("UPDATE brands SET name_key='incorrect'")); } QVERIFY(!backup.prepareRestore(corrupt,info,error));
    QCOMPARE(scalar(database,"SELECT COUNT(*) FROM brands"),1);
}

void WorkflowTests::executionUiResumesAndPurchases()
{
    QTemporaryDir directory; const auto path=directory.filePath("ui-execution.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize()); BudgetRepository budgets(database); QVERIFY(budgets.saveSplit(QDate::currentDate(),10000,100000)); qint64 sessionId=0;
    {
        MainWindow window(path); window.resize(1180,860); window.show(); settle(); window.showPage(MainWindow::Page::Execution);
        window.findChild<QPushButton *>("executionAddRuleButton")->click(); settle(); auto *rules=window.findChild<QDialog *>("executionRulesEditor"); QVERIFY(rules);
        rules->findChild<QPlainTextEdit *>()->setPlainText(QStringLiteral("先核对用途\n检查退换条件")); rules->findChild<QPushButton *>("executionSaveRuleButton")->click(); settle();
        window.showPage(MainWindow::Page::Execution); window.findChild<QLineEdit *>("executionNameEdit")->setText(QStringLiteral("键盘")); window.findChild<QDoubleSpinBox *>("executionEstimatedAmountSpinBox")->setValue(250); auto *mode=window.findChild<QComboBox *>("executionModeComboBox"); mode->setCurrentIndex(mode->findData("research")); window.findChild<QSpinBox *>("executionMinutesSpinBox")->setValue(1); window.findChild<QPushButton *>("refreshDecisionBriefButton")->click();
        const auto text=window.findChild<QPlainTextEdit *>("decisionBriefText")->toPlainText(); QVERIFY(text.contains(QStringLiteral("先核对用途"))); QVERIFY(!text.contains("Fixture Brand")); QVERIFY(text.contains(QStringLiteral("¥1000.00")));
        window.findChild<QPushButton *>("startExecutionButton")->click(); auto *execution=window.findChild<ExecutionWidget *>(); sessionId=execution->activeSessionId(); QVERIFY(sessionId>0); window.findChild<QCheckBox *>("researchCheck0")->setChecked(true); window.findChild<QPlainTextEdit *>("executionNotesEdit")->setPlainText(QStringLiteral("已比较参数")); window.findChild<QPushButton *>("saveExecutionNotesButton")->click(); screenshot(window,"execution"); reveal(window.findChild<QPlainTextEdit *>("executionNotesEdit")); screenshot(window,"research-notes");
    }
    MainWindow restarted(path); restarted.resize(1180,860); restarted.show(); restarted.showPage(MainWindow::Page::Execution); settle(); QCOMPARE(restarted.findChild<ExecutionWidget *>()->activeSessionId(),sessionId); QVERIFY(restarted.findChild<QCheckBox *>("researchCheck0")->isChecked()); QCOMPARE(restarted.findChild<QPlainTextEdit *>("executionNotesEdit")->toPlainText(),QStringLiteral("已比较参数"));
    QVERIFY(!restarted.findChild<QComboBox *>("executionBrandComboBox")); restarted.findChild<QPushButton *>("executionBuyButton")->click(); settle(); auto *dialog=restarted.findChild<AddTransactionDialog *>(); QVERIFY(dialog); QCOMPARE(dialog->description(),QStringLiteral("键盘")); dialog->reject(); settle(); QCOMPARE(restarted.findChild<ExecutionWidget *>()->activeSessionId(),sessionId);
    restarted.findChild<QPushButton *>("executionBuyButton")->click(); settle(); dialog=restarted.findChild<AddTransactionDialog *>(); QVERIFY(dialog); dialog->findChild<QDoubleSpinBox *>("amountSpinBox")->setValue(249.99); dialog->findChild<QPushButton *>("saveButton")->click(); settle(); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),1); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks"),2); QCOMPARE(restarted.findChild<ExecutionWidget *>()->activeSessionId(),0);
    restarted.showPage(MainWindow::Page::Review); settle(); QVERIFY(!restarted.findChild<QTableWidget *>("purchaseReviewTable"));
}

void WorkflowTests::reviewAndPriceUiUseRealData()
{
    QTemporaryDir directory; const auto path=directory.filePath("ui-review-price.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize()); ShoppingProfileRepository profile(database); auto trusted=brand(); QVERIFY(profile.saveBrand(trusted)); PurchaseRepository purchases(database); auto old=purchase(QStringLiteral("每天在用的键盘"),40); QVERIFY(purchases.record(old,trusted.id));
    MainWindow window(path); window.resize(1180,860); window.show(); settle();
    QVERIFY(!window.findChild<QPushButton *>("homeReviewButton"));
    QVERIFY(!window.findChild<QTableWidget *>("purchaseReviewTable"));
    qint64 due=0; ReviewRepository legacy(database); QVERIFY(legacy.dueCount(due)); QCOMPARE(due,2);
    QList<BrandEntry> brands; QVERIFY(profile.loadBrands(brands)); QCOMPARE(brands[0].weight,5);
    QDialog *dialog=nullptr;
    window.showPage(MainWindow::Page::Home); window.findChild<QPushButton *>("homeDecisionButton")->click(); settle(); auto *input=window.findChild<DecisionDialog *>(); QVERIFY(input); input->findChild<QPlainTextEdit *>("decisionInput")->setPlainText(QStringLiteral("准备蹲的耳机")); input->findChild<QPushButton *>("decisionManualButton")->click(); input->findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(300); input->findChild<QPushButton *>("decisionContinueButton")->click(); input->findChild<QPushButton *>("decisionTrackButton")->click(); input->findChild<QPushButton *>("decisionCommitButton")->click(); settle(); QCOMPARE(window.currentPage(),MainWindow::Page::Wishlist); auto *pricePanel=window.findChild<WishPriceDialog *>(); QVERIFY(pricePanel); auto *tracks=window.findChild<QTableWidget *>("wishlistTable"); QVERIFY(tracks->rowCount()>=1);
    pricePanel->findChild<QPushButton *>("wishPriceTargetButton")->click(); settle(); dialog=window.findChild<QDialog *>("wishPriceTargetDialog"); QVERIFY(dialog); dialog->findChild<QDoubleSpinBox *>("wishTargetAmountSpinBox")->setValue(250); dialog->findChild<QPushButton *>("saveWishPriceTargetButton")->click(); settle();
    pricePanel->findChild<QPushButton *>("recordWishPriceButton")->click(); settle(); dialog=window.findChild<QDialog *>("priceRecordDialog"); QVERIFY(dialog); dialog->findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(230); dialog->findChild<QLineEdit *>("priceSourceEdit")->setText(QStringLiteral("京东手动记录")); dialog->findChild<QPushButton *>("savePriceRecordButton")->click(); settle(); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM price_records"),2); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),1); screenshot(*pricePanel,"prices"); pricePanel->reject(); settle();
    window.showPage(MainWindow::Page::Home); QVERIFY(!window.findChild<QWidget *>("workflowOverview"));
    PriceRepository prices(database); QList<PriceTrack> rows; QVERIFY(prices.load(rows)); QVERIFY(rows[0].targetHit); QVERIFY(rows[0].priceDrop);
    window.showPage(MainWindow::Page::Wishlist); tracks->selectRow(0); settle(); window.findChild<QPushButton *>("wishPriceButton")->click(); settle();
    pricePanel=window.findChild<WishPriceDialog *>(); QVERIFY(pricePanel);
    QVERIFY(pricePanel->findChild<QLabel *>("wishPriceSummaryLabel")->text().contains(QStringLiteral("¥230.00")));
    pricePanel->reject(); settle();
    window.showPage(MainWindow::Page::Settings); window.findChild<QPushButton *>("darkThemeButton")->click(); window.showPage(MainWindow::Page::Wishlist); tracks->selectRow(0); settle(); window.findChild<QPushButton *>("wishPriceButton")->click(); settle(); const auto chart=window.findChild<QWidget *>("priceTrendChart")->grab().toImage(); QVERIFY(chart.pixelColor(0,0).lightness()<100); screenshot(window,"prices-dark");
}

void WorkflowTests::advisorUsesContextAndRestoreCancelsResponse()
{
    QTemporaryDir directory; const auto path=directory.filePath("advisor.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize()); BudgetRepository budgets(database); QVERIFY(budgets.saveSplit(QDate::currentDate(),10000,80000)); ShoppingProfileRepository profile(database); QVERIFY(profile.saveRules({QStringLiteral("核对自己的实际需要")})); auto trusted=brand(); QVERIFY(profile.saveBrand(trusted));
    AdvisorServer server; QVERIFY(server.server.isListening()); QString error; QVERIFY(AISettingsStore::save(path+".ui.ini",server.settings(),error)); DatabaseBackupService backup(database); const auto saved=directory.filePath("before-session.sqlite"); QVERIFY(backup.backup(saved,error));
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Execution); window.findChild<QLineEdit *>("executionNameEdit")->setText(QStringLiteral("耳机")); window.findChild<QDoubleSpinBox *>("executionEstimatedAmountSpinBox")->setValue(200); window.findChild<QPushButton *>("generateAIBriefButton")->click();
    auto *chat=window.findChild<DecisionChatWidget *>();
    QTRY_VERIFY_WITH_TIMEOUT(chat->plainText().contains(server.answer),5000); QCOMPARE(server.requests.size(),1); const auto json=QJsonDocument::fromJson(server.requests[0].mid(server.requests[0].indexOf("\r\n\r\n")+4)); const auto context=json["messages"][0]["content"].toString(); QVERIFY(context.contains(QStringLiteral("¥800.00"))); QVERIFY(context.contains(QStringLiteral("核对自己的实际需要"))); QVERIFY(!context.contains("Fixture Brand"));
    reveal(chat); screenshot(window,"advice");
    QVERIFY(budgets.saveSplit(QDate::currentDate(),10000,70000)); QVERIFY(window.findChild<ExecutionWidget *>()->refreshReferences()); QVERIFY(chat->plainText().isEmpty());
    window.findChild<QPushButton *>("startExecutionButton")->click(); QVERIFY(window.findChild<ExecutionWidget *>()->activeSessionId()>0); server.delay=250; window.findChild<QPlainTextEdit *>("decisionQuestionEdit")->setPlainText(QStringLiteral("还应核实什么？")); window.findChild<QPushButton *>("askDecisionAIButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),2,5000);
    window.showPage(MainWindow::Page::Data); auto *management=window.findChild<DataManagementWidget *>(); QVERIFY(management->requestRestore(saved)); settle(); auto *confirmation=window.findChild<QMessageBox *>("restoreConfirmationDialog"); QVERIFY(confirmation); confirmation->button(QMessageBox::Yes)->click(); settle(); QTest::qWait(300); QCOMPARE(window.findChild<ExecutionWidget *>()->activeSessionId(),0); QVERIFY(chat->plainText().isEmpty()); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),0);
}
QTEST_MAIN(WorkflowTests)
#include "workflow_tests.moc"
