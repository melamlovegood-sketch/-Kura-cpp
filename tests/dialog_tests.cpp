#include "AddTransactionDialog.h"
#include "AppTheme.h"
#include "BillImportDialog.h"
#include "BillTestFixtures.h"
#include "TransactionRepository.h"
#include <QTableWidget>
#include <QTemporaryDir>
#include <QStackedWidget>
#include "DecisionDialog.h"
#include "ModernSelectors.h"
#include "ShoppingProfileDialog.h"
#include "WishDialog.h"
#include "mainwindow.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtTest>

static void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }
static bool hasVisibleShade(QWidget &owner) { for(auto *shade:owner.findChildren<QWidget *>("appDialogShade",Qt::FindDirectChildrenOnly)) if(shade->isVisible()) return true; return false; }
static void capture(QWidget &owner,QWidget &dialog,const QString &name)
{
    const auto directory=qEnvironmentVariable("KURA_DIALOG_PREVIEW_DIR"); if(directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory)); settle();
    QPixmap scene=owner.grab(); QPainter painter(&scene);
    const auto position=dialog.mapToGlobal(QPoint())-owner.mapToGlobal(QPoint());
    if(!dialog.mask().isEmpty()) painter.setClipRegion(dialog.mask().translated(position));
    painter.drawPixmap(position,dialog.grab()); painter.end();
    QVERIFY(scene.save(directory+"/dialog-"+name+".png"));
}
class DialogTests:public QObject {
    Q_OBJECT
private slots:
    void wishValidationPresetsAndKeyboard();
    void allDialogFamiliesAndThemes();
    void confirmationCancelAndShadeLifetime();
    void compactScrollingAndStickyActions();
    void importWorkflowAndCompactViewport();
};
void DialogTests::wishValidationPresetsAndKeyboard()
{
    MainWindow window(":memory:"); window.resize(1180,860); window.showPage(MainWindow::Page::Wishlist); window.show(); settle();
    WishDialog dialog(&window); dialog.open(); settle();
    QVERIFY(dialog.windowFlags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(dialog.findChild<QWidget *>("appDialogHeader"));
    QVERIFY(hasVisibleShade(window));
    auto *name=dialog.findChild<QLineEdit *>("wishNameEdit"); auto *amount=dialog.findChild<QDoubleSpinBox *>("wishAmountSpinBox");
    auto *save=dialog.findChild<QPushButton *>("saveWishButton"); QSignalSpy requested(&dialog,&WishDialog::saveRequested);
    save->click(); settle(); QCOMPARE(requested.count(),0); QVERIFY(name->property("fieldError").toBool());
    name->setText(QStringLiteral("一双球鞋")); amount->setValue(399);
    dialog.findChild<QPushButton *>("wishCoolingPreset7")->click();
    QCOMPARE(dialog.item().coolingDays,7);
    QCOMPARE(dialog.findChild<QSpinBox *>("wishDaysSpinBox")->buttonSymbols(),QAbstractSpinBox::NoButtons);
    capture(window,dialog,"wish-example");
    dialog.findChild<QPushButton *>("wishMoreOptionsButton")->click(); settle();
    auto *category=dialog.findChild<ModernComboBox *>("wishCategoryComboBox"); QVERIFY(category);
    category->showPopup(); settle(); auto *popup=category->findChild<QFrame *>("modernComboPopup"); QVERIFY(popup && popup->isVisible());
    popup->findChild<QPushButton *>("chooseOption4")->click(); settle(); QCOMPARE(category->currentIndex(),4);
    category->showPopup(); settle();
    auto *search=category->findChild<QLineEdit *>("selectorSearchEdit"); QVERIFY(search);
    search->setText(QStringLiteral("学习")); settle();
    QTest::keyClick(search,Qt::Key_Down); QTest::keyClick(QApplication::focusWidget(),Qt::Key_Return); settle();
    QCOMPARE(category->currentData().toString(),QStringLiteral("study"));
    save->click(); QCOMPARE(requested.count(),1); QVERIFY(!save->isEnabled());
    dialog.showSaveError(QStringLiteral("数据库暂时被占用，请重试。")); QVERIFY(save->isEnabled()); QCOMPARE(name->text(),QStringLiteral("一双球鞋"));
    dialog.completeSave(); settle(); QVERIFY(!dialog.isVisible());
    QVERIFY(!hasVisibleShade(window));
}
void DialogTests::allDialogFamiliesAndThemes()
{
    MainWindow window(":memory:"); window.resize(1180,860); window.show(); settle();
    DatabaseManager database(":memory:"); QVERIFY(database.initialize());
    for(const auto &theme:QStringList{"warm","cool","dark"}) {
        window.findChild<QPushButton *>(theme+"ThemeButton")->click(); settle();
        AddTransactionDialog transaction(&window); transaction.open(); settle();
        QVERIFY(transaction.property("kuraDialog").toBool());
        QCOMPARE(transaction.findChild<QLabel *>("appDialogTitle")->text(),transaction.windowTitle());
        capture(window,transaction,"transaction-"+theme); transaction.reject(); settle();
        WishDialog wish(&window); wish.open(); settle(); capture(window,wish,"wish-"+theme); wish.reject(); settle();
        ShoppingProfileDialog profile(database,&window); profile.open(); settle(); QVERIFY(profile.property("kuraDialog").toBool()); profile.reject(); settle();
        DecisionDialog decision(database,AISettings{},&window); decision.open(); settle();
        QVERIFY(decision.property("kuraDialog").toBool()); capture(window,decision,"decision-"+theme); decision.reject(); settle();
        BillImportDialog imports(database,QString(),&window); imports.open(); settle();
        QVERIFY(imports.property("dialogFixedViewport").toBool());
        QVERIFY(imports.findChild<QPushButton *>("chooseBillFileButton")->isVisible());
        QVERIFY(imports.property("kuraDialog").toBool()); capture(window,imports,"import-"+theme); imports.reject(); settle();
    }
}
void DialogTests::confirmationCancelAndShadeLifetime()
{
    MainWindow window(":memory:"); window.resize(1180,860); window.show(); settle();
    auto *dialog=new QDialog(&window); auto *layout=new QVBoxLayout(dialog); layout->addWidget(new QLabel(QStringLiteral("表单保持打开"),dialog)); dialog->setWindowTitle(QStringLiteral("编辑商品")); dialog->open(); settle();
    auto *confirmation=new QMessageBox(QMessageBox::Question,QStringLiteral("删除商品"),QStringLiteral("确认删除这件商品？已有消费记录会保留。"),QMessageBox::Yes|QMessageBox::No,dialog);
    confirmation->setDefaultButton(QMessageBox::No); confirmation->open(); settle();
    QVERIFY(confirmation->property("kuraDialog").toBool()); QVERIFY(confirmation->findChild<QWidget *>("appDialogHeader"));
    const auto messageImage=confirmation->grab().toImage(); QVERIFY(messageImage.pixelColor(8,messageImage.height()/2).alpha()>200);
    QCOMPARE(confirmation->defaultButton(),confirmation->button(QMessageBox::No));
    capture(window,*confirmation,"confirmation");
    QTest::keyClick(confirmation,Qt::Key_Escape); settle(); QVERIFY(!confirmation->isVisible()); QVERIFY(dialog->isVisible());
    QVERIFY(hasVisibleShade(window));
    delete confirmation; dialog->findChild<QToolButton *>("appDialogCloseButton")->click(); settle();
    QVERIFY(!hasVisibleShade(window)); delete dialog;
}
void DialogTests::compactScrollingAndStickyActions()
{
    MainWindow window(":memory:"); window.resize(800,600); window.show(); settle();
    WishDialog wish(&window); wish.open(); settle();
    wish.findChild<QPushButton *>("wishMoreOptionsButton")->click(); settle();
    QVERIFY(wish.width()<=800); QVERIFY(wish.height()<=600);
    auto *scroll=wish.findChild<QScrollArea *>("appDialogScroll"); QVERIFY(scroll);
    QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); QVERIFY(scroll->verticalScrollBar()->maximum()>0);
    auto *save=wish.findChild<QPushButton *>("saveWishButton"); QVERIFY(save->isVisible());
    QVERIFY(!wish.mask().isEmpty()); QVERIFY(!wish.mask().contains(QPoint(0,0))); QVERIFY(wish.mask().contains(wish.rect().center()));
    QCOMPARE(wish.findChild<QWidget *>("appDialogCard")->geometry(),wish.rect());
    QVERIFY(!scroll->isAncestorOf(save));
    const auto before=save->mapTo(&wish,QPoint()); scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); settle();
    QCOMPARE(save->mapTo(&wish,QPoint()),before); capture(window,wish,"compact-bottom");
    QTest::keyClick(&wish,Qt::Key_Escape); settle(); QVERIFY(!wish.isVisible());
}
void DialogTests::importWorkflowAndCompactViewport()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("import.sqlite")); QVERIFY(database.initialize());
    auto rows=BillTestFixtures::wechat("fixture-first",QStringLiteral("食堂午饭"),"15.00"); rows[2][0]=QDate::currentDate().toString(Qt::ISODate)+" 12:00:00";
    const QStringList names{QStringLiteral("咖啡"),QStringLiteral("地铁票"),QStringLiteral("读书笔记本"),QStringLiteral("日用洗衣液"),QStringLiteral("晚餐"),QStringLiteral("电影票")};
    for(int i=0;i<80;++i) { auto row=rows[2]; row[0]=QDate::currentDate().toString(Qt::ISODate)+QStringLiteral(" %1:30:00").arg(i%23,2,10,QChar('0')); row[3]=names[i%names.size()]+QString::number(i); row[7]=QStringLiteral("fixture-%1").arg(i); row[5]=QString::number((i%9)+1)+".50"; rows.append(row); }
    const auto path=directory.filePath(QStringLiteral("虚构账单.csv")); QVERIFY(BillTestFixtures::write(path,BillTestFixtures::csv(rows)));
    MainWindow window(":memory:"); window.resize(800,600); window.show(); settle(); BillImportDialog dialog(database,QString(),&window); dialog.open(); settle();
    auto *pages=dialog.findChild<QStackedWidget *>("billImportPages"); QCOMPARE(pages->currentIndex(),0); QVERIFY(dialog.findChild<QPushButton *>("chooseBillFileButton")->isVisible()); QVERIFY(!dialog.findChild<QTableWidget *>("billImportPreviewTable")->isVisible()); capture(window,dialog,"import-choose");
    QSignalSpy ready(&dialog,&BillImportDialog::previewReady); QVERIFY(dialog.loadFile(path)); QTRY_COMPARE_WITH_TIMEOUT(ready.count(),1,10000); settle();
    QCOMPARE(pages->currentIndex(),1); QVERIFY(dialog.height()<=560); auto *table=dialog.findChild<QTableWidget *>("billImportPreviewTable"); QCOMPARE(table->rowCount(),81); QVERIFY(dialog.findChildren<ModernComboBox *>().size()<5);
    auto *save=dialog.findChild<QPushButton *>("commitBillImportButton"); QVERIFY(save->isVisible() && save->isEnabled()); QVERIFY(dialog.rect().contains(QRect(save->mapTo(&dialog,QPoint()),save->size())));
    const auto position=save->mapTo(&dialog,QPoint()); table->verticalScrollBar()->setValue(table->verticalScrollBar()->maximum()); settle(); QCOMPARE(save->mapTo(&dialog,QPoint()),position); table->scrollToTop();
    auto *search=dialog.findChild<QLineEdit *>("importSearchEdit"); search->setText(QStringLiteral("食堂午饭")); settle(); QCOMPARE(table->isRowHidden(0),false); QVERIFY(table->isRowHidden(1));
    dialog.findChild<QPushButton *>("toggleBillBulkCategoryButton")->click(); auto *bulk=dialog.findChild<QComboBox *>("bulkBillCategoryComboBox"); bulk->setCurrentIndex(bulk->findData("study")); dialog.findChild<QPushButton *>("applyBillCategoryButton")->click();
    QCOMPARE(table->item(0,5)->data(Qt::UserRole).toString(),QStringLiteral("study")); QVERIFY(table->item(1,5)->data(Qt::UserRole).toString()!=QStringLiteral("study")); search->clear(); settle(); capture(window,dialog,"import-review-compact");
    TransactionRepository records(database); QList<Transaction> loaded; QVERIFY(records.findAll(loaded)); QVERIFY(loaded.isEmpty()); QSignalSpy saved(&dialog,&BillImportDialog::imported); save->click(); settle(); QCOMPARE(saved.count(),1); QCOMPARE(pages->currentIndex(),3); QVERIFY(records.findAll(loaded)); QCOMPARE(loaded.size(),81); capture(window,dialog,"import-complete");
    dialog.findChild<QPushButton *>("importAnotherFileButton")->click(); QCOMPARE(pages->currentIndex(),0); QVERIFY(dialog.loadFile(path)); QTRY_COMPARE_WITH_TIMEOUT(ready.count(),2,10000); QVERIFY(!save->isEnabled()); QVERIFY(dialog.findChild<QLabel *>("importEmptyHint")->isVisible()); dialog.findChild<QPushButton *>("importExcludedRowsButton")->click(); settle(); QVERIFY(table->isVisible()); QVERIFY(!(table->item(0,0)->flags()&Qt::ItemIsUserCheckable)); dialog.reject();
}
QTEST_MAIN(DialogTests)
#include "dialog_tests.moc"
