#include "AIStubServer.h"
#include "AddTransactionDialog.h"
#include "AppTheme.h"
#include "BillingAggregation.h"
#include "BillingCharts.h"
#include "BillingWorkspace.h"
#include "BudgetRepository.h"
#include "DemoData.h"
#include "ExpenseComposer.h"
#include "ModernSelectors.h"
#include "TransactionRepository.h"
#include "mainwindow.h"
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QImage>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
void click(QWidget &owner,const char *name) { auto *button=owner.findChild<QPushButton *>(name); QVERIFY2(button,name); button->click(); settle(); }
void fillManual(QWidget &owner,const QString &name,double amount) {
    owner.findChild<QLineEdit *>("bookkeepingNameEdit")->setText(name);
    owner.findChild<QDoubleSpinBox *>("bookkeepingAmountSpinBox")->setValue(amount);
    owner.findChild<ModernComboBox *>("bookkeepingCategoryComboBox")->setCurrentIndex(0);
}
}
class BillingTests:public QObject {
    Q_OBJECT
private slots:
    void aggregationBoundaries();
    void manualSaveDetailsEditDeleteAndRestart();
    void aiConfirmationCancelAndRetry();
    void lockedSavePreservesDraft();
    void monthChartsBudgetAndResponsivePreviews();
    void focusedEntryPreservesDraftAndBudgetContext();
    void directChartViewsKeepRangeAndOpenSelectedRecords();
};
void BillingTests::aggregationBoundaries()
{
    QList<Transaction> records{
        Transaction(1,"leap",1234,"餐饮",QDate(2024,2,29),{}, {},"daily"),
        Transaction(2,"previous",600,"餐饮",QDate(2023,9,30),{}, {},"daily"),
        Transaction(3,"outside",900,"餐饮",QDate(2023,8,31),{}, {},"daily"),
        Transaction(4,"next",300,"餐饮",QDate(2024,3,1),{}, {},"daily")};
    BillingSnapshot snapshot; QString error;
    QVERIFY(BillingAggregation::calculate(records,QDate(2024,2,15),snapshot,error,QDate(2024,3,1)));
    QCOMPARE(snapshot.days.size(),29); QCOMPARE(snapshot.totalCents,1234);
    QCOMPARE(snapshot.todayCents,300); QCOMPARE(snapshot.records.size(),1);
    QCOMPARE(snapshot.trend.size(),6); QCOMPARE(snapshot.trend[0].month,QDate(2023,9,1));
    QCOMPARE(snapshot.trend[0].cents,600); QCOMPARE(snapshot.trend.last().cents,1234);
    QVERIFY(!BillingAggregation::calculate(records,QDate(),snapshot,error));
    QCOMPARE(snapshot.totalCents,1234); // Failure preserves the last complete snapshot.
    QCOMPARE(BillingAggregation::money(123456789),QStringLiteral("¥1,234,567.89"));
}
void BillingTests::manualSaveDetailsEditDeleteAndRestart()
{
    QTemporaryDir directory; const auto path=directory.filePath("billing.sqlite");
    {
        MainWindow window(path); window.showPage(MainWindow::Page::Billing); window.show(); settle();
        click(window,"bookkeepingManualModeButton"); fillManual(window,QStringLiteral("午饭"),15.25);
        click(window,"saveBookkeepingManualButton");
        QCOMPARE(window.findChild<QLabel *>("billingMonthTotalLabel")->text(),QStringLiteral("¥15.25"));
        DatabaseManager database(path); QVERIFY(database.initialize()); TransactionRepository repository(database);
        QList<Transaction> records; QVERIFY(repository.findAll(records)); QCOMPARE(records.size(),1);
        const auto id=records[0].id();
        // Exclude the record in the full-list filters before editing from a recent row.
        click(window,"billingAllRecordsButton");
        auto *category=window.findChild<QComboBox *>("categoryFilter"); category->setCurrentIndex(category->count()-1);
        click(window,"applyFilterButton"); click(window,"billingRecordsBackButton");
        const auto rowName=QStringLiteral("billingRecentRecord%1").arg(id).toUtf8(); click(window,rowName.constData());
        click(window,"billingDetailEditButton");
        auto *edit=window.findChild<AddTransactionDialog *>(); QVERIFY(edit);
        edit->findChild<QDoubleSpinBox *>("amountSpinBox")->setValue(20);
        click(*edit,"saveButton");
        QCOMPARE(window.findChild<QLabel *>("billingMonthTotalLabel")->text(),QStringLiteral("¥20.00"));
        click(window,rowName.constData()); click(window,"billingDetailDeleteButton");
        auto *confirmation=window.findChild<QMessageBox *>("deleteConfirmationDialog"); QVERIFY(confirmation);
        confirmation->button(QMessageBox::Yes)->click(); settle();
        QVERIFY(repository.findAll(records)); QVERIFY(records.isEmpty());
        QCOMPARE(window.findChild<QLabel *>("billingMonthTotalLabel")->text(),QStringLiteral("¥0.00"));
        fillManual(window,QStringLiteral("保存到下次"),8); click(window,"saveBookkeepingManualButton");
    }
    MainWindow restarted(path); restarted.showPage(MainWindow::Page::Billing);
    QCOMPARE(restarted.findChild<QLabel *>("billingMonthTotalLabel")->text(),QStringLiteral("¥8.00"));
}
void BillingTests::aiConfirmationCancelAndRetry()
{
    QTemporaryDir directory; const auto path=directory.filePath("ai.sqlite");
    AIStubServer stub; QString error; QVERIFY(AISettingsStore::save(path+".ui.ini",stub.settings(),error));
    MainWindow window(path); window.showPage(MainWindow::Page::Billing); window.show(); settle();
    QImage receipt(200,100,QImage::Format_RGB32); receipt.fill(Qt::white);
    const auto receiptPath=directory.filePath("fictional-receipt.png"); QVERIFY(receipt.save(receiptPath));
    QVERIFY(window.findChild<ExpenseComposer *>()->setScreenshot(receiptPath));
    auto *input=window.findChild<QPlainTextEdit *>("bookkeepingInput"); input->setPlainText(QStringLiteral("一本笔记本 12.34 元"));
    click(window,"parseBookkeepingButton");
    auto *confirm=window.findChild<QWidget *>("bookkeepingConfirmation"); QTRY_VERIFY(confirm->isVisible());
    QVERIFY(window.findChild<QPushButton *>("parseBookkeepingButton")->isHidden());
    QVERIFY(window.findChild<QPushButton *>("saveBookkeepingDraftButton")->isVisible());
    DatabaseManager database(path); QVERIFY(database.initialize()); TransactionRepository repository(database);
    QList<Transaction> records; QVERIFY(repository.findAll(records)); QVERIFY(records.isEmpty());
    QVERIFY(stub.requestBodies.first().contains("data:image/jpeg;base64,"));
    window.findChild<QDoubleSpinBox *>("bookkeepingDraftAmountSpinBox")->setValue(9.99);
    click(window,"saveBookkeepingDraftButton");
    QVERIFY(repository.findAll(records)); QCOMPARE(records.size(),1); QCOMPARE(records[0].amountCents(),999);
    QVERIFY(!records[0].sourceImage().isEmpty());
    stub.delay=1000; input->setPlainText(QStringLiteral("再买一本")); click(window,"parseBookkeepingButton");
    click(window,"cancelBookkeepingAiButton");
    QCOMPARE(input->toPlainText(),QStringLiteral("再买一本")); QVERIFY(!confirm->isVisible());
    stub.delay=0; click(window,"parseBookkeepingButton"); QTRY_VERIFY(confirm->isVisible());
    input->setPlainText(QStringLiteral("更改输入使旧结果失效")); QVERIFY(!confirm->isVisible());
    QVERIFY(repository.findAll(records)); QCOMPARE(records.size(),1);
}
void BillingTests::lockedSavePreservesDraft()
{
    QTemporaryDir directory; const auto path=directory.filePath("lock.sqlite"); MainWindow window(path);
    window.showPage(MainWindow::Page::Billing); click(window,"bookkeepingManualModeButton"); fillManual(window,QStringLiteral("锁定重试"),6.78);
    DatabaseManager database(path); QVERIFY(database.initialize()); QSqlQuery lock(database.database());
    QVERIFY(lock.exec("BEGIN IMMEDIATE")); click(window,"saveBookkeepingManualButton");
    QCOMPARE(window.findChild<QLineEdit *>("bookkeepingNameEdit")->text(),QStringLiteral("锁定重试"));
    QVERIFY(!window.findChild<QLabel *>("bookkeepingErrorLabel")->isHidden());
    QVERIFY(lock.exec("ROLLBACK")); click(window,"saveBookkeepingManualButton");
    TransactionRepository repository(database); QList<Transaction> records; QVERIFY(repository.findAll(records));
    QCOMPARE(records.size(),1); QCOMPARE(records[0].amountCents(),678);
}
void BillingTests::monthChartsBudgetAndResponsivePreviews()
{
    QTemporaryDir directory; const auto path=directory.filePath("demo.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize()); QString error; QVERIFY(DemoData::populate(database,error));
    MainWindow window(path); window.resize(1440,960); window.showPage(MainWindow::Page::Billing); window.show(); settle();
    auto *workspace=window.findChild<BillingWorkspace *>(); QVERIFY(workspace);
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    const auto capture=[&](const QString &name) { settle(); if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(window.grab().save(preview+"/billing-"+name+".png")); } };
    capture("entry"); click(window,"billingChartsViewButton");
    auto *calendar=window.findChild<BillingCalendar *>(); QVERIFY(calendar->isVisible()); capture("calendar");
    QVERIFY(!window.findChild<QWidget *>("billingSidebar")->isVisible());
    QTest::mouseClick(calendar,Qt::LeftButton,Qt::NoModifier,calendar->cellRect(1).center().toPoint());
    QCOMPARE(calendar->selectedDate().day(),1);
    click(window,"billingTrendViewButton"); auto *trend=window.findChild<BillingTrendChart *>(); QVERIFY(trend->isVisible());
    click(window,"billingSixMonthTrendButton"); QCOMPARE(trend->points().size(),6); capture("trend");
    click(window,"billingChartDetailsButton"); QVERIFY(window.findChild<QWidget *>("billingSidebar")->isVisible()); capture("details");
    click(window,"billingChartDetailsButton");
    click(window,"billingQuickRecordButton"); click(window,"billingBudgetSummaryButton");
    auto *budget=window.findChild<QDialog *>("billingBudgetDialog"); QVERIFY(budget->isVisible());
    window.findChild<QDoubleSpinBox *>("basicBudgetAmountSpinBox")->setValue(1000);
    window.findChild<QDoubleSpinBox *>("budgetAmountSpinBox")->setValue(2000); click(window,"saveBudgetButton");
    QVERIFY(window.findChild<QLabel *>("billingBudgetTotalLabel")->text().contains("3,000")); budget->accept(); settle();
    QVERIFY(window.findChild<QLabel *>("billingBudgetSummaryLabel")->text().contains("2,763"));
    auto *month=window.findChild<MonthPicker *>("billingMonthPicker"); month->openPicker(); settle();
    QVERIFY(month->findChild<QWidget *>("monthPickerPopup")); click(*month,"pickMonth2");
    QCOMPARE(workspace->month().month(),2);
    QVERIFY(workspace->setMonth(QDate::currentDate()));
    for (const auto &theme:QStringList{"warm","cool","dark"}) {
        AppTheme::apply(&window,theme); window.resize(800,600); settle();
        auto *scroll=window.findChild<QScrollArea *>("billingWorkspaceScrollArea");
        QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
        capture("compact-"+theme);
        click(window,"billingChartsViewButton"); click(window,"billingCalendarViewButton");
        QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); capture("calendar-compact-"+theme);
        click(window,"billingSixMonthTrendButton");
        QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); capture("trend-compact-"+theme);
        click(window,"billingChartDetailsButton");
        QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); capture("details-compact-"+theme);
        click(window,"billingChartDetailsButton"); click(window,"billingRecordViewButton");
    }
}
void BillingTests::focusedEntryPreservesDraftAndBudgetContext()
{
    QTemporaryDir directory; const auto path=directory.filePath("focused-entry.sqlite");
    MainWindow window(path); window.resize(1280,800); window.showPage(MainWindow::Page::Billing); window.show(); settle();
    auto *sidebar=window.findChild<QWidget *>("billingSidebar");
    auto *examples=window.findChild<QWidget *>("bookkeepingExamples");
    auto *summary=window.findChild<QLabel *>("billingBudgetSummaryLabel");
    auto *workspace=window.findChild<BillingWorkspace *>();
    QVERIFY(!sidebar->isVisible()); QVERIFY(examples->isVisible());
    QVERIFY(summary->text().contains(QStringLiteral("还没有设置")));
    auto *input=window.findChild<QPlainTextEdit *>("bookkeepingInput");
    input->setPlainText(QStringLiteral("午饭 15 元，等会儿确认"));
    QVERIFY(!examples->isVisible());
    click(window,"billingChartsViewButton"); QVERIFY(!sidebar->isVisible());
    click(window,"billingChartDetailsButton"); QVERIFY(sidebar->isVisible());
    click(window,"billingRecordViewButton"); QVERIFY(!sidebar->isVisible());
    QCOMPARE(input->toPlainText(),QStringLiteral("午饭 15 元，等会儿确认"));
    click(window,"bookkeepingManualModeButton"); fillManual(window,QStringLiteral("午饭"),15);
    click(window,"saveBookkeepingManualButton"); click(window,"bookkeepingAiModeButton");
    QVERIFY(input->toPlainText().isEmpty()); QVERIFY(!examples->isVisible());
    click(window,"billingBudgetSummaryButton");
    auto *budget=window.findChild<QDialog *>("billingBudgetDialog"); QVERIFY(budget->isVisible());
    window.findChild<QDoubleSpinBox *>("basicBudgetAmountSpinBox")->setValue(0);
    window.findChild<QDoubleSpinBox *>("budgetAmountSpinBox")->setValue(10);
    click(window,"saveBudgetButton"); budget->accept(); settle();
    QVERIFY(summary->text().contains(QStringLiteral("已超出"))); QVERIFY(summary->text().contains("5.00"));
    QVERIFY(workspace->setMonth(QDate::currentDate().addMonths(-1)));
    QVERIFY(summary->text().contains(QStringLiteral("还没有设置")));
    QVERIFY(!examples->isVisible()); // An empty month does not bring onboarding back for existing users.
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        QVERIFY(QDir().mkpath(preview));
        QVERIFY(window.grab().save(preview+"/billing-empty-month.png"));
    }
}
void BillingTests::directChartViewsKeepRangeAndOpenSelectedRecords()
{
    QTemporaryDir directory; const auto path=directory.filePath("chart-ranges.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize()); TransactionRepository repository(database);
    Transaction previous(0,QStringLiteral("跨年消费"),1200,QStringLiteral("购物"),QDate(2023,12,12),{}, {},"online_shopping");
    Transaction current(0,QStringLiteral("一月消费"),3400,QStringLiteral("购物"),QDate(2024,1,9),{}, {},"online_shopping");
    Transaction outside(0,QStringLiteral("范围以外"),9900,QStringLiteral("购物"),QDate(2023,7,1),{}, {},"online_shopping");
    QVERIFY(repository.add(previous)); QVERIFY(repository.add(current)); QVERIFY(repository.add(outside));
    MainWindow window(path); window.resize(1280,800); window.showPage(MainWindow::Page::Billing); window.show(); settle();
    auto *workspace=window.findChild<BillingWorkspace *>(); QVERIFY(workspace->setMonth(QDate(2024,1,1)));
    QSignalSpy records(workspace,&BillingWorkspace::recordsRequested);
    click(window,"billingChartsViewButton"); click(window,"billingSixMonthTrendButton");
    QVERIFY(window.findChild<BillingTrendChart *>()->isVisible());
    QCOMPARE(window.findChild<QLabel *>("billingChartAmountLabel")->text(),QStringLiteral("¥46.00"));
    QVERIFY(window.findChild<QLabel *>("billingChartRangeLabel")->text().contains("2023.08"));
    QVERIFY(window.findChild<QLabel *>("billingChartRangeLabel")->text().contains("2024.01"));
    auto *trend=window.findChild<BillingTrendChart *>();
    QTest::mouseClick(trend,Qt::LeftButton,Qt::NoModifier,trend->rect().center()); settle();
    QVERIFY(!window.findChild<QLabel *>("billingTrendReadout")->text().contains(QStringLiteral("点击")));
    QVERIFY(!window.findChild<QPushButton *>("billingTrendRecordsButton")->isVisible());
    trend->pointSelected(QDate(2023,12,1),1200,1); settle();
    auto *open=window.findChild<QPushButton *>("billingTrendRecordsButton"); QVERIFY(open->isVisible()); click(window,"billingTrendRecordsButton");
    QCOMPARE(records.size(),1); QCOMPARE(records[0][0].toDate(),QDate(2023,12,1)); QCOMPARE(records[0][1].toDate(),QDate(2023,12,31));
    click(window,"billingRecordsBackButton");
    QVERIFY(window.findChild<QPushButton *>("billingSixMonthTrendButton")->isChecked());
    click(window,"billingTrendViewButton");
    QCOMPARE(window.findChild<QLabel *>("billingChartAmountLabel")->text(),QStringLiteral("¥34.00"));
    QVERIFY(!open->isVisible());
    trend->pointSelected(QDate(2024,1,9),3400,1); click(window,"billingTrendRecordsButton");
    QCOMPARE(records.size(),2); QCOMPARE(records[1][0].toDate(),QDate(2024,1,9)); QCOMPARE(records[1][1].toDate(),QDate(2024,1,9));
    click(window,"billingRecordsBackButton"); click(window,"billingRecordViewButton"); click(window,"billingChartsViewButton");
    QVERIFY(window.findChild<QPushButton *>("billingTrendViewButton")->isChecked());
    click(window,"billingCalendarViewButton");
    auto *calendar=window.findChild<BillingCalendar *>();
    QTest::mouseClick(calendar,Qt::LeftButton,Qt::NoModifier,calendar->cellRect(9).center().toPoint()); settle();
    click(window,"billingDayRecordsButton");
    QCOMPARE(records.size(),3); QCOMPARE(records[2][0].toDate(),QDate(2024,1,9)); QCOMPARE(records[2][1].toDate(),QDate(2024,1,9));
    click(window,"billingRecordsBackButton"); click(window,"billingTrendViewButton");
    QVERIFY(workspace->setMonth(QDate(2024,2,1))); QVERIFY(!open->isVisible());
    QCOMPARE(window.findChild<QLabel *>("billingChartAmountLabel")->text(),QStringLiteral("¥0.00"));
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(window.grab().save(preview+"/billing-empty-trend.png")); }
}
QTEST_MAIN(BillingTests)
#include "billing_tests.moc"
