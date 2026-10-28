#include "AIStubServer.h"
#include "mainwindow.h"
#include "OnboardingDialog.h"
#include "OnboardingTour.h"
#include "SplashScreen.h"
#include "BillImportDialog.h"
#include "BudgetRepository.h"
#include "PricePerspectiveWidget.h"
#include "ShoppingProfileRepository.h"
#include "TransactionRepository.h"
#include "WishlistRepository.h"
#include "WishPoolRepository.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QUuid>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariantAnimation>
#include <QtTest>
namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QApplication::processEvents(); }
void press(QWidget &owner,const char *name) {
    auto *button=owner.findChild<QAbstractButton *>(name); QVERIFY2(button,name); QVERIFY2(button->isEnabled(),name);
    button->click(); settle();
}
OnboardingDialog *open(MainWindow &window) { window.show(); window.startFirstRunGuide(); settle(); return window.findChild<OnboardingDialog *>(); }
void skipSetup(OnboardingDialog &guide) { press(guide,"onboardingNextButton"); for (int i=0;i<5;++i) press(guide,"onboardingSkipButton"); QCOMPARE(guide.step(),6); }
void capture(QWidget &window,const QString &name) {
    settle(); const auto folder=qEnvironmentVariable("KURA_ONBOARDING_PREVIEW_DIR");
    if (!folder.isEmpty()) { QVERIFY(QDir().mkpath(folder)); QVERIFY(window.grab().save(folder+"/"+name+".png")); }
}
void tour(MainWindow &window,bool screenshots=false,const QString &prefix={}) {
    const QList<int> clicks={1,2,4,6,8,10};
    for (int i=0;i<12;++i) {
        auto *overlay=window.findChild<OnboardingTour *>(); QVERIFY(overlay); QCOMPARE(overlay->step(),i); settle();
        auto *bubble=overlay->findChild<QFrame *>("onboardingBubble"); QVERIFY(bubble);
        QVERIFY(overlay->rect().contains(bubble->geometry()));
        auto *body=overlay->findChild<QLabel *>("tourBody"); QVERIFY(body->height()>=body->heightForWidth(body->width()));
        if (screenshots) capture(window,QString("%1-tour-%2").arg(prefix).arg(i));
        if (clicks.contains(i)) {
            QVERIFY(!overlay->spotlightRect().isEmpty());
            QVERIFY(!bubble->geometry().contains(overlay->spotlightRect().center()));
            QTest::mouseClick(overlay,Qt::LeftButton,Qt::NoModifier,overlay->spotlightRect().center()); settle();
        } else press(*overlay,"tourNextButton");
    }
    QVERIFY(!window.findChild<OnboardingDialog *>());
}
void noSpending(DatabaseManager &database) { QList<Transaction> items; TransactionRepository repository(database); QVERIFY(repository.findAll(items)); QVERIFY(items.isEmpty()); }
}
class OnboardingTests : public QObject {
    Q_OBJECT
private slots:
    void newUserDefaultStorageIsEmptyAndExistingUserKeepsRecords();
    void amountsAcceptKeyboardInputAndPersist();
    void setupSavesRealSettingsAndTourUsesRealControls();
    void skipsPreserveExistingDataAndReplay();
    void waitsForSplashAndResumesWithoutDuplicates();
    void importThenTourAndDatabaseFailure();
    void themesAndLargeTypeFit();
};
void OnboardingTests::newUserDefaultStorageIsEmptyAndExistingUserKeepsRecords()
{
    // Exercise the real default storage path with isolated test identities.
    // Never open, rename or clear the developer's normal application data.
    const auto originalOrganization=QCoreApplication::organizationName();
    const auto originalApplication=QCoreApplication::applicationName();
    const bool originalTestMode=QStandardPaths::isTestModeEnabled();
    const auto organization=QStringLiteral("KuraInstallTest-")+QUuid::createUuid().toString(QUuid::Id128);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(organization);
    QCoreApplication::setApplicationName(QStringLiteral("KuraCpp"));
    const auto firstPath=DatabaseManager::defaultDatabasePath();
    const auto secondPath=QFileInfo(firstPath).dir().absolutePath()+QStringLiteral("-other/kura.sqlite");
    const auto cleanup=qScopeGuard([&] {
        QDir(QFileInfo(firstPath).absolutePath()).removeRecursively();
        QDir(QFileInfo(secondPath).absolutePath()).removeRecursively();
        QDir().rmdir(QFileInfo(QFileInfo(firstPath).absolutePath()).absolutePath());
        QCoreApplication::setOrganizationName(originalOrganization);
        QCoreApplication::setApplicationName(originalApplication);
        QStandardPaths::setTestModeEnabled(originalTestMode);
    });
    QVERIFY(firstPath.contains(organization)); QVERIFY(!QFile::exists(firstPath));
    {
        MainWindow fresh; auto *guide=open(fresh); QVERIFY(guide); QCOMPARE(guide->step(),0);
        DatabaseManager database; QVERIFY(database.initialize()); noSpending(database);
        QList<WishItem> wishes; WishlistRepository wishlist(database); QVERIFY(wishlist.findAll(wishes)); QVERIFY(wishes.isEmpty());
        WishPoolSnapshot pool; WishPoolRepository pools(database); QVERIFY(pools.load(pool)); QCOMPARE(pool.wishId,qint64(0));
        capture(fresh,"fresh-user-default-storage");
        Transaction record(0,QStringLiteral("isolated upgrade fixture"),100,Transaction::categories().first(),QDate::currentDate());
        TransactionRepository transactions(database); QVERIFY2(transactions.add(record),qPrintable(transactions.lastError()));
    }
    {
        QSettings preferences(firstPath+QStringLiteral(".ui.ini"),QSettings::IniFormat);
        preferences.setValue("onboarding/version",2); preferences.setValue("onboarding/dismissed",true); preferences.sync();
    }
    {
        MainWindow reopened; QVERIFY(!open(reopened));
        DatabaseManager database; QVERIFY(database.initialize());
        QList<Transaction> records; TransactionRepository transactions(database); QVERIFY(transactions.findAll(records));
        QCOMPARE(records.size(),1); QCOMPARE(records[0].description(),QStringLiteral("isolated upgrade fixture"));
    }
    QCoreApplication::setApplicationName(QStringLiteral("KuraCpp-other"));
    QCOMPARE(DatabaseManager::defaultDatabasePath(),secondPath);
    {
        MainWindow anotherUser; auto *guide=open(anotherUser); QVERIFY(guide); QCOMPARE(guide->step(),0);
        DatabaseManager database; QVERIFY(database.initialize()); noSpending(database);
    }
}
void OnboardingTests::amountsAcceptKeyboardInputAndPersist()
{
    QTemporaryDir directory; const auto path=directory.filePath("amount-input.sqlite");
    {
    MainWindow window(path); auto *guide=open(window); QVERIFY(guide);
    const auto typeAmount=[guide](const char *name,const char *text,double expected) {
        auto *spin=guide->findChild<QDoubleSpinBox *>(name); QVERIFY(spin);
        auto *editor=spin->findChild<QLineEdit *>(); QVERIFY(editor);
        // Click at the end, as a user does, without first selecting the hint.
        QTest::mouseClick(editor,Qt::LeftButton,Qt::NoModifier,QPoint(editor->width()-5,editor->height()/2));
        QTest::keyClicks(editor,text);
        QCOMPARE(spin->value(),expected);
        QVERIFY(spin->hasAcceptableInput());
    };
    press(*guide,"onboardingNextButton"); press(*guide,"onboardingSkipButton");
    typeAmount("onboardingBasicBudget","1000",1000.);
    typeAmount("onboardingFreeBudget","500.50",500.50);
    press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),3);
    press(*guide,"pricePerspectiveStudentButton"); typeAmount("pricePerspectiveDailyFoodSpinBox","30.50",30.50);
    capture(window,"perspective-student-typed");
    press(*guide,"pricePerspectiveWorkerButton"); typeAmount("pricePerspectiveSalarySpinBox","6500.75",6500.75);
    capture(window,"perspective-worker-typed");
    press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),4);
    guide->findChild<QLineEdit *>("onboardingWishName")->setText(QStringLiteral("一条羊绒围巾"));
    capture(window,"amount-empty");
    typeAmount("onboardingWishPrice","199.90",199.90);
    capture(window,"amount-filled");
    auto *price=guide->findChild<QDoubleSpinBox *>("onboardingWishPrice");
    auto *editor=price->findChild<QLineEdit *>();
    QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier); QTest::keyClick(editor,Qt::Key_Backspace);
    QTest::keyClicks(editor,"239.80");
    QCOMPARE(price->value(),239.80);
    press(*guide,"onboardingWishPriority2Button"); press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),5);
    press(*guide,"onboardingPreviousButton");
    QCOMPARE(price->value(),239.80);
    price->setFocus(); price->selectAll(); QTest::keyClicks(price,"259.60");
    press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),5);
    }
    DatabaseManager database(path); QVERIFY(database.initialize()); noSpending(database);
    MonthlyBudgetStatus budget; BudgetRepository budgets(database);
    QVERIFY(budgets.loadStatus(QDate::currentDate(),budget));
    QCOMPARE(budget.basicLimitCents,100000); QCOMPARE(budget.discretionaryLimitCents,50050);
    const auto perspective=PricePerspectiveStore::load(path+".ui.ini"); QCOMPARE(perspective.mode,PricePerspective::Worker);
    QCOMPARE(perspective.dailyFoodCents,3050); QCOMPARE(perspective.monthlyIncomeCents,650075);
    WishlistRepository repository(database); QList<WishItem> wishes; QVERIFY(repository.findAll(wishes));
    QCOMPARE(wishes.size(),1); QCOMPARE(wishes[0].estimatedCents,25960);
    MainWindow reopened(path); auto *resumed=open(reopened); QVERIFY(resumed);
    QCOMPARE(resumed->step(),5); press(*resumed,"onboardingPreviousButton"); QCOMPARE(resumed->step(),4);
    QCOMPARE(resumed->findChild<QDoubleSpinBox *>("onboardingWishPrice")->value(),259.60);
}
void OnboardingTests::setupSavesRealSettingsAndTourUsesRealControls()
{
    QTemporaryDir directory; const auto path=directory.filePath("new.sqlite"); AIStubServer server;
    {
        MainWindow window(path); auto *guide=open(window); QVERIFY(guide); QCOMPARE(guide->geometry(),window.centralWidget()->rect());
        QVERIFY(guide->findChild<QLabel *>("onboardingBody")->text().contains(QStringLiteral("小松鼠")));
        window.startFirstRunGuide(); QCOMPARE(window.findChildren<OnboardingDialog *>().size(),1);
        press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),1);
        press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),1); QVERIFY(guide->findChild<QLabel *>("onboardingError")->isVisible());
        guide->findChild<QComboBox *>("onboardingAIProvider")->setCurrentIndex(2);
        const auto fixture=server.settings(); guide->findChild<QLineEdit *>("onboardingAIUrl")->setText(fixture.baseUrl);
        guide->findChild<QLineEdit *>("onboardingAIModel")->setText(fixture.model); guide->findChild<QLineEdit *>("onboardingAIKey")->setText(fixture.apiKey);
        QCOMPARE(guide->findChild<QLineEdit *>("onboardingAIKey")->echoMode(),QLineEdit::Password); QVERIFY(server.requests.isEmpty());
        press(*guide,"onboardingAITestButton"); QTRY_COMPARE(server.requests.size(),1);
        QTRY_COMPARE(guide->findChild<QPushButton *>("onboardingAITestButton")->text(),QStringLiteral("测试连接"));
        press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),2); QCOMPARE(AISettingsStore::loadPersonal(path+".ui.ini").apiKey,fixture.apiKey);
        guide->findChild<QDoubleSpinBox *>("onboardingBasicBudget")->setValue(1000);
        guide->findChild<QDoubleSpinBox *>("onboardingFreeBudget")->setValue(500); press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),3);
        press(*guide,"pricePerspectiveStudentButton"); guide->findChild<QDoubleSpinBox *>("pricePerspectiveDailyFoodSpinBox")->setValue(30);
        press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),4);
        guide->findChild<QLineEdit *>("onboardingWishName")->setText(QStringLiteral("一双 Nike 699 块"));
        press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),4);
        press(*guide,"onboardingWishPriority1Button"); press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),5);
        press(*guide,"onboardingPreviousButton"); press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),5);
        guide->findChild<QPlainTextEdit *>("onboardingRules")->setPlainText(QStringLiteral("先想想家里有没有类似的\n只买真正会用的"));
        press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),6);
        press(*guide,"onboardingNextButton"); tour(window);
        QCOMPARE(window.currentPage(),MainWindow::Page::Home); QCOMPARE(server.requests.size(),1);
        QVERIFY(window.findChild<QAbstractButton *>("bookkeepingAiModeButton")->isChecked());
    }
    MainWindow reopened(path); QVERIFY(!open(reopened));
    DatabaseManager database(path); QVERIFY(database.initialize()); noSpending(database);
    MonthlyBudgetStatus budget; BudgetRepository budgets(database); QVERIFY(budgets.loadStatus(QDate::currentDate(),budget)); QCOMPARE(budget.basicLimitCents,100000); QCOMPARE(budget.discretionaryLimitCents,50000);
    QList<WishItem> wishes; WishlistRepository repository(database); QVERIFY(repository.findAll(wishes)); QCOMPARE(wishes.size(),1); QCOMPARE(wishes[0].name,QStringLiteral("一双 Nike")); QCOMPARE(wishes[0].estimatedCents,69900); QCOMPARE(wishes[0].priority,1);
    WishPoolSnapshot pool; WishPoolRepository pools(database); QVERIFY(pools.load(pool)); QCOMPARE(pool.wishId,wishes[0].id);
    QStringList rules; ShoppingProfileRepository profiles(database); QVERIFY(profiles.loadRules(rules)); QCOMPARE(rules.size(),2);
    const auto perspective=PricePerspectiveStore::load(path+".ui.ini"); QCOMPARE(perspective.mode,PricePerspective::Student); QCOMPARE(perspective.dailyFoodCents,3000);
    QSettings settings(path+".ui.ini",QSettings::IniFormat); QCOMPARE(settings.value("onboarding/version").toInt(),2); QVERIFY(settings.value("onboarding/completed").toBool());
    QFile file(path+".ui.ini"); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(!file.readAll().contains("fixture-only-key"));
    reopened.showPage(MainWindow::Page::Settings);
    QCOMPARE(reopened.findChild<QDoubleSpinBox *>("pricePerspectiveDailyFoodSpinBox")->value(),30.);
}
void OnboardingTests::skipsPreserveExistingDataAndReplay()
{
    QTemporaryDir directory; const auto path=directory.filePath("existing.sqlite");
    {
        DatabaseManager database(path); QVERIFY(database.initialize()); BudgetRepository budgets(database); QVERIFY(budgets.saveSplit(QDate::currentDate(),80000,20000));
        ShoppingProfileRepository rules(database); QVERIFY(rules.saveRules({QStringLiteral("已有原则") }));
        WishlistRepository wishes(database); WishItem item; item.name=QStringLiteral("旧心愿"); item.estimatedCents=10000; QVERIFY(wishes.add(item)); WishPoolRepository pool(database); QVERIFY(pool.setGoal(item.id));
        QSettings settings(path+".ui.ini",QSettings::IniFormat); settings.setValue("onboarding/dismissed",true); settings.setValue("appearance/theme","cool");
    }
    {
        MainWindow window(path); auto *guide=open(window); QVERIFY(guide); skipSetup(*guide); press(*guide,"onboardingExploreButton"); QVERIFY(!window.findChild<OnboardingDialog *>());
    }
    MainWindow reopened(path); QVERIFY(!open(reopened)); reopened.showPage(MainWindow::Page::Settings); press(reopened,"settingsAboutButton"); press(reopened,"replayOnboardingButton");
    auto *guide=reopened.findChild<OnboardingDialog *>(); QVERIFY(guide); QCOMPARE(guide->step(),0);
    QTest::keyClick(guide,Qt::Key_Escape); settle(); QVERIFY(!reopened.findChild<OnboardingDialog *>()); QCOMPARE(reopened.currentPage(),MainWindow::Page::Settings);
    DatabaseManager database(path); QVERIFY(database.initialize()); noSpending(database);
    MonthlyBudgetStatus budget; BudgetRepository budgets(database); QVERIFY(budgets.loadStatus(QDate::currentDate(),budget)); QCOMPARE(budget.limitCents,100000);
    QStringList rules; ShoppingProfileRepository profiles(database); QVERIFY(profiles.loadRules(rules)); QCOMPARE(rules,QStringList{QStringLiteral("已有原则")});
    QList<WishItem> wishes; WishlistRepository repository(database); QVERIFY(repository.findAll(wishes)); QCOMPARE(wishes.size(),1);
    QSettings settings(path+".ui.ini",QSettings::IniFormat); QCOMPARE(settings.value("appearance/theme").toString(),QString("cool"));
}
void OnboardingTests::waitsForSplashAndResumesWithoutDuplicates()
{
    QTemporaryDir directory; const auto path=directory.filePath("resume.sqlite");
    {
        MainWindow window(path); window.show(); auto *splash=SplashScreen::present(&window); QVERIFY(splash); auto *animation=splash->findChild<QVariantAnimation *>(); animation->pause();
        window.startFirstRunGuide(); settle(); QVERIFY(!window.findChild<OnboardingDialog *>()); animation->setCurrentTime(animation->duration()); emit animation->finished(); settle();
        auto *guide=window.findChild<OnboardingDialog *>(); QVERIFY(guide); press(*guide,"onboardingNextButton"); press(*guide,"onboardingSkipButton"); press(*guide,"onboardingSkipButton"); QCOMPARE(guide->step(),3);
    }
    {
        MainWindow window(path); auto *guide=open(window); QVERIFY(guide); QCOMPARE(guide->step(),3); press(*guide,"onboardingSkipButton");
        guide->findChild<QLineEdit *>("onboardingWishName")->setText(QStringLiteral("耳机")); guide->findChild<QDoubleSpinBox *>("onboardingWishPrice")->setValue(200); press(*guide,"onboardingWishPriority2Button"); press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),5);
    }
    qint64 readyAt=0;
    { DatabaseManager database(path); QVERIFY(database.initialize()); WishlistRepository repository(database); QList<WishItem> wishes; QVERIFY(repository.findAll(wishes)); QCOMPARE(wishes.size(),1); readyAt=wishes[0].readyAt.toMSecsSinceEpoch(); }
    MainWindow window(path); auto *guide=open(window); QVERIFY(guide); QCOMPARE(guide->step(),5); press(*guide,"onboardingPreviousButton"); press(*guide,"onboardingNextButton"); press(*guide,"onboardingSkipButton"); press(*guide,"onboardingExploreButton");
    DatabaseManager database(path); QVERIFY(database.initialize()); noSpending(database); WishlistRepository repository(database); QList<WishItem> wishes; QVERIFY(repository.findAll(wishes)); QCOMPARE(wishes.size(),1); QCOMPARE(wishes[0].readyAt.toMSecsSinceEpoch(),readyAt);
}
void OnboardingTests::importThenTourAndDatabaseFailure()
{
    QTemporaryDir directory; const auto path=directory.filePath("failure.sqlite"); MainWindow window(path); auto *guide=open(window); QVERIFY(guide);
    DatabaseManager database(path); QVERIFY(database.initialize()); QSqlQuery query(database.database());
    QVERIFY(query.exec("CREATE TRIGGER onboarding_test_block BEFORE INSERT ON monthly_budgets BEGIN SELECT RAISE(ABORT,'fixture blocked'); END"));
    press(*guide,"onboardingNextButton"); press(*guide,"onboardingSkipButton"); guide->findChild<QDoubleSpinBox *>("onboardingFreeBudget")->setValue(500);
    press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),2); QVERIFY(guide->findChild<QLabel *>("onboardingError")->isVisible()); QVERIFY(query.exec("DROP TRIGGER onboarding_test_block"));
    press(*guide,"onboardingNextButton"); QCOMPARE(guide->step(),3); press(*guide,"onboardingSkipButton"); press(*guide,"onboardingSkipButton"); press(*guide,"onboardingSkipButton");
    press(*guide,"onboardingImportButton"); auto *dialog=window.findChild<BillImportDialog *>(); QVERIFY(dialog); QVERIFY(dialog->isVisible()); QVERIFY(!guide->isVisible());
    dialog->reject(); settle(); QVERIFY(guide->isVisible()); QVERIFY(window.findChild<OnboardingTour *>()); tour(window); noSpending(database);
}
void OnboardingTests::themesAndLargeTypeFit()
{
    QTemporaryDir directory;
    for (const QString &theme:{"warm","cool","dark"}) {
        const auto path=directory.filePath(theme+".sqlite");
        { QSettings settings(path+".ui.ini",QSettings::IniFormat); settings.setValue("appearance/theme",theme); settings.setValue("appearance/fontScale",130); settings.setValue("appearance/reducedMotion",true); }
        MainWindow window(path); window.resize(800,600); auto *guide=open(window); QVERIFY(guide); QVERIFY(!SplashScreen::present(&window));
        for (int step=0;step<7;++step) {
            QCOMPARE(guide->step(),step); QCOMPARE(guide->geometry(),window.centralWidget()->rect());
            auto *scroll=guide->findChild<QScrollArea *>("onboardingSetupScroll"); auto *next=guide->findChild<QPushButton *>("onboardingNextButton"); scroll->ensureWidgetVisible(next); settle();
            if (step==0) {
                auto *title=guide->findChild<QLabel *>("onboardingTitle");
                QVERIFY(scroll->viewport()->rect().contains(QRect(title->mapTo(scroll->viewport(),QPoint()),title->size())));
            }
            QVERIFY(scroll->viewport()->rect().intersects(QRect(next->mapTo(scroll->viewport(),QPoint()),next->size())));
            QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
            capture(window,QString("%1-setup-%2").arg(theme).arg(step));
            if (step==0 || step==6) press(*guide,"onboardingNextButton"); else press(*guide,"onboardingSkipButton");
        }
        tour(window,true,theme);
    }
}
QTEST_MAIN(OnboardingTests)
#include "onboarding_tests.moc"
