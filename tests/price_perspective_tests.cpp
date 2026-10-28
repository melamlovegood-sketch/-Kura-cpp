#include "PricePerspective.h"
#include "PricePerspectiveWidget.h"
#include "PricePerspectiveHint.h"
#include "AppearanceWidget.h"
#include "DecisionDialog.h"
#include "ExecutionWidget.h"
#include "WishlistWidget.h"
#include "WishlistRepository.h"
#include "BudgetRepository.h"
#include "AppTheme.h"
#include "mainwindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QClipboard>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
void click(QWidget &owner,const char *name) { auto *button=owner.findChild<QPushButton *>(name); QVERIFY2(button,name); button->click(); settle(); }
void value(QWidget &owner,const char *name,double amount) { auto *field=owner.findChild<QDoubleSpinBox *>(name); QVERIFY2(field,name); field->setValue(amount); settle(); }
qint64 count(DatabaseManager &db,const QString &table) { QSqlQuery q(db.database()); return q.exec("SELECT COUNT(*) FROM "+table) && q.next()?q.value(0).toLongLong():-1; }
}
class PricePerspectiveTests:public QObject {
    Q_OBJECT
private slots:
    void emptyAmountsAcceptMouseKeyboardAndPasteAndPersist();
    void studentAndWorkerExamples();
    void missingInvalidAndSmallAmounts();
    void preferencesPreserveBothModesAndOtherSettings();
    void settingsPreviewAndSaveFailures();
    void hintsRefreshAcrossBuyingSurfaces();
};
void PricePerspectiveTests::emptyAmountsAcceptMouseKeyboardAndPasteAndPersist() {
    QTemporaryDir directory; const auto path=directory.filePath("keyboard.ini");
    {
        PricePerspectiveWidget widget(path); AppTheme::apply(&widget); widget.resize(700,850); widget.show(); settle();
        const auto enter=[&widget](const char *name,const char *text,double expected) {
            auto *spin=widget.findChild<QDoubleSpinBox *>(name); QVERIFY(spin);
            auto *editor=spin->findChild<QLineEdit *>(); QVERIFY(editor); QVERIFY(editor->text().isEmpty());
            QVERIFY(!editor->placeholderText().isEmpty()); QVERIFY(!editor->isReadOnly());
            QTest::mouseClick(editor,Qt::LeftButton,Qt::NoModifier,QPoint(editor->width()-5,editor->height()/2));
            QTest::keyClicks(editor,text); QCOMPARE(spin->value(),expected); QVERIFY(spin->hasAcceptableInput());
        };
        click(widget,"pricePerspectiveStudentButton"); enter("pricePerspectiveDailyFoodSpinBox","30.50",30.50);
        click(widget,"pricePerspectiveWorkerButton"); enter("pricePerspectiveSalarySpinBox","6500.75",6500.75);
        click(widget,"pricePerspectiveStudentButton");
        auto *food=widget.findChild<QDoubleSpinBox *>("pricePerspectiveDailyFoodSpinBox"); QCOMPARE(food->value(),30.50);
        auto *editor=food->findChild<QLineEdit *>(); editor->setFocus();
        QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier); QTest::keyClick(editor,Qt::Key_Backspace);
        QVERIFY(editor->text().isEmpty()); QCOMPARE(food->value(),0.0);
        QTest::keyClicks(editor,"abc"); QVERIFY(editor->text().isEmpty());
        auto *oldClipboard=new QMimeData; const auto *clipboard=QApplication::clipboard()->mimeData();
        if (clipboard) for (const auto &format:clipboard->formats()) oldClipboard->setData(format,clipboard->data(format));
        QApplication::clipboard()->setText("35.60");
        QTest::keyClick(editor,Qt::Key_V,Qt::ControlModifier); QApplication::clipboard()->setMimeData(oldClipboard);
        QCOMPARE(food->value(),35.60); QVERIFY(food->hasAcceptableInput());
        auto *sample=widget.findChild<QDoubleSpinBox *>("pricePerspectiveSampleSpinBox"); editor=sample->findChild<QLineEdit *>();
        editor->setFocus(); QTest::keyClick(editor,Qt::Key_A,Qt::ControlModifier); QTest::keyClick(editor,Qt::Key_Backspace);
        enter("pricePerspectiveSampleSpinBox","20.25",20.25);
        QVERIFY(widget.save());
    }
    const auto stored=PricePerspectiveStore::load(path); QCOMPARE(stored.dailyFoodCents,3560); QCOMPARE(stored.monthlyIncomeCents,650075);
    PricePerspectiveWidget reopened(path); QCOMPARE(reopened.findChild<QDoubleSpinBox *>("pricePerspectiveDailyFoodSpinBox")->value(),35.60);
    QCOMPARE(reopened.findChild<QDoubleSpinBox *>("pricePerspectiveSalarySpinBox")->value(),6500.75);
}
void PricePerspectiveTests::studentAndWorkerExamples() {
    PricePerspective p; p.mode=PricePerspective::Student; p.dailyFoodCents=3000; p.mealsPerDay=3;
    QVERIFY(p.reference().contains("10.00")); auto result=p.quote(2000); QCOMPARE(result.primary,QStringLiteral("约相当于你 2 顿饭的预算")); QCOMPARE(result.compact,QStringLiteral("约 2 顿饭的预算")); QVERIFY(result.secondary.isEmpty());
    p.mealsPerDay=2; QVERIFY(p.quote(2000).primary.contains("1.3")); p.mealsPerDay=3; QVERIFY(p.quote(9000).secondary.contains("3 天"));
    p.mode=PricePerspective::Worker; p.monthlyIncomeCents=1000000; p.workDaysPerMonth=20; p.dailyWorkHours=10;
    QVERIFY(p.reference().contains("50.00")); result=p.quote(200000); QCOMPARE(result.primary,QStringLiteral("约相当于你工作 40 小时")); QCOMPARE(result.secondary,QStringLiteral("按你的设置，约 4 个工作日"));
    QCOMPARE(p.quote(2000).primary,QStringLiteral("约相当于你工作 24 分钟"));
    p.dailyWorkHours=8; QCOMPARE(p.quote(200000).primary,QStringLiteral("约相当于你工作 32 小时")); QCOMPARE(p.quote(200000).secondary,QStringLiteral("按你的设置，约 4 个工作日"));
    p.workDaysPerMonth=21.75; QVERIFY(p.quote(200000).primary.contains("34.8 小时"));
}
void PricePerspectiveTests::missingInvalidAndSmallAmounts() {
    PricePerspective p; QVERIFY(p.quote(2000).isEmpty()); p.mode=PricePerspective::Student; QVERIFY(p.quote(2000).isEmpty()); p.dailyFoodCents=3000;
    QVERIFY(p.quote(0).isEmpty()); QVERIFY(p.quote(-1).isEmpty()); QVERIFY(p.quote(Transaction::MaximumAmountCents+1).isEmpty()); QVERIFY(p.quote(1).primary.contains(QStringLiteral("不足 0.1")));
    p.mealsPerDay=0; QVERIFY(!p.validationError().isEmpty()); QVERIFY(p.quote(2000).isEmpty()); p.mealsPerDay=3;
    p.mode=PricePerspective::Worker; p.monthlyIncomeCents=1000000; p.workDaysPerMonth=0; QVERIFY(p.quote(2000).isEmpty());
    p.workDaysPerMonth=20; p.dailyWorkHours=std::numeric_limits<double>::quiet_NaN(); QVERIFY(p.quote(2000).isEmpty()); p.dailyWorkHours=10;
    QVERIFY(!p.quote(Transaction::MaximumAmountCents).isEmpty()); QVERIFY(p.quote(1).primary.contains(QStringLiteral("不足 0.1")));
    p.monthlyIncomeCents=1; QVERIFY(p.reference().contains(QStringLiteral("不足¥0.01")));
    p.mode=PricePerspective::Student; p.dailyFoodCents=1; QVERIFY(p.reference().contains(QStringLiteral("不足¥0.01")));
    p.mode=PricePerspective::Off; QVERIFY(p.quote(2000).isEmpty());
}
void PricePerspectiveTests::preferencesPreserveBothModesAndOtherSettings() {
    QTemporaryDir dir; const auto path=dir.filePath("profile.ini"); { QSettings settings(path,QSettings::IniFormat); settings.setValue("appearance/theme","cool"); settings.setValue("bookkeeping/budgetMode",true); }
    PricePerspective p; p.mode=PricePerspective::Student; p.dailyFoodCents=3000; p.monthlyIncomeCents=1000000; p.workDaysPerMonth=20; p.dailyWorkHours=10; QString error;
    QVERIFY(PricePerspectiveStore::save(path,p,error)); p.mode=PricePerspective::Worker; QVERIFY(PricePerspectiveStore::save(path,p,error)); auto loaded=PricePerspectiveStore::load(path); QCOMPARE(loaded.dailyFoodCents,3000); QCOMPARE(loaded.monthlyIncomeCents,1000000); QCOMPARE(loaded.workDaysPerMonth,20);
    p.mode=PricePerspective::Off; QVERIFY(PricePerspectiveStore::save(path,p,error)); loaded=PricePerspectiveStore::load(path); QVERIFY(loaded.quote(200000).isEmpty()); QCOMPARE(loaded.dailyFoodCents,3000); QCOMPARE(loaded.monthlyIncomeCents,1000000);
    p.mode=PricePerspective::Student; p.mealsPerDay=0; QVERIFY(!PricePerspectiveStore::save(path,p,error)); QCOMPARE(PricePerspectiveStore::load(path).mode,PricePerspective::Off);
    QSettings settings(path,QSettings::IniFormat); QCOMPARE(settings.value("appearance/theme").toString(),QString("cool")); QVERIFY(settings.value("bookkeeping/budgetMode").toBool());
    settings.setValue("pricePerspective/dailyWorkHours",0); settings.sync(); loaded=PricePerspectiveStore::load(path); QVERIFY(!loaded.ready()); QVERIFY(loaded.quote(200000).isEmpty());
}
void PricePerspectiveTests::settingsPreviewAndSaveFailures() {
    QTemporaryDir dir; const auto path=dir.filePath("settings.ini"); PricePerspectiveWidget widget(path); AppTheme::apply(&widget); widget.resize(540,600); widget.show();
    click(widget,"pricePerspectiveStudentButton"); value(widget,"pricePerspectiveDailyFoodSpinBox",30); value(widget,"pricePerspectiveSampleSpinBox",20);
    QVERIFY(widget.findChild<QLabel *>("pricePerspectiveExampleLabel")->text().contains(QStringLiteral("2 顿饭"))); QCOMPARE(PricePerspectiveStore::load(path).mode,PricePerspective::Off);
    click(widget,"pricePerspectiveWorkerButton"); value(widget,"pricePerspectiveSalarySpinBox",10000); value(widget,"pricePerspectiveWorkDaysSpinBox",20); value(widget,"pricePerspectiveHoursSpinBox",10); value(widget,"pricePerspectiveSampleSpinBox",2000);
    QVERIFY(widget.findChild<QLabel *>("pricePerspectiveExampleLabel")->text().contains(QStringLiteral("40 小时"))); QVERIFY(widget.findChild<QLabel *>("pricePerspectiveExampleLabel")->text().contains(QStringLiteral("4 个工作日")));
    QSignalSpy saved(&widget,&PricePerspectiveWidget::saved); click(widget,"savePricePerspectiveButton"); QCOMPARE(saved.size(),1); QCOMPARE(PricePerspectiveStore::load(path).mode,PricePerspective::Worker);
    click(widget,"pricePerspectiveStudentButton"); QCOMPARE(widget.findChild<QDoubleSpinBox *>("pricePerspectiveDailyFoodSpinBox")->value(),30.0); click(widget,"savePricePerspectiveButton");
    click(widget,"pricePerspectiveOffButton"); click(widget,"savePricePerspectiveButton"); PricePerspectiveWidget restored(path); QCOMPARE(restored.findChild<QDoubleSpinBox *>("pricePerspectiveSalarySpinBox")->value(),10000.0); QCOMPARE(restored.findChild<QDoubleSpinBox *>("pricePerspectiveDailyFoodSpinBox")->value(),30.0);
    PricePerspectiveWidget failed(dir.path()); click(failed,"pricePerspectiveStudentButton"); value(failed,"pricePerspectiveDailyFoodSpinBox",30); QSignalSpy noSave(&failed,&PricePerspectiveWidget::saved); click(failed,"savePricePerspectiveButton"); QCOMPARE(noSave.size(),0); QCOMPARE(PricePerspectiveStore::load(dir.path()).mode,PricePerspective::Off); QVERIFY(failed.findChild<QLabel *>("pricePerspectiveStatusLabel")->text().contains(QStringLiteral("失败")));
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!preview.isEmpty()) { QDir().mkpath(preview); for (const auto &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) { AppTheme::apply(&widget,theme); click(widget,"pricePerspectiveStudentButton"); value(widget,"pricePerspectiveSampleSpinBox",20); QVERIFY(widget.grab().save(preview+"/price-student-"+theme+".png")); click(widget,"pricePerspectiveWorkerButton"); value(widget,"pricePerspectiveSampleSpinBox",2000); QVERIFY(widget.grab().save(preview+"/price-worker-"+theme+".png")); } }
}
void PricePerspectiveTests::hintsRefreshAcrossBuyingSurfaces() {
    QTemporaryDir dir; const auto dbPath=dir.filePath("buying.sqlite"),settingsPath=PricePerspectiveStore::settingsPath(dbPath); DatabaseManager db(dbPath); QVERIFY(db.initialize());
    WishlistRepository wishes(db); WishItem wish; wish.name=QStringLiteral("羊毛大衣"); wish.estimatedCents=200000; wish.category=QStringLiteral("购物"); wish.coolingDays=3; QVERIFY(wishes.add(wish)); BudgetRepository budgets(db); QVERIFY(budgets.saveSplit(QDate::currentDate(),100000,50000));
    PricePerspective p; p.mode=PricePerspective::Student; p.dailyFoodCents=3000; QString error; QVERIFY(PricePerspectiveStore::save(settingsPath,p,error));
    MainWindow window(dbPath); window.resize(1100,850); window.show(); window.showPage(MainWindow::Page::Execution); settle();
    auto *execution=window.findChild<ExecutionWidget *>(); ParsedProduct product; product.name=QStringLiteral("奶茶"); product.amountCents=2000; QVERIFY(execution->prepareDecision(product));
    auto *modeHint=window.findChild<PricePerspectiveHint *>("executionModeCostHintLabel"); QVERIFY(modeHint && !modeHint->isHidden()); QVERIFY(modeHint->text().contains(QStringLiteral("2 顿饭")));
    value(window,"executionEstimatedAmountSpinBox",40); QVERIFY(modeHint->text().contains(QStringLiteral("4 顿饭")));
    DecisionDialog dialog(db,AISettings{},&window); dialog.show(); click(dialog,"decisionManualButton"); value(dialog,"decisionAmountSpinBox",20); auto *decisionHint=dialog.findChild<PricePerspectiveHint *>("decisionCostHintLabel"); QVERIFY(!decisionHint->isHidden()); QVERIFY(decisionHint->text().contains(QStringLiteral("2 顿饭"))); dialog.hide();
    window.showPage(MainWindow::Page::Settings); click(window,"settingsPricePerspectiveButton"); click(window,"pricePerspectiveWorkerButton"); value(window,"pricePerspectiveSalarySpinBox",10000); value(window,"pricePerspectiveWorkDaysSpinBox",20); value(window,"pricePerspectiveHoursSpinBox",10); click(window,"savePricePerspectiveButton");
    QVERIFY(modeHint->text().contains(QStringLiteral("48 分钟"))); QVERIFY(decisionHint->text().contains(QStringLiteral("24 分钟")));
    auto *table=window.findChild<QTableWidget *>("wishlistTable"); QVERIFY(table); auto *listHint=table->findChild<QLabel *>("productCostHintLabel"); QVERIFY(listHint && listHint->text().contains(QStringLiteral("40 小时"))); QVERIFY(listHint->toolTip().contains(QStringLiteral("4 个工作日")));
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!preview.isEmpty()) {
        QDir().mkpath(preview); QVERIFY(window.grab().save(preview+"/price-settings-page.png"));
        window.showPage(MainWindow::Page::Execution); settle(); QVERIFY(window.grab().save(preview+"/price-execution-page.png"));
        window.showPage(MainWindow::Page::Wishlist); window.resize(800,650); settle(); QVERIFY(window.grab().save(preview+"/price-wishlist-compact.png"));
        window.showPage(MainWindow::Page::Settings); settle();
    }
    click(window,"pricePerspectiveOffButton"); click(window,"savePricePerspectiveButton"); QVERIFY(modeHint->isHidden()); QVERIFY(decisionHint->isHidden()); QVERIFY(listHint->isHidden());
    QCOMPARE(count(db,"transactions"),0); QCOMPARE(count(db,"review_tasks"),0); QCOMPARE(count(db,"wishlist"),1); MonthlyBudgetStatus budget; QVERIFY(budgets.loadStatus(QDate::currentDate(),budget)); QCOMPARE(budget.basicLimitCents,100000); QCOMPARE(budget.discretionaryLimitCents,50000);
}
QTEST_MAIN(PricePerspectiveTests)
#include "price_perspective_tests.moc"
