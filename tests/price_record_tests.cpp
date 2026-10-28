#include "AIService.h"
#include "AIStubServer.h"
#include "AppTheme.h"
#include "PriceRecordDialog.h"
#include "PriceWidget.h"
#include "WishlistRepository.h"
#include "WishPriceDialog.h"
#include "mainwindow.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

static QJsonObject price(const QJsonValue &amount=QStringLiteral("228.00"))
{
    return {{"name",QStringLiteral("MUJI 家居服套装")},{"amount",amount},{"category","online_shopping"},
        {"source",QStringLiteral("淘宝")},{"note",QStringLiteral("M 码，券后价；请核对优惠条件。")}};
}
static QByteArray envelope(const QJsonObject &object)
{
    return QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{
        {"content",QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact))}}}}}}}).toJson();
}
static QString screenshotFile(const QString &directory)
{
    QImage image(900,460,QImage::Format_RGB32); image.fill(QColor("#f6f1e9"));
    QPainter painter(&image); painter.setPen(QColor("#453b32")); auto font=painter.font(); font.setPixelSize(32); painter.setFont(font);
    painter.drawText(QRect(40,30,820,350),Qt::AlignLeft|Qt::TextWordWrap,QStringLiteral("虚构测试截图\nMUJI 家居服套装\nM 码\n券后价 ¥228.00")); painter.end();
    const auto path=directory+"/price.png"; return image.save(path)?path:QString();
}
static qint64 count(DatabaseManager &database,const QString &table)
{
    QSqlQuery query(database.database()); if (!query.exec("SELECT COUNT(*) FROM "+table) || !query.next()) return -1; return query.value(0).toLongLong();
}
static void saveSettings(DatabaseManager &database,AIStubServer &server)
{
    QString error; QVERIFY2(AISettingsStore::save(database.databasePath()+".ui.ini",server.settings(),error),qPrintable(error));
}
static void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); }

class PriceRecordTests:public QObject {
    Q_OBJECT
private slots:
    void decoderPreservesUnknownAndRejectsInvalidMoney();
    void screenshotDraftRequiresSaveAndCanBeCorrected();
    void appendKeepsIdentityAndRequiresMatch();
    void firstWishScreenshotKeepsIdentityAndRequiresMatch();
    void unknownPriceClearsOldDraftAndCannotSave();
    void failureAndCancellationAllowManualEntry();
    void replacingScreenshotClearsRecognizedPrice();
    void mainWindowOpensBothEntriesAndSettings();
};

void PriceRecordTests::decoderPreservesUnknownAndRejectsInvalidMoney()
{
    ParsedPrice result; QString error;
    QVERIFY(AIService::decodePrice(envelope(price()),result,error)); QCOMPARE(result.amountCents,22800); QCOMPARE(result.source,QStringLiteral("淘宝"));
    QVERIFY(AIService::decodePrice(envelope(price(QJsonValue::Null)),result,error)); QCOMPARE(result.amountCents,0);
    auto partial=price("0.01"); partial["name"]=""; QVERIFY(AIService::decodePrice(envelope(partial),result,error)); QCOMPARE(result.amountCents,1);
    for (const auto &amount:QJsonArray{true,-1,0,12.345,"228-253","1e3","0.00","100000000","12.345"}) {
        result.amountCents=123; QVERIFY(!AIService::decodePrice(envelope(price(amount)),result,error)); QCOMPARE(result.amountCents,123);
    }
    auto incomplete=price(); incomplete.remove("amount"); QVERIFY(!AIService::decodePrice(envelope(incomplete),result,error));
}

void PriceRecordTests::screenshotDraftRequiresSaveAndCanBeCorrected()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("new.sqlite")); QVERIFY(database.initialize());
    AIStubServer server; server.handler=[](const QJsonObject &) { return QString::fromUtf8(QJsonDocument(price()).toJson()); }; saveSettings(database,server);
    PriceRecordDialog dialog(database); AppTheme::apply(&dialog,"warm"); dialog.show(); QSignalSpy saved(&dialog,&PriceRecordDialog::priceSaved);
    QVERIFY(dialog.setScreenshot(screenshotFile(directory.path()))); QCOMPARE(server.requests.size(),0);
    dialog.findChild<QPushButton *>("priceAnalyzeButton")->click();
    QVERIFY(!dialog.findChild<QPushButton *>("savePriceRecordButton")->isEnabled());
    QTRY_COMPARE_WITH_TIMEOUT(dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->value(),228.,5000);
    QCOMPARE(dialog.findChild<QLineEdit *>("priceNameEdit")->text(),QStringLiteral("MUJI 家居服套装")); QCOMPARE(count(database,"price_records"),0);
    QVERIFY(server.requestBodies.first().contains("data:image/jpeg;base64,"));
    QVERIFY(!server.requests.first().contains("enable_search"));
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        QVERIFY(QDir().mkpath(preview));
        settle(); QVERIFY(dialog.grab().save(preview+"/price-record-default.png"));
        for (const auto &theme:{"warm","cool","dark"}) {
            AppTheme::apply(&dialog,QLatin1String(theme)); dialog.resize(480,520); settle();
            QVERIFY(dialog.findChild<QPushButton *>("savePriceRecordButton")->isVisible());
            QVERIFY(dialog.grab().save(preview+"/price-record-"+QLatin1String(theme)+".png"));
        }
    }
    dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(229.90);
    dialog.findChild<QLineEdit *>("priceSourceEdit")->setText(QStringLiteral("淘宝 · M码 · 券后价"));
    dialog.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(saved.size(),1);
    dialog.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),1); QCOMPARE(count(database,"transactions"),0);
    PriceRepository repository(database); QList<PriceTrack> tracks; QVERIFY(repository.load(tracks)); QCOMPARE(tracks.first().latestCents,22990);
    QList<PriceRecord> records; QVERIFY(repository.history(tracks.first().id,records)); QCOMPARE(records.first().source,QStringLiteral("淘宝 · M码 · 券后价"));
    DatabaseManager reopened(database.databasePath()); QVERIFY(reopened.initialize()); PriceRepository persisted(reopened); QVERIFY(persisted.load(tracks)); QCOMPARE(tracks.first().latestCents,22990);
}

void PriceRecordTests::appendKeepsIdentityAndRequiresMatch()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("append.sqlite")); QVERIFY(database.initialize());
    PriceRepository repository(database); qint64 id=0; QVERIFY(repository.intake(QStringLiteral("原商品"),"study",30000,QStringLiteral("手动记录"),"initial",id));
    QList<PriceTrack> tracks; QVERIFY(repository.load(tracks)); AIStubServer server;
    server.handler=[](const QJsonObject &) { auto value=price(); value["name"]=QStringLiteral("截图里的另一商品"); return QString::fromUtf8(QJsonDocument(value).toJson()); }; saveSettings(database,server);
    PriceRecordDialog dialog(database,&tracks.first()); dialog.show(); QVERIFY(dialog.setScreenshot(screenshotFile(directory.path())));
    dialog.findChild<QPushButton *>("priceAnalyzeButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->value(),228.,5000);
    QCOMPARE(dialog.findChild<QLineEdit *>("priceNameEdit")->text(),QStringLiteral("原商品"));
    QCOMPARE(dialog.findChild<QComboBox *>("priceCategoryComboBox")->currentData().toString(),QStringLiteral("study"));
    QVERIFY(dialog.findChild<QLabel *>("priceAIResultLabel")->text().contains(QStringLiteral("截图里的另一商品")));
    auto *save=dialog.findChild<QPushButton *>("savePriceRecordButton"); save->click(); QCOMPARE(count(database,"price_records"),1);
    dialog.findChild<QCheckBox *>("priceConfirmMatchCheckBox")->setChecked(true); save->click(); QCOMPARE(count(database,"price_records"),2); QCOMPARE(count(database,"price_tracks"),1);
    QVERIFY(repository.load(tracks)); QCOMPARE(tracks.first().id,id); QCOMPARE(tracks.first().latestCents,22800);
}

void PriceRecordTests::firstWishScreenshotKeepsIdentityAndRequiresMatch()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("wish-image.sqlite")); QVERIFY(database.initialize());
    WishlistRepository wishes(database); WishItem wish; wish.name=QStringLiteral("已选定的商品"); wish.estimatedCents=30000; wish.detailCategory="study"; wish.category=QStringLiteral("学习"); QVERIFY(wishes.add(wish));
    AIStubServer server; server.handler=[](const QJsonObject &) { return QString::fromUtf8(QJsonDocument(price()).toJson()); }; saveSettings(database,server);
    PriceTrack context; context.name=wish.name; context.category=wish.detailCategory;
    PriceRecordDialog dialog(database,&context,nullptr,wish.id); dialog.show(); QVERIFY(dialog.setScreenshot(screenshotFile(directory.path())));
    dialog.findChild<QPushButton *>("priceAnalyzeButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->value(),228.,5000);
    QCOMPARE(dialog.findChild<QLineEdit *>("priceNameEdit")->text(),wish.name); QCOMPARE(dialog.findChild<QComboBox *>("priceCategoryComboBox")->currentData().toString(),wish.detailCategory);
    dialog.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),0);
    dialog.findChild<QCheckBox *>("priceConfirmMatchCheckBox")->setChecked(true); dialog.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),1);
    QList<WishItem> loaded; QVERIFY(wishes.findAll(loaded)); QCOMPARE(loaded.size(),1); QCOMPARE(loaded.first().readyAt,wish.readyAt);
}

void PriceRecordTests::unknownPriceClearsOldDraftAndCannotSave()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("unknown.sqlite")); QVERIFY(database.initialize()); AIStubServer server;
    server.handler=[](const QJsonObject &) { return QString::fromUtf8(QJsonDocument(price(QJsonValue::Null)).toJson()); }; saveSettings(database,server);
    PriceRecordDialog dialog(database); dialog.show(); dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(999);
    QVERIFY(dialog.setScreenshot(screenshotFile(directory.path()))); dialog.findChild<QPushButton *>("priceAnalyzeButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->value(),0.,5000);
    dialog.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),0);
    dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(228); dialog.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),1);
}

void PriceRecordTests::failureAndCancellationAllowManualEntry()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("manual.sqlite")); QVERIFY(database.initialize());
    PriceRecordDialog missing(database); missing.show(); QVERIFY(missing.setScreenshot(screenshotFile(directory.path())));
    missing.findChild<QPushButton *>("priceAnalyzeButton")->click(); QVERIFY(missing.findChild<QPushButton *>("savePriceRecordButton")->isEnabled());
    QVERIFY(missing.findChild<QLabel *>("priceRecordErrorLabel")->text().contains("API Key"));
    missing.findChild<QLineEdit *>("priceNameEdit")->setText(QStringLiteral("手动填写的商品")); missing.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(88);
    missing.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),1);
    AIStubServer server; server.delay=250; server.handler=[](const QJsonObject &) { return QString::fromUtf8(QJsonDocument(price()).toJson()); }; saveSettings(database,server);
    PriceRecordDialog cancel(database); cancel.show(); QVERIFY(cancel.setScreenshot(screenshotFile(directory.path()))); cancel.findChild<QLineEdit *>("priceNameEdit")->setText(QStringLiteral("保留我的输入"));
    cancel.findChild<QPushButton *>("priceAnalyzeButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),1,5000);
    cancel.findChild<QPushButton *>("priceManualButton")->click(); QTest::qWait(350);
    QCOMPARE(cancel.findChild<QLineEdit *>("priceNameEdit")->text(),QStringLiteral("保留我的输入")); QCOMPARE(cancel.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->value(),0.);
    cancel.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(66); cancel.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),2);
    PriceRecordDialog close(database); close.show(); QVERIFY(close.setScreenshot(screenshotFile(directory.path()))); close.findChild<QPushButton *>("priceAnalyzeButton")->click();
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(),2,5000); close.reject(); QTest::qWait(350); QCOMPARE(count(database,"price_records"),2);
    server.status=401; server.delay=0; PriceRecordDialog rejected(database); rejected.show();
    QVERIFY(rejected.setScreenshot(screenshotFile(directory.path())));
    rejected.findChild<QLineEdit *>("priceNameEdit")->setText(QStringLiteral("失败后手动记录"));
    rejected.findChild<QPushButton *>("priceAnalyzeButton")->click();
    QTRY_VERIFY_WITH_TIMEOUT(rejected.findChild<QLabel *>("priceRecordErrorLabel")->text().contains("API Key"),5000);
    QCOMPARE(rejected.findChild<QLineEdit *>("priceNameEdit")->text(),QStringLiteral("失败后手动记录"));
    rejected.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(55); rejected.findChild<QPushButton *>("savePriceRecordButton")->click();
    QCOMPARE(count(database,"price_records"),3);
}

void PriceRecordTests::replacingScreenshotClearsRecognizedPrice()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("replace.sqlite")); QVERIFY(database.initialize()); AIStubServer server;
    server.handler=[](const QJsonObject &) { return QString::fromUtf8(QJsonDocument(price()).toJson()); }; saveSettings(database,server);
    PriceRecordDialog dialog(database); dialog.show(); const auto path=screenshotFile(directory.path()); QVERIFY(dialog.setScreenshot(path));
    dialog.findChild<QPushButton *>("priceAnalyzeButton")->click(); QTRY_COMPARE_WITH_TIMEOUT(dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->value(),228.,5000);
    QVERIFY(dialog.setScreenshot(path)); QCOMPARE(dialog.findChild<QDoubleSpinBox *>("priceAmountSpinBox")->value(),0.);
    dialog.findChild<QPushButton *>("savePriceRecordButton")->click(); QCOMPARE(count(database,"price_records"),0);
}

void PriceRecordTests::mainWindowOpensBothEntriesAndSettings()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("window.sqlite")); QVERIFY(database.initialize());
    MainWindow window(database.databasePath()); window.show(); window.showPage(MainWindow::Page::Wishlist);
    WishlistRepository wishes(database); WishItem wish; wish.name=QStringLiteral("新商品"); wish.estimatedCents=10000; QVERIFY(wishes.add(wish));
    window.showPage(MainWindow::Page::Wishlist); auto *table=window.findChild<QTableWidget *>("wishlistTable"); table->selectRow(0); settle();
    window.findChild<QPushButton *>("wishPriceButton")->click(); settle(); auto *prices=window.findChild<WishPriceDialog *>(); QVERIFY(prices);
    prices->findChild<QPushButton *>("recordWishPriceButton")->click(); settle(); auto *dialog=window.findChild<PriceRecordDialog *>(); QVERIFY(dialog);
    dialog->findChild<QDoubleSpinBox *>("priceAmountSpinBox")->setValue(100); dialog->findChild<QPushButton *>("savePriceRecordButton")->click(); settle();
    prices->findChild<QPushButton *>("recordWishPriceButton")->click(); settle(); dialog=window.findChild<PriceRecordDialog *>(); QVERIFY(dialog); QVERIFY(dialog->findChild<QLineEdit *>("priceNameEdit")->isReadOnly());
    dialog->findChild<QPushButton *>("priceAISettingsButton")->click(); settle(); QCOMPARE(window.currentPage(),MainWindow::Page::Settings);
}

QTEST_MAIN(PriceRecordTests)
#include "price_record_tests.moc"
