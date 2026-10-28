#include <QAction>
#include "AIService.h"
#include "AISettings.h"
#include "AddTransactionDialog.h"
#include "BudgetRepository.h"
#include "CategoryCatalog.h"
#include "DatabaseBackupService.h"
#include "DecisionDialog.h"
#include "ExecutionWidget.h"
#include "ProductRepository.h"
#include "PriceRepository.h"
#include "AppTheme.h"
#include <QDateEdit>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include "TransactionRepository.h"
#include "WishlistRepository.h"
#include "WishPoolRepository.h"
#include "mainwindow.h"
#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSettings>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

static QByteArray imageBytes()
{
    QImage image(360,180,QImage::Format_RGB32); image.fill(QColor("#f5f0e8"));
    QByteArray result; QBuffer buffer(&result); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"JPEG",80); return result;
}
static QByteArray envelope(const QJsonObject &product)
{
    return QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",QString::fromUtf8(QJsonDocument(product).toJson(QJsonDocument::Compact))}}}}}}}).toJson(QJsonDocument::Compact);
}
struct FakeAI {
    QTcpServer server;
    QList<QByteArray> requests;
    int status=200, delay=0;
    QByteArray response=envelope(QJsonObject{{"name",QStringLiteral("蓝牙耳机")},{"amount","299.00"},{"category","online_shopping"},{"note",QStringLiteral("用于通勤")}});
    FakeAI() {
        server.listen(QHostAddress::LocalHost);
        QObject::connect(&server,&QTcpServer::newConnection,&server,[this] {
            while (server.hasPendingConnections()) {
                auto *socket=server.nextPendingConnection();
                QObject::connect(socket,&QTcpSocket::readyRead,socket,[this,socket] {
                    auto data=socket->property("request").toByteArray()+socket->readAll(); socket->setProperty("request",data);
                    const auto end=data.indexOf("\r\n\r\n");
                    if (end<0 || socket->property("responding").toBool()) return;
                    const auto length=QRegularExpression(QStringLiteral("content-length: *([0-9]+)"),QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(data.left(end)));
                    if (!length.hasMatch() || data.size()<end+4+length.captured(1).toInt()) return;
                    socket->setProperty("responding",true); requests.append(data);
                    const auto body=response; const auto code=status;
                    QTimer::singleShot(delay,socket,[socket,body,code] {
                        if (socket->state()==QAbstractSocket::UnconnectedState) return;
                        socket->write("HTTP/1.1 "+QByteArray::number(code)+" Test\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
                        socket->disconnectFromHost();
                    });
                });
                QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            }
        });
    }
    AISettings settings() const { AISettings result; result.baseUrl=QStringLiteral("http://127.0.0.1:%1/v1").arg(server.serverPort()); result.model="test-vision"; result.apiKey="local-test-key"; return result; }
};
static void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
static qint64 count(DatabaseManager &database,const QString &table)
{
    QSqlQuery query(database.database()); if (!query.exec("SELECT COUNT(*) FROM "+table) || !query.next()) return -1; return query.value(0).toLongLong();
}
static WishItem wish(const QString &name,qint64 cents=10000)
{
    WishItem result; result.name=name; result.estimatedCents=cents; result.category=QStringLiteral("购物"); result.detailCategory="online_shopping"; return result;
}
static void choose(DecisionDialog *dialog,const char *button)
{
    if (dialog->property("decisionStage").toInt()==1) dialog->findChild<QPushButton *>("decisionContinueButton")->click();
    dialog->findChild<QPushButton *>(button)->click(); dialog->findChild<QPushButton *>("decisionCommitButton")->click();
}
class DecisionTests:public QObject {
    Q_OBJECT
private slots:
    void splitBudgetsAndCategoryCorrection();
    void poolPendingIdempotencyAndLifecycle();
    void backupRoundTripIncludesNewData();
    void legacyBackupUpgradesWithoutChangingSource();
    void encryptedSettingsAndStrictParsing();
    void networkImageErrorsAndCancellation();
    void threeChoicesThroughMainWindow();
    void failuresAndCancelDoNotWrite();
    void progressiveChoicesAndBack();
    void sharedEntriesAndResearchHandoff();
};

void DecisionTests::splitBudgetsAndCategoryCorrection()
{
    QTemporaryDir directory; const auto path=directory.filePath("split.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize());
    BudgetRepository budgets(database); TransactionRepository expenses(database); const auto date=QDate::currentDate();
    QVERIFY(budgets.saveSplit(date,4000,6000));
    Transaction basic(0,QStringLiteral("洗发水"),1000,QStringLiteral("购物"),date,{}, {},"daily_supplies");
    Transaction discretionary(0,QStringLiteral("衣服"),2000,QStringLiteral("购物"),date,{}, {},"online_shopping");
    Transaction food(0,QStringLiteral("食堂"),750,QStringLiteral("餐饮"),date,{}, {},"canteen");
    Transaction ride(0,QStringLiteral("地铁"),100,QStringLiteral("交通"),date);
    for (auto *record : {&basic,&discretionary,&food,&ride}) QVERIFY(expenses.add(*record));
    MonthlyBudgetStatus status; QVERIFY(budgets.loadStatus(date,status));
    QVERIFY(status.hasSplit); QCOMPARE(status.limitCents,10000); QCOMPARE(status.basicSpentCents,1850); QCOMPARE(status.discretionarySpentCents,2000);
    Transaction corrected(basic.id(),basic.description(),1000,QStringLiteral("购物"),date,{}, {},"online_shopping");
    QVERIFY(expenses.update(corrected)); QVERIFY(budgets.loadStatus(date,status)); QCOMPARE(status.basicSpentCents,850); QCOMPARE(status.discretionarySpentCents,3000);
    QVERIFY(!budgets.saveSplit(date,MonthlyBudget::MaximumLimitCents,1));
    QVERIFY(budgets.loadStatus(date,status)); QCOMPARE(status.limitCents,10000);
    QVERIFY(budgets.saveSplit(date,0,0)); QVERIFY(budgets.loadStatus(date,status)); QVERIFY(status.hasSplit); QCOMPARE(status.limitCents,0); QVERIFY(status.isOverBudget());
    QVERIFY(budgets.save(MonthlyBudget(date,5000))); QVERIFY(budgets.loadStatus(date,status)); QVERIFY(!status.hasSplit); QCOMPARE(status.limitCents,5000);
    DatabaseManager reopened(path); QVERIFY(reopened.initialize()); TransactionRepository again(reopened); QList<Transaction> rows; QVERIFY(again.findAll(rows));
    QCOMPARE(rows.size(),4); for (const auto &record : rows) if (record.id()==basic.id()) QCOMPARE(record.detailCategory(),QStringLiteral("online_shopping"));
}

void DecisionTests::poolPendingIdempotencyAndLifecycle()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("pool.sqlite")); QVERIFY(database.initialize());
    WishPoolRepository pool(database); WishlistRepository wishes(database); WishPoolSnapshot state;
    QVERIFY(pool.addSaving(500,QStringLiteral("忍住奶茶"),"one")); QVERIFY(pool.addSaving(500,QStringLiteral("忍住奶茶"),"one"));
    QVERIFY(!pool.addSaving(600,QStringLiteral("不同内容"),"one")); QVERIFY(pool.load(state)); QCOMPARE(state.pendingCents,500); QCOMPARE(count(database,"savings_records"),1);
    auto first=wish(QStringLiteral("键盘"),1000), second=wish(QStringLiteral("耳机"),2000);
    QVERIFY(wishes.add(first)); QVERIFY(wishes.add(second)); QVERIFY(pool.setGoal(first.id)); QVERIFY(pool.load(state)); QCOMPARE(state.savedCents,500); QCOMPARE(state.pendingCents,0);
    QVERIFY(!pool.setGoal(second.id)); QVERIFY(pool.addSaving(600,QStringLiteral("忍住零食"),"two")); QVERIFY(pool.load(state)); QCOMPARE(state.savedCents,1100);
    QSqlQuery failure(database.database());
    QVERIFY(failure.exec("CREATE TRIGGER refuse_goal BEFORE INSERT ON wish_pools BEGIN SELECT RAISE(FAIL,'test'); END"));
    QVERIFY(!pool.setGoal(second.id,true)); QVERIFY(pool.load(state)); QCOMPARE(state.wishId,first.id); QCOMPARE(state.savedCents,1100);
    QVERIFY(failure.exec("DROP TRIGGER refuse_goal"));
    Transaction expense(0,QStringLiteral("键盘"),1000,QStringLiteral("购物"),QDate::currentDate());
    QVERIFY(!wishes.purchase(first.id,expense)); QCOMPARE(count(database,"transactions"),0);
    QVERIFY(wishes.abandon(first.id)); QVERIFY(pool.load(state)); QCOMPARE(state.id,0);
    QVERIFY(pool.setGoal(second.id)); QVERIFY(pool.load(state)); QCOMPARE(state.savedCents,0);
    QList<SavingRecord> history; QVERIFY(pool.history(history)); QCOMPARE(history.size(),2); QCOMPARE(history[0].goalName,QStringLiteral("键盘"));
    auto third=wish(QStringLiteral("教材")); QVERIFY(wishes.add(third)); QVERIFY(pool.setGoal(third.id,true)); QVERIFY(wishes.remove(third.id)); QVERIFY(pool.load(state)); QCOMPARE(state.id,0);
    QSqlQuery lock(database.database()); QVERIFY(lock.exec("CREATE TRIGGER refuse_saving BEFORE INSERT ON savings_records BEGIN SELECT RAISE(FAIL,'test'); END"));
    QVERIFY(!pool.addSaving(100,QStringLiteral("失败的录入"),"retry")); QCOMPARE(count(database,"savings_records"),2);
    QVERIFY(lock.exec("DROP TRIGGER refuse_saving")); QVERIFY(pool.addSaving(100,QStringLiteral("失败的录入"),"retry")); QCOMPARE(count(database,"savings_records"),3);
    QVERIFY(pool.load(state)); QCOMPARE(state.pendingCents,100);
    auto ready=wish(QStringLiteral("到期可购买"),200); QVERIFY(wishes.add(ready,QDateTime::currentDateTimeUtc().addDays(-4))); QVERIFY(pool.setGoal(ready.id));
    Transaction bought(0,ready.name,200,QStringLiteral("购物"),QDate::currentDate());
    QVERIFY(failure.exec("CREATE TRIGGER refuse_close BEFORE UPDATE ON wish_pools BEGIN SELECT RAISE(FAIL,'test'); END"));
    QVERIFY(!wishes.purchase(ready.id,bought)); QCOMPARE(count(database,"transactions"),0); QCOMPARE(bought.id(),0);
    QVERIFY(pool.load(state)); QCOMPARE(state.wishId,ready.id);
    QVERIFY(failure.exec("DROP TRIGGER refuse_close")); QVERIFY(wishes.purchase(ready.id,bought));
    QVERIFY(pool.load(state)); QCOMPARE(state.id,0); QCOMPARE(count(database,"transactions"),1);
}

void DecisionTests::backupRoundTripIncludesNewData()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("all.sqlite")); QVERIFY(database.initialize());
    TransactionRepository expenses(database); WishlistRepository wishes(database); WishPoolRepository pool(database); BudgetRepository budgets(database);
    auto item=wish(QStringLiteral("有截图的耳机")); item.sourceImage=imageBytes(); QVERIFY(wishes.add(item)); QVERIFY(pool.setGoal(item.id)); QVERIFY(pool.addSaving(1234,QStringLiteral("忍住外卖"),"saved-token"));
    Transaction record(0,QStringLiteral("洗衣液"),875,QStringLiteral("购物"),QDate::currentDate(),QTime::currentTime(),QStringLiteral("截图备注"),"daily_supplies",imageBytes());
    QVERIFY(expenses.add(record)); QVERIFY(budgets.saveSplit(QDate::currentDate(),5000,15000));
    DatabaseBackupService backup(database); QString error,safety; BackupInfo info; const auto path=directory.filePath("backup.sqlite");
    QVERIFY2(backup.backup(path,error),qPrintable(error)); QVERIFY2(backup.prepareRestore(path,info,error),qPrintable(error)); QCOMPARE(info.poolCount,1); QCOMPARE(info.savingCount,1);
    QVERIFY(pool.addSaving(999,QStringLiteral("备份之后"),"after")); QVERIFY(wishes.remove(item.id));
    QVERIFY2(backup.restorePrepared(safety,error),qPrintable(error));
    WishPoolSnapshot state; QVERIFY(pool.load(state)); QCOMPARE(state.savedCents,1234); QCOMPARE(state.wishId,item.id);
    QVERIFY(pool.addSaving(1234,QStringLiteral("忍住外卖"),"saved-token")); QCOMPARE(count(database,"savings_records"),1);
    QList<Transaction> rows; QVERIFY(expenses.findAll(rows)); QCOMPARE(rows.size(),1); QCOMPARE(rows[0].sourceImage(),imageBytes()); QCOMPARE(rows[0].detailCategory(),QStringLiteral("daily_supplies"));
    QList<WishItem> items; QVERIFY(wishes.findAll(items)); QCOMPARE(items[0].sourceImage,imageBytes());
    MonthlyBudgetStatus status; QVERIFY(budgets.loadStatus(QDate::currentDate(),status)); QVERIFY(status.hasSplit); QCOMPARE(status.basicLimitCents,5000); QCOMPARE(status.basicSpentCents,875);
}

void DecisionTests::legacyBackupUpgradesWithoutChangingSource()
{
    QTemporaryDir directory; const auto oldPath=directory.filePath("old.sqlite");
    { DatabaseManager old(oldPath); QVERIFY(old.initialize()); TransactionRepository expenses(old); Transaction record(0,QStringLiteral("旧午饭"),2800,QStringLiteral("餐饮"),QDate::currentDate()); QVERIFY(expenses.add(record));
      BudgetRepository budgets(old); QVERIFY(budgets.save(MonthlyBudget(QDate::currentDate(),10000)));
      QSqlQuery query(old.database());
      for (const auto &sql : {"DROP TABLE bill_import_keys","DROP TABLE transaction_reviews","DROP TABLE monthly_stories","DROP TABLE price_records","DROP TABLE price_tracks","DROP TABLE review_tasks","DROP TABLE execution_sessions","DROP TABLE brands","DROP TABLE shopping_rules","DROP TABLE savings_records","DROP TABLE wish_pools","ALTER TABLE transactions DROP COLUMN detail_category","ALTER TABLE transactions DROP COLUMN source_image","ALTER TABLE wishlist DROP COLUMN detail_category","ALTER TABLE wishlist DROP COLUMN source_image","ALTER TABLE monthly_budgets DROP COLUMN basic_limit_cents","ALTER TABLE monthly_budgets DROP COLUMN discretionary_limit_cents"}) QVERIFY(query.exec(sql)); }
    QFile oldFile(oldPath); QVERIFY(oldFile.open(QIODevice::ReadOnly)); const auto before=oldFile.readAll(); oldFile.close();
    DatabaseManager current(directory.filePath("current.sqlite")); QVERIFY(current.initialize()); auto item=wish(QStringLiteral("当前目标")); WishlistRepository wishes(current); QVERIFY(wishes.add(item));
    WishPoolRepository pools(current); QVERIFY(pools.setGoal(item.id)); QVERIFY(pools.addSaving(500,QStringLiteral("旧状态"),"old-state"));
    DatabaseBackupService backup(current); QString error,safety; BackupInfo info;
    QVERIFY2(backup.prepareRestore(oldPath,info,error),qPrintable(error)); QCOMPARE(info.poolCount,0); QVERIFY(backup.restorePrepared(safety,error));
    QVERIFY(oldFile.open(QIODevice::ReadOnly)); QCOMPARE(oldFile.readAll(),before);
    TransactionRepository expenses(current); QList<Transaction> rows; QVERIFY(expenses.findAll(rows)); QCOMPARE(rows.size(),1); QCOMPARE(rows[0].category(),QStringLiteral("餐饮")); QCOMPARE(rows[0].detailCategory(),QStringLiteral("daily"));
    MonthlyBudgetStatus status; BudgetRepository budgets(current); QVERIFY(budgets.loadStatus(QDate::currentDate(),status)); QVERIFY(!status.hasSplit); QCOMPARE(status.limitCents,10000);
    WishPoolSnapshot state; QVERIFY(pools.load(state)); QCOMPARE(state.id,0); QCOMPARE(state.pendingCents,0);
}

void DecisionTests::encryptedSettingsAndStrictParsing()
{
    QCOMPARE(AISettings{}.model,QStringLiteral("qwen3-vl-plus"));
    QVERIFY(AISettings{}.apiKey.isEmpty());
    QTemporaryDir directory; const auto path=directory.filePath("settings.ini"); AISettings settings; settings.apiKey="local-fixture-secret"; QString error;
    QVERIFY2(AISettingsStore::save(path,settings,error),qPrintable(error)); QCOMPARE(AISettingsStore::load(path).apiKey,settings.apiKey);
    QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(!file.readAll().contains(settings.apiKey.toUtf8())); file.close();
    settings.apiKey.clear(); QVERIFY(AISettingsStore::save(path,settings,error)); QVERIFY(AISettingsStore::load(path).apiKey.isEmpty());
    const auto legacyPath=directory.filePath("legacy.ini"); AISettings legacy; legacy.model="qwen-vl-plus"; legacy.apiKey="legacy-fixture-key";
    QVERIFY(AISettingsStore::save(legacyPath,legacy,error));
    { QSettings oldVersion(legacyPath,QSettings::IniFormat); oldVersion.remove("ai/modelSelectionVersion"); oldVersion.sync(); }
    QFile legacyFile(legacyPath); QVERIFY(legacyFile.open(QIODevice::ReadOnly)); const auto legacyBytes=legacyFile.readAll(); legacyFile.close();
    const auto upgraded=AISettingsStore::load(legacyPath); QCOMPARE(upgraded.model,QStringLiteral("qwen3-vl-plus")); QCOMPARE(upgraded.apiKey,legacy.apiKey);
    QVERIFY(legacyFile.open(QIODevice::ReadOnly)); QCOMPARE(legacyFile.readAll(),legacyBytes); legacyFile.close();
    legacy.baseUrl="https://example.test/v1"; QVERIFY(AISettingsStore::save(legacyPath,legacy,error)); QCOMPARE(AISettingsStore::load(legacyPath).model,QStringLiteral("qwen-vl-plus"));
    legacy.baseUrl=AISettings{}.baseUrl; legacy.model="qwen3-vl-32b-instruct"; QVERIFY(AISettingsStore::save(legacyPath,legacy,error)); QCOMPARE(AISettingsStore::load(legacyPath).model,legacy.model);
    legacy.model="qwen-vl-plus"; QVERIFY(AISettingsStore::save(legacyPath,legacy,error)); QCOMPARE(AISettingsStore::load(legacyPath).model,legacy.model);
    ParsedProduct product;
    QJsonObject good{{"name",QStringLiteral("奶茶")},{"amount","0.01"},{"category","daily"},{"note",""}};
    QVERIFY(AIService::decodeProduct(envelope(good),product,error)); QCOMPARE(product.amountCents,1);
    good["amount"]=QJsonValue::Null; QVERIFY(AIService::decodeProduct(envelope(good),product,error)); QCOMPARE(product.amountCents,0);
    for (const auto &bad : QJsonArray{true,-5,12.345,"NaN","1e4","100000000","0.001"}) { good["amount"]=bad; QVERIFY(!AIService::decodeProduct(envelope(good),product,error)); }
    good["amount"]="10.00"; good["name"]=""; QVERIFY(!AIService::decodeProduct(envelope(good),product,error));
    settings.baseUrl="http://example.com/v1"; QVERIFY(!settings.validationError().isEmpty()); settings.baseUrl="https://example.com/v1"; settings.apiKey="bad\nkey"; QVERIFY(!settings.validationError().isEmpty());
}

void DecisionTests::networkImageErrorsAndCancellation()
{
    FakeAI server; QVERIFY(server.server.isListening()); AIService service; QSignalSpy parsed(&service,&AIService::parsed),failed(&service,&AIService::failed);
    QVERIFY(service.analyze(server.settings(),QStringLiteral("想买耳机"),imageBytes())); QVERIFY(!service.analyze(server.settings(),"duplicate",{}));
    QTRY_COMPARE_WITH_TIMEOUT(parsed.count(),1,5000); QCOMPARE(failed.count(),0); QCOMPARE(server.requests.size(),1);
    const auto request=server.requests[0]; QVERIFY(request.startsWith("POST /v1/chat/completions ")); QVERIFY(request.contains("Authorization: Bearer local-test-key")); QVERIFY(request.contains("data:image/jpeg;base64,"));
    QCOMPARE(qvariant_cast<ParsedProduct>(parsed.takeFirst()[0]).amountCents,29900);
    server.status=401; QVERIFY(service.analyze(server.settings(),"retry",{})); QTRY_COMPARE_WITH_TIMEOUT(failed.count(),1,5000); QVERIFY(failed.takeFirst()[0].toString().contains("API Key"));
    server.status=200; server.delay=200;
    QVERIFY(service.analyze(server.settings(),"cancel",{})); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),3,5000); service.cancel(); QVERIFY(!service.busy()); QTest::qWait(250); QCOMPARE(parsed.count(),0); QCOMPARE(failed.count(),0);
    auto invalid=server.settings(); invalid.apiKey.clear(); QVERIFY(!service.analyze(invalid,"no-key",{})); QCOMPARE(server.requests.size(),3); QCOMPARE(failed.count(),1);
}

void DecisionTests::threeChoicesThroughMainWindow()
{
    QTemporaryDir directory; const auto path=directory.filePath("ui.sqlite"); FakeAI server; QString error;
    QVERIFY(AISettingsStore::save(path+".ui.ini",server.settings(),error));
    MainWindow window(path); window.resize(1180,860); window.show(); settle();
    DatabaseManager database(path); QVERIFY(database.initialize());
    const QString preview=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!preview.isEmpty()) QVERIFY(QDir().mkpath(preview));
    window.findChild<QPushButton *>("homeDecisionButton")->click(); settle();
    auto *dialog=window.findChild<DecisionDialog *>(); QVERIFY(dialog && dialog->isVisible());
    const auto imagePath=directory.filePath("product.jpg"); QFile image(imagePath); QVERIFY(image.open(QIODevice::WriteOnly)); image.write(imageBytes()); image.close();
    QVERIFY(dialog->setScreenshot(imagePath)); dialog->findChild<QPushButton *>("decisionAnalyzeButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(dialog->findChild<QLineEdit *>("decisionNameEdit")->text(),QStringLiteral("蓝牙耳机"),5000);
    dialog->findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(129.99);
    settle();
    if (!preview.isEmpty()) QVERIFY(dialog->grab().save(preview+"/decision-confirm.png"));
    dialog->findChild<QSpinBox *>("decisionCoolingDaysSpinBox")->setValue(7); choose(dialog,"decisionWishButton"); settle();
    QCOMPARE(window.currentPage(),MainWindow::Page::Wishlist); QCOMPARE(count(database,"transactions"),0); QCOMPARE(count(database,"wishlist"),1); QCOMPARE(count(database,"savings_records"),0);
    QList<WishItem> items; WishlistRepository wishes(database); QVERIFY(wishes.findAll(items)); QCOMPARE(items[0].estimatedCents,12999); QCOMPARE(items[0].coolingDays,7); QVERIFY(!items[0].sourceImage.isEmpty());
    window.findChild<QTableWidget *>("wishlistTable")->selectRow(0); window.findChild<QAction *>("setWishPoolGoalAction")->trigger(); settle(); QCOMPARE(window.currentPage(),MainWindow::Page::Home);
    window.findChild<QPushButton *>("homeDecisionButton")->click(); settle(); dialog=window.findChild<DecisionDialog *>(); QVERIFY(dialog);
    dialog->findChild<QPlainTextEdit *>("decisionInput")->setPlainText(QStringLiteral("忍住奶茶")); dialog->findChild<QPushButton *>("decisionManualButton")->click();
    dialog->findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(15); choose(dialog,"decisionHoldButton"); choose(dialog,"decisionHoldButton"); settle();
    WishPoolRepository pool(database); WishPoolSnapshot state; QVERIFY(pool.load(state)); QCOMPARE(state.savedCents,1500); QCOMPARE(count(database,"savings_records"),1); QCOMPARE(count(database,"transactions"),0); QCOMPARE(count(database,"wishlist"),1);
    window.findChild<QPushButton *>("homeDecisionButton")->click(); settle(); dialog=window.findChild<DecisionDialog *>(); QVERIFY(dialog);
    dialog->findChild<QPlainTextEdit *>("decisionInput")->setPlainText(QStringLiteral("确定买杯子")); dialog->findChild<QPushButton *>("decisionManualButton")->click();
    dialog->findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(35);
    auto *category=dialog->findChild<QComboBox *>("decisionCategoryComboBox"); category->setCurrentIndex(category->findData("daily_supplies"));
    dialog->findChild<QPushButton *>("decisionBackButton")->click(); QVERIFY(dialog->setScreenshot(imagePath)); dialog->findChild<QPushButton *>("decisionManualButton")->click(); dialog->findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(35); category->setCurrentIndex(category->findData("daily_supplies"));
    choose(dialog,"decisionBuyButton"); settle(); QCOMPARE(count(database,"transactions"),1); QCOMPARE(count(database,"wishlist"),1);
    QList<Transaction> expenses; TransactionRepository records(database); QVERIFY(records.findAll(expenses)); QCOMPARE(expenses[0].detailCategory(),QStringLiteral("daily_supplies")); QVERIFY(!expenses[0].sourceImage().isEmpty());
    AddTransactionDialog editor(expenses[0]); QCOMPARE(editor.transaction().sourceImage(),expenses[0].sourceImage()); QCOMPARE(editor.transaction().detailCategory(),expenses[0].detailCategory());
    QVERIFY(pool.load(state)); QCOMPARE(state.savedCents,1500);
    window.showPage(MainWindow::Page::Billing); window.findChild<QDoubleSpinBox *>("basicBudgetAmountSpinBox")->setValue(100); window.findChild<QDoubleSpinBox *>("budgetAmountSpinBox")->setValue(200); window.findChild<QPushButton *>("saveBudgetButton")->click(); window.showPage(MainWindow::Page::Home); settle();
    QVERIFY(window.findChild<QLabel *>("homeBasicBudgetLabel")->text().contains(QStringLiteral("¥65.00")));
    if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+"/decision-home.png"));
    window.showPage(MainWindow::Page::Settings); settle(); if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+"/decision-settings.png"));
}

void DecisionTests::failuresAndCancelDoNotWrite()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("cancel.sqlite")); QVERIFY(database.initialize());
    AISettings missing; DecisionDialog dialog(database,missing); dialog.show(); settle();
    dialog.findChild<QPlainTextEdit *>("decisionInput")->setPlainText(QStringLiteral("一杯水")); dialog.findChild<QPushButton *>("decisionAnalyzeButton")->click();
    QVERIFY(dialog.findChild<QLabel *>("decisionErrorLabel")->text().contains("API Key")); QCOMPARE(count(database,"transactions"),0);
    dialog.findChild<QPushButton *>("decisionManualButton")->click(); dialog.findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(10);
    QSqlQuery query(database.database()); QVERIFY(query.exec("CREATE TRIGGER fail_expense BEFORE INSERT ON transactions BEGIN SELECT RAISE(FAIL,'test'); END"));
    choose(&dialog,"decisionBuyButton"); QVERIFY(dialog.isVisible()); QCOMPARE(dialog.findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->value(),10); QCOMPARE(count(database,"transactions"),0);
    QVERIFY(query.exec("DROP TRIGGER fail_expense")); choose(&dialog,"decisionBuyButton"); QCOMPARE(count(database,"transactions"),1);
    DecisionDialog cancelled(database,missing); cancelled.show(); cancelled.findChild<QPushButton *>("decisionManualButton")->click(); cancelled.reject();
    QCOMPARE(count(database,"transactions"),1); QCOMPARE(count(database,"wishlist"),0); QCOMPARE(count(database,"savings_records"),0);
}
void DecisionTests::progressiveChoicesAndBack()
{
    QTemporaryDir dir; DatabaseManager db(dir.filePath("stages.sqlite")); QVERIFY(db.initialize());
    DecisionDialog dialog(db,AISettings{}); AppTheme::apply(&dialog,"warm"); dialog.show(); settle();
    auto button=[&](const char *name) { return dialog.findChild<QPushButton *>(name); };
    auto *date=dialog.findChild<QDateEdit *>("decisionDateEdit"); auto *days=dialog.findChild<QSpinBox *>("decisionCoolingDaysSpinBox");
    const QString preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    auto shot=[&](const QString &name) { settle(); if (!preview.isEmpty()) { QDir().mkpath(preview); QVERIFY(dialog.grab().save(preview+"/purchase-"+name+".png")); } };
    QVERIFY(!button("decisionBuyButton")->isVisible()); QVERIFY(!date->isVisible()); QVERIFY(!days->isVisible()); shot("input");
    dialog.findChild<QPlainTextEdit *>("decisionInput")->setPlainText(QStringLiteral("耳机")); button("decisionManualButton")->click(); settle();
    QVERIFY(!date->isVisible()); QVERIFY(!days->isVisible()); QVERIFY(!button("decisionBuyButton")->isVisible());
    QVERIFY(!dialog.findChild<QComboBox *>("decisionBrandComboBox")->isVisible());
    dialog.findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(299); shot("confirm");
    button("decisionExtrasButton")->click(); dialog.findChild<QPlainTextEdit *>("decisionNoteEdit")->setPlainText(QStringLiteral("通勤时用"));
    button("decisionBackButton")->click(); QCOMPARE(dialog.property("decisionStage").toInt(),0);
    button("decisionAnalyzeButton")->click(); QCOMPARE(dialog.property("decisionStage").toInt(),1);
    QCOMPARE(dialog.findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->value(),299.0);
    QCOMPARE(dialog.findChild<QPlainTextEdit *>("decisionNoteEdit")->toPlainText(),QStringLiteral("通勤时用"));
    button("decisionContinueButton")->click(); QVERIFY(button("decisionBuyButton")->isVisible()); QVERIFY(!button("decisionCommitButton")->isVisible()); shot("choices");
    button("decisionBuyButton")->click(); QVERIFY(date->isVisible()); QVERIFY(!days->isVisible());
    date->setDate(QDate::currentDate().addDays(-2)); shot("buy");
    button("decisionWishButton")->click(); QVERIFY(!date->isVisible()); QVERIFY(days->isVisible()); days->setValue(7); shot("wishlist");
    button("decisionHoldButton")->click(); QVERIFY(!date->isVisible()); QVERIFY(!days->isVisible());
    QCOMPARE(count(db,"transactions"),0); QCOMPARE(count(db,"wishlist"),0); QCOMPARE(count(db,"savings_records"),0);
    button("decisionBackButton")->click(); QCOMPARE(dialog.findChild<QPlainTextEdit *>("decisionNoteEdit")->toPlainText(),QStringLiteral("通勤时用"));
    dialog.findChild<QLineEdit *>("decisionNameEdit")->setText(QStringLiteral("通勤耳机"));
    button("decisionContinueButton")->click(); QVERIFY(dialog.findChild<QLabel *>("decisionSummaryLabel")->text().contains(QStringLiteral("通勤耳机")));
    button("decisionBuyButton")->click();
    for (const auto &theme:{"warm","cool","dark"}) { AppTheme::apply(&dialog,theme); dialog.resize(450,500); shot(QStringLiteral("%1-small").arg(theme)); QCOMPARE(dialog.findChild<QScrollArea *>()->horizontalScrollBar()->maximum(),0); }
    button("decisionCommitButton")->click(); button("decisionCommitButton")->click(); QCOMPARE(count(db,"transactions"),1); QCOMPARE(count(db,"review_tasks"),2);
    QList<Transaction> records; TransactionRepository repo(db); QVERIFY(repo.findAll(records)); QCOMPARE(records[0].date(),QDate::currentDate().addDays(-2)); QCOMPARE(records[0].note(),QStringLiteral("通勤时用"));
}

void DecisionTests::sharedEntriesAndResearchHandoff()
{
    QTemporaryDir dir; const auto path=dir.filePath("shared.sqlite"); DatabaseManager db(path); QVERIFY(db.initialize());
    MainWindow window(path); window.show(); settle();
    window.findChild<QPushButton *>("homeDecisionButton")->click(); settle();
    auto *home=window.findChild<DecisionDialog *>(); QVERIFY(home); QCOMPARE(home->property("decisionStage").toInt(),0); home->reject(); settle();
    window.showPage(MainWindow::Page::Execution); QVERIFY(!window.findChild<QLineEdit *>("executionNameEdit")->isVisible());
    window.findChild<QPushButton *>("executionInputButton")->click(); settle(); auto *dialog=window.findChild<DecisionDialog *>(); QVERIFY(dialog);
    QCOMPARE(dialog->property("decisionStage").toInt(),0);
    const auto imagePath=dir.filePath("item.png"); QImage image(180,100,QImage::Format_RGB32); image.fill(Qt::gray); QVERIFY(image.save(imagePath)); QVERIFY(dialog->setScreenshot(imagePath));
    dialog->findChild<QPlainTextEdit *>("decisionInput")->setPlainText(QStringLiteral("耳机")); dialog->findChild<QPushButton *>("decisionManualButton")->click();
    dialog->findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(299); dialog->findChild<QPlainTextEdit *>("decisionNoteEdit")->setPlainText(QStringLiteral("通勤"));
    dialog->findChild<QPushButton *>("decisionContinueButton")->click(); dialog->findChild<QPushButton *>("decisionResearchButton")->click(); settle();
    QVERIFY(!window.findChild<DecisionDialog *>()); QCOMPARE(window.currentPage(),MainWindow::Page::Execution);
    QCOMPARE(window.findChild<QLineEdit *>("executionNameEdit")->text(),QStringLiteral("耳机")); QCOMPARE(window.findChild<QDoubleSpinBox *>("executionEstimatedAmountSpinBox")->value(),299.0);
    QCOMPARE(window.findChild<QLineEdit *>("executionPurposeEdit")->text(),QStringLiteral("通勤"));
    QCOMPARE(count(db,"execution_sessions"),0); QCOMPARE(count(db,"transactions"),0); QCOMPARE(count(db,"wishlist"),0);
    window.findChild<QPushButton *>("startExecutionButton")->click(); settle(); QCOMPARE(count(db,"execution_sessions"),1);
    auto *execution=window.findChild<ExecutionWidget *>(); const auto session=execution->activeSessionId(); QVERIFY(session>0);
    ProductRepository products(db); ProductContext context; ProductProfile profile; QList<ProductEvidence> evidence;
    QVERIFY(products.context("execution",session,context)); QVERIFY(products.ensure(context,profile)); QVERIFY(products.evidence(profile.id,evidence)); QVERIFY(!evidence.isEmpty()); QVERIFY(!evidence.last().image.isEmpty());
    const auto originalImage=evidence.last().image;
    QVERIFY(products.save(profile,imageBytes(),QJsonObject{{"note",QStringLiteral("另一张材质图")}}));
    QVERIFY(execution->reloadState()); window.findChild<QPushButton *>("executionBuyButton")->click(); settle();
    auto *purchase=window.findChild<AddTransactionDialog *>(); QVERIFY(purchase); QCOMPARE(purchase->transaction().sourceImage(),originalImage); purchase->reject(); settle();
    window.showPage(MainWindow::Page::Home); window.findChild<QPushButton *>("homeDecisionButton")->click(); settle(); dialog=window.findChild<DecisionDialog *>(); QVERIFY(dialog);
    QVERIFY(!dialog->findChild<QPushButton *>("decisionResearchButton")->isEnabled());
    QVERIFY(!execution->prepareDecision(ParsedProduct{QStringLiteral("不能覆盖"),"online_shopping",{},500})); QCOMPARE(execution->activeSessionId(),session); dialog->reject();
}

QTEST_MAIN(DecisionTests)
#include "decision_tests.moc"
