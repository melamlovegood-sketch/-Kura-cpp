#include "AIStubServer.h"
#include "AchievementCelebration.h"
#include "BillTestFixtures.h"
#include "BillImportDialog.h"
#include "MonthlyReviewWidget.h"
#include "MonthlyReviewRepository.h"
#include "DataManagementWidget.h"
#include "TransactionRepository.h"
#include "DatabaseBackupService.h"
#include "mainwindow.h"
#include <QCheckBox>
#include <QAction>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

static void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
static qint64 scalar(DatabaseManager &database,const QString &sql) { QSqlQuery query(database.database()); if (!query.exec(sql) || !query.next()) return -1; return query.value(0).toLongLong(); }
static void screenshot(QWidget &widget,const QString &name) { settle(); const auto directory=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!directory.isEmpty()) { QVERIFY(QDir().mkpath(directory)); QVERIFY(widget.grab().save(directory+"/supplement-"+name+".png")); } }
class SupplementUiTests:public QObject { Q_OBJECT
private slots:
    void importPreviewClassificationRollbackAndRefresh();
    void monthlyTagsStoriesSwitchAndRestore();
    void connectionProbesUseOnlyExplicitFixtureRequests();
    void invalidPersonalityKeepsSavedCard();
};

void SupplementUiTests::importPreviewClassificationRollbackAndRefresh()
{
    QTemporaryDir directory; const auto path=directory.filePath("import-ui.sqlite"); AIStubServer server; QString error; QVERIFY(AISettingsStore::save(path+".ui.ini",server.settings(),error));
    auto rows=BillTestFixtures::wechat("PRIVATE-TXN-NOT-FOR-AI",QStringLiteral("不明商品"),"6.50"); rows[2][0]=QDate::currentDate().toString(Qt::ISODate)+" 12:30:45"; rows[2][2]=QStringLiteral("某商户"); const auto csv=directory.filePath(QStringLiteral("示例账单.csv")); QVERIFY(BillTestFixtures::write(csv,BillTestFixtures::csv(rows)));
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Data); settle(); window.findChild<QPushButton *>("importBillsButton")->click(); settle(); auto *dialog=window.findChild<BillImportDialog *>(); QVERIFY(dialog && dialog->isVisible()); QSignalSpy ready(dialog,&BillImportDialog::previewReady); QVERIFY(dialog->loadFile(csv)); QTRY_COMPARE_WITH_TIMEOUT(ready.count(),1,10000);
    DatabaseManager database(path); QVERIFY(database.initialize()); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),0); auto *table=dialog->findChild<QTableWidget *>("billImportPreviewTable"); QCOMPARE(table->rowCount(),1); QVERIFY(dialog->findChild<QPushButton *>("commitBillImportButton")->isEnabled()); QCOMPARE(server.requests.size(),0);
    dialog->findChild<QPushButton *>("classifyBillsButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),1,5000); QTRY_COMPARE_WITH_TIMEOUT(table->item(0,5)->data(Qt::UserRole).toString(),QStringLiteral("food"),5000); QVERIFY(!server.requestBodies[0].contains("PRIVATE-TXN-NOT-FOR-AI")); QVERIFY(dialog->findChild<QPushButton *>("commitBillImportButton")->isEnabled());
    table->item(0,3)->setText(QStringLiteral("已确认名称")); QVERIFY(dialog->findChild<QPushButton *>("commitBillImportButton")->isEnabled()); screenshot(*dialog,"import-preview");
    QSqlQuery query(database.database()); QVERIFY(query.exec("CREATE TRIGGER fail_import BEFORE INSERT ON transactions BEGIN SELECT RAISE(FAIL,'fixture'); END")); dialog->findChild<QPushButton *>("commitBillImportButton")->click(); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),0); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM bill_import_keys"),0); QVERIFY(dialog->findChild<QLabel *>("billImportErrorLabel")->text().contains(QStringLiteral("整批未保存"))); QVERIFY(dialog->findChild<QPushButton *>("commitBillImportButton")->isEnabled()); QVERIFY(query.exec("DROP TRIGGER fail_import"));
    dialog->findChild<QPushButton *>("commitBillImportButton")->click(); settle(); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),1); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM bill_import_keys"),1); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks"),0); QCOMPARE(dialog->findChild<QPushButton *>("commitBillImportButton")->text(),QStringLiteral("完成")); dialog->reject(); settle();
    window.showPage(MainWindow::Page::Review); QVERIFY(window.findChild<MonthlyReviewWidget *>()->isVisible()); QCOMPARE(window.findChild<QLabel *>("monthlySpentLabel")->text(),QStringLiteral("¥6.50"));
    window.showPage(MainWindow::Page::Data); window.findChild<QPushButton *>("importBillsButton")->click(); settle(); dialog=window.findChild<BillImportDialog *>(); QVERIFY(dialog); QSignalSpy repeated(dialog,&BillImportDialog::previewReady); QVERIFY(dialog->loadFile(csv)); QTRY_COMPARE_WITH_TIMEOUT(repeated.count(),1,10000); QVERIFY(!dialog->findChild<QPushButton *>("commitBillImportButton")->isEnabled()); QVERIFY(dialog->findChild<QLabel *>("billImportSummaryLabel")->text().contains(QStringLiteral("已重复 1"))); dialog->reject();
}

void SupplementUiTests::monthlyTagsStoriesSwitchAndRestore()
{
    QTemporaryDir directory; const auto path=directory.filePath("monthly-ui.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize()); const auto month=MonthlyBudget::firstDay(QDate::currentDate()); TransactionRepository expenses(database);
    Transaction food(0,QStringLiteral("食堂午饭"),1000,QStringLiteral("餐饮"),month,QTime(12,0),{},"canteen"),shopping(0,QStringLiteral("键盘"),2000,QStringLiteral("购物"),month,QTime(13,0),{},"online_shopping"),previous(0,QStringLiteral("上月地铁"),500,QStringLiteral("交通"),month.addDays(-1)); for (auto *record:{&food,&shopping,&previous}) QVERIFY(expenses.add(*record));
    AIStubServer server; QString error; QVERIFY(AISettingsStore::save(path+".ui.ini",server.settings(),error)); MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Review); settle(); auto *monthly=window.findChild<MonthlyReviewWidget *>(); QVERIFY(monthly && monthly->isVisible()); QCOMPARE(window.findChild<QLabel *>("monthlySpentLabel")->text(),QStringLiteral("¥30.00"));
    window.findChild<QPushButton *>("manageMonthlyTagsButton")->click(); settle(); auto *tags=window.findChild<QDialog *>("monthlyTagsDialog"); QVERIFY(tags); auto *table=tags->findChild<QTableWidget *>("monthlyTagTransactionsTable"); table->selectRow(0); tags->findChild<QComboBox *>("monthlyTagComboBox")->setCurrentIndex(0); tags->findChild<QLineEdit *>("monthlyTagNoteEdit")->setText(QStringLiteral("每天都在用")); tags->findChild<QPushButton *>("saveMonthlyTagButton")->click(); QCOMPARE(window.findChild<QWidget *>("monthlyWorthBoard")->findChild<QLabel *>("monthlyReviewExpenseNameLabel")->text(),QStringLiteral("键盘")); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM review_tasks"),0); screenshot(*tags,"monthly-tags"); tags->accept(); settle();
    window.findChild<QAction *>("saveLocalMonthlyStoryAction")->trigger(); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM monthly_stories"),1); DatabaseBackupService backup(database); const auto copy=directory.filePath("local-snapshot.sqlite"); QVERIFY(backup.backup(copy,error));
    window.findChild<QPushButton *>("generateMonthlyStoryButton")->click(); QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("monthlyStorySourceLabel")->text().contains(QStringLiteral("AI 解读")),5000); QVERIFY(!window.findChild<QLabel *>("monthlyStoryText")->text().contains("schema_version")); QVERIFY(window.findChild<QLabel *>("monthlyStoryPersonaLabel")->text().contains(QStringLiteral("生活拼图型"))); QCOMPARE(server.requests.size(),1);
    const auto prompt=server.requests[0].value("messages").toArray()[0].toObject().value("content").toString();
    QVERIFY(prompt.contains(QStringLiteral("松鼠型"))); QVERIFY(prompt.contains(QStringLiteral("猫头鹰型"))); QVERIFY(prompt.contains(QStringLiteral("120至180字")));
    server.delay=300; window.findChild<QAction *>("regenerateMonthlyStoryAction")->trigger(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),2,5000); window.findChild<QPushButton *>("previousReviewMonthButton")->click(); QTest::qWait(350); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM monthly_stories"),1); QCOMPARE(window.findChild<QLabel *>("monthlySpentLabel")->text(),QStringLiteral("¥5.00"));
    window.findChild<QDateEdit *>("monthlyReviewMonthEdit")->setDate(month); window.findChild<QAction *>("regenerateMonthlyStoryAction")->trigger(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),3,5000);
    // A real user acknowledges the new reward before interacting with the data page.
    if(auto *celebration=window.findChild<AchievementCelebration *>()) {
        celebration->findChild<QPushButton *>("keepAchievementButton")->click(); settle();
    }
    window.showPage(MainWindow::Page::Data); auto *management=window.findChild<DataManagementWidget *>(); QVERIFY(management->requestRestore(copy)); settle(); auto *confirm=window.findChild<QMessageBox *>("restoreConfirmationDialog"); QVERIFY(confirm); confirm->button(QMessageBox::Yes)->click(); settle(); QTest::qWait(350);
    MonthlyReviewRepository repository(database); MonthlyReviewData data; MonthlyStory story; QVERIFY(repository.load(month,data)); QVERIFY(repository.loadStory(data,story)); QCOMPARE(story.source,QStringLiteral("local")); window.showPage(MainWindow::Page::Review); QVERIFY(window.findChild<QLabel *>("monthlyStorySourceLabel")->text().contains(QStringLiteral("本地事实总结"))); screenshot(window,"monthly-home");
    Transaction edited(shopping.id(),shopping.description(),3000,shopping.category(),shopping.date(),shopping.time(),{},shopping.detailCategory()); QVERIFY(expenses.update(edited)); QVERIFY(monthly->refresh()); QVERIFY(window.findChild<QLabel *>("monthlyStorySourceLabel")->text().contains(QStringLiteral("数据已变化"))); window.findChild<QAction *>("saveLocalMonthlyStoryAction")->trigger(); QVERIFY(window.findChild<QLabel *>("monthlyStorySourceLabel")->text().contains(QStringLiteral("当前数据一致")));
}

void SupplementUiTests::connectionProbesUseOnlyExplicitFixtureRequests()
{
    QTemporaryDir directory; const auto path=directory.filePath("connection.sqlite"); AIStubServer server; MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Settings); settle(); const auto settings=server.settings(); window.findChild<QLineEdit *>("aiBaseUrlEdit")->setText(settings.baseUrl); window.findChild<QLineEdit *>("aiModelEdit")->setText(settings.model); window.findChild<QLineEdit *>("aiApiKeyEdit")->setText(settings.apiKey); QCOMPARE(server.requests.size(),0);
    window.findChild<QPushButton *>("testAITextButton")->click(); QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains(QStringLiteral("文字连接成功")),5000); QCOMPARE(server.requests.size(),1); QVERIFY(AISettingsStore::load(path+".ui.ini").apiKey.isEmpty());
    window.findChild<QPushButton *>("testAIImageButton")->click(); QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains(QStringLiteral("截图样例识别通过")),5000); QCOMPARE(server.requests.size(),2); QVERIFY(server.requestBodies[1].contains("data:image/jpeg;base64,"));
    server.status=401; window.findChild<QPushButton *>("testAITextButton")->click(); QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains("API Key"),5000); QCOMPARE(window.findChild<QLineEdit *>("aiApiKeyEdit")->text(),settings.apiKey); server.status=200; server.delay=300; window.findChild<QPushButton *>("testAITextButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),4,5000); window.findChild<QPushButton *>("cancelAIConnectionTestButton")->click(); QTest::qWait(350); QVERIFY(window.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains(QStringLiteral("已取消等待")));
    DatabaseManager database(path); QVERIFY(database.initialize()); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM transactions"),0); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM monthly_stories"),0);
}
void SupplementUiTests::invalidPersonalityKeepsSavedCard()
{
    QTemporaryDir directory; const auto path=directory.filePath("invalid-persona.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize());
    TransactionRepository transactions(database); const auto month=MonthlyBudget::firstDay(QDate::currentDate());
    Transaction expense(0,QStringLiteral("午饭"),1500,QStringLiteral("餐饮"),month,QTime(12,0),{},"canteen"); QVERIFY(transactions.add(expense));
    MonthlyReviewRepository monthly(database); MonthlyReviewData data; QVERIFY(monthly.load(month,data));
    const auto good=MonthlyStoryTemplate::local(data); const auto saved=MonthlyStoryTemplate::encode(good);
    QVERIFY(monthly.saveStory(month,data.factsHash,saved,MonthlyStoryTemplate::personaCaption(good),"local"));
    AIStubServer server; QString error; QVERIFY(AISettingsStore::save(path+".ui.ini",server.settings(),error));
    auto wrong=good; wrong.personaId="owl"; wrong.customName.clear(); wrong.subtitle=QStringLiteral("给未来充电"); wrong.evidenceIds={QString::number(expense.id())};
    server.handler=[wrong](const QJsonObject &) { return MonthlyStoryTemplate::encode(wrong); };
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Review); settle();
    const auto original=window.findChild<QLabel *>("monthlyStoryText")->text();
    window.findChild<QPushButton *>("generateMonthlyStoryButton")->click();
    QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("monthlyReviewErrorLabel")->isVisible(),5000);
    QVERIFY(window.findChild<QLabel *>("monthlyReviewErrorLabel")->text().contains(QStringLiteral("不匹配")));
    QCOMPARE(window.findChild<QLabel *>("monthlyStoryText")->text(),original);
    QVERIFY(window.findChild<QPushButton *>("generateMonthlyStoryButton")->isEnabled());
    MonthlyStory story; QVERIFY(monthly.loadStory(data,story)); QCOMPARE(story.story,saved); QCOMPARE(story.source,QStringLiteral("local"));
}

QTEST_MAIN(SupplementUiTests)
#include "supplement_ui_tests.moc"
