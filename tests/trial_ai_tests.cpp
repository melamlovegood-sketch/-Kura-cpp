#include "AIStubServer.h"
#include "AppearanceWidget.h"
#include "mainwindow.h"
#include "OnboardingDialog.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TrialAITests : public QObject {
    Q_OBJECT
private slots:
    void cleanup() { qApp->setProperty("kuraTrialConfigPath",QVariant()); }
    void packageDefaultOnlyAppliesToUnconfiguredUsers();
    void onboardingDefaultsToTrialAndRemembersPersonalChoice();
    void explicitChoicePreservesPersonalCredentials();
    void unavailableTrialNeverFallsBackToPersonalKey();
    void uiSwitchingAndRevocationUseLocalService();
    void trialSettingsFitAtLargeText();
};

void TrialAITests::packageDefaultOnlyAppliesToUnconfiguredUsers()
{
    QTemporaryDir directory; const auto path=directory.filePath("new-user.ini");
    const auto trialPath=directory.filePath("kura-trial.json"); qApp->setProperty("kuraTrialConfigPath",trialPath);
    AIStubServer server; auto shared=server.settings(); shared.apiKey="shared-fixture-credential"; QString error;
    QVERIFY(AISettingsStore::exportTrial(trialPath,shared,error,true));
    QVERIFY(AISettingsStore::trial().trialDefaultForNewUsers);
    const auto fresh=AISettingsStore::loadPersonal(path); QVERIFY(fresh.useTrial); QVERIFY(fresh.apiKey.isEmpty());
    QCOMPARE(AISettingsStore::load(path).apiKey,shared.apiKey); QVERIFY(!QFile::exists(path));
    AppearanceWidget settings(path); QVERIFY(!settings.findChild<QPushButton *>("useTrialAIButton")->isEnabled());
    QVERIFY(settings.findChild<QPushButton *>("usePersonalAIButton")->isEnabled());
    QVERIFY(settings.findChild<QLineEdit *>("aiApiKeyEdit")->text().isEmpty());
    settings.findChild<QPushButton *>("usePersonalAIButton")->click();
    QVERIFY(!AISettingsStore::load(path).useTrial); QVERIFY(AISettingsStore::load(path).apiKey.isEmpty());
    // An existing personal credential remains personal even before useTrial existed.
    auto personal=server.settings(); personal.apiKey="personal-fixture-credential";
    const auto legacy=directory.filePath("legacy.ini"); QVERIFY(AISettingsStore::save(legacy,personal,error));
    { QSettings saved(legacy,QSettings::IniFormat); saved.remove("ai/useTrial"); saved.sync(); }
    QVERIFY(!AISettingsStore::load(legacy).useTrial); QCOMPARE(AISettingsStore::load(legacy).apiKey,personal.apiKey);
    QVERIFY(QFile::remove(trialPath));
    QVERIFY(!AISettingsStore::load(directory.filePath("no-trial.ini")).useTrial);
    QCOMPARE(server.requests.size(),0);
}

void TrialAITests::onboardingDefaultsToTrialAndRemembersPersonalChoice()
{
    QTemporaryDir directory; const auto trialPath=directory.filePath("kura-trial.json"); qApp->setProperty("kuraTrialConfigPath",trialPath);
    AIStubServer server; QString error; QVERIFY(AISettingsStore::exportTrial(trialPath,server.settings(),error,true));
    const auto path=directory.filePath("fresh.sqlite");
    {
        MainWindow window(path); window.show(); window.startFirstRunGuide(); QApplication::processEvents();
        auto *guide=window.findChild<OnboardingDialog *>(); QVERIFY(guide);
        guide->findChild<QPushButton *>("onboardingNextButton")->click(); QCOMPARE(guide->step(),1);
        auto *trial=guide->findChild<QPushButton *>("onboardingTrialButton"); QVERIFY(trial->isEnabled()); QVERIFY(trial->isChecked());
        QVERIFY(trial->text().contains(QStringLiteral("已选择")));
        auto *key=guide->findChild<QLineEdit *>("onboardingAIKey"); QVERIFY(key->text().isEmpty()); QVERIFY(!key->isEnabled());
        QVERIFY(guide->findChild<QLabel *>("onboardingAITrialHint")->text().contains(QStringLiteral("无需填写")));
        const auto folder=qEnvironmentVariable("KURA_TRIAL_PREVIEW_DIR");
        if (!folder.isEmpty()) { QVERIFY(QDir().mkpath(folder)); QVERIFY(window.grab().save(folder+"/trial-onboarding-default.png")); }
        guide->findChild<QPushButton *>("onboardingNextButton")->click(); QCOMPARE(guide->step(),2);
        QVERIFY(AISettingsStore::loadPersonal(path+".ui.ini").useTrial); QVERIFY(AISettingsStore::loadPersonal(path+".ui.ini").apiKey.isEmpty());
    }
    const auto personalPath=directory.filePath("personal.sqlite"); AISettings personal;
    QVERIFY(AISettingsStore::save(personalPath+".ui.ini",personal,error));
    MainWindow window(personalPath); window.show(); window.startFirstRunGuide(); QApplication::processEvents();
    auto *guide=window.findChild<OnboardingDialog *>(); QVERIFY(guide);
    guide->findChild<QPushButton *>("onboardingNextButton")->click();
    auto *trial=guide->findChild<QPushButton *>("onboardingTrialButton"); QVERIFY(!trial->isChecked());
    auto *key=guide->findChild<QLineEdit *>("onboardingAIKey"); QVERIFY(key->isEnabled()); key->setText("personal-fixture-credential");
    guide->findChild<QPushButton *>("onboardingNextButton")->click(); QCOMPARE(guide->step(),2);
    const auto chosen=AISettingsStore::load(personalPath+".ui.ini"); QVERIFY(!chosen.useTrial); QCOMPARE(chosen.apiKey,QString("personal-fixture-credential"));
    QCOMPARE(server.requests.size(),0);
}

void TrialAITests::explicitChoicePreservesPersonalCredentials()
{
    QTemporaryDir directory; const auto path=directory.filePath("preferences.ini");
    const auto trialPath=directory.filePath("kura-trial.json"); qApp->setProperty("kuraTrialConfigPath",trialPath);
    AIStubServer server; auto shared=server.settings(); shared.apiKey="shared-fixture-credential"; shared.model="trial-model";
    auto personal=server.settings(); personal.apiKey="personal-fixture-credential"; personal.model="personal-model";
    QString error; QVERIFY(AISettingsStore::save(path,personal,error)); QVERIFY(AISettingsStore::exportTrial(trialPath,shared,error));
    // Merely distributing a trial configuration does not opt a user in.
    QCOMPARE(AISettingsStore::load(path).apiKey,personal.apiKey);
    QVERIFY(AISettingsStore::load(directory.filePath("new-user.ini")).apiKey.isEmpty());
    personal.useTrial=true; QVERIFY(AISettingsStore::save(path,personal,error));
    const auto effective=AISettingsStore::load(path,&error); QVERIFY(error.isEmpty()); QVERIFY(effective.useTrial);
    QCOMPARE(effective.apiKey,shared.apiKey); QCOMPARE(effective.model,shared.model);
    QCOMPARE(AISettingsStore::loadPersonal(path).apiKey,personal.apiKey);
    QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto savedBytes=file.readAll(); file.close();
    QVERIFY(!savedBytes.contains(shared.apiKey.toUtf8())); QVERIFY(!savedBytes.contains(personal.apiKey.toUtf8()));
    auto restored=AISettingsStore::loadPersonal(path); restored.useTrial=false; QVERIFY(AISettingsStore::save(path,restored,error));
    QCOMPARE(AISettingsStore::load(path).apiKey,personal.apiKey); QCOMPARE(AISettingsStore::load(path).model,personal.model);
    QCOMPARE(server.requests.size(),0);
}

void TrialAITests::unavailableTrialNeverFallsBackToPersonalKey()
{
    QTemporaryDir directory; const auto path=directory.filePath("preferences.ini");
    const auto trialPath=directory.filePath("kura-trial.json"); qApp->setProperty("kuraTrialConfigPath",trialPath);
    AIStubServer server; auto personal=server.settings(); personal.useTrial=true; QString error;
    QVERIFY(AISettingsStore::save(path,personal,error));
    auto missing=AISettingsStore::load(path,&error); QVERIFY(!error.isEmpty()); QVERIFY(missing.apiKey.isEmpty());
    AIService service; QSignalSpy failed(&service,&AIService::failed);
    QVERIFY(!service.advise(missing,"fixture","hello")); QCOMPARE(failed.count(),1); QCOMPARE(server.requests.size(),0);
    QCOMPARE(AISettingsStore::loadPersonal(path).apiKey,personal.apiKey);
    QFile corrupt(trialPath); QVERIFY(corrupt.open(QIODevice::WriteOnly)); corrupt.write("{invalid configuration}"); corrupt.close();
    missing=AISettingsStore::load(path,&error); QVERIFY(!error.isEmpty()); QVERIFY(missing.apiKey.isEmpty());
    QVERIFY(!service.advise(missing,"fixture","hello")); QCOMPARE(server.requests.size(),0);
    QVERIFY(corrupt.open(QIODevice::WriteOnly|QIODevice::Truncate));
    corrupt.write("{\"format\":1,\"baseUrl\":\"http://remote.example/v1\",\"model\":\"test\",\"apiKey\":\"fixture\"}"); corrupt.close();
    QVERIFY(!AISettingsStore::load(path).validationError().isEmpty());
    auto invalid=personal; invalid.apiKey.clear(); QVERIFY(!AISettingsStore::exportTrial(directory.filePath("empty.json"),invalid,error));
    QVERIFY(!QFile::exists(directory.filePath("empty.json")));
}

void TrialAITests::uiSwitchingAndRevocationUseLocalService()
{
    QTemporaryDir directory; const auto path=directory.filePath("preferences.ini");
    const auto trialPath=directory.filePath("kura-trial.json"); qApp->setProperty("kuraTrialConfigPath",trialPath);
    AIStubServer server; auto shared=server.settings(); shared.model="trial-model"; shared.apiKey="shared-fixture-credential";
    QString error; QVERIFY(AISettingsStore::exportTrial(trialPath,shared,error));
    {
        AppearanceWidget window(path); window.resize(720,1100); window.show();
        window.findChild<QPushButton *>("settingsAIButton")->click();
        auto *key=window.findChild<QLineEdit *>("aiApiKeyEdit"); QVERIFY(key->text().isEmpty());
        auto *enable=window.findChild<QPushButton *>("useTrialAIButton"); QVERIFY(enable->isEnabled()); enable->click();
        QVERIFY(AISettingsStore::loadPersonal(path).useTrial); QVERIFY(key->text().isEmpty()); QVERIFY(!key->isEnabled());
        QCOMPARE(server.requests.size(),0);
        window.findChild<QPushButton *>("testAITextButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains(QStringLiteral("文字连接成功")),5000);
        QCOMPARE(server.requests.size(),1); QCOMPARE(server.requests.last().value("model").toString(),shared.model);
        server.status=401; window.findChild<QPushButton *>("testAIImageButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains(QStringLiteral("试用暂不可用或已结束")),5000);
        QCOMPARE(server.requests.size(),2); QVERIFY(key->text().isEmpty());
        server.status=429; window.findChild<QPushButton *>("testAITextButton")->click();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains(QStringLiteral("试用额度不足")),5000);
        QCOMPARE(server.requests.size(),3);
    }
    AppearanceWidget restarted(path); restarted.resize(720,1100); restarted.show(); restarted.findChild<QPushButton *>("settingsAIButton")->click();
    auto *key=restarted.findChild<QLineEdit *>("aiApiKeyEdit"); QVERIFY(key->text().isEmpty()); QVERIFY(!key->isEnabled());
    QCOMPARE(server.requests.size(),3);
    restarted.findChild<QPushButton *>("usePersonalAIButton")->click(); QVERIFY(key->isEnabled());
    auto personal=server.settings(); personal.model="personal-model"; personal.apiKey="personal-fixture-credential";
    restarted.findChild<QLineEdit *>("aiBaseUrlEdit")->setText(personal.baseUrl);
    restarted.findChild<QLineEdit *>("aiModelEdit")->setText(personal.model); key->setText(personal.apiKey);
    restarted.findChild<QPushButton *>("saveAISettingsButton")->click(); server.status=200;
    restarted.findChild<QPushButton *>("testAITextButton")->click();
    QTRY_VERIFY_WITH_TIMEOUT(restarted.findChild<QLabel *>("aiSettingsStatusLabel")->text().contains(QStringLiteral("文字连接成功")),5000);
    QCOMPARE(server.requests.size(),4); QCOMPARE(server.requests.last().value("model").toString(),personal.model);
    restarted.findChild<QPushButton *>("useTrialAIButton")->click();
    QCOMPARE(key->text(),personal.apiKey); QCOMPARE(AISettingsStore::load(path).model,shared.model);
    QVERIFY(QFile::remove(trialPath));
    AppearanceWidget unavailable(path); QVERIFY(unavailable.findChild<QPushButton *>("usePersonalAIButton")->isEnabled());
    unavailable.findChild<QPushButton *>("usePersonalAIButton")->click();
    QCOMPARE(AISettingsStore::load(path).apiKey,personal.apiKey); QCOMPARE(server.requests.size(),4);
}

void TrialAITests::trialSettingsFitAtLargeText()
{
    QTemporaryDir directory; const auto path=directory.filePath("layout.sqlite");
    const auto trialPath=directory.filePath("kura-trial.json"); qApp->setProperty("kuraTrialConfigPath",trialPath);
    AIStubServer server; QString error; QVERIFY(AISettingsStore::exportTrial(trialPath,server.settings(),error));
    { QSettings settings(path+".ui.ini",QSettings::IniFormat); settings.setValue("appearance/fontScale",130); }
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Settings);
    window.findChild<QPushButton *>("settingsAIButton")->click(); window.findChild<QPushButton *>("useTrialAIButton")->click();
    for (const auto size:{QSize(800,600),QSize(1180,860)}) {
        window.resize(size); QApplication::processEvents();
        for (auto *scroll:window.findChildren<QScrollArea *>()) if (scroll->isVisible()) QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        const auto folder=qEnvironmentVariable("KURA_TRIAL_PREVIEW_DIR");
        if (!folder.isEmpty()) { QVERIFY(QDir().mkpath(folder)); QVERIFY(window.grab().save(folder+QStringLiteral("/trial-ai-%1.png").arg(size.width()))); }
    }
    QCOMPARE(server.requests.size(),0);
}

QTEST_MAIN(TrialAITests)
#include "trial_ai_tests.moc"
