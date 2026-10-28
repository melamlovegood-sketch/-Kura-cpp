#include "AIStubServer.h"
#include "AppTheme.h"
#include "DatabaseBackupService.h"
#include "ProductDetailsDialog.h"
#include "ProductShareDialog.h"
#include "WishlistRepository.h"
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QtTest>

namespace {
ProductContext sample() { return {QString(),QStringLiteral("轻便保暖围巾"),"online_shopping",0,12900,{},QStringLiteral("冬天通勤，想要一条轻便的围巾。")}; }
QByteArray imageBytes(const QColor &color) {
    QImage image(600,360,QImage::Format_RGB32); image.fill(color); QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG"); return bytes;
}
void settle() { QApplication::processEvents(); QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete); QApplication::processEvents(); }
void click(QWidget &owner,const char *name) { auto *button=owner.findChild<QPushButton *>(name); QVERIFY2(button,name); button->click(); settle(); }
void capture(QWidget &widget,const QString &name) {
    const auto directory=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (directory.isEmpty()) return;
    QVERIFY(QDir().mkpath(directory)); settle(); QVERIFY(widget.grab().save(directory+"/"+name+".png"));
}
}

class ProductShareTests:public QObject {
    Q_OBJECT
private slots:
    void reasonsStayConsistentAndSurviveBackup();
    void personalFieldsAreManualAndDraftsStayPrivate();
    void selectedContentClipboardAndPng();
    void longTextAndSmallPreview();
};

void ProductShareTests::reasonsStayConsistentAndSurviveBackup()
{
    QTemporaryDir directory; const auto path=directory.filePath("share.sqlite"); qint64 profileId=0,wishId=0;
    {
        DatabaseManager database(path); QVERIFY(database.initialize()); WishlistRepository wishes(database);
        WishItem wish; wish.name=sample().name; wish.estimatedCents=12900; wish.reason=sample().purchaseReason; wish.detailCategory="online_shopping";
        QVERIFY(wishes.add(wish)); wishId=wish.id; const auto deadline=wish.readyAt;
        ProductRepository products(database); ProductContext context; ProductProfile profile;
        QVERIFY(products.context("wish",wish.id,context)); QCOMPARE(context.purchaseReason,wish.reason);
        QVERIFY(products.ensure(context,profile)); profileId=profile.id;
        QCOMPARE(ProductRepository::purchaseReason(profile,context),wish.reason);
        profile.fields.insert("purchase_reason",QStringLiteral("想要更轻的，也喜欢这个颜色。\n先比较保暖程度。"));
        profile.fields.insert("alternatives",QStringLiteral("继续用已有的围巾；也可以先借用。")); QVERIFY(products.save(profile));
        QList<WishItem> stored; QVERIFY(wishes.findAll(stored)); QCOMPARE(stored.first().reason,profile.fields.value("purchase_reason").toString()); QCOMPARE(stored.first().readyAt,deadline);
        wish=stored.first(); wish.reason=QStringLiteral("已有的也能用，暂时观望。"); QVERIFY(wishes.update(wish));
        QVERIFY(products.load(profileId,profile)); QCOMPARE(profile.fields.value("purchase_reason").toString(),wish.reason);
        QCOMPARE(profile.fields.value("alternatives").toString(),QStringLiteral("继续用已有的围巾；也可以先借用。"));
        QString error,safety; BackupInfo info; DatabaseBackupService backups(database); const auto backup=directory.filePath("backup.sqlite");
        QVERIFY(backups.backup(backup,error)); profile.fields.insert("alternatives","changed"); QVERIFY(products.save(profile));
        QVERIFY2(backups.prepareRestore(backup,info,error),qPrintable(error)); QVERIFY2(backups.restorePrepared(safety,error),qPrintable(error));
        QVERIFY(products.load(profileId,profile)); QCOMPARE(profile.fields.value("alternatives").toString(),QStringLiteral("继续用已有的围巾；也可以先借用。"));
    }
    DatabaseManager reopened(path); QVERIFY(reopened.initialize()); ProductRepository products(reopened); ProductProfile profile; ProductContext context;
    QVERIFY(products.context("wish",wishId,context)); QVERIFY(products.load(profileId,profile));
    QCOMPARE(ProductRepository::purchaseReason(profile,context),QStringLiteral("已有的也能用，暂时观望。"));
    profile.fields.insert("purchase_reason",QStringLiteral("")); QVERIFY(products.save(profile));
    QVERIFY(ProductRepository::purchaseReason(profile,context).isEmpty());
    WishItem carried; carried.name=context.name; carried.estimatedCents=context.amountCents; carried.detailCategory=context.category; carried.reason=QStringLiteral("旧备注");
    WishlistRepository wishes(reopened); QVERIFY(wishes.add(carried,QDateTime::currentDateTimeUtc(),profile.id)); QVERIFY(carried.reason.isEmpty());
    auto invalid=profile; invalid.fields.insert("purchase_reason",QString(501,QChar('x'))); QVERIFY(!products.save(invalid));
}

void ProductShareTests::personalFieldsAreManualAndDraftsStayPrivate()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("details.sqlite")); QVERIFY(database.initialize());
    ProductRepository products(database); ProductProfile profile; QVERIFY(products.ensure(sample(),profile));
    AIStubServer server; QString error; QVERIFY(AISettingsStore::save(database.databasePath()+".ui.ini",server.settings(),error));
    ProductDetailsDialog details(database,sample(),profile.id); AppTheme::apply(&details,"warm"); details.show(); settle();
    QVERIFY(details.findChild<QLabel *>("productDetailsSummaryLabel")->text().contains(sample().purchaseReason));
    click(details,"editProductDetailsButton"); auto *reason=details.findChild<QPlainTextEdit *>("productPurchaseReasonEdit"); auto *alternatives=details.findChild<QPlainTextEdit *>("productAlternativesEdit");
    QCOMPARE(reason->toPlainText(),sample().purchaseReason); reason->setPlainText(QStringLiteral("未保存草稿"));
    QVERIFY(!details.findChild<QPushButton *>("shareProductCardButton")->isEnabled());
    click(details,"cancelProductDetailsEditButton"); QVERIFY(products.load(profile.id,profile)); QCOMPARE(profile.fields.value("purchase_reason").toString(),sample().purchaseReason);
    click(details,"editProductDetailsButton"); reason->setPlainText(QStringLiteral("喜欢这个颜色\n通勤时用")); alternatives->setPlainText(QStringLiteral("继续用旧围巾")); click(details,"saveProductDetailsButton");
    QVERIFY(products.load(profile.id,profile)); QCOMPARE(profile.fields.value("alternatives").toString(),QStringLiteral("继续用旧围巾"));
    capture(details,"product-personal-details");
    click(details,"shareProductCardButton"); auto *share=details.findChild<ProductShareDialog *>(); QVERIFY(share); QVERIFY(!share->cardImage().isNull()); share->reject(); settle();
    QCOMPARE(server.requests.size(),0);
    const auto response=QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",QStringLiteral("{\"fields\":{\"purchase_reason\":\"商家编写的理由\"},\"note\":\"\"}")}}}}}}}).toJson();
    QJsonObject fields; QString note; QVERIFY(!AIService::decodeDetails(response,fields,note,error));
}

void ProductShareTests::selectedContentClipboardAndPng()
{
    QTemporaryDir directory; ProductContext context=sample(); context.image=imageBytes(QColor("#b7cdd8"));
    ProductProfile profile; profile.name=context.name; profile.fields={{"brand",QStringLiteral("示例品牌")},{"materials",QStringLiteral("羊毛混纺，成分待核对")},{"alternatives",QStringLiteral("已有的聚酯纤维围巾")}};
    profile.notes=QStringLiteral("私有需求，默认不分享"); profile.conversation="PRIVATE_AI_CONVERSATION";
    const QList<ProductEvidence> images{{1,0,imageBytes(QColor("#e8d8bf")),{}}};
    ProductShareDialog dialog(context,profile,images); AppTheme::apply(&dialog,"warm"); dialog.show(); settle();
    const auto initial=dialog.cardImage(); auto hidden=profile; hidden.notes="different private notes"; hidden.conversation="different private chat";
    QCOMPARE(initial,ProductShareDialog::renderCard(context,hidden,context.image));
    QVERIFY(!dialog.findChild<QCheckBox *>("shareIncludeNotes")->isChecked());
    dialog.findChild<QCheckBox *>("shareIncludeImage")->setChecked(false); settle(); const auto noImage=dialog.cardImage(); QVERIFY(noImage.height()<initial.height());
    dialog.findChild<QCheckBox *>("shareIncludeReason")->setChecked(false); settle(); QVERIFY(dialog.cardImage().height()<noImage.height());
    auto noReason=context; noReason.purchaseReason="hidden reason changed"; ProductShareOptions options; options.image=false; options.reason=false;
    QCOMPARE(dialog.cardImage(),ProductShareDialog::renderCard(noReason,hidden,{},options));
    click(dialog,"copyProductCardButton"); QCOMPARE(QApplication::clipboard()->image(),dialog.cardImage());
    const auto png=directory.filePath(QStringLiteral("围巾分享卡.png")); QVERIFY(dialog.saveTo(png)); QCOMPARE(QImage(png),dialog.cardImage());
    QVERIFY(!dialog.saveTo(directory.filePath("missing/card.png"))); QVERIFY(!dialog.findChild<QLabel *>("productShareStatus")->text().isEmpty());
    dialog.findChild<QCheckBox *>("shareIncludeImage")->setChecked(true); dialog.findChild<QCheckBox *>("shareIncludeReason")->setChecked(true); dialog.findChild<QComboBox *>("shareImageSourceCombo")->setCurrentIndex(1); settle(); QVERIFY(dialog.cardImage()!=initial);
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR"); if (!preview.isEmpty()) { QVERIFY(QDir().mkpath(preview)); QVERIFY(dialog.saveTo(preview+"/product-share-card.png")); }
    capture(dialog,"product-share-preview");
}

void ProductShareTests::longTextAndSmallPreview()
{
    ProductContext context=sample(); ProductProfile profile; profile.fields={{"brand",QStringLiteral("示例品牌")},{"purchase_reason",QStringLiteral("冬天通勤想要一条更轻的围巾。\n").repeated(20)},{"alternatives",QStringLiteral("继续用已有的围巾，或借用。\n").repeated(40)},{"other",QString(1000,QChar('X'))}};
    const auto longCard=ProductShareDialog::renderCard(context,profile,{}); QVERIFY(longCard.height()>2500);
    auto shortProfile=profile; shortProfile.fields.remove("alternatives"); QVERIFY(longCard.height()>ProductShareDialog::renderCard(context,shortProfile,{}).height());
    QWidget host; host.resize(800,600); AppTheme::apply(&host,"warm"); host.show();
    ProductShareDialog dialog(context,profile,{},&host); dialog.show(); settle();
    for (const auto &theme:QStringList{"warm","cool","dark"}) {
        AppTheme::apply(&host,theme); settle(); auto *scroll=dialog.findChild<QScrollArea *>("productSharePreviewScroll"); QVERIFY(scroll);
        QCOMPARE(scroll->horizontalScrollBar()->maximum(),0); QVERIFY(scroll->verticalScrollBar()->maximum()>0);
        for (const auto *name:{"copyProductCardButton","saveProductCardButton"}) {
            const auto *button=dialog.findChild<QPushButton *>(name); QVERIFY(button->isVisible());
            QVERIFY(dialog.rect().contains(button->mapTo(&dialog,button->rect().bottomRight())));
        }
        capture(dialog,"product-share-small-"+theme);
    }
    QVERIFY(!dialog.findChild<QCheckBox *>("shareIncludeImage")->isEnabled());
}

QTEST_MAIN(ProductShareTests)
#include "product_share_tests.moc"
