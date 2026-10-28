#include <QToolButton>
#include <QAction>
#include "AIStubServer.h"
#include "AIService.h"
#include "AppTheme.h"
#include "DatabaseBackupService.h"
#include "DecisionDialog.h"
#include "ExecutionRepository.h"
#include "ProductDetailsDialog.h"
#include "ProductList.h"
#include "ProductRepository.h"
#include "WishPriceDialog.h"
#include "PriceRepository.h"
#include "PurchaseRepository.h"
#include "WishlistRepository.h"
#include "mainwindow.h"
#include <QBuffer>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

static ProductContext draft() { return {QString(),QStringLiteral("MUJI 家居服套装"),"online_shopping",0,25300,{}}; }
static QByteArray imageBytes()
{
    QImage image(900,500,QImage::Format_RGB32); image.fill(Qt::white); QPainter p(&image); auto font=p.font(); font.setPixelSize(28); p.setFont(font);
    p.drawText(QRect(30,30,840,430),Qt::AlignLeft|Qt::TextWordWrap,QStringLiteral("虚构测试图\n品牌 MUJI/无印良品\n货号 FD1PJA6A\n莫代尔94%，其余成分…\n可选尺码 S/M L/XL\n2026年冬季")); p.end();
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG"); return bytes;
}
static QJsonObject extraction() { return {{"fields",QJsonObject{{"brand","MUJI"},{"model","FD1PJA6A"},{"materials",QStringLiteral("莫代尔94%；其余成分截断，待补充")},{"specification",QStringLiteral("可选 S/M、L/XL，尚未确定所选尺码")},{"care",""}}},{"note",QStringLiteral("成分被截断；请核对同款，不推测剩余比例。")}}; }
static QString json(const QJsonObject &object) { return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)); }
static qint64 scalar(DatabaseManager &database,const QString &sql) { QSqlQuery query(database.database()); return query.exec(sql) && query.next()?query.value(0).toLongLong():-1; }
static void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }

class ProductDetailsTests:public QObject {
    Q_OBJECT
private slots:
    void lifecycleIdentityAndNoNameMerge();
    void backupIncludesEvidenceAndChatAndSupportsOldBackups();
    void extractionIsDraftAndPreservesMissingFields();
    void conversationUsesSavedFactsAndPersists();
    void staleEditsAndCancelledRequestsDoNotOverwrite();
    void productNamesOpenUnifiedDetailsAndPriceActionsAreCompact();
    void decisionDraftCarriesProfileToSavedProduct();
};

void ProductDetailsTests::lifecycleIdentityAndNoNameMerge()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("identity.sqlite")); QVERIFY(database.initialize()); ProductRepository products(database);
    ProductProfile profile; QVERIFY(products.ensure(draft(),profile)); profile.fields={{"brand","MUJI"},{"model","FD1PJA6A"}}; QVERIFY(products.save(profile));
    WishItem wish; wish.name=draft().name; wish.estimatedCents=25300; wish.category=QStringLiteral("购物"); wish.detailCategory="online_shopping"; wish.coolingDays=1;
    WishlistRepository wishes(database); QVERIFY(wishes.add(wish,QDateTime::currentDateTimeUtc().addDays(-2),profile.id));
    Transaction bought(0,wish.name,22800,QStringLiteral("购物"),QDate::currentDate(),{},QString(),"online_shopping");
    QVERIFY(wishes.purchase(wish.id,bought)); ProductContext context; QVERIFY(products.context("transaction",bought.id(),context)); ProductProfile linked; QVERIFY(products.ensure(context,linked)); QCOMPARE(linked.id,profile.id);
    PriceRepository prices(database); qint64 priceId=0; QVERIFY(prices.intake(wish.name,"online_shopping",25300,QStringLiteral("手动记录"),"price",priceId));
    QVERIFY(products.context("price",priceId,context)); QVERIFY(products.ensure(context,linked)); QVERIFY(linked.id!=profile.id);
    QVERIFY(products.attach("price",priceId,profile.id)); QVERIFY(products.ensure(context,linked)); QCOMPARE(linked.id,profile.id);
    QVERIFY(products.aiContext(profile.id,context).contains("253"));
    ExecutionRepository executions(database); ExecutionSession session; session.name=wish.name; session.category="online_shopping"; session.durationSeconds=60;
    QVERIFY(executions.start(session,QDateTime::currentDateTimeUtc(),profile.id)); WishItem deferred=wish; deferred.id=0; QVERIFY(executions.deferToWishlist(session.id,deferred));
    QVERIFY(products.context("wish",deferred.id,context)); QVERIFY(products.ensure(context,linked)); QCOMPARE(linked.id,profile.id);
    ExecutionSession next; next.name=wish.name; next.category="online_shopping"; next.durationSeconds=60; QVERIFY(executions.start(next,QDateTime::currentDateTimeUtc(),profile.id));
    PurchaseRepository purchases(database); Transaction direct(0,wish.name,20000,QStringLiteral("购物"),QDate::currentDate(),{},QString(),"online_shopping"); QVERIFY(purchases.record(direct,0,next.id));
    QVERIFY(products.context("transaction",direct.id(),context)); QVERIFY(products.ensure(context,linked)); QCOMPARE(linked.id,profile.id);
    WishItem invalid=deferred; invalid.id=0; QVERIFY(!wishes.add(invalid,QDateTime::currentDateTimeUtc(),999999)); QCOMPARE(invalid.id,0);
}

void ProductDetailsTests::backupIncludesEvidenceAndChatAndSupportsOldBackups()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("backup.sqlite")); QVERIFY(database.initialize()); ProductRepository products(database); ProductProfile profile;
    QVERIFY(products.ensure(draft(),profile)); profile.fields=extraction().value("fields").toObject(); profile.notes=QStringLiteral("宿舍穿，怕闷热");
    QVERIFY(products.save(profile,imageBytes(),extraction())); const auto chat=QString::fromUtf8(QJsonDocument(QJsonArray{QJsonObject{{"role","user"},{"content",QStringLiteral("值吗？")}},QJsonObject{{"role","assistant"},{"content",QStringLiteral("还需核对厚度与使用需求。")}}}).toJson(QJsonDocument::Compact));
    QVERIFY(products.saveConversation(profile.id,chat,QString()));
    QString error,safety; DatabaseBackupService backups(database); BackupInfo info; const auto copy=dir.filePath("copy.sqlite");
    QVERIFY(backups.backup(copy,error)); QVERIFY2(backups.prepareRestore(copy,info,error),qPrintable(error));
    profile.fields={{"brand","changed"}}; QVERIFY(products.save(profile)); QVERIFY2(backups.restorePrepared(safety,error),qPrintable(error));
    QVERIFY(products.load(profile.id,profile)); QCOMPARE(profile.fields.value("brand").toString(),QStringLiteral("MUJI")); QCOMPARE(profile.conversation,chat);
    QList<ProductEvidence> shots; QVERIFY(products.evidence(profile.id,shots)); QCOMPARE(shots.size(),1); QCOMPARE(shots.first().image,imageBytes());
    const auto old=dir.filePath("old.sqlite"); QVERIFY(backups.backup(old,error));
    { DatabaseManager previous(old); QVERIFY(previous.initialize()); QSqlQuery query(previous.database()); for (const auto &name:{"product_evidence","product_links","product_profiles"}) QVERIFY(query.exec("DROP TABLE "+QLatin1String(name))); }
    QVERIFY2(backups.prepareRestore(old,info,error),qPrintable(error)); QVERIFY(backups.restorePrepared(safety,error)); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM product_profiles"),0);
    const auto corrupt=dir.filePath("corrupt.sqlite"); QVERIFY(backups.backup(corrupt,error));
    { DatabaseManager other(corrupt); QVERIFY(other.initialize()); QSqlQuery query(other.database()); QVERIFY(query.exec("DROP TABLE product_evidence")); }
    QVERIFY(!backups.prepareRestore(corrupt,info,error));
}

void ProductDetailsTests::extractionIsDraftAndPreservesMissingFields()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("extract.sqlite")); QVERIFY(database.initialize()); ProductRepository products(database); ProductProfile profile; QVERIFY(products.ensure(draft(),profile));
    profile.fields={{"care",QStringLiteral("手洗")},{"brand",QStringLiteral("原品牌")}}; QVERIFY(products.save(profile));
    AIStubServer server; server.handler=[](const QJsonObject &) { return json(extraction()); }; QString error; QVERIFY(AISettingsStore::save(database.databasePath()+".ui.ini",server.settings(),error));
    const auto path=dir.filePath("details.png"); QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(imageBytes()); file.close();
    ProductDetailsDialog dialog(database,draft(),profile.id); AppTheme::apply(&dialog,"warm"); dialog.show(); QVERIFY(dialog.setScreenshot(path)); QCOMPARE(server.requests.size(),0);
    dialog.findChild<QPushButton *>("recognizeProductDetailsButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(dialog.findChild<QLineEdit *>("productField_brand")->text(),QStringLiteral("MUJI"),5000);
    QCOMPARE(dialog.findChild<QLineEdit *>("productField_care")->text(),QStringLiteral("手洗"));
    QVERIFY(products.load(profile.id,profile)); QCOMPARE(profile.fields.value("brand").toString(),QStringLiteral("原品牌")); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM product_evidence"),0);
    QVERIFY(server.requestBodies.first().contains("data:image/jpeg;base64,")); QVERIFY(!server.requests.first().contains("enable_search"));
    dialog.findChild<QPlainTextEdit *>("productUserNotesEdit")->setPlainText(QStringLiteral("宿舍穿，M码，怕闷热")); dialog.findChild<QPushButton *>("saveProductDetailsButton")->click();
    QVERIFY(products.load(profile.id,profile)); QCOMPARE(profile.fields.value("care").toString(),QStringLiteral("手洗")); QCOMPARE(scalar(database,"SELECT COUNT(*) FROM product_evidence"),1);
    QVERIFY(profile.fields.value("materials").toString().contains(QStringLiteral("待补充")));
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!preview.isEmpty()) {
        QVERIFY(QDir().mkpath(preview));
        for (const auto &theme:{"warm","cool","dark"}) { AppTheme::apply(&dialog,QLatin1String(theme)); dialog.resize(640,740); settle(); QVERIFY(dialog.grab().save(preview+"/product-details-"+QLatin1String(theme)+".png")); }
        dialog.resize(460,500); settle(); QVERIFY(dialog.grab().save(preview+"/product-details-small.png"));
        dialog.findChild<QToolButton *>("productChatToggle")->click(); settle(); auto *ask=dialog.findChild<QPushButton *>("askProductAIButton"); QVERIFY(ask->isVisible());
        QVERIFY(dialog.rect().contains(ask->mapTo(&dialog,QPoint(0,0))));
        QVERIFY(dialog.rect().contains(ask->mapTo(&dialog,ask->rect().bottomRight())));
    }
    QJsonObject fields; QString note; QVERIFY(AIService::decodeDetails(QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",json(extraction())}}}}}}}).toJson(),fields,note,error));
    auto malformed=extraction(); malformed["fields"]=QJsonObject{{"materials",94}};
    QVERIFY(!AIService::decodeDetails(QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",json(malformed)}}}}}}}).toJson(),fields,note,error));
}

void ProductDetailsTests::conversationUsesSavedFactsAndPersists()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("chat.sqlite")); QVERIFY(database.initialize()); ProductRepository products(database); ProductProfile profile; QVERIFY(products.ensure(draft(),profile)); profile.fields=extraction().value("fields").toObject(); profile.notes=QStringLiteral("在宿舍穿"); QVERIFY(products.save(profile));
    PriceRepository prices(database); qint64 id=0; QVERIFY(prices.intake(draft().name,"online_shopping",22800,QStringLiteral("淘宝记录"),"first",id,QDateTime::currentDateTimeUtc(),profile.id)); QVERIFY(prices.record(id,25300,QStringLiteral("后来记录"),"second")); ProductContext context; QVERIFY(products.context("price",id,context));
    AIStubServer server; server.handler=[](const QJsonObject &) { return QStringLiteral("资料显示莫代尔94%，剩余成分未知。253元是否值得，还需核对厚度与宿舍温度。"); }; QString error; QVERIFY(AISettingsStore::save(database.databasePath()+".ui.ini",server.settings(),error));
    ProductDetailsDialog dialog(database,context); dialog.show(); dialog.findChild<QToolButton *>("productChatToggle")->click(); dialog.findChild<QPlainTextEdit *>("productQuestionEdit")->setPlainText(QStringLiteral("材质与品牌值这个价吗？")); dialog.findChild<QPushButton *>("askProductAIButton")->click();
    QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPlainTextEdit *>("productConversationText")->toPlainText().contains(QStringLiteral("剩余成分未知")),5000);
    const auto request=server.requests.first(); const auto system=request.value("messages").toArray().first().toObject().value("content").toString();
    QVERIFY(system.contains("FD1PJA6A")); QVERIFY(system.contains("253")); QVERIFY(system.contains("228")); QVERIFY(system.contains(QStringLiteral("在宿舍穿")));
    dialog.findChild<QPlainTextEdit *>("productQuestionEdit")->setPlainText(QStringLiteral("那我还要确认什么？")); dialog.findChild<QPushButton *>("askProductAIButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),2,5000); QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("askProductAIButton")->isEnabled(),5000);
    QCOMPARE(server.requests[1].value("messages").toArray().size(),4);
    QVERIFY(products.load(profile.id,profile)); QCOMPARE(QJsonDocument::fromJson(profile.conversation.toUtf8()).array().size(),4);
    ProductDetailsDialog reopened(database,context); QVERIFY(reopened.findChild<QPlainTextEdit *>("productConversationText")->toPlainText().contains(QStringLiteral("剩余成分未知")));
}

void ProductDetailsTests::staleEditsAndCancelledRequestsDoNotOverwrite()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("cancel.sqlite")); QVERIFY(database.initialize()); ProductRepository products(database); ProductProfile first; QVERIFY(products.ensure(draft(),first)); auto stale=first; first.fields={{"brand","MUJI"}}; QVERIFY(products.save(first)); stale.fields={{"brand","other"}}; QVERIFY(!products.save(stale));
    AIStubServer server; server.delay=300; server.handler=[](const QJsonObject &) { return QStringLiteral("迟到的回答"); }; QString error; QVERIFY(AISettingsStore::save(database.databasePath()+".ui.ini",server.settings(),error));
    ProductDetailsDialog dialog(database,draft(),first.id); dialog.show(); dialog.findChild<QToolButton *>("productChatToggle")->click(); dialog.findChild<QPushButton *>("askProductAIButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),1,5000); dialog.reject(); QTest::qWait(400);
    QVERIFY(products.load(first.id,first)); QVERIFY(first.conversation.isEmpty());
    ProductDetailsDialog manual(database,draft(),first.id); manual.show(); manual.findChild<QPushButton *>("editProductDetailsButton")->click(); manual.findChild<QLineEdit *>("productField_model")->setText("manual"); manual.findChild<QPushButton *>("cancelProductDetailsEditButton")->click(); QVERIFY(products.load(first.id,first)); QVERIFY(first.fields.value("model").toString().isEmpty());
}

void ProductDetailsTests::productNamesOpenUnifiedDetailsAndPriceActionsAreCompact()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("entry.sqlite")); QVERIFY(database.initialize()); PriceRepository prices(database); qint64 id=0; QVERIFY(prices.intake(draft().name,"online_shopping",25300,QStringLiteral("手动记录"),"entry",id)); QVERIFY(prices.configure(id,20000,"https://example.com/product"));
    MainWindow window(database.databasePath()); window.resize(1250,920); window.show(); window.showPage(MainWindow::Page::Wishlist); settle();
    auto *table=window.findChild<QTableWidget *>("wishlistTable"); QCOMPARE(table->rowCount(),1); table->selectRow(0); settle();
    window.findChild<QPushButton *>("wishProductInfoButton")->click(); settle(); auto *dialog=window.findChild<ProductDetailsDialog *>(); QVERIFY(dialog); dialog->reject(); settle();
    QVERIFY(window.findChild<QPushButton *>("wishPriceButton")->isVisible());
    window.findChild<QPushButton *>("wishPriceButton")->click(); settle(); auto *pricesDialog=window.findChild<WishPriceDialog *>(); QVERIFY(pricesDialog); QCOMPARE(pricesDialog->findChild<QTableWidget *>("wishPriceHistoryTable")->rowCount(),1); pricesDialog->reject(); settle();
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(window.grab().save(preview+"/price-card-simple.png")); }
}

void ProductDetailsTests::decisionDraftCarriesProfileToSavedProduct()
{
    QTemporaryDir dir; DatabaseManager database(dir.filePath("draft.sqlite")); QVERIFY(database.initialize()); DecisionDialog dialog(database,AISettings{}); dialog.show();
    dialog.findChild<QPlainTextEdit *>("decisionInput")->setPlainText(draft().name); dialog.findChild<QPushButton *>("decisionManualButton")->click(); dialog.findChild<QDoubleSpinBox *>("decisionAmountSpinBox")->setValue(253);
    dialog.findChild<QPushButton *>("decisionExtrasButton")->click(); dialog.findChild<QPushButton *>("decisionProductDetailsButton")->click(); settle(); auto *details=dialog.findChild<ProductDetailsDialog *>(); QVERIFY(details); const auto profileId=details->profileId();
    details->findChild<QPushButton *>("editProductDetailsButton")->click(); details->findChild<QLineEdit *>("productField_model")->setText("FD1PJA6A"); details->findChild<QPushButton *>("saveProductDetailsButton")->click(); details->reject(); settle();
    dialog.findChild<QPushButton *>("decisionContinueButton")->click(); dialog.findChild<QPushButton *>("decisionTrackButton")->click(); dialog.findChild<QPushButton *>("decisionCommitButton")->click(); PriceRepository prices(database); QList<PriceTrack> tracks; QVERIFY(prices.load(tracks)); QCOMPARE(tracks.size(),1); ProductRepository products(database); ProductContext context; ProductProfile profile; QVERIFY(products.context("price",tracks.first().id,context)); QVERIFY(products.ensure(context,profile)); QCOMPARE(profile.id,profileId); QCOMPARE(profile.fields.value("model").toString(),QStringLiteral("FD1PJA6A"));
}

QTEST_MAIN(ProductDetailsTests)
#include "product_details_tests.moc"
