#include "AddTransactionDialog.h"
#include "AppTheme.h"
#include "BudgetWidget.h"
#include "BudgetRepository.h"
#include "DecisionDialog.h"
#include "ExecutionWidget.h"
#include "ExpenseComposer.h"
#include "OnboardingDialog.h"
#include "PricePerspectiveWidget.h"
#include "PriceRecordDialog.h"
#include "PriceWidget.h"
#include "WishDialog.h"
#include "WishPriceDialog.h"
#include "WishlistRepository.h"
#include "UnifiedWishlistRepository.h"
#include "TransactionRepository.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>
#include <memory>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
void press(QWidget &owner,const char *name) {
    auto *button=owner.findChild<QPushButton *>(name); QVERIFY2(button,name); QVERIFY2(button->isEnabled(),name); button->click(); settle();
}
void type(QDoubleSpinBox *spin,const char *text,bool replace=false) {
    QVERIFY(spin); auto *editor=spin->findChild<QLineEdit *>(); QVERIFY(editor);
    for (auto *parent=spin->parentWidget();parent;parent=parent->parentWidget())
        if (auto *scroll=qobject_cast<QScrollArea *>(parent)) scroll->ensureWidgetVisible(spin,0,12);
    settle(); QTest::mouseClick(editor,Qt::LeftButton,Qt::NoModifier,QPoint(editor->width()-5,editor->height()/2));
    if (replace) QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier);
    QTest::keyClicks(editor,text); QVERIFY(spin->hasAcceptableInput());
}
}
class AmountInputTests:public QObject {
    Q_OBJECT
private slots:
    void everyAmountEntry_data();
    void everyAmountEntry();
    void typedValuesReachStorageAndZeroKeepsItsMeaning();
};
void AmountInputTests::everyAmountEntry_data() {
    QTest::addColumn<QString>("entry"); QTest::addColumn<QString>("fieldName");
    const QList<QPair<const char *,const char *>> entries={
        {"transaction-new","amountSpinBox"},{"transaction-edit","amountSpinBox"},
        {"budget-basic","basicBudgetAmountSpinBox"},{"budget-free","budgetAmountSpinBox"},
        {"decision","decisionAmountSpinBox"},{"execution","executionEstimatedAmountSpinBox"},
        {"expense-manual","bookkeepingAmountSpinBox"},{"expense-ai","bookkeepingDraftAmountSpinBox"},
        {"onboarding-basic","onboardingBasicBudget"},{"onboarding-free","onboardingFreeBudget"},{"onboarding-wish","onboardingWishPrice"},
        {"perspective-food","pricePerspectiveDailyFoodSpinBox"},{"perspective-salary","pricePerspectiveSalarySpinBox"},{"perspective-preview","pricePerspectiveSampleSpinBox"},
        {"price-record","priceAmountSpinBox"},{"wish-new","wishAmountSpinBox"},{"wish-edit","wishAmountSpinBox"},
        {"price-target","priceTargetSpinBox"},{"wish-target","wishTargetAmountSpinBox"}
    };
    for (const auto &entry:entries) QTest::newRow(entry.first)<<QString::fromLatin1(entry.first)<<QString::fromLatin1(entry.second);
}
void AmountInputTests::everyAmountEntry() {
    QFETCH(QString,entry); QFETCH(QString,fieldName);
    QTemporaryDir directory; DatabaseManager database(directory.filePath("amounts.sqlite")); QVERIFY(database.initialize());
    const auto preferences=directory.filePath("preferences.ini"); std::unique_ptr<QWidget> view;
    if (entry=="transaction-new") view=std::make_unique<AddTransactionDialog>(nullptr,preferences);
    else if (entry=="transaction-edit") view=std::make_unique<AddTransactionDialog>(Transaction(1,"fixture",9900,"购物",QDate::currentDate()),nullptr,preferences);
    else if (entry.startsWith("budget")) { auto *budget=new BudgetWidget(database); view.reset(budget); budget->setDatabaseAvailable(true); }
    else if (entry=="decision") { auto *dialog=new DecisionDialog(database,AISettings{}); view.reset(dialog); press(*dialog,"decisionManualButton"); }
    else if (entry=="execution") {
        auto *execution=new ExecutionWidget(database,preferences); view.reset(execution); execution->setDatabaseAvailable(true);
        ParsedProduct product; product.name="fixture"; QVERIFY(execution->prepareDecision(product));
        auto *toggle=execution->findChild<QToolButton *>("executionExtraToggle"); QVERIFY(toggle); toggle->click();
    } else if (entry.startsWith("expense")) {
        auto *composer=new ExpenseComposer(database,preferences); view.reset(composer); composer->setDatabaseAvailable(true);
        if (entry=="expense-manual") press(*composer,"bookkeepingManualModeButton");
        else { ParsedProduct product; product.name="fixture"; auto *service=composer->findChild<AIService *>(); QVERIFY(service); service->parsed(product); }
    } else if (entry.startsWith("onboarding")) {
        { QSettings settings(preferences,QSettings::IniFormat); settings.setValue("onboarding/setupStep",entry=="onboarding-wish"?4:2); }
        view=std::make_unique<QWidget>(); view->resize(800,900); new OnboardingDialog(database,preferences,view.get(),true);
    } else if (entry.startsWith("perspective")) {
        view=std::make_unique<PricePerspectiveWidget>(preferences);
        press(*view,entry=="perspective-salary"?"pricePerspectiveWorkerButton":"pricePerspectiveStudentButton");
    } else if (entry=="price-record") view=std::make_unique<PriceRecordDialog>(database);
    else if (entry=="wish-new") view=std::make_unique<WishDialog>();
    else if (entry=="wish-edit") { WishItem item; item.name="fixture"; item.estimatedCents=9900; view=std::make_unique<WishDialog>(item); }
    else if (entry=="price-target") {
        PriceRepository prices(database); qint64 track=0; QVERIFY(prices.intake("fixture","online_shopping",9900,"fixture","price-fixture",track));
        auto *pricesView=new PriceWidget(database); view.reset(pricesView); pricesView->setDatabaseAvailable(true); QVERIFY(pricesView->refresh(track));
        auto *action=pricesView->findChild<QAction *>("configurePriceAction"); QVERIFY(action); QVERIFY(action->isEnabled()); action->trigger();
    } else if (entry=="wish-target") {
        WishItem item; item.name="fixture"; item.estimatedCents=9900; WishlistRepository wishes(database); QVERIFY(wishes.add(item));
        UnifiedWishlistRepository catalog(database); qint64 track=0; QVERIFY(catalog.recordPrice(item.id,9900,"fixture","wish-price-fixture",QDateTime::currentDateTimeUtc(),track));
        view=std::make_unique<WishPriceDialog>(database,item.id); press(*view,"wishPriceTargetButton");
    }
    QVERIFY(view); AppTheme::apply(view.get()); view->resize(1000,1000); view->show(); settle();
    auto *spin=view->findChild<QDoubleSpinBox *>(fieldName); QVERIFY(spin); QVERIFY(spin->isEnabled());
    if (entry.endsWith("-edit")) QCOMPARE(spin->value(),99.0);
    spin->setValue(0); auto *editor=spin->findChild<QLineEdit *>(); QVERIFY(editor);
    QVERIFY(editor->text().isEmpty()); QVERIFY(!editor->placeholderText().isEmpty()); QVERIFY(!editor->isReadOnly());
    QVERIFY(spin->specialValueText().isEmpty());
    type(spin,"123.45"); QCOMPARE(spin->value(),123.45);
    type(spin,"67.89",true); QCOMPARE(spin->value(),67.89);
    QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier); QTest::keyClick(editor,Qt::Key_Backspace);
    QCOMPARE(spin->value(),0.0); QVERIFY(editor->text().isEmpty());
    // Re-enter while the field still has focus; focus-only workarounds miss this.
    type(spin,"9.90"); QCOMPARE(spin->value(),9.90);
    auto *savedClipboard=new QMimeData; const auto *clipboard=QApplication::clipboard()->mimeData();
    if (clipboard) for (const auto &format:clipboard->formats()) savedClipboard->setData(format,clipboard->data(format));
    QApplication::clipboard()->setText("29.90"); QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier); QTest::keyClick(editor,Qt::Key_V,Qt::ControlModifier);
    QApplication::clipboard()->setMimeData(savedClipboard); QCOMPARE(spin->value(),29.90); QVERIFY(spin->hasAcceptableInput());
    spin->interpretText(); QCOMPARE(spin->value(),29.90);
    if (const auto folder=qEnvironmentVariable("KURA_AMOUNT_PREVIEW_DIR"); !folder.isEmpty() && entry=="execution") {
        QVERIFY(QDir().mkpath(folder)); QVERIFY(view->grab().save(folder+"/execution-typed-amount.png"));
    }
}
void AmountInputTests::typedValuesReachStorageAndZeroKeepsItsMeaning() {
    QTemporaryDir directory; DatabaseManager database(directory.filePath("storage.sqlite")); QVERIFY(database.initialize());
    BudgetWidget budget(database); budget.setDatabaseAvailable(true); budget.show(); settle();
    auto *basic=budget.findChild<QDoubleSpinBox *>("basicBudgetAmountSpinBox"); auto *free=budget.findChild<QDoubleSpinBox *>("budgetAmountSpinBox");
    type(basic,"500.50"); type(free,"1200.25"); press(budget,"saveBudgetButton");
    MonthlyBudgetStatus status; BudgetRepository budgets(database); QVERIFY(budgets.loadStatus(QDate::currentDate(),status));
    QCOMPARE(status.basicLimitCents,50050); QCOMPARE(status.discretionaryLimitCents,120025);
    type(basic,"0",true); press(budget,"saveBudgetButton"); QVERIFY(budgets.loadStatus(QDate::currentDate(),status)); QCOMPARE(status.basicLimitCents,0); QVERIFY(status.hasBudget);
    ExpenseComposer composer(database,directory.filePath("preferences.ini")); composer.setDatabaseAvailable(true); composer.show(); press(composer,"bookkeepingManualModeButton");
    auto *amount=composer.findChild<QDoubleSpinBox *>("bookkeepingAmountSpinBox"); type(amount,"19.90"); press(composer,"saveBookkeepingManualButton");
    QList<Transaction> records; TransactionRepository transactions(database); QVERIFY(transactions.findAll(records)); QCOMPARE(records.size(),1); QCOMPARE(records[0].amountCents(),1990);
    QVERIFY(!composer.findChild<QPushButton *>("saveBookkeepingManualButton")->isEnabled());
    WishDialog wish; wish.show(); wish.findChild<QLineEdit *>("wishNameEdit")->setText("typed wish"); press(wish,"wishPriority2Button");
    type(wish.findChild<QDoubleSpinBox *>("wishAmountSpinBox"),"199.90"); WishlistRepository wishes(database); auto item=wish.item(); QVERIFY(wishes.add(item));
    WishDialog edit(item); edit.show(); type(edit.findChild<QDoubleSpinBox *>("wishAmountSpinBox"),"149.75",true); QVERIFY(wishes.update(edit.item()));
    QList<WishItem> items; QVERIFY(wishes.findAll(items)); QCOMPARE(items.size(),1); QCOMPARE(items[0].estimatedCents,14975);
    PriceRepository prices(database); qint64 track=0; QVERIFY(prices.intake("target fixture","online_shopping",9900,"fixture","target-fixture",track));
    PriceWidget priceView(database); priceView.setDatabaseAvailable(true); priceView.show(); QVERIFY(priceView.refresh(track));
    priceView.findChild<QAction *>("configurePriceAction")->trigger(); settle();
    auto *target=priceView.findChild<QDoubleSpinBox *>("priceTargetSpinBox"); type(target,"79.90"); press(priceView,"savePriceTargetButton");
    QList<PriceTrack> tracks; QVERIFY(prices.load(tracks)); QCOMPARE(tracks[0].targetCents,7990);
    priceView.findChild<QAction *>("configurePriceAction")->trigger(); settle(); target=priceView.findChild<QDoubleSpinBox *>("priceTargetSpinBox");
    auto *editor=target->findChild<QLineEdit *>(); editor->setFocus(); QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier); QTest::keyClick(editor,Qt::Key_Backspace);
    press(priceView,"savePriceTargetButton"); QVERIFY(prices.load(tracks)); QCOMPARE(tracks[0].targetCents,0);
}
QTEST_MAIN(AmountInputTests)
#include "amount_input_tests.moc"
