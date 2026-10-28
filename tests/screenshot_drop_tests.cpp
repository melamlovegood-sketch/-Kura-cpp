#include "AIStubServer.h"
#include "DecisionDialog.h"
#include "ExpenseComposer.h"
#include "PriceRecordDialog.h"
#include "ProductDetailsDialog.h"
#include "TransactionRepository.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

namespace {
void settle() { QApplication::processEvents(); }
std::unique_ptr<QWidget> entry(int kind,DatabaseManager &database,const AISettings &settings)
{
    if (kind==0) return std::make_unique<DecisionDialog>(database,settings);
    if (kind==1) return std::make_unique<ProductDetailsDialog>(database,ProductContext{{},QStringLiteral("测试商品"),"online_shopping",0,1234});
    if (kind==2) return std::make_unique<PriceRecordDialog>(database);
    auto composer=std::make_unique<ExpenseComposer>(database,database.databasePath()+".ui.ini"); composer->setDatabaseAvailable(true); return composer;
}
QImage image(const QColor &color=QColor("#1255ee")) { QImage result(240,160,QImage::Format_RGB32); result.fill(color); return result; }
QWidget *target(QWidget &root,const QString &name)
{
    auto *widget=root.findChild<QWidget *>(name);
    if (!widget) return nullptr;
    for (auto *scroll:root.findChildren<QScrollArea *>()) if (scroll->widget() && scroll->widget()->isAncestorOf(widget)) scroll->ensureWidgetVisible(widget,0,0);
    settle();
    if (auto *editor=qobject_cast<QPlainTextEdit *>(widget)) widget=editor->viewport();
    return widget;
}
bool drop(QWidget *widget,const QMimeData &mime)
{
    if (!widget || !widget->isVisible()) return false;
    const QPoint position=widget->rect().center();
    QDragEnterEvent enter(position,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(widget,&enter);
    if (!enter.isAccepted()) return false;
    QDragMoveEvent move(position,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); move.acceptProposedAction(); QApplication::sendEvent(widget,&move);
    QDropEvent release(position,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier); QApplication::sendEvent(widget,&release); settle(); return release.isAccepted();
}
void fileMime(QMimeData &mime,const QString &path) { mime.setUrls({QUrl::fromLocalFile(path)}); mime.setText(QUrl::fromLocalFile(path).toString()); }
const char *upload(int kind) { return kind==0?"decisionImageButton":kind==1?"productDetailsImageButton":kind==2?"priceImageButton":"bookkeepingUploadButton"; }
void show(QWidget &root,int kind,bool editing=true)
{
    root.resize(760,900); root.show(); settle();
    if (kind==1 && editing) root.findChild<QPushButton *>("editProductDetailsButton")->click();
    settle();
}
}

class ScreenshotDropTests:public QObject {
    Q_OBJECT
private slots:
    void allUploadTargets_data();
    void allUploadTargets();
    void droppedImagesFeedParser_data();
    void droppedImagesFeedParser();
    void invalidFilesAndTextEditing();
    void nestedDialogsKeepTheirOwnScreenshot();
};
void ScreenshotDropTests::allUploadTargets_data()
{
    QTest::addColumn<int>("kind"); QTest::addColumn<QString>("name"); QTest::addColumn<bool>("memory");
    const QList<QPair<int,QString>> targets{
        {0,"decisionInput"},{0,"decisionImageButton"},{0,"decisionStatusLabel"},
        {1,"productDetailsImageButton"},{1,"productField_brand"},{1,"productPurchaseReasonEdit"},{1,"productDetailsPreview"},{1,"productDetailsSummaryLabel"},
        {2,"priceImageButton"},{2,"priceNameEdit"},{2,"priceSourceEdit"},{2,"priceImagePreview"},
        {3,"bookkeepingInput"},{3,"bookkeepingUploadButton"},{3,"bookkeepingScreenshotLabel"},{3,"bookkeepingNameEdit"}};
    for (const auto &row:targets) for (bool memory:{false,true}) QTest::newRow(qPrintable(row.second+(memory?"-image":"-file"))) << row.first << row.second << memory;
}
void ScreenshotDropTests::allUploadTargets()
{
    QFETCH(int,kind); QFETCH(QString,name); QFETCH(bool,memory);
    QTemporaryDir directory; DatabaseManager database(directory.filePath("drop.sqlite")); QVERIFY(database.initialize());
    AIStubServer server; auto root=entry(kind,database,server.settings()); show(*root,kind,name!="productDetailsSummaryLabel");
    const auto path=directory.filePath("image.png"),old=directory.filePath("old.png"); QVERIFY(image().save(path)); QVERIFY(image(Qt::red).save(old));
    if (name=="productDetailsPreview") QVERIFY(static_cast<ProductDetailsDialog *>(root.get())->setScreenshot(old));
    if (name=="priceImagePreview") QVERIFY(static_cast<PriceRecordDialog *>(root.get())->setScreenshot(old));
    if (name=="bookkeepingScreenshotLabel") QVERIFY(static_cast<ExpenseComposer *>(root.get())->setScreenshot(old));
    if (name=="bookkeepingNameEdit") root->findChild<QPushButton *>("bookkeepingManualModeButton")->click();
    auto *field=root->findChild<QWidget *>(name); QVERIFY(field);
    const auto text=QStringLiteral("保留已输入的内容");
    if (auto *line=qobject_cast<QLineEdit *>(field)) line->setText(text);
    if (auto *plain=qobject_cast<QPlainTextEdit *>(field)) plain->setPlainText(text);
    QMimeData mime; if (memory) mime.setImageData(image()); else fileMime(mime,path);
    QVERIFY2(drop(target(*root,name),mime),qPrintable(name));
    if (auto *line=qobject_cast<QLineEdit *>(field)) QCOMPARE(line->text(),text);
    if (auto *plain=qobject_cast<QPlainTextEdit *>(field)) QCOMPARE(plain->toPlainText(),text);
    if (kind==0) QVERIFY(!root->findChild<QPushButton *>("decisionRemoveImageButton")->isHidden());
    if (kind==1 || kind==2) {
        auto *preview=root->findChild<QLabel *>(kind==1?"productDetailsPreview":"priceImagePreview"); QVERIFY(!preview->isHidden());
        const auto color=preview->pixmap().toImage().pixelColor(preview->pixmap().rect().center()); QVERIFY(color.blue()>180 && color.red()<80);
    }
    if (kind==3) QVERIFY(!root->findChild<QLabel *>("bookkeepingScreenshotLabel")->isHidden());
    QCOMPARE(server.requests.size(),0);
    QList<Transaction> records; TransactionRepository repository(database); QVERIFY(repository.findAll(records)); QVERIFY(records.isEmpty());
}
void ScreenshotDropTests::droppedImagesFeedParser_data()
{
    QTest::addColumn<int>("kind"); for (int kind=0;kind<4;++kind) QTest::newRow(qPrintable(QString::number(kind))) << kind;
}
void ScreenshotDropTests::droppedImagesFeedParser()
{
    QFETCH(int,kind);
    QTemporaryDir directory; DatabaseManager database(directory.filePath("parser.sqlite")); QVERIFY(database.initialize()); AIStubServer server; server.delay=150;
    server.handler=[](const QJsonObject &) { return QStringLiteral("{\"name\":\"测试商品\",\"amount\":\"12.34\",\"category\":\"online_shopping\",\"source\":\"测试平台\",\"note\":\"\",\"fields\":{\"brand\":\"测试品牌\"}}"); };
    QString error; QVERIFY(AISettingsStore::save(database.databasePath()+".ui.ini",server.settings(),error));
    auto root=entry(kind,database,server.settings()); show(*root,kind);
    QMimeData mime; mime.setImageData(image()); QVERIFY(drop(target(*root,upload(kind)),mime));
    auto *parse=root->findChild<QPushButton *>(kind==0?"decisionAnalyzeButton":kind==1?"recognizeProductDetailsButton":kind==2?"priceAnalyzeButton":"parseBookkeepingButton"); QVERIFY(parse); QVERIFY(parse->isEnabled()); parse->click();
    QMimeData replacement; replacement.setImageData(image(Qt::red)); QVERIFY(!drop(root.get(),replacement));
    QTRY_COMPARE(server.requests.size(),1);
    if (kind==0) QTRY_COMPARE(root->findChild<QLineEdit *>("decisionNameEdit")->text(),QStringLiteral("测试商品"));
    if (kind==1) QTRY_COMPARE(root->findChild<QLineEdit *>("productField_brand")->text(),QStringLiteral("测试品牌"));
    if (kind==2) QTRY_COMPARE(root->findChild<QLineEdit *>("priceNameEdit")->text(),QStringLiteral("测试商品"));
    if (kind==3) QTRY_COMPARE(root->findChild<QLineEdit *>("bookkeepingDraftNameEdit")->text(),QStringLiteral("测试商品"));
    const auto parts=server.requests.first().value("messages").toArray().last().toObject().value("content").toArray();
    const auto url=parts.last().toObject().value("image_url").toObject().value("url").toString(); QVERIFY(url.startsWith("data:image/jpeg;base64,"));
    const auto sent=QImage::fromData(QByteArray::fromBase64(url.section(',',1).toLatin1())); QVERIFY(!sent.isNull()); QVERIFY(sent.pixelColor(0,0).blue()>180);
}
void ScreenshotDropTests::invalidFilesAndTextEditing()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("invalid.sqlite")); QVERIFY(database.initialize());
    for (int kind=0;kind<4;++kind) {
        auto root=entry(kind,database,AISettings{}); show(*root,kind);
        const QString name=kind==0?"decisionInput":kind==1?"productField_brand":kind==2?"priceSourceEdit":"bookkeepingInput";
        auto *field=root->findChild<QWidget *>(name); const QString original="keep";
        if (auto *line=qobject_cast<QLineEdit *>(field)) line->setText(original); else qobject_cast<QPlainTextEdit *>(field)->setPlainText(original);
        QMimeData invalid; const auto path=directory.filePath("bad.png"); QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("invalid image"); file.close(); fileMime(invalid,path);
        QVERIFY(!drop(target(*root,name),invalid));
        QMimeData multiple; multiple.setUrls({QUrl::fromLocalFile(path),QUrl::fromLocalFile(path)}); QVERIFY(!drop(target(*root,name),multiple));
        if (auto *line=qobject_cast<QLineEdit *>(field)) QCOMPARE(line->text(),original); else QCOMPARE(qobject_cast<QPlainTextEdit *>(field)->toPlainText(),original);
        QMimeData text; text.setText(" words"); QVERIFY(drop(target(*root,name),text));
        if (auto *line=qobject_cast<QLineEdit *>(field)) QVERIFY(line->text().contains("words")); else QVERIFY(qobject_cast<QPlainTextEdit *>(field)->toPlainText().contains("words"));
    }
    ExpenseComposer unavailable(database,{}); unavailable.show(); QMimeData mime; mime.setImageData(image()); QVERIFY(!drop(&unavailable,mime));
}
void ScreenshotDropTests::nestedDialogsKeepTheirOwnScreenshot()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("nested.sqlite")); QVERIFY(database.initialize());
    DecisionDialog parent(database,AISettings{}); show(parent,0);
    ProductDetailsDialog child(database,ProductContext{{},"商品","online_shopping",0,1234},0,&parent); show(child,1);
    QMimeData mime; mime.setImageData(image()); QVERIFY(drop(target(child,"productField_brand"),mime));
    QVERIFY(!child.findChild<QLabel *>("productDetailsPreview")->isHidden()); QVERIFY(parent.findChild<QPushButton *>("decisionRemoveImageButton")->isHidden());
}
QTEST_MAIN(ScreenshotDropTests)
#include "screenshot_drop_tests.moc"
