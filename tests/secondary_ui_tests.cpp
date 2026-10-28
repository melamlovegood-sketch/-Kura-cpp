#include "AIStubServer.h"
#include "AppTheme.h"
#include "BillImportDialog.h"
#include "BillTestFixtures.h"
#include "DataManagementWidget.h"
#include "PriceRecordDialog.h"
#include "ProductDetailsDialog.h"
#include "PurchaseRepository.h"
#include "ReviewWidget.h"
#include "ShoppingProfileDialog.h"
#include "TransactionRepository.h"
#include "WishDialog.h"
#include "mainwindow.h"
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

namespace {
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QApplication::processEvents(); }
void click(QWidget &owner,const char *name) { auto *button=owner.findChild<QPushButton *>(name); QVERIFY2(button,name); button->click(); settle(); }
void capture(QWidget &widget,const QString &name) {
    settle(); const auto dir=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if(!dir.isEmpty()) { QVERIFY(QDir().mkpath(dir)); QVERIFY(widget.grab().save(dir+"/secondary-"+name+".png")); }
}
QByteArray imageBytes() { QImage image(160,90,QImage::Format_RGB32); image.fill(QColor("#e0c7a7")); QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG"); return bytes; }
}
class SecondaryUiTests:public QObject {
    Q_OBJECT
private slots:
    void wishOptionsAndCustomCooling();
    void screenshotActionsAppearWhenNeeded();
    void productReadingEditingAndDiscussion();
    void dataOperationsAndRestoreScope();
    void recordPageShowsOnlyFilteredSummary();
    void reviewOpensOnClickAndFeedsBrandHistory();
    void brandListAndSeparateEditor();
    void importSelectionTools();
};
void SecondaryUiTests::wishOptionsAndCustomCooling()
{
    QWidget host; host.resize(800,600); AppTheme::apply(&host,"warm"); host.show();
    WishItem original; original.name=QStringLiteral("通勤耳机"); original.estimatedCents=32900; original.coolingDays=5; original.priority=1; original.reason=QStringLiteral("地铁上使用"); original.detailCategory="online_shopping";
    WishDialog dialog(original,&host); dialog.open(); settle();
    QVERIFY(!dialog.findChild<QWidget *>("wishMoreOptions")->isVisible());
    QVERIFY(dialog.findChild<QSpinBox *>("wishDaysSpinBox")->isVisible()); QCOMPARE(dialog.item().reason,original.reason);
    click(dialog,"wishCoolingPreset7"); QCOMPARE(dialog.item().coolingDays,7); QVERIFY(!dialog.findChild<QSpinBox *>("wishDaysSpinBox")->isVisible());
    click(dialog,"wishCoolingCustomButton"); dialog.findChild<QSpinBox *>("wishDaysSpinBox")->setValue(9);
    QCOMPARE(dialog.item().coolingDays,9); click(dialog,"wishMoreOptionsButton");
    QVERIFY(dialog.findChild<QLineEdit *>("wishReasonEdit")->isVisible());
    dialog.findChild<QLineEdit *>("wishReasonEdit")->setText(QStringLiteral("通勤使用，先比较舒适度"));
    click(dialog,"wishMoreOptionsButton"); QCOMPARE(dialog.item().priority,1);
    QCOMPARE(dialog.item().reason,QStringLiteral("通勤使用，先比较舒适度"));
    capture(dialog,"wish");
    for(const auto &theme:QStringList{"warm","cool","dark"}) { AppTheme::apply(&host,theme); click(dialog,"wishMoreOptionsButton"); capture(dialog,"wish-options-"+theme); auto *scroll=dialog.findChild<QScrollArea *>("appDialogScroll"); QVERIFY(scroll); QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); click(dialog,"wishMoreOptionsButton"); }
}
void SecondaryUiTests::screenshotActionsAppearWhenNeeded()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("price.sqlite")); QVERIFY(database.initialize());
    PriceRecordDialog dialog(database); AppTheme::apply(&dialog,"warm"); dialog.show(); settle();
    QVERIFY(dialog.findChild<QLineEdit *>("priceNameEdit")->isEnabled());
    QVERIFY(!dialog.findChild<QPushButton *>("priceAnalyzeButton")->isVisible()); QVERIFY(!dialog.findChild<QPushButton *>("priceRemoveImageButton")->isVisible());
    QVERIFY(!dialog.findChild<QPushButton *>("priceAISettingsButton")->isVisible()); capture(dialog,"price-manual");
    const auto path=dir.filePath("price.png"); QFile image(path); QVERIFY(image.open(QIODevice::WriteOnly)); image.write(imageBytes()); image.close();
    QVERIFY(dialog.setScreenshot(path)); QVERIFY(dialog.findChild<QPushButton *>("priceAnalyzeButton")->isVisible()); QVERIFY(dialog.findChild<QPushButton *>("priceAISettingsButton")->isVisible());
    click(dialog,"priceRemoveImageButton"); QVERIFY(!dialog.findChild<QPushButton *>("priceAnalyzeButton")->isVisible());
    dialog.findChild<QLineEdit *>("priceNameEdit")->setText(QStringLiteral("手动记录的耳机")); dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(199);
    QSignalSpy saved(&dialog,&PriceRecordDialog::priceSaved); click(dialog,"savePriceRecordButton"); QCOMPARE(saved.count(),1);
}
void SecondaryUiTests::productReadingEditingAndDiscussion()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("product.sqlite")); QVERIFY(database.initialize());
    ProductContext context{QString(),QStringLiteral("日常针织衫"),"online_shopping",0,22800,imageBytes()};
    ProductRepository products(database); ProductProfile profile; QVERIFY(products.ensure(context,profile)); profile.fields={{"brand","MUJI"},{"materials",QStringLiteral("棉")}}; profile.notes=QStringLiteral("在宿舍穿"); QVERIFY(products.save(profile));
    AIStubServer server; server.handler=[](const QJsonObject &) { return QStringLiteral("先核对尺码和洗护条件。"); }; QString error; QVERIFY(AISettingsStore::save(database.databasePath()+".ui.ini",server.settings(),error));
    ProductDetailsDialog dialog(database,context,profile.id); AppTheme::apply(&dialog,"warm"); dialog.show(); settle();
    QVERIFY(dialog.findChild<QLabel *>("productDetailsSummaryLabel")->isVisible()); QVERIFY(dialog.findChild<QPushButton *>("sourceImageButton")->isVisible());
    QVERIFY(!dialog.findChild<QWidget *>("productDetailsEditor")->isVisible()); QVERIFY(!dialog.findChild<QWidget *>("productChatPanel")->isVisible());
    QVERIFY(!dialog.findChild<QPushButton *>("askProductAIButton")->isVisible()); QCOMPARE(server.requests.size(),0); capture(dialog,"product-reading");
    click(dialog,"editProductDetailsButton"); dialog.findChild<QLineEdit *>("productField_brand")->setText(QStringLiteral("未保存")); click(dialog,"cancelProductDetailsEditButton");
    QVERIFY(products.load(profile.id,profile)); QCOMPARE(profile.fields.value("brand").toString(),QStringLiteral("MUJI"));
    dialog.findChild<QToolButton *>("productChatToggle")->click(); settle(); QCOMPARE(server.requests.size(),0);
    dialog.findChild<QPlainTextEdit *>("productQuestionEdit")->setPlainText(QStringLiteral("需要核对什么？")); click(dialog,"askProductAIButton");
    QTRY_VERIFY(dialog.findChild<QPlainTextEdit *>("productConversationText")->toPlainText().contains(QStringLiteral("尺码")));
    dialog.findChild<QToolButton *>("productChatToggle")->click(); settle(); QVERIFY(!dialog.findChild<QWidget *>("productChatPanel")->isVisible());
    QVERIFY(products.load(profile.id,profile)); QVERIFY(!profile.conversation.isEmpty());
}
void SecondaryUiTests::dataOperationsAndRestoreScope()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("data.sqlite")); QVERIFY(database.initialize()); TransactionRepository records(database);
    Transaction food(0,QStringLiteral("午饭"),1500,QStringLiteral("餐饮"),QDate::currentDate()),metro(0,QStringLiteral("地铁"),600,QStringLiteral("交通"),QDate::currentDate()); QVERIFY(records.add(food)); QVERIFY(records.add(metro));
    DataManagementWidget widget(database); AppTheme::apply(&widget,"warm"); widget.resize(650,500); widget.setDatabaseAvailable(true); widget.show(); settle();
    TransactionFilter filter; filter.category=QStringLiteral("交通"); widget.setAppliedFilter(filter);
    QVERIFY(widget.findChild<QPushButton *>("importBillsButton")->isVisible()); QVERIFY(!widget.findChild<QComboBox *>("exportScopeComboBox")->isVisible()); capture(widget,"data-home");
    click(widget,"openExportOptionsButton"); widget.findChild<QComboBox *>("exportScopeComboBox")->setCurrentIndex(1);
    const auto csv=dir.filePath("filtered.csv"); QVERIFY(widget.exportTo(csv,false)); QFile file(csv); QVERIFY(file.open(QIODevice::ReadOnly)); const auto text=QString::fromUtf8(file.readAll()); QVERIFY(text.contains(QStringLiteral("地铁"))); QVERIFY(!text.contains(QStringLiteral("午饭")));
    click(widget,"dataOperationBackButton"); click(widget,"openExportOptionsButton"); QCOMPARE(widget.findChild<QComboBox *>("exportScopeComboBox")->currentIndex(),1);
    click(widget,"dataOperationBackButton"); click(widget,"openBackupOptionsButton"); const auto copy=dir.filePath("backup.sqlite"); QVERIFY(widget.backupTo(copy));
    click(widget,"dataOperationBackButton"); click(widget,"openRestoreOptionsButton"); QVERIFY(widget.requestRestore(copy)); settle(); auto *confirm=widget.findChild<QMessageBox *>("restoreConfirmationDialog"); QVERIFY(confirm);
    QVERIFY(confirm->text().contains(QStringLiteral("全部业务数据"))); QVERIFY(confirm->text().contains(QStringLiteral("自动备份"))); QVERIFY(!confirm->detailedText().isEmpty());
    confirm->button(QMessageBox::No)->click(); settle(); QList<Transaction> loaded; QVERIFY(records.findAll(loaded)); QCOMPARE(loaded.size(),2);
}
void SecondaryUiTests::recordPageShowsOnlyFilteredSummary()
{
    QTemporaryDir dir; MainWindow window(dir.filePath("records.sqlite")); window.resize(800,600); window.show(); window.showPage(MainWindow::Page::Billing); click(window,"billingAllRecordsButton");
    QVERIFY(window.findChild<QLabel *>("filteredExpenseLabel")->isVisible());
    for(const auto *name:{"totalExpenseLabel","monthExpenseLabel","todayExpenseLabel"}) QVERIFY(!window.findChild<QLabel *>(name)->isVisible());
    QVERIFY(!window.findChild<QTableWidget *>("categorySummaryTable")->isVisible());
    for(const auto &theme:QStringList{"warm","cool","dark"}) { AppTheme::apply(&window,theme); capture(window,"records-"+theme); }
}
void SecondaryUiTests::reviewOpensOnClickAndFeedsBrandHistory()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("review.sqlite")); QVERIFY(database.initialize()); ShoppingProfileRepository brands(database); BrandEntry brand; brand.name="MUJI"; brand.category="online_shopping"; QVERIFY(brands.saveBrand(brand));
    Transaction expense(0,QStringLiteral("针织衫"),22800,QStringLiteral("购物"),QDate::currentDate().addDays(-40),QTime(12,0),{},"online_shopping"); PurchaseRepository purchases(database); QVERIFY(purchases.record(expense,brand.id));
    ReviewWidget widget(database); AppTheme::apply(&widget,"warm"); widget.resize(760,500); widget.setDatabaseAvailable(true); widget.show(); settle();
    auto *table=widget.findChild<QTableWidget *>("purchaseReviewTable"); QVERIFY(table->rowCount()>0); QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,table->visualItemRect(table->item(0,0)).center()); settle();
    auto *dialog=widget.findChild<QDialog *>("purchaseReviewDialog"); QVERIFY(dialog && dialog->isVisible());
    dialog->findChild<QComboBox *>("reviewFrequencyComboBox")->setCurrentIndex(3); dialog->findChild<QComboBox *>("reviewWorthinessComboBox")->setCurrentIndex(1); capture(*dialog,"review"); click(*dialog,"savePurchaseReviewButton");
    QList<BrandEntry> loaded; QVERIFY(brands.loadBrands(loaded)); QCOMPARE(loaded.first().worthCount,1); QCOMPARE(loaded.first().regretCount,0); QCOMPARE(loaded.first().weight,1);
}
void SecondaryUiTests::brandListAndSeparateEditor()
{
    DatabaseManager database(":memory:"); QVERIFY(database.initialize()); ShoppingProfileDialog dialog(database); AppTheme::apply(&dialog,"warm"); dialog.show(); dialog.findChild<QTabWidget *>("shoppingProfileTabs")->setCurrentIndex(1); settle();
    QVERIFY(!dialog.findChild<QLineEdit *>("brandNameEdit")); click(dialog,"newBrandButton"); auto *editor=dialog.findChild<QDialog *>("brandEditorDialog"); QVERIFY(editor && editor->isVisible());
    editor->findChild<QLineEdit *>("brandNameEdit")->setText("MUJI"); QVERIFY(!editor->findChild<QSpinBox *>("brandWeightSpinBox")->isVisible()); click(*editor,"saveBrandButton");
    auto *table=dialog.findChild<QTableWidget *>("brandTable"); QCOMPARE(table->rowCount(),1); QVERIFY(table->item(0,2)->text().contains(QStringLiteral("值得 0")));
    click(dialog,"newBrandButton"); editor=dialog.findChild<QDialog *>("brandEditorDialog"); editor->findChild<QLineEdit *>("brandNameEdit")->setText("MUJI"); click(*editor,"saveBrandButton");
    QVERIFY(editor->isVisible()); QVERIFY(editor->findChild<QLabel *>("brandEditorErrorLabel")->isVisible()); editor->reject(); settle();
    table->selectRow(0); click(dialog,"editBrandButton"); editor=dialog.findChild<QDialog *>("brandEditorDialog"); QCOMPARE(editor->findChild<QLineEdit *>("brandNameEdit")->text(),QStringLiteral("MUJI")); editor->reject(); settle();
    capture(dialog,"brands");
}
void SecondaryUiTests::importSelectionTools()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("import.sqlite")); QVERIFY(database.initialize());
    const auto path=dir.filePath("bills.csv"); auto rows=BillTestFixtures::wechat("secondary-import",QStringLiteral("午饭"),"15.00"); rows[2][0]=QDate::currentDate().toString(Qt::ISODate)+" 12:00:00"; QVERIFY(BillTestFixtures::write(path,BillTestFixtures::csv(rows)));
    BillImportDialog dialog(database,{}); AppTheme::apply(&dialog,"warm"); dialog.show(); settle(); QVERIFY(!dialog.findChild<QFrame *>("importOptionsPanel")->isVisible());
    QSignalSpy ready(&dialog,&BillImportDialog::previewReady); QVERIFY(dialog.loadFile(path)); QTRY_COMPARE(ready.count(),1);
    auto *table=dialog.findChild<QTableWidget *>("billImportPreviewTable"); QVERIFY(table->isColumnHidden(6));
    table->item(0,0)->setCheckState(Qt::Unchecked); settle(); QVERIFY(!dialog.findChild<QPushButton *>("toggleBillBulkCategoryButton")->isVisible());
    table->item(0,0)->setCheckState(Qt::Checked); settle(); QVERIFY(dialog.findChild<QPushButton *>("toggleBillBulkCategoryButton")->isVisible()); capture(dialog,"import");
}
QTEST_MAIN(SecondaryUiTests)
#include "secondary_ui_tests.moc"
