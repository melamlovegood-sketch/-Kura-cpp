#include "ExpenseClassification.h"
#include "ExpenseComposer.h"
#include "AddTransactionDialog.h"
#include "AppTheme.h"
#include "BillingAggregation.h"
#include "BudgetRepository.h"
#include "CategoryCatalog.h"
#include "CategorySummary.h"
#include "CsvExporter.h"
#include "DatabaseBackupService.h"
#include "MonthlyReviewRepository.h"
#include "BillImportRepository.h"
#include "TransactionRepository.h"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class ExpenseClassificationTests:public QObject {
    Q_OBJECT
private slots:
    void dimensionsSurviveRestartAndBudgetChanges();
    void legacyMigrationPreservesBalances();
    void modeSwitchPreservesDraftAndPreference();
    void backupAndCsvRetainIndependentDimensions();
};
void ExpenseClassificationTests::dimensionsSurviveRestartAndBudgetChanges() {
    QTemporaryDir dir; const auto path=dir.filePath("expenses.sqlite"); const QDate month(2026,10,1);
    {
        DatabaseManager db(path); QVERIFY(db.initialize()); TransactionRepository repo(db);
        Transaction meal(0,QStringLiteral("食堂"),3000,QStringLiteral("餐饮"),month,{}, {},"daily",{},"food","basic");
        Transaction drink(0,QStringLiteral("奶茶"),1800,QStringLiteral("餐饮"),month,{}, {},"daily",{},"food","discretionary");
        Transaction shoes(0,QStringLiteral("必要的鞋"),39900,QStringLiteral("购物"),month,{}, {},"online_shopping",{},"clothing","basic");
        QVERIFY(repo.add(meal)); QVERIFY(repo.add(drink)); QVERIFY(repo.add(shoes));
    }
    DatabaseManager db(path); QVERIFY(db.initialize()); TransactionRepository repo(db); QList<Transaction> records; QVERIFY(repo.findAll(records)); QCOMPARE(records.size(),3);
    BudgetRepository budgets(db); QVERIFY(budgets.saveSplit(month,100000,50000)); MonthlyBudgetStatus status; QVERIFY(budgets.loadStatus(month,status));
    QCOMPARE(status.basicSpentCents,42900); QCOMPARE(status.discretionarySpentCents,1800);
    const auto summary=CategorySummary::calculatePurposes(records,month); QCOMPARE(summary[0].amountCents,4800); QCOMPARE(summary[2].amountCents,39900);
    BillingSnapshot snapshot; QString error; QVERIFY(BillingAggregation::calculate(records,month,snapshot,error)); QCOMPARE(snapshot.basicCents,status.basicSpentCents);
    MonthlyReviewRepository review(db); MonthlyReviewData data; QVERIFY2(review.load(month,data),qPrintable(review.lastError())); QCOMPARE(data.basicCents,42900); const auto before=data.factsHash;
    for (const auto &record:records) if (record.purposeCategory()=="clothing") {
        Transaction edited(record.id(),record.description(),record.amountCents(),record.category(),record.date(),record.time(),record.note(),record.detailCategory(),record.sourceImage(),"clothing","discretionary"); QVERIFY(repo.update(edited));
    }
    QVERIFY(review.load(month,data)); QVERIFY(data.factsHash!=before); QCOMPARE(data.basicCents,3000); QCOMPARE(data.discretionaryCents,41700);
    QVERIFY(budgets.saveSplit(month,200000,50000)); QVERIFY(budgets.loadStatus(month,status)); QCOMPARE(status.discretionaryLimitCents,50000); // No implicit transfer.
}
void ExpenseClassificationTests::legacyMigrationPreservesBalances() {
    QTemporaryDir dir; const auto path=dir.filePath("legacy.sqlite"); const QDate month(2026,10,1);
    {
        DatabaseManager db(path); QVERIFY(db.initialize()); QSqlQuery q(db.database());
        QVERIFY(q.exec("INSERT INTO transactions(id,description,amount_cents,category,date,detail_category) VALUES(41,'meal',3000,'餐饮','2026-10-01','canteen'),(42,'shop',7000,'购物','2026-10-01','online_shopping')"));
        QVERIFY(q.exec("ALTER TABLE transactions DROP COLUMN purpose_category")); QVERIFY(q.exec("ALTER TABLE transactions DROP COLUMN budget_scope"));
    }
    DatabaseManager db(path); QVERIFY(db.initialize()); TransactionRepository repo(db); QList<Transaction> records; QVERIFY(repo.findAll(records)); QCOMPARE(records.size(),2);
    for (const auto &record:records) { if (record.id()==41) { QCOMPARE(record.purposeCategory(),QString("food")); QVERIFY(record.isBasic()); } else { QCOMPARE(record.id(),42); QCOMPARE(record.purposeCategory(),QString("unclassified")); QVERIFY(!record.isBasic()); } }
    BudgetRepository budgets(db); MonthlyBudgetStatus status; QVERIFY(budgets.loadStatus(month,status)); QCOMPARE(status.basicSpentCents,3000); QCOMPARE(status.discretionarySpentCents,7000);
    QVERIFY(db.initialize()); QVERIFY(repo.findAll(records)); QCOMPARE(records.size(),2);
}
void ExpenseClassificationTests::modeSwitchPreservesDraftAndPreference() {
    QTemporaryDir dir; const auto settings=dir.filePath("preferences.ini");
    {
        ExpenseClassification selector(settings); selector.resize(420,180); selector.show();
        QCOMPARE(selector.purposeCombo()->count(),6); QCOMPARE(selector.purpose(),QString("unclassified")); QVERIFY(!selector.budgetMode());
        selector.purposeCombo()->setCurrentIndex(selector.purposeCombo()->findData("clothing")); selector.findChild<QPushButton *>("expenseBasicButton")->click();
        selector.findChild<QPushButton *>("expenseByBudgetButton")->click(); QVERIFY(selector.budgetMode()); QCOMPARE(selector.purpose(),QString("clothing")); QCOMPARE(selector.scope(),QString("basic"));
        selector.findChild<QPushButton *>("expenseByPurposeButton")->click(); QCOMPARE(selector.purpose(),QString("clothing")); QCOMPARE(selector.scope(),QString("basic"));
        selector.findChild<QPushButton *>("expenseByBudgetButton")->click();
    }
    ExpenseClassification restored(settings); QVERIFY(restored.budgetMode());
    DatabaseManager db(dir.filePath("entry.sqlite")); QVERIFY(db.initialize()); ExpenseComposer composer(db,settings); composer.setDatabaseAvailable(true); AppTheme::apply(&composer); composer.resize(620,460); composer.show();
    composer.findChild<QPushButton *>("bookkeepingManualModeButton")->click(); composer.findChild<QDoubleSpinBox *>("bookkeepingAmountSpinBox")->setValue(18);
    auto selectors=composer.findChildren<ExpenseClassification *>(); for (auto *selector:selectors) if (selector->isVisible()) { selector->setValues("food","basic"); selector->findChild<QPushButton *>("expenseByPurposeButton")->click(); }
    for (auto *selector:selectors) QVERIFY(!selector->budgetMode());
    QSignalSpy saved(&composer,&ExpenseComposer::expenseSaved); composer.findChild<QPushButton *>("saveBookkeepingManualButton")->click(); QCOMPARE(saved.size(),1);
    TransactionRepository repo(db); QList<Transaction> records; QVERIFY(repo.findAll(records)); QCOMPARE(records.size(),1); QCOMPARE(records[0].purposeCategory(),QString("food")); QCOMPARE(records[0].budgetScope(),QString("basic")); QVERIFY(!records[0].description().isEmpty());
    AddTransactionDialog editor(records[0]); auto *selector=editor.findChild<ExpenseClassification *>(); selector->purposeCombo()->setCurrentIndex(selector->purposeCombo()->findData("fun")); QCOMPARE(editor.transaction().budgetScope(),QString("basic")); QCOMPARE(editor.transaction().purposeCategory(),QString("fun"));
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!preview.isEmpty()) { QDir().mkpath(preview); composer.findChild<QLineEdit *>("bookkeepingNameEdit")->setText(QStringLiteral("奶茶")); composer.findChild<QDoubleSpinBox *>("bookkeepingAmountSpinBox")->setValue(18); for (auto *item:selectors) { item->setValues("food","discretionary"); item->useBudgetMode(false); } QApplication::processEvents(); QVERIFY(composer.grab().save(preview+"/expense-purpose.png")); for (auto *item:selectors) item->useBudgetMode(true); QApplication::processEvents(); QVERIFY(composer.grab().save(preview+"/expense-budget.png")); }
}
void ExpenseClassificationTests::backupAndCsvRetainIndependentDimensions() {
    QTemporaryDir dir; DatabaseManager db(dir.filePath("source.sqlite")); QVERIFY(db.initialize()); TransactionRepository repo(db);
    Transaction expense(0,QStringLiteral("鞋"),39900,QStringLiteral("购物"),QDate(2026,10,3),{}, {},"online_shopping",{},"clothing","basic"); QVERIFY(repo.add(expense));
    Transaction other(0,QStringLiteral("其他消费"),100,QStringLiteral("其他"),QDate(2026,10,4),{}, {},"other",{},"unclassified","discretionary"); QVERIFY(repo.add(other));
    QList<Transaction> records; QVERIFY(repo.findAll(records)); QString error; const auto csv=dir.filePath("export.csv"); QVERIFY(CsvExporter::write(csv,records,error)); BillParseResult parsed; QVERIFY(BillParser::parseFile(csv,parsed,error)); QCOMPARE(parsed.rows.size(),2);
    DatabaseManager imported(dir.filePath("imported.sqlite")); QVERIFY(imported.initialize()); BillImportRepository importer(imported); for (auto &row:parsed.rows) row.confirmed=true; BillImportOutcome outcome; QVERIFY2(importer.commit(parsed,outcome),qPrintable(importer.lastError())); QCOMPARE(outcome.inserted,2);
    TransactionRepository importedRepo(imported); QVERIFY(importedRepo.findAll(records)); for (const auto &record:records) if (record.amountCents()==39900) { QCOMPARE(record.purposeCategory(),QString("clothing")); QCOMPARE(record.budgetScope(),QString("basic")); } else QCOMPARE(record.purposeCategory(),QString("unclassified"));
    DatabaseBackupService backups(db); const auto backup=dir.filePath("backup.sqlite"); QVERIFY2(backups.backup(backup,error),qPrintable(error)); QVERIFY(repo.remove(expense.id())); BackupInfo info; QVERIFY2(backups.prepareRestore(backup,info,error),qPrintable(error)); QString safety; QVERIFY2(backups.restorePrepared(safety,error),qPrintable(error)); QVERIFY(repo.findAll(records)); QCOMPARE(records.size(),2); for (const auto &record:records) if (record.id()==expense.id()) { QCOMPARE(record.purposeCategory(),QString("clothing")); QVERIFY(record.isBasic()); }
}
QTEST_MAIN(ExpenseClassificationTests)
#include "expense_classification_tests.moc"
