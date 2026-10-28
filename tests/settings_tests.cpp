#include "AIStubServer.h"
#include "AppearanceWidget.h"
#include "AutoBackupService.h"
#include "BuildInfo.h"
#include "DataManagementWidget.h"
#include "DecisionDialog.h"
#include "ExecutionWidget.h"
#include "PricePerspective.h"
#include "ShoppingProfileRepository.h"
#include "SplashScreen.h"
#include "TransactionRepository.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QSqlQuery>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QApplication::processEvents(); }
void click(QWidget &owner,const char *name) { auto *button=owner.findChild<QAbstractButton *>(name); QVERIFY2(button,name); button->click(); settle(); }
QString text(QWidget &owner,const char *name) { auto *label=owner.findChild<QLabel *>(name); return label?label->text():QString(); }
void capture(QWidget &owner,const QString &name) {
    settle(); const auto folder=qEnvironmentVariable("KURA_SETTINGS_PREVIEW_DIR");
    if (!folder.isEmpty()) { QVERIFY(QDir().mkpath(folder)); QVERIFY(owner.grab().save(folder+"/"+name+".png")); }
}
}
class SettingsTests : public QObject {
    Q_OBJECT
private slots:
    void preferencesPersistAndFollowSystem();
    void summariesAndAIDraftsUseActualState();
    void rulesStayInExecutionAndBrandsStayOut();
    void automaticBackupsRotateAndRecover();
    void automaticBackupFailureKeepsLastSuccess();
    void themesAndLargeTypeFit();
};

void SettingsTests::preferencesPersistAndFollowSystem()
{
    QTemporaryDir directory; const auto path=directory.filePath("preferences.sqlite");
    {
        MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Settings); settle();
        click(window,"settingsAppearanceButton"); click(window,"systemThemeButton");
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark); settle();
        QCOMPARE(window.property("theme").toString(),QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark?QStringLiteral("dark"):QStringLiteral("warm"));
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light); settle();
        QCOMPARE(window.property("theme").toString(),QStringLiteral("warm"));
        auto *scale=window.findChild<QComboBox *>("settingsFontScaleComboBox"); scale->setCurrentIndex(scale->findData(130));
        click(window,"settingsReducedMotionCheckBox");
        QCOMPARE(window.property("fontScale").toInt(),130); QVERIFY(window.property("reducedMotion").toBool());
        QVERIFY(!SplashScreen::present(&window));
        click(window,"settingsBackButton"); QVERIFY(text(window,"settingsAppearanceButtonSummary").contains("130%"));
        click(window,"settingsAboutButton"); QVERIFY(text(window,"settingsVersionLabel").contains(KURA_VERSION));
        click(window,"copyVersionInfoButton"); QVERIFY(QApplication::clipboard()->text().contains(KURA_VERSION));
    }
    MainWindow restarted(path); QCOMPARE(restarted.property("fontScale").toInt(),130); QVERIFY(restarted.property("reducedMotion").toBool());
    QVERIFY(restarted.findChild<QPushButton *>("systemThemeButton")->isChecked());
    QGuiApplication::styleHints()->unsetColorScheme();
}

void SettingsTests::summariesAndAIDraftsUseActualState()
{
    QTemporaryDir directory; const auto path=directory.filePath("state.sqlite"); AIStubServer server;
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Settings); settle();
    QCOMPARE(text(window,"settingsAIButtonSummary"),QStringLiteral("未配置"));
    QCOMPARE(text(window,"settingsPricePerspectiveButtonSummary"),QStringLiteral("未开启"));
    click(window,"settingsAIButton"); const auto config=server.settings();
    window.findChild<QLineEdit *>("aiBaseUrlEdit")->setText(config.baseUrl);
    window.findChild<QLineEdit *>("aiModelEdit")->setText(config.model);
    auto *key=window.findChild<QLineEdit *>("aiApiKeyEdit"); key->setText(config.apiKey);
    click(window,"showAIKeyButton"); QCOMPARE(key->echoMode(),QLineEdit::Normal);
    click(window,"settingsBackButton"); QCOMPARE(key->echoMode(),QLineEdit::Password);
    QVERIFY(window.findChild<QPushButton *>("unsavedAISettingsButton")->isVisible());
    QCOMPARE(text(window,"settingsAIButtonSummary"),QStringLiteral("有未保存修改"));
    click(window,"unsavedAISettingsButton"); QCOMPARE(key->text(),config.apiKey);
    click(window,"saveAISettingsButton"); QCOMPARE(server.requests.size(),0);
    QVERIFY(text(window,"aiLastTestLabel").contains(QStringLiteral("尚未测试")));
    click(window,"testAITextButton"); QTRY_VERIFY_WITH_TIMEOUT(text(window,"aiLastTestLabel").contains(QStringLiteral("文字测试通过")),5000);
    QCOMPARE(server.requests.size(),1);
    click(window,"testAIImageButton"); QTRY_VERIFY_WITH_TIMEOUT(text(window,"aiLastTestLabel").contains(QStringLiteral("截图测试通过")),5000);
    QCOMPARE(server.requests.size(),2);
    window.findChild<QLineEdit *>("aiModelEdit")->setText("another-fixture-model");
    QVERIFY(text(window,"aiLastTestLabel").contains(QStringLiteral("尚未测试")));
    click(window,"testAITextButton"); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),3,5000);
    QTRY_VERIFY_WITH_TIMEOUT(text(window,"aiLastTestLabel").contains(QStringLiteral("文字测试通过")),5000);
    server.status=401; click(window,"testAITextButton"); QTRY_VERIFY_WITH_TIMEOUT(text(window,"aiLastTestLabel").contains(QStringLiteral("文字测试未通过")),5000);
    window.findChild<QLineEdit *>("aiModelEdit")->setText(config.model);
    click(window,"settingsBackButton"); QCOMPARE(text(window,"settingsAIButtonSummary"),QStringLiteral("已配置"));
    auto *data=window.findChild<DataManagementWidget *>(); QVERIFY(data->backupTo(directory.filePath("manual.sqlite")));
    QVERIFY(text(window,"settingsDataButtonSummary").contains(QStringLiteral("最近备份")));
    PricePerspective price; price.mode=PricePerspective::Student; price.dailyFoodCents=3000;
    QString error; QVERIFY(PricePerspectiveStore::save(path+".ui.ini",price,error));
    window.showPage(MainWindow::Page::Home); window.showPage(MainWindow::Page::Settings);
    QCOMPARE(text(window,"settingsPricePerspectiveButtonSummary"),QStringLiteral("餐费换算"));
    MainWindow restarted(path); QVERIFY(text(restarted,"aiLastTestLabel").contains(QStringLiteral("截图测试通过"))); QVERIFY(!text(restarted,"aiLastTestLabel").contains(QStringLiteral("文字测试未通过")));
    QCOMPARE(server.requests.size(),4);
}

void SettingsTests::rulesStayInExecutionAndBrandsStayOut()
{
    QTemporaryDir directory; const auto path=directory.filePath("legacy.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize()); ShoppingProfileRepository profile(database);
    BrandEntry legacy; legacy.name="Legacy trusted brand"; legacy.category="online_shopping"; legacy.weight=9; QVERIFY(profile.saveBrand(legacy));
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Settings); settle();
    QVERIFY(!window.findChild<QPushButton *>("shoppingProfileButton")); QVERIFY(!window.findChild<QComboBox *>("executionBrandComboBox"));
    window.showPage(MainWindow::Page::Execution); click(window,"executionRulesToggle"); click(window,"executionAddRuleButton");
    auto *editor=window.findChild<QDialog *>("executionRulesEditor"); QVERIFY(editor);
    editor->findChild<QPlainTextEdit *>("executionRuleEditorText")->setPlainText(QStringLiteral("先确认用途")); click(*editor,"executionSaveRuleButton");
    QVERIFY(text(window,"executionRulesLabel").contains(QStringLiteral("先确认用途")));
    click(window,"executionEditRulesButton"); editor=window.findChild<QDialog *>("executionRulesEditor"); QVERIFY(editor);
    editor->findChild<QPlainTextEdit *>("executionRuleEditorText")->setPlainText(QStringLiteral("先确认用途\n核对退换条件")); click(*editor,"executionSaveRuleButton");
    ParsedProduct product; product.name=QStringLiteral("耳机"); product.category="online_shopping"; product.amountCents=20000;
    QVERIFY(window.findChild<ExecutionWidget *>()->prepareDecision(product));
    const auto context=window.findChild<QPlainTextEdit *>("decisionBriefText")->toPlainText();
    QVERIFY(context.contains(QStringLiteral("核对退换条件"))); QVERIFY(!context.contains(legacy.name)); QVERIFY(!context.contains(QStringLiteral("信任品牌")));
    DecisionDialog decision(database,{}); QVERIFY(!decision.findChild<QComboBox *>("decisionBrandComboBox"));
    MainWindow restarted(path); QVERIFY(text(restarted,"executionRulesLabel").contains(QStringLiteral("核对退换条件")));
    QList<BrandEntry> old; QVERIFY(profile.loadBrands(old)); QCOMPARE(old.size(),1); QCOMPARE(old.first().weight,9);
}

void SettingsTests::automaticBackupsRotateAndRecover()
{
    QTemporaryDir directory; const auto path=directory.filePath("data.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize());
    AutoBackupService service(database,path+".ui.ini"); QString error;
    const auto now=QDateTime(QDate(2026,10,25),QTime(8,0));
    QVERIFY(!service.enabled()); QVERIFY(service.runIfDue(error,now)); QVERIFY(!QDir(service.directory()).exists());
    TransactionRepository expenses(database); Transaction record(0,QStringLiteral("午餐"),2000,QStringLiteral("餐饮"),QDate::currentDate()); QVERIFY(expenses.add(record));
    QVERIFY(service.configure(true,2,error)); QVERIFY2(service.runIfDue(error,now),qPrintable(error));
    const auto generated=[&] { return QDir(service.directory()).entryList({"kura-auto-*.sqlite"},QDir::Files,QDir::Name); };
    QCOMPARE(generated().size(),1); const auto first=generated().first();
    AutoBackupService reopened(database,path+".ui.ini"); QVERIFY(reopened.runIfDue(error,now.addSecs(30))); QCOMPARE(generated().size(),1);
    QFile manual(QDir(service.directory()).filePath("manual.sqlite")); QVERIFY(manual.open(QIODevice::WriteOnly)); manual.write("keep this manual file"); manual.close();
    Transaction second(0,QStringLiteral("晚餐"),2500,QStringLiteral("餐饮"),QDate::currentDate()); QVERIFY(expenses.add(second));
    QVERIFY(service.runIfDue(error,now.addDays(1))); QVERIFY(service.runIfDue(error,now.addDays(2))); QCOMPARE(generated().size(),2); QVERIFY(!generated().contains(first)); QVERIFY(manual.exists());
    DatabaseBackupService backup(database); BackupInfo info;
    const auto snapshot=QDir(service.directory()).filePath(generated().last()); QVERIFY2(backup.prepareRestore(snapshot,info,error),qPrintable(error)); QCOMPARE(info.expenseCount,2);
    QVERIFY(expenses.remove(second.id())); QString safety; QVERIFY(backup.restorePrepared(safety,error));
    QList<Transaction> restored; QVERIFY(expenses.findAll(restored)); QCOMPARE(restored.size(),2);
    QVERIFY(service.configure(false,2,error)); QVERIFY(service.runIfDue(error,now.addDays(3))); QCOMPARE(generated().size(),2);
}

void SettingsTests::automaticBackupFailureKeepsLastSuccess()
{
    QTemporaryDir directory; const auto path=directory.filePath("failure.sqlite"); DatabaseManager database(path); QVERIFY(database.initialize());
    AutoBackupService service(database,path+".ui.ini"); QString error; QVERIFY(service.configure(true,7,error));
    QFile blocker(service.directory()); QVERIFY(blocker.open(QIODevice::WriteOnly)); blocker.write("not a folder"); blocker.close();
    QVERIFY(!service.runIfDue(error)); QVERIFY(!error.isEmpty());
    QSettings settings(path+".ui.ini",QSettings::IniFormat); QVERIFY(!settings.value("backup/lastAutoSuccess").toDateTime().isValid());
    QVERIFY(blocker.remove()); QVERIFY2(service.runIfDue(error),qPrintable(error)); QVERIFY(service.lastBackupText().contains(QStringLiteral("最近备份")));
}

void SettingsTests::themesAndLargeTypeFit()
{
    QTemporaryDir directory; MainWindow window(directory.filePath("visual.sqlite")); window.show(); window.showPage(MainWindow::Page::Settings); settle();
    for (const auto &theme:QStringList{"warm","cool","dark"}) {
        click(window,"settingsAppearanceButton"); click(window,(theme+"ThemeButton").toUtf8().constData()); click(window,"settingsBackButton");
        for (const auto size:{QSize(1180,860),QSize(800,600)}) {
            window.resize(size); settle(); capture(window,QStringLiteral("settings-%1-%2").arg(theme).arg(size.width()));
            auto *entry=window.findChild<QPushButton *>("settingsAppearanceButton"); QVERIFY(entry->isVisible());
            auto *summary=window.findChild<QLabel *>("settingsAppearanceButtonSummary"); QVERIFY(summary->isVisible());
            QVERIFY(summary->height()>0); QVERIFY(entry->rect().contains(summary->geometry()));
            for (auto *label:entry->findChildren<QLabel *>()) QVERIFY(label->height()>0);

            for (auto *scroll:window.findChildren<QScrollArea *>()) if (scroll->isVisible()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        }
    }
    click(window,"settingsAppearanceButton"); window.findChild<QComboBox *>("settingsFontScaleComboBox")->setCurrentIndex(2);
    for (const auto *entry:{"settingsAppearanceButton","settingsAIButton","settingsPricePerspectiveButton","settingsAboutButton"}) {
        click(window,"settingsBackButton"); click(window,entry); settle(); capture(window,QStringLiteral("large-")+entry);
        for (auto *scroll:window.findChildren<QScrollArea *>()) if (scroll->isVisible()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
    }
    window.showPage(MainWindow::Page::Data); click(window,"openBackupOptionsButton"); capture(window,"backup-settings-large");
    for (auto *scroll:window.findChildren<QScrollArea *>()) if (scroll->isVisible()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
}

QTEST_MAIN(SettingsTests)
#include "settings_tests.moc"
