#include "EmptyAmountSpinBox.h"
#include "ModernSelectors.h"
#include "PriceRecordDialog.h"
#include "ScreenshotDropTarget.h"
#include "UnifiedWishlistRepository.h"
#include "ProductDetailsDialog.h"
#include "CategoryCatalog.h"
#include "ShellWidgets.h"
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QUuid>
#include <QTimer>

PriceRecordDialog::PriceRecordDialog(DatabaseManager &database,const PriceTrack *track,QWidget *parent,qint64 wishId)
    :QDialog(parent),m_database(database),m_repository(database),m_service(this),
      m_settingsPath(database.databasePath()==QStringLiteral(":memory:")?QString():database.databasePath()+QStringLiteral(".ui.ini")),
      m_token(QUuid::createUuid().toString(QUuid::Id128)),m_trackName(track?track->name:QString()),m_trackId(track?track->id:0),m_wishId(wishId)
{
    setObjectName("priceRecordDialog"); setWindowTitle(wishId?QStringLiteral("记录商品价格"):track?QStringLiteral("追加价格记录"):QStringLiteral("记录价格"));
    setAcceptDrops(true); resize(590,670); setMinimumSize(450,400);
    new ScreenshotDropTarget(this,[this] { return !m_service.busy() && !m_saved; },
        [this](const QString &path) { return setScreenshot(path); },[this](const QImage &image) { return useImage(image); });
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(24,20,24,20); layout->setSpacing(12);
    auto *content=new QWidget; content->setProperty("page",true);
    auto *body=new QVBoxLayout(content); body->setContentsMargins(0,0,8,0); body->setSpacing(12);
    auto *drop=new QFrame(content); drop->setObjectName("priceScreenshotArea"); drop->setProperty("card",true);
    auto *dropLayout=new QVBoxLayout(drop); dropLayout->setContentsMargins(14,12,14,12); dropLayout->setSpacing(8);
    m_choose=new QPushButton(QStringLiteral("＋ 选择或拖入价格截图"),drop); m_choose->setObjectName("priceImageButton"); m_choose->setMinimumHeight(48); m_choose->setProperty("quiet",true);
    m_paste=new QPushButton(QStringLiteral("粘贴截图"),drop); m_paste->setObjectName("pricePasteImageButton"); m_paste->setProperty("link",true);
    dropLayout->addWidget(m_choose); dropLayout->addWidget(m_paste,0,Qt::AlignHCenter);
    m_preview=new QLabel(drop); m_preview->setObjectName("priceImagePreview"); m_preview->setAlignment(Qt::AlignCenter); m_preview->setMaximumHeight(90); m_preview->hide(); dropLayout->addWidget(m_preview);
    auto *recognize=new QHBoxLayout;
    m_analyze=new QPushButton(QStringLiteral("识别截图价格"),drop); m_analyze->setObjectName("priceAnalyzeButton");
    m_remove=new QPushButton(QStringLiteral("移除截图"),drop); m_remove->setObjectName("priceRemoveImageButton"); m_remove->setProperty("link",true);
    m_manual=new QPushButton(QStringLiteral("取消识别"),drop); m_manual->setObjectName("priceManualButton"); m_manual->setProperty("link",true);
    m_settings=new QPushButton(QStringLiteral("配置 AI 服务"),drop); m_settings->setObjectName("priceAISettingsButton"); m_settings->setProperty("link",true);
    recognize->addWidget(m_analyze); recognize->addWidget(m_remove); recognize->addStretch(); recognize->addWidget(m_manual); recognize->addWidget(m_settings); dropLayout->addLayout(recognize);
    m_status=ShellWidgets::label(QString(),drop,"muted"); m_status->setObjectName("priceAIStatusLabel"); m_status->setTextFormat(Qt::PlainText); m_status->hide(); dropLayout->addWidget(m_status); body->addWidget(drop);
    m_aiNote=ShellWidgets::label(QString(),content,"muted"); m_aiNote->setObjectName("priceAIResultLabel"); m_aiNote->setTextFormat(Qt::PlainText); m_aiNote->hide(); body->addWidget(m_aiNote);
    m_form=new QWidget(content); auto *form=new QFormLayout(m_form); form->setContentsMargins(0,0,0,0); form->setSpacing(10);
    m_name=new QLineEdit(m_trackName,m_form); m_name->setObjectName("priceNameEdit"); m_name->setMaxLength(100); m_name->setReadOnly(track);
    m_category=new ModernComboBox(m_form); m_category->setObjectName("priceCategoryComboBox"); for (const auto &key:CategoryCatalog::keys()) m_category->addItem(CategoryCatalog::label(key),key);
    m_category->setCurrentIndex(m_category->findData(track?track->category:QStringLiteral("online_shopping"))); m_category->setEnabled(!track);
    m_amount=new EmptyAmountSpinBox(m_form,QStringLiteral("请填写价格（元）")); m_amount->setObjectName("priceAmountSpinBox"); m_amount->setRange(0,99999999.99); m_amount->setDecimals(2);
    m_source=new QLineEdit(QStringLiteral("手动记录"),m_form); m_source->setObjectName("priceSourceEdit"); m_source->setMaxLength(80); m_source->setPlaceholderText(QStringLiteral("如：淘宝 · M 码 · 券后价"));
    m_time=new QDateTimeEdit(QDateTime::currentDateTime(),m_form); m_time->setObjectName("priceObservedTimeEdit"); m_time->setDisplayFormat("yyyy-MM-dd HH:mm:ss"); m_time->setCalendarPopup(true); m_time->setDateTimeRange(QDateTime(QDate(1,1,1),QTime(0,0)),QDateTime::currentDateTime().addSecs(60));
    form->addRow(QStringLiteral("商品"),m_name); form->addRow(QStringLiteral("实际看到的价格（元）"),m_amount); form->addRow(QStringLiteral("平台／来源与条件"),m_source); body->addWidget(m_form);
    auto *more=new QPushButton(QStringLiteral("更多信息"),content); more->setObjectName("priceMoreOptionsButton"); more->setCheckable(true); more->setProperty("link",true); body->addWidget(more,0,Qt::AlignLeft);
    m_options=new QWidget(content); m_options->setObjectName("priceMoreOptions"); auto *options=new QVBoxLayout(m_options); options->setContentsMargins(0,0,0,0);
    auto *extra=new QFormLayout; extra->addRow(QStringLiteral("品类"),m_category); extra->addRow(QStringLiteral("记录时间"),m_time); options->addLayout(extra); body->addWidget(m_options); m_options->hide();
    connect(more,&QPushButton::toggled,this,[this,more](bool open) { m_options->setVisible(open); more->setText(open?QStringLiteral("收起信息"):QStringLiteral("更多信息")); });
    connect(more,&QPushButton::toggled,this,[this](bool open) { if(open) QTimer::singleShot(0,this,[this] { if(auto *scroll=findChild<QScrollArea *>("priceRecordScroll")) scroll->ensureWidgetVisible(m_options,0,8); }); });
    auto *product=new QPushButton(QStringLiteral("补充商品详情 →"),content); product->setObjectName("priceRecordProductDetailsButton"); options->addWidget(product,0,Qt::AlignLeft);
    connect(product,&QPushButton::clicked,this,[this] {
        if (m_service.busy()) return;
        if (m_wishId) { ProductDetailsDialog::openFor(m_database,"wish",m_wishId,this); return; }
        if (m_trackId) { ProductDetailsDialog::openFor(m_database,"price",m_trackId,this); return; }
        if (m_name->text().trimmed().isEmpty()) { showError(QStringLiteral("请先填写商品名，再补充详情。")); return; }
        ProductContext context{QString(),m_name->text().trimmed(),m_category->currentData().toString(),0,qRound64(m_amount->value()*100),m_image};
        auto *dialog=new ProductDetailsDialog(m_database,context,m_productProfileId,this); m_productProfileId=dialog->profileId(); dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog,&ProductDetailsDialog::profileChanged,this,[this](qint64 id) { m_productProfileId=id; }); dialog->open();
    });
    m_confirmMatch=new QCheckBox(QStringLiteral("已核对截图商品及规格与本商品一致"),content); m_confirmMatch->setObjectName("priceConfirmMatchCheckBox"); m_confirmMatch->hide(); body->addWidget(m_confirmMatch); body->addStretch();
    auto *scroll=new QScrollArea(this); scroll->setObjectName("priceRecordScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(content); layout->addWidget(scroll,1);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("priceRecordErrorLabel"); m_error->setTextFormat(Qt::PlainText); m_error->hide(); layout->addWidget(m_error);
    auto *buttons=new QHBoxLayout;
    auto *cancel=new QPushButton(QStringLiteral("取消"),this); cancel->setObjectName("cancelPriceRecordButton");
    m_save=new QPushButton(QStringLiteral("保存价格"),this); m_save->setObjectName("savePriceRecordButton"); m_save->setProperty("primary",true);
    buttons->addWidget(cancel); buttons->addStretch(); buttons->addWidget(m_save); layout->addLayout(buttons);
    connect(cancel,&QPushButton::clicked,this,&PriceRecordDialog::reject);
    connect(m_choose,&QPushButton::clicked,this,[this] { const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("选择商品价格截图"),{},QStringLiteral("图片 (*.png *.jpg *.jpeg *.webp *.bmp)")); if (!path.isEmpty()) setScreenshot(path); });
    connect(m_paste,&QPushButton::clicked,this,[this] { const auto image=QApplication::clipboard()->image(); if (image.isNull()) showError(QStringLiteral("剪贴板里没有图片，请先复制截图。")); else useImage(image); });
    connect(m_remove,&QPushButton::clicked,this,[this] { m_image.clear(); m_aiDraft=false; m_aiFailed=false; m_preview->hide(); m_aiNote->hide(); m_confirmMatch->hide(); m_confirmMatch->setChecked(false); m_error->hide(); setBusy(false); });
    connect(m_analyze,&QPushButton::clicked,this,&PriceRecordDialog::analyze);
    connect(m_manual,&QPushButton::clicked,this,[this] { m_service.cancel(); setBusy(false); m_status->setText(QStringLiteral("可以直接手动填写，核对后保存。")); m_error->hide(); m_amount->setFocus(); });
    connect(m_settings,&QPushButton::clicked,this,[this] { reject(); emit settingsRequested(); });
    connect(m_save,&QPushButton::clicked,this,&PriceRecordDialog::save);
    connect(&m_service,&AIService::failed,this,[this](const QString &message) { m_aiFailed=true; setBusy(false); m_status->setText(QStringLiteral("识别未完成，已保留填写内容，可重试或手动填写。")); showError(message); });
    connect(&m_service,&AIService::priceParsed,this,[this](const ParsedPrice &price) {
        m_aiDraft=true;
        if (!m_trackId && !m_wishId) { m_name->setText(price.name); m_category->setCurrentIndex(m_category->findData(price.category)); }
        m_amount->setValue(double(price.amountCents)/100);
        m_source->setText(price.source.isEmpty()?QStringLiteral("截图识别确认"):price.source+QStringLiteral(" · 截图确认"));
        m_aiNote->setText(QStringLiteral("截图商品：%1\n%2").arg(price.name.isEmpty()?QStringLiteral("名称未识别"):price.name,price.note)); m_aiNote->show();
        m_confirmMatch->setChecked(false); m_confirmMatch->setVisible(m_trackId>0 || m_wishId>0);
        m_error->hide(); setBusy(false);
        m_status->setText(price.amountCents?QStringLiteral("已填入识别结果，请核对价格、规格和优惠条件后保存。重要条件可补入来源栏。"):
            QStringLiteral("未识别出唯一明确的价格，请手动填写；尚未保存。"));
    });
    for (auto *button:findChildren<QPushButton *>()) button->setAutoDefault(false);
    setBusy(false);
}

void PriceRecordDialog::showError(const QString &message) { m_error->setText(message); m_error->show(); }
void PriceRecordDialog::setBusy(bool busy)
{
    m_options->setEnabled(!busy); m_form->setEnabled(!busy); m_confirmMatch->setEnabled(!busy); m_save->setEnabled(!busy && !m_saved);
    for (auto *button:{m_choose,m_paste,m_remove,m_settings}) button->setEnabled(!busy);
    const bool image=!m_image.isEmpty();
    m_analyze->setVisible(image); m_remove->setVisible(image); m_manual->setVisible(busy);
    m_status->setVisible(image);
    const auto settings=AISettingsStore::load(m_settingsPath);
    m_settings->setVisible(!busy && image && (m_aiFailed || settings.apiKey.trimmed().isEmpty() || !settings.validationError().isEmpty()));
    m_choose->setText(image?QStringLiteral("更换截图"):QStringLiteral("＋ 选择或拖入价格截图"));
    m_analyze->setEnabled(!busy && image);
    m_analyze->setText(busy?QStringLiteral("识别中…"):QStringLiteral("识别截图价格"));
    m_manual->setText(QStringLiteral("取消识别"));
}
bool PriceRecordDialog::setScreenshot(const QString &path)
{
    if (m_service.busy()) return false;
    QImageReader reader(path); reader.setAutoTransform(true); const auto size=reader.size();
    if (!QFileInfo(path).isFile() || QFileInfo(path).size()>20*1024*1024 || !size.isValid() || qint64(size.width())*size.height()>25000000) {
        showError(QStringLiteral("请选择不超过 20 MB、2500 万像素的有效图片。")); return false;
    }
    reader.setScaledSize(size.scaled(1600,1600,Qt::KeepAspectRatio));
    return useImage(reader.read());
}
bool PriceRecordDialog::useImage(const QImage &image)
{
    if (m_service.busy()) return false;
    if (image.isNull() || qint64(image.width())*image.height()>25000000) { showError(QStringLiteral("无法读取截图，请选择有效图片（最多 2500 万像素）。")); return false; }
    const auto scaled=image.scaled(1600,1600,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    QImage canvas(scaled.size(),QImage::Format_RGB32); canvas.fill(Qt::white); { QPainter painter(&canvas); painter.drawImage(0,0,scaled); }
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); canvas.save(&buffer,"JPEG",85);
    if (bytes.isEmpty() || bytes.size()>2*1024*1024) { showError(QStringLiteral("截图压缩后仍过大，请先裁剪商品区域。")); return false; }
    m_image=bytes; m_preview->setPixmap(QPixmap::fromImage(canvas).scaled(430,90,Qt::KeepAspectRatio,Qt::SmoothTransformation)); m_preview->show(); m_remove->show();
    if (m_aiDraft) { m_amount->setValue(0); m_source->setText(QStringLiteral("手动记录")); }
    m_aiDraft=false; m_aiFailed=false; m_aiNote->hide(); m_confirmMatch->hide(); m_confirmMatch->setChecked(false); m_error->hide();
    m_status->setText(QStringLiteral("点击识别时，这张截图会发送到设置中的 AI 服务；也可以手动填写。")); setBusy(false); return true;
}
void PriceRecordDialog::analyze()
{
    m_aiFailed=false; m_error->hide(); setBusy(true); m_status->setText(QStringLiteral("正在识别截图中的商品、价格与平台…"));
    QString error; const auto settings=AISettingsStore::load(m_settingsPath,&error);
    if (!error.isEmpty()) { m_aiFailed=true; setBusy(false); showError(error); return; }
    const auto context=m_trackId?QStringLiteral("正在为已有商品追加记录。已有商品名：%1。请识别截图中的实际商品及价格，供用户核对是否同款同规格。").arg(m_trackName):QString();
    if (!m_service.analyzePrice(settings,context,m_image)) setBusy(false);
}
void PriceRecordDialog::save()
{
    if (m_saved || m_service.busy()) return;
    if (!m_amount->hasAcceptableInput() || !m_time->hasAcceptableInput()) { showError(QStringLiteral("请输入有效金额和日期时间。")); return; }
    m_amount->interpretText(); m_time->interpretText();
    if (m_name->text().trimmed().isEmpty() || m_amount->value()<=0 || m_source->text().trimmed().isEmpty()) { showError(QStringLiteral("请确认商品名、实际价格和来源；未识别的价格需要手动补充。")); return; }
    if (m_aiDraft && (m_trackId || m_wishId) && !m_confirmMatch->isChecked()) { showError(QStringLiteral("请先核对截图商品和规格，并勾选确认。")); return; }
    m_save->setEnabled(false); qint64 id=m_trackId;
    UnifiedWishlistRepository catalog(m_database);
    const bool ok=m_wishId?catalog.recordPrice(m_wishId,qRound64(m_amount->value()*100),m_source->text(),m_token,m_time->dateTime().toUTC(),id):id?m_repository.record(id,qRound64(m_amount->value()*100),m_source->text(),m_token,m_time->dateTime().toUTC()):
        m_repository.intake(m_name->text(),m_category->currentData().toString(),qRound64(m_amount->value()*100),m_source->text(),m_token,id,m_time->dateTime().toUTC(),m_productProfileId);
    if (!ok) { showError(m_wishId?catalog.lastError():m_repository.lastError()); m_save->setEnabled(true); return; }
    m_saved=true; emit priceSaved(id); accept();
}
void PriceRecordDialog::reject() { m_service.cancel(); QDialog::reject(); }
