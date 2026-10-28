#include "ModernSelectors.h"
#include "DecisionDialog.h"
#include "CategoryCatalog.h"
#include "ShellWidgets.h"
#include "TransactionRepository.h"
#include "WishlistRepository.h"
#include "WishPoolRepository.h"
#include "ShoppingProfileRepository.h"
#include "PurchaseRepository.h"
#include "PriceRepository.h"
#include "ProductDetailsDialog.h"
#include <QBuffer>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QUuid>

DecisionDialog::DecisionDialog(DatabaseManager &database, const AISettings &settings, QWidget *parent)
    :QDialog(parent),m_database(database),m_settings(settings),m_service(this),m_token(QUuid::createUuid().toString(QUuid::Id128))
{
    setObjectName(QStringLiteral("decisionDialog")); setWindowTitle(QStringLiteral("我现在想买……"));
    setProperty("dialogFixedViewport",true); setAcceptDrops(true); resize(600,610); setMinimumSize(380,400);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(22,20,22,20); layout->setSpacing(14);
    m_stepLabel=ShellWidgets::label(QString(),this,"muted"); m_stepLabel->setObjectName("decisionStepLabel"); layout->addWidget(m_stepLabel);
    auto *content=new QWidget; content->setProperty("page",true);
    auto *body=new QVBoxLayout(content); body->setContentsMargins(0,0,8,0); body->setSpacing(12);
    m_inputPage=new QWidget(content); m_inputPage->setObjectName("decisionInputPage");
    auto *inputLayout=new QVBoxLayout(m_inputPage); inputLayout->setContentsMargins(0,0,0,0); inputLayout->setSpacing(12);
    inputLayout->addWidget(ShellWidgets::label(QStringLiteral("这次想买什么？"),m_inputPage,"heading"));
    m_text=new QPlainTextEdit(m_inputPage); m_text->setObjectName("decisionInput"); m_text->setPlaceholderText(QStringLiteral("例如：想买一副 299 元的耳机\n也可以拖入商品截图"));
    m_text->setMinimumHeight(110); m_text->setMaximumHeight(150); inputLayout->addWidget(m_text);
    auto *inputActions=new QHBoxLayout;
    m_imageButton=new QPushButton(QStringLiteral("添加截图"),m_inputPage); m_imageButton->setObjectName("decisionImageButton");
    m_removeImage=new QPushButton(QStringLiteral("移除截图"),m_inputPage); m_removeImage->setObjectName("decisionRemoveImageButton"); m_removeImage->hide();
    m_manual=new QPushButton(QStringLiteral("手动填写 →"),m_inputPage); m_manual->setObjectName("decisionManualButton");
    inputActions->addWidget(m_imageButton); inputActions->addWidget(m_removeImage); inputActions->addStretch(); inputActions->addWidget(m_manual); inputLayout->addLayout(inputActions);
    m_imagePreview=new QLabel(m_inputPage); m_imagePreview->setAlignment(Qt::AlignCenter); m_imagePreview->setMaximumHeight(110); m_imagePreview->hide(); inputLayout->addWidget(m_imagePreview);
    m_status=ShellWidgets::label(QStringLiteral("文字或截图会交给 AI 识别，你也可以直接手动填写。"),m_inputPage,"muted"); m_status->setObjectName("decisionStatusLabel"); inputLayout->addWidget(m_status);
    m_settingsButton=new QPushButton(QStringLiteral("设置 AI 服务 →"),m_inputPage); m_settingsButton->setObjectName("decisionSettingsButton");
    m_settingsButton->setVisible(!m_settings.validationError().isEmpty() || m_settings.apiKey.trimmed().isEmpty()); inputLayout->addWidget(m_settingsButton,0,Qt::AlignLeft);
    body->addWidget(m_inputPage);

    QFrame *draft; auto *draftLayout=ShellWidgets::card(content,&draft); m_draft=draft; m_draft->setObjectName("decisionConfirmPage");
    draftLayout->addWidget(ShellWidgets::label(QStringLiteral("核对商品信息"),draft,"heading"));
    auto *form=new QFormLayout; form->setSpacing(12);
    m_name=new QLineEdit(draft); m_name->setObjectName("decisionNameEdit"); m_name->setMaxLength(100);
    m_amount=new QDoubleSpinBox(draft); m_amount->setObjectName("decisionAmountSpinBox"); m_amount->setDecimals(2); m_amount->setRange(0,99999999.99); m_amount->setSuffix(QStringLiteral(" 元")); m_amount->setSpecialValueText(QStringLiteral("待确认"));
    m_category=new ModernComboBox(draft); m_category->setObjectName("decisionCategoryComboBox");
    for (const auto &key:CategoryCatalog::keys()) m_category->addItem(CategoryCatalog::label(key),key);
    form->addRow(QStringLiteral("商品名称"),m_name); form->addRow(QStringLiteral("金额"),m_amount); form->addRow(QStringLiteral("分类"),m_category); draftLayout->addLayout(form);
    auto *extrasToggle=new QPushButton(QStringLiteral("补充品牌、备注与详情"),draft); extrasToggle->setObjectName("decisionExtrasButton"); extrasToggle->setCheckable(true); draftLayout->addWidget(extrasToggle,0,Qt::AlignLeft);
    m_extras=new QWidget(draft); auto *extrasLayout=new QVBoxLayout(m_extras); extrasLayout->setContentsMargins(0,0,0,0);
    auto *extraForm=new QFormLayout;
    m_brand=new ModernComboBox(m_extras); m_brand->setObjectName("decisionBrandComboBox");
    m_note=new QPlainTextEdit(m_extras); m_note->setObjectName("decisionNoteEdit"); m_note->setMaximumHeight(70);
    extraForm->addRow(QStringLiteral("品牌（可选）"),m_brand); extraForm->addRow(QStringLiteral("备注（可选）"),m_note); extrasLayout->addLayout(extraForm);
    auto *details=new QPushButton(QStringLiteral("商品详情 →"),m_extras); details->setObjectName("decisionProductDetailsButton"); extrasLayout->addWidget(details,0,Qt::AlignLeft);
    draftLayout->addWidget(m_extras); m_extras->hide(); connect(extrasToggle,&QPushButton::toggled,m_extras,&QWidget::setVisible);
    connect(extrasToggle,&QPushButton::toggled,this,[extrasToggle](bool open) { extrasToggle->setText(open?QStringLiteral("收起补充信息"):QStringLiteral("补充品牌、备注与详情")); }); body->addWidget(draft);
    connect(details,&QPushButton::clicked,this,[this] {
        if (!validateDraft()) return;
        ProductContext context{QString(),m_name->text().trimmed(),m_category->currentData().toString(),0,qRound64(m_amount->value()*100),m_image};
        auto *dialog=new ProductDetailsDialog(m_database,context,m_productProfileId,this); m_productProfileId=dialog->profileId(); dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog,&ProductDetailsDialog::profileChanged,this,[this](qint64 id) { m_productProfileId=id; }); dialog->open();
    });

    m_choicePage=new QWidget(content); m_choicePage->setObjectName("decisionChoicePage");
    auto *choiceLayout=new QVBoxLayout(m_choicePage); choiceLayout->setContentsMargins(0,0,0,0); choiceLayout->setSpacing(12);
    choiceLayout->addWidget(ShellWidgets::label(QStringLiteral("接下来，怎么做？"),m_choicePage,"heading"));
    m_summary=ShellWidgets::label(QString(),m_choicePage); m_summary->setObjectName("decisionSummaryLabel"); choiceLayout->addWidget(m_summary);
    m_wish=new QPushButton(QStringLiteral("加入清单"),m_choicePage); m_wish->setObjectName("decisionWishButton");
    m_buy=new QPushButton(QStringLiteral("购买并记账"),m_choicePage); m_buy->setObjectName("decisionBuyButton");
    m_hold=new QPushButton(QStringLiteral("这次不买"),m_choicePage); m_hold->setObjectName("decisionHoldButton");
    auto *choices=new QHBoxLayout;
    for (auto *button:{m_wish,m_buy,m_hold}) { button->setCheckable(true); button->setProperty("decisionChoice",true); choices->addWidget(button,1); } choiceLayout->addLayout(choices);
    m_purchaseOptions=new QWidget(m_choicePage); auto *purchaseForm=new QFormLayout(m_purchaseOptions); purchaseForm->setContentsMargins(0,0,0,0);
    m_date=new DatePicker(QDate::currentDate(),m_purchaseOptions); m_date->setObjectName("decisionDateEdit"); m_date->setDateRange(QDate(1,1,1),QDate(9999,12,31)); m_date->setDisplayFormat("yyyy-MM-dd"); m_date->setCalendarPopup(true);
    purchaseForm->addRow(QStringLiteral("购买日期"),m_date); choiceLayout->addWidget(m_purchaseOptions);
    m_wishlistOptions=new QWidget(m_choicePage); auto *wishForm=new QFormLayout(m_wishlistOptions); wishForm->setContentsMargins(0,0,0,0);
    m_days=new QSpinBox(m_wishlistOptions); m_days->setObjectName("decisionCoolingDaysSpinBox"); m_days->setRange(1,30); m_days->setValue(3); m_days->setSuffix(QStringLiteral(" 天"));
    wishForm->addRow(QStringLiteral("冷静期"),m_days); choiceLayout->addWidget(m_wishlistOptions);
    m_choiceHint=ShellWidgets::label(QString(),m_choicePage,"muted"); m_choiceHint->setObjectName("decisionChoiceHint"); choiceLayout->addWidget(m_choiceHint);
    m_track=new QPushButton(QStringLiteral("先蹲一下价格 →"),m_choicePage); m_track->setObjectName("decisionTrackButton"); m_track->setCheckable(true); m_track->hide();
    m_research=new QPushButton(QStringLiteral("还想比较？继续研究 →"),m_choicePage); m_research->setObjectName("decisionResearchButton"); choiceLayout->addWidget(m_research,0,Qt::AlignLeft);
    body->addWidget(m_choicePage); body->addStretch();
    m_scroll=new QScrollArea(this); m_scroll->setFrameShape(QFrame::NoFrame); m_scroll->setWidgetResizable(true); m_scroll->setWidget(content); layout->addWidget(m_scroll,1);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("decisionErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    auto *footer=new QHBoxLayout;
    m_back=new QPushButton(QStringLiteral("上一步"),this); m_back->setObjectName("decisionBackButton"); footer->addWidget(m_back);
    auto *cancel=new QPushButton(QStringLiteral("取消"),this); cancel->setObjectName("decisionCancelButton"); footer->addWidget(cancel); footer->addStretch();
    m_parse=new QPushButton(QStringLiteral("下一步 →"),this); m_parse->setObjectName("decisionAnalyzeButton"); m_parse->setProperty("primary",true); footer->addWidget(m_parse);
    m_continue=new QPushButton(QStringLiteral("下一步 →"),this); m_continue->setObjectName("decisionContinueButton"); m_continue->setProperty("primary",true); footer->addWidget(m_continue);
    m_commit=new QPushButton(QStringLiteral("确认"),this); m_commit->setObjectName("decisionCommitButton"); m_commit->setProperty("primary",true); footer->addWidget(m_commit); layout->addLayout(footer);
    connect(cancel,&QPushButton::clicked,this,&DecisionDialog::reject);
    connect(m_back,&QPushButton::clicked,this,[this] { showStage(qMax(0,m_stage-1)); });
    connect(m_continue,&QPushButton::clicked,this,[this] { if (validateDraft()) showStage(2); });
    connect(m_commit,&QPushButton::clicked,this,[this] { saveChoice(m_choice); });
    connect(m_parse,&QPushButton::clicked,this,&DecisionDialog::analyze);
    connect(m_manual,&QPushButton::clicked,this,[this] { if (m_hasDraft) { showStage(1); return; } ParsedProduct p; p.name=m_text->toPlainText().trimmed().left(100); showDraft(p); });
    connect(m_text,&QPlainTextEdit::textChanged,this,&DecisionDialog::invalidateDraft);
    connect(m_removeImage,&QPushButton::clicked,this,[this] { m_image.clear(); m_imagePreview->hide(); m_removeImage->hide(); invalidateDraft(); });
    connect(m_imageButton,&QPushButton::clicked,this,[this] { const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("选择商品截图"),{},QStringLiteral("图片 (*.png *.jpg *.jpeg *.webp *.bmp)")); if (!path.isEmpty()) setScreenshot(path); });
    connect(m_settingsButton,&QPushButton::clicked,this,[this] { reject(); emit settingsRequested(); });
    connect(m_hold,&QPushButton::clicked,this,[this] { selectChoice("hold"); });
    connect(m_wish,&QPushButton::clicked,this,[this] { selectChoice("wishlist"); });
    connect(m_buy,&QPushButton::clicked,this,[this] { selectChoice("buy"); });
    connect(m_track,&QPushButton::clicked,this,[this] { selectChoice("track"); });
    connect(m_research,&QPushButton::clicked,this,[this] {
        if (m_saved || !validateDraft()) return;
        ParsedProduct product; product.name=m_name->text().trimmed(); product.amountCents=qRound64(m_amount->value()*100); product.category=m_category->currentData().toString(); product.note=m_note->toPlainText().trimmed();
        emit researchRequested(product,m_image,m_brand->currentData().toLongLong(),m_productProfileId);
    });
    connect(m_category,&QComboBox::currentIndexChanged,this,&DecisionDialog::refreshBrands);
    connect(&m_service,&AIService::parsed,this,[this](const ParsedProduct &product) { setBusy(false); showDraft(product); });
    connect(&m_service,&AIService::failed,this,[this](const QString &message) { setBusy(false); m_status->setText(QStringLiteral("输入已保留，可以重试或手动填写。")); m_error->setText(message); m_error->show(); m_settingsButton->show(); });
    refreshBrands(); showStage(0); setBusy(false);
    for (auto *button:{m_manual,m_removeImage,m_settingsButton,m_track,m_research,extrasToggle,details}) button->setProperty("decisionQuiet",true);
    for (auto *button:findChildren<QPushButton *>()) button->setAutoDefault(false);
}

void DecisionDialog::showStage(int stage)
{
    m_stage=stage; setProperty("decisionStage",stage); setBusy(m_service.busy());
    m_inputPage->setVisible(stage==0); m_draft->setVisible(stage==1); m_choicePage->setVisible(stage==2);
    m_parse->setVisible(stage==0); m_continue->setVisible(stage==1); m_back->setVisible(stage>0);
    m_commit->setVisible(stage==2 && !m_choice.isEmpty());
    m_stepLabel->setText(QStringLiteral("%1 / 3 · %2").arg(stage+1).arg(stage==0?QStringLiteral("输入商品"):stage==1?QStringLiteral("核对信息"):QStringLiteral("选择去向")));
    m_error->hide();
    if (stage==2) {
        m_summary->setText(QStringLiteral("%1 · %2 · %3").arg(m_name->text().trimmed(),m_amount->value()>0?Transaction::formatAmount(qRound64(m_amount->value()*100)):QStringLiteral("金额待确认"),CategoryCatalog::label(m_category->currentData().toString())));
    }
    if (stage==0) m_text->setFocus(); else if (stage==1) m_name->setFocus();
    QTimer::singleShot(0,this,[this] { m_scroll->verticalScrollBar()->setValue(0); });
}

void DecisionDialog::selectChoice(const QString &choice)
{
    if (m_stage!=2 || m_saved) return;
    m_choice=choice;
    m_hold->setChecked(choice=="hold"); m_wish->setChecked(choice=="wishlist"); m_buy->setChecked(choice=="buy"); m_track->setChecked(choice=="track");
    m_purchaseOptions->setVisible(choice=="buy"); m_wishlistOptions->setVisible(choice=="wishlist");
    m_choiceHint->setText(choice=="buy"?QStringLiteral("记账后会安排 7 天和 30 天购买复盘。"):
        choice=="wishlist"?QStringLiteral("从确认保存时开始冷静期。"):
        choice=="hold"?QStringLiteral("这笔忍住金额会记入许愿池，不产生消费。"):
        QStringLiteral("只记录当前价，不产生消费。"));
    m_commit->setText(choice=="buy"?QStringLiteral("确认购买并记账"):choice=="wishlist"?QStringLiteral("确认加入清单"):choice=="hold"?QStringLiteral("确认这次不买"):QStringLiteral("确认记录价格"));
    m_commit->show(); m_error->hide();
}

bool DecisionDialog::validateDraft()
{
    QString error;
    if (m_name->text().trimmed().isEmpty()) error=QStringLiteral("请先填写商品名称。");
    else if (!m_amount->hasAcceptableInput()) error=QStringLiteral("请输入有效金额，最多两位小数。");
    else if (m_note->toPlainText().trimmed().size()>500) error=QStringLiteral("备注最多 500 个字符。");
    if (!error.isEmpty()) { m_error->setText(error); m_error->show(); return false; }
    m_amount->interpretText(); return true;
}

void DecisionDialog::setInitialText(const QString &text)
{
    if (m_stage==0 && !m_hasDraft && m_text->toPlainText().isEmpty()) m_text->setPlainText(text);
}

void DecisionDialog::setResearchAvailable(bool available)
{
    m_research->setEnabled(available);
    m_research->setText(available?QStringLiteral("还想比较？继续研究 →"):QStringLiteral("执行页已有进行中的决策"));
}

void DecisionDialog::showHandoffError(const QString &message)
{
    m_error->setText(message); m_error->show();
}

void DecisionDialog::setBusy(bool busy)
{
    m_text->setEnabled(!busy); m_parse->setEnabled(!busy && (m_hasDraft || !m_text->toPlainText().trimmed().isEmpty() || !m_image.isEmpty())); m_imageButton->setEnabled(!busy); m_removeImage->setEnabled(!busy); m_manual->setEnabled(!busy); m_settingsButton->setEnabled(!busy); m_draft->setEnabled(!busy);
    for (auto *button : {m_hold,m_wish,m_buy,m_track}) button->setEnabled(!busy && m_stage==2 && !m_saved);
    m_back->setEnabled(!busy); m_continue->setEnabled(!busy); m_commit->setEnabled(!busy && !m_saved);
    m_parse->setText(busy ? QStringLiteral("识别中…") : QStringLiteral("下一步 →"));
}
void DecisionDialog::invalidateDraft()
{
    if (m_service.busy()) return;
    m_productProfileId=0; m_hasDraft=false;
    m_choice.clear(); showStage(0); setBusy(false);
}
void DecisionDialog::showDraft(const ParsedProduct &product)
{
    m_hasDraft=true;
    m_name->setText(product.name); m_amount->setValue(double(product.amountCents)/100);
    m_category->setCurrentIndex(qMax(0,m_category->findData(product.category))); m_note->setPlainText(product.note);
    m_choice.clear(); m_purchaseOptions->hide(); m_wishlistOptions->hide();
    for (auto *button:{m_hold,m_wish,m_buy,m_track}) button->setChecked(false);
    showStage(1); setBusy(false);
    QTimer::singleShot(0,this,[this] { m_scroll->ensureWidgetVisible(m_name,0,24); });
}
void DecisionDialog::refreshBrands()
{
    ShoppingProfileRepository profile(m_database); QList<BrandEntry> brands; const auto selected=m_brand->currentData().toLongLong(); m_brand->clear(); m_brand->addItem(QStringLiteral("未选择品牌"),qint64(0));
    if (!profile.loadBrands(brands,m_category->currentData().toString())) return;
    for (const auto &brand:brands) m_brand->addItem(QStringLiteral("%1 · 权重 %2").arg(brand.name).arg(brand.weight),brand.id);
    m_brand->setCurrentIndex(qMax(0,m_brand->findData(selected)));
}
void DecisionDialog::analyze()
{
    if (m_hasDraft) { showStage(1); return; }
    m_error->hide(); setBusy(true); m_status->setText(QStringLiteral("正在识别，信息会先显示供你确认…"));
    if (!m_service.analyze(m_settings,m_text->toPlainText(),m_image)) setBusy(false);
}
void DecisionDialog::reject() { m_service.cancel(); QDialog::reject(); }

bool DecisionDialog::setScreenshot(const QString &path)
{
    if (m_service.busy() || m_stage!=0) return false;
    QString error;
    QImageReader reader(path); reader.setAutoTransform(true);
    const QSize size=reader.size();
    if (!QFileInfo(path).isFile() || QFileInfo(path).size()>20*1024*1024 || !size.isValid() || qint64(size.width())*size.height()>25000000)
        error=QStringLiteral("请选择不超过 20 MB、2500 万像素的有效图片。");
    QImage image;
    if (error.isEmpty()) { reader.setScaledSize(size.scaled(1600,1600,Qt::KeepAspectRatio)); image=reader.read(); if (image.isNull()) error=QStringLiteral("无法读取这张图片，请换一张截图。"); }
    QByteArray bytes;
    if (error.isEmpty()) {
        QImage canvas(image.size(),QImage::Format_RGB32); canvas.fill(Qt::white); { QPainter p(&canvas); p.drawImage(0,0,image); }
        QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); canvas.save(&buffer,"JPEG",80);
        if (bytes.isEmpty() || bytes.size()>2*1024*1024) error=QStringLiteral("截图压缩后仍过大，请先裁剪商品区域。");
        else image=canvas;
    }
    if (!error.isEmpty()) { m_error->setText(error); m_error->show(); return false; }
    m_image=bytes; m_imagePreview->setPixmap(QPixmap::fromImage(image).scaled(400,72,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    m_imagePreview->show(); m_removeImage->show(); invalidateDraft();
    m_status->setText(QStringLiteral("解析时会将这张截图发送到设置中的 AI 服务。")); return true;
}
void DecisionDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_stage==0 && !m_service.busy() && event->mimeData()->hasUrls() && event->mimeData()->urls().size()==1 && event->mimeData()->urls()[0].isLocalFile()) event->acceptProposedAction();
}
void DecisionDialog::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls() && event->mimeData()->urls().size()==1 && setScreenshot(event->mimeData()->urls()[0].toLocalFile())) event->acceptProposedAction();
}
void DecisionDialog::saveChoice(const QString &choice)
{
    if (m_saved || m_service.busy() || m_stage!=2 || m_choice.isEmpty()) return;
    QString inputError;
    if (!m_amount->hasAcceptableInput()) inputError=QStringLiteral("请输入有效金额，最多两位小数。");
    else if (choice==QStringLiteral("buy") && !m_date->hasAcceptableInput()) inputError=QStringLiteral("请选择有效的购买记账日期。");
    else if (choice==QStringLiteral("wishlist") && !m_days->hasAcceptableInput()) inputError=QStringLiteral("请填写 1 至 30 天的冷静期。");
    if (!inputError.isEmpty()) { m_error->setText(inputError); m_error->show(); return; }
    if (choice==QStringLiteral("buy")) m_date->interpretText();
    if (choice==QStringLiteral("wishlist")) m_days->interpretText();
    m_amount->interpretText(); const qint64 cents=qRound64(m_amount->value()*100);
    const QString name=m_name->text().trimmed(), detail=m_category->currentData().toString(), note=m_note->toPlainText().trimmed();
    Transaction expense(0,name,cents,CategoryCatalog::legacyCategory(detail),choice==QStringLiteral("buy")?m_date->date():QDate::currentDate(),QTime::currentTime(),note,detail,m_image);
    QString error=expense.validationError();
    if (note.size()>500) error=QStringLiteral("理由／备注最多 500 个字符。");
    if (!m_amount->hasAcceptableInput()) error=QStringLiteral("请输入有效金额，最多两位小数。");
    if (!error.isEmpty()) { m_error->setText(error); m_error->show(); return; }
    qint64 id=0; bool ok=false;
    for (auto *button : {m_hold,m_wish,m_buy,m_track}) button->setEnabled(false);
    if (choice==QStringLiteral("hold")) {
        WishPoolRepository repository(m_database); ok=repository.addSaving(cents,name+(note.isEmpty()?QString():QStringLiteral(" · ")+note),m_token); error=repository.lastError();
    } else if (choice==QStringLiteral("wishlist")) {
        WishItem item; item.name=name; item.estimatedCents=cents; item.category=expense.category(); item.detailCategory=detail; item.sourceImage=m_image; item.reason=note; item.coolingDays=m_days->value();
        WishlistRepository repository(m_database); ok=repository.add(item,QDateTime::currentDateTimeUtc(),m_productProfileId); id=item.id; error=repository.lastError();
    } else if (choice==QStringLiteral("buy")) {
        PurchaseRepository repository(m_database); ok=repository.record(expense,m_brand->currentData().toLongLong(),0,m_productProfileId); id=expense.id(); error=repository.lastError();
    } else if (choice==QStringLiteral("track")) {
        PriceRepository repository(m_database); ok=repository.intake(name,detail,cents,m_image.isEmpty()?QStringLiteral("文字输入确认"):QStringLiteral("截图输入确认"),m_token,id,QDateTime::currentDateTimeUtc(),m_productProfileId); error=repository.lastError();
    }
    if (!ok) { m_error->setText(error.isEmpty()?QStringLiteral("保存失败，请重试。"):error); m_error->show(); setBusy(false); return; }
    m_saved=true; QDialog::accept(); emit committed(choice,id);
}
