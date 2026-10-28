#include "CategoryCatalog.h"
#include "CategoryRepository.h"
#include "CategoryWidgets.h"
#include "CategorySummary.h"
#include "DecisionDialog.h"
#include "ExpenseClassification.h"
#include "MonthlyReviewRepository.h"
#include "TransactionRepository.h"
#include "WishlistRepository.h"
#include "PriceRepository.h"
#include "CsvExporter.h"
#include "BillImportRepository.h"
#include "DatabaseBackupService.h"
#include "WishDialog.h"
#include "AppTheme.h"
#include "mainwindow.h"
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

namespace {
void settle(){QApplication::processEvents();QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
template<class T>T *field(QWidget &widget,const char *name){auto *value=widget.findChild<T *>(name);if(!value)qFatal("Missing %s",name);return value;}
Transaction record(const QString &key){return Transaction(0,QStringLiteral("一本书"),2300,CategoryCatalog::legacyCategory(key),QDate::currentDate(),{}, {},key,{},key,"basic");}
}
class CategoryManagementTests:public QObject {
    Q_OBJECT
private slots:
    void defaultSelectorsAndRemovedExecutionEntry();
    void settingsSaveRenameDisableAndRestart();
    void customRecordsStatisticsCsvBackupAndRollback();
    void legacyUpgradePreservesRecordsIndexesAndSequence();
    void themesAndLargeText();
};
void CategoryManagementTests::defaultSelectorsAndRemovedExecutionEntry()
{
    QTemporaryDir dir;DatabaseManager db(dir.filePath("defaults.sqlite"));QVERIFY2(db.initialize(),qPrintable(db.lastError()));
    MainWindow window(db.databasePath());window.show();settle();
    DecisionDialog dialog(db,{},&window);field<QPushButton>(dialog,"decisionManualButton")->click();
    auto *category=field<QComboBox>(dialog,"decisionCategoryComboBox");
    QCOMPARE(category->count(),5);
    QCOMPARE(category->currentData().toString(),QStringLiteral("other"));
    const QStringList labels={QStringLiteral("吃喝"),QStringLiteral("玩乐"),QStringLiteral("服饰鞋包"),QStringLiteral("交通"),QStringLiteral("其他")};
    for(int i=0;i<labels.size();++i)QCOMPARE(category->itemText(i),labels[i]);
    WishDialog wish(&window);auto *wishCategory=field<QComboBox>(wish,"wishCategoryComboBox");
    for(int i=0;i<5;++i)QCOMPARE(wishCategory->itemData(i),category->itemData(i));
    window.showPage(MainWindow::Page::Execution);settle();
    QVERIFY(!window.findChild<QPushButton *>("executionInputButton"));
    QVERIFY(field<QPushButton>(window,"executionNextButton")->isVisible());
    auto *executionCategory=field<QComboBox>(window,"executionCategoryComboBox");
    QCOMPARE(executionCategory->count(),5);
    for(int i=0;i<5;++i)QCOMPARE(executionCategory->itemText(i),labels[i]);
    ParsedProduct old;old.name=QStringLiteral("奶茶");old.amountCents=1500;old.category="daily";
    CategoryWidgets::select(category,old.category);QCOMPARE(category->currentData().toString(),QStringLiteral("food"));
}
void CategoryManagementTests::settingsSaveRenameDisableAndRestart()
{
    QTemporaryDir dir;const auto path=dir.filePath("settings.sqlite");
    {
        MainWindow window(path);window.show();window.showPage(MainWindow::Page::Settings);settle();
        field<QPushButton>(window,"settingsCategoriesButton")->click();settle();
        auto *settings=field<CategorySettingsWidget>(window,"categorySettingsWidget");QVERIFY(settings->isVisible());
        field<QLineEdit>(*settings,"newCategoryNameEdit")->setText(QStringLiteral("书籍"));field<QPushButton>(*settings,"addCategoryButton")->click();settle();
        DatabaseManager db(path);QVERIFY(db.initialize());CategoryRepository repository(db.database());QList<UserCategory> categories;QVERIFY(repository.load(categories));QCOMPARE(categories.size(),1);
        const auto first=categories.first();auto *classification=field<ExpenseClassification>(window,"expenseClassification");
        QVERIFY(classification->purposeCombo()->findData(first.key)>=5);
        window.showPage(MainWindow::Page::Billing);field<QPushButton>(window,"bookkeepingManualModeButton")->click();
        field<QLineEdit>(window,"bookkeepingNameEdit")->setText(QStringLiteral("阅读用书"));field<QDoubleSpinBox>(window,"bookkeepingAmountSpinBox")->setValue(23);
        field<QComboBox>(window,"bookkeepingCategoryComboBox")->setCurrentIndex(field<QComboBox>(window,"bookkeepingCategoryComboBox")->findData(first.key));
        field<QPushButton>(window,"saveBookkeepingManualButton")->click();settle();
        QList<Transaction> records;QVERIFY(TransactionRepository(db).findAll(records));QCOMPARE(records.size(),1);QCOMPARE(records.first().purposeCategory(),first.key);
        window.showPage(MainWindow::Page::Settings);field<QPushButton>(window,"settingsCategoriesButton")->click();settle();
        field<QLineEdit>(*settings,qPrintable(QStringLiteral("categoryName_%1").arg(first.id)))->setText(QStringLiteral("阅读"));field<QPushButton>(*settings,qPrintable(QStringLiteral("renameCategory_%1").arg(first.id)))->click();settle();
        QVERIFY(TransactionRepository(db).findAll(records));QCOMPARE(records.first().purposeCategory(),CategoryCatalog::customKey(QStringLiteral("阅读")));QCOMPARE(records.first().amountCents(),2300);
        field<QPushButton>(*settings,qPrintable(QStringLiteral("toggleCategory_%1").arg(first.id)))->click();settle();
        QVERIFY(repository.load(categories));QVERIFY(categories.isEmpty());
        QVERIFY(field<QComboBox>(window,"categoryFilter")->findData(CategoryCatalog::customKey(QStringLiteral("阅读")))>=0);
        DecisionDialog newItem(db,{},&window);QCOMPARE(field<QComboBox>(newItem,"decisionCategoryComboBox")->findData(CategoryCatalog::customKey(QStringLiteral("阅读"))),-1);
        QVERIFY(TransactionRepository(db).findAll(records));QCOMPARE(records.size(),1);
    }
    DatabaseManager reopened(path);QVERIFY(reopened.initialize());QList<UserCategory> categories;QVERIFY(CategoryRepository(reopened.database()).load(categories,true));QCOMPARE(categories.size(),1);QVERIFY(!categories.first().active);
    DatabaseManager other(":memory:");QVERIFY(other.initialize());QVERIFY(CategoryRepository(other.database()).load(categories));QVERIFY(categories.isEmpty());
}
void CategoryManagementTests::customRecordsStatisticsCsvBackupAndRollback()
{
    QTemporaryDir dir;DatabaseManager db(dir.filePath("records.sqlite"));QVERIFY(db.initialize());CategoryRepository categories(db.database());
    QVERIFY(categories.add(QStringLiteral("书籍")));QVERIFY(!categories.add(QStringLiteral("吃喝")));QVERIFY(!categories.add(QStringLiteral("  ")));QVERIFY(!categories.add(QStringLiteral("书籍")));
    const auto key=CategoryCatalog::customKey(QStringLiteral("书籍"));auto expense=record(key);QVERIFY(TransactionRepository(db).add(expense));
    WishItem wish;wish.name=QStringLiteral("诗集");wish.estimatedCents=5000;wish.category=QStringLiteral("其他");wish.detailCategory=key;wish.coolingDays=1;wish.priority=2;QVERIFY(WishlistRepository(db).add(wish));
    QSqlQuery q(db.database());QVERIFY(q.exec("CREATE TRIGGER fail_category_rename BEFORE UPDATE OF purpose_category ON transactions BEGIN SELECT RAISE(ABORT,'fixture'); END"));
    QVERIFY(!categories.rename(key,QStringLiteral("阅读")));QList<UserCategory> names;QVERIFY(categories.load(names));QCOMPARE(names.first().key,key);
    QVERIFY(q.exec("DROP TRIGGER fail_category_rename"));QVERIFY(categories.rename(key,QStringLiteral("阅读")));const auto updated=CategoryCatalog::customKey(QStringLiteral("阅读"));
    QList<Transaction> records;QVERIFY(TransactionRepository(db).findAll(records));QCOMPARE(records.first().purposeCategory(),updated);QCOMPARE(records.first().budgetScope(),QStringLiteral("basic"));
    QList<WishItem> wishes;QVERIFY(WishlistRepository(db).findAll(wishes));QCOMPARE(wishes.first().detailCategory,updated);
    const auto summary=CategorySummary::calculatePurposes(records,QDate::currentDate());bool counted=false;for(const auto &category:summary)if(category.category==QStringLiteral("阅读")){QCOMPARE(category.amountCents,2300);counted=true;}QVERIFY(counted);
    MonthlyReviewData monthly;QVERIFY(MonthlyReviewRepository(db).load(QDate::currentDate(),monthly));counted=false;for(const auto &category:monthly.categories)if(category.key==updated){QCOMPARE(category.cents,2300);counted=true;}QVERIFY(counted);
    // A custom label can match a historical alias without becoming a default category on CSV import.
    const auto alias=CategoryCatalog::customKey(QStringLiteral("出行"));QVERIFY(categories.add(QStringLiteral("出行")));
    Transaction aliasRecord(0,QStringLiteral("公交卡"),1000,QStringLiteral("其他"),QDate::currentDate(),{}, {},alias,{},alias,"discretionary");
    QVERIFY(TransactionRepository(db).add(aliasRecord));QVERIFY(TransactionRepository(db).findAll(records));
    QString error;const auto csv=dir.filePath("custom.csv");QVERIFY(CsvExporter::write(csv,records,error));BillParseResult parsed;QVERIFY2(BillParser::parseFile(csv,parsed,error),qPrintable(error));QCOMPARE(parsed.rows.size(),2);QSet<QString> purposes;for(const auto &row:parsed.rows)purposes.insert(row.purposeCategory);QVERIFY(purposes.contains(updated));QVERIFY(purposes.contains(alias));
    DatabaseManager destination(dir.filePath("import.sqlite"));QVERIFY(destination.initialize());BillImportRepository imports(destination);QVERIFY(imports.prepare(parsed));for(auto &row:parsed.rows){row.selected=true;row.confirmed=true;}BillImportOutcome outcome;QVERIFY2(imports.commit(parsed,outcome),qPrintable(imports.lastError()));QCOMPARE(outcome.inserted,2);QVERIFY(CategoryRepository(destination.database()).load(names));QCOMPARE(names.size(),2);
    DatabaseBackupService backup(db);const auto copy=dir.filePath("copy.sqlite");QVERIFY2(backup.backup(copy,error),qPrintable(error));
    QVERIFY(categories.setActive(updated,false));BackupInfo info;QVERIFY2(backup.prepareRestore(copy,info,error),qPrintable(error));QString safety;QVERIFY2(backup.restorePrepared(safety,error),qPrintable(error));QVERIFY(categories.load(names));QCOMPARE(names.first().key,updated);
}
void CategoryManagementTests::legacyUpgradePreservesRecordsIndexesAndSequence()
{
    QTemporaryDir dir;const auto path=dir.filePath("legacy.sqlite");const QString connection="legacy-category-fixture";
    {
        auto old=QSqlDatabase::addDatabase("QSQLITE",connection);old.setDatabaseName(path);QVERIFY(old.open());QSqlQuery q(old);
        QVERIFY(q.exec("CREATE TABLE transactions(id INTEGER PRIMARY KEY AUTOINCREMENT,description TEXT NOT NULL,amount_cents INTEGER NOT NULL,category TEXT NOT NULL,date TEXT NOT NULL,occurred_time TEXT NOT NULL DEFAULT '',note TEXT NOT NULL DEFAULT '',detail_category TEXT NOT NULL DEFAULT '',source_image BLOB NOT NULL DEFAULT X'',purpose_category TEXT NOT NULL DEFAULT '' CHECK(purpose_category IN ('','food','fun','clothing','travel','other','unclassified')),budget_scope TEXT NOT NULL DEFAULT '')"));
        QVERIFY(q.exec("CREATE INDEX fixture_date_index ON transactions(date)"));QVERIFY(q.exec("INSERT INTO transactions(id,description,amount_cents,category,date,detail_category) VALUES(7,'old',1234,'餐饮','2026-10-01','canteen')"));QVERIFY(q.exec("UPDATE sqlite_sequence SET seq=70 WHERE name='transactions'"));old.close();
    }
    QSqlDatabase::removeDatabase(connection);DatabaseManager db(path);QVERIFY2(db.initialize(),qPrintable(db.lastError()));
    QList<Transaction> records;QVERIFY(TransactionRepository(db).findAll(records));QCOMPARE(records.first().id(),7);QCOMPARE(records.first().amountCents(),1234);QVERIFY(records.first().isBasic());
    QSqlQuery q(db.database());QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE name='fixture_date_index'"));QVERIFY(q.next());q.finish();
    const auto key=CategoryCatalog::customKey(QStringLiteral("书籍"));QVERIFY(CategoryRepository(db.database()).add(QStringLiteral("书籍")));auto added=record(key);QVERIFY2(TransactionRepository(db).add(added),qPrintable(TransactionRepository(db).lastError()));QCOMPARE(added.id(),71);
    QVERIFY(db.initialize());QVERIFY(TransactionRepository(db).findAll(records));QCOMPARE(records.size(),2);
}
void CategoryManagementTests::themesAndLargeText()
{
    QTemporaryDir dir;DatabaseManager db(dir.filePath("preview.sqlite"));QVERIFY(db.initialize());QVERIFY(CategoryRepository(db.database()).add(QStringLiteral("运动与健康")));
    const auto output=qEnvironmentVariable("KURA_CATEGORY_PREVIEW_DIR");if(!output.isEmpty())QVERIFY(QDir().mkpath(output));
    for(const QString &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}){
        MainWindow window(db.databasePath());window.setProperty("fontScale",130);AppTheme::apply(&window,theme);window.resize(850,780);window.show();window.showPage(MainWindow::Page::Settings);field<QPushButton>(window,"settingsCategoriesButton")->click();settle();
        auto *settings=field<CategorySettingsWidget>(window,"categorySettingsWidget");QVERIFY(settings->isVisible());
        QVERIFY(field<QPushButton>(*settings,"addCategoryButton")->isEnabled());
        if(!output.isEmpty())QVERIFY(window.grab().save(QDir(output).filePath("categories-"+theme+"-130.png")));
    }
}
QTEST_MAIN(CategoryManagementTests)
#include "category_management_tests.moc"
