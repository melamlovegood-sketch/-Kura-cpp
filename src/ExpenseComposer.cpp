#include "ExpenseComposer.h"
#include "ModernSelectors.h"
#include "ShellWidgets.h"
#include "CategoryCatalog.h"
#include "TransactionRepository.h"
#include <QBuffer>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>

namespace {
class EntryStack:public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;
    QSize sizeHint() const override { return currentWidget()?currentWidget()->sizeHint():QSize(); }
    QSize minimumSizeHint() const override { return currentWidget()?currentWidget()->minimumSizeHint():QSize(); }
};
QDoubleSpinBox *amountField(QWidget *parent,const char *name)
{
    auto *field=new QDoubleSpinBox(parent); field->setObjectName(QLatin1String(name)); field->setRange(0,99999999.99); field->setDecimals(2); field->setButtonSymbols(QAbstractSpinBox::NoButtons); field->setPrefix(QStringLiteral("¥ ")); field->setMinimumHeight(36); return field;
}
void fillCategories(ModernComboBox *combo) { for (const auto &key:CategoryCatalog::keys()) combo->addItem(CategoryCatalog::label(key)+(CategoryCatalog::isBasic(key)?QStringLiteral(" · 基础生活"):QStringLiteral(" · 可支配")),key); combo->setCurrentIndex(combo->findData("daily")); }
}
ExpenseComposer::ExpenseComposer(DatabaseManager &database,const QString &settingsPath,QWidget *parent):QWidget(parent),m_database(database),m_settingsPath(settingsPath),m_ai(this)
{
    setObjectName("expenseComposer"); setAttribute(Qt::WA_StyledBackground); setProperty("billingCard",true); auto *layout=new QVBoxLayout(this); layout->setContentsMargins(22,18,22,16); layout->setSpacing(10);
    auto *heading=new QHBoxLayout; auto *title=ShellWidgets::label(QStringLiteral("记下这一笔"),this); title->setObjectName("bookkeepingTitle"); heading->addWidget(title,1);
    m_aiMode=new QPushButton(QStringLiteral("AI 记账"),this); m_aiMode->setObjectName("bookkeepingAiModeButton"); m_manualMode=new QPushButton(QStringLiteral("手动"),this); m_manualMode->setObjectName("bookkeepingManualModeButton"); for (auto *button:{m_aiMode,m_manualMode}) { button->setCheckable(true); button->setProperty("billingSegment",true); heading->addWidget(button); } m_aiMode->setChecked(true); layout->addLayout(heading);
    m_modes=new EntryStack(this); m_modes->setObjectName("bookkeepingEntryModes"); auto *aiPage=new QWidget; auto *aiLayout=new QVBoxLayout(aiPage); aiLayout->setContentsMargins(0,0,0,0); aiLayout->setSpacing(9);
    m_text=new QPlainTextEdit(aiPage); m_text->setObjectName("bookkeepingInput"); m_text->setPlaceholderText(QStringLiteral("说一句，如「中午食堂 15 块」，或上传支付截图…")); m_text->setFixedHeight(72); m_text->setTabChangesFocus(true); aiLayout->addWidget(m_text);
    m_examples=new QWidget(aiPage); m_examples->setObjectName("bookkeepingExamples"); auto *examples=new QHBoxLayout(m_examples); examples->setContentsMargins(0,0,0,0); examples->addWidget(ShellWidgets::label(QStringLiteral("试一句"),aiPage,"muted")); for (const auto &text:{QStringLiteral("食堂午饭 15 元"),QStringLiteral("地铁 6 元")}) { auto *example=new QPushButton(text,m_examples); example->setProperty("billingExample",true); examples->addWidget(example); connect(example,&QPushButton::clicked,this,[this,text] { m_text->setPlainText(text); m_text->setFocus(); }); } examples->addStretch(); aiLayout->addWidget(m_examples);
    m_file=ShellWidgets::label(QString(),aiPage,"muted"); m_file->setObjectName("bookkeepingScreenshotLabel"); m_file->hide(); aiLayout->addWidget(m_file);
    m_confirmation=new QFrame(aiPage); m_confirmation->setObjectName("bookkeepingConfirmation"); m_confirmation->setProperty("billingConfirmation",true); auto *confirmation=new QVBoxLayout(m_confirmation); confirmation->setContentsMargins(0,14,0,0); confirmation->setSpacing(10); confirmation->addWidget(ShellWidgets::label(QStringLiteral("✓ 确认一下，再保存"),m_confirmation,"secondary"));
    auto *fields=new QHBoxLayout; m_aiName=new QLineEdit(m_confirmation); m_aiName->setObjectName("bookkeepingDraftNameEdit"); m_aiName->setMaxLength(100); m_aiAmount=amountField(m_confirmation,"bookkeepingDraftAmountSpinBox"); m_aiAmount->setMaximumWidth(155); fields->addWidget(m_aiName,1); fields->addWidget(m_aiAmount); confirmation->addLayout(fields);
    m_aiCategory=new ModernComboBox(m_confirmation); m_aiCategory->setObjectName("bookkeepingDraftCategoryComboBox"); fillCategories(m_aiCategory); confirmation->addWidget(m_aiCategory);
    m_aiNote=new QPlainTextEdit(m_confirmation); m_aiNote->setObjectName("bookkeepingDraftNoteEdit"); m_aiNote->setPlaceholderText(QStringLiteral("可选备注，最多 1000 字")); m_aiNote->setFixedHeight(65); m_aiNote->hide(); auto *noteToggle=new QPushButton(QStringLiteral("备注 ▾"),m_confirmation); noteToggle->setProperty("billingQuiet",true); confirmation->addWidget(noteToggle,0,Qt::AlignLeft); confirmation->addWidget(m_aiNote); connect(noteToggle,&QPushButton::clicked,m_aiNote,[this] { m_aiNote->setVisible(m_aiNote->isHidden()); });
    m_aiSave=new QPushButton(QStringLiteral("保存这笔"),this); m_aiSave->setObjectName("saveBookkeepingDraftButton"); m_aiSave->setProperty("primary",true);  aiLayout->addWidget(m_confirmation); m_confirmation->hide();
    auto *manualPage=new QWidget; auto *manualLayout=new QVBoxLayout(manualPage); manualLayout->setContentsMargins(0,0,0,0); manualLayout->setSpacing(12); auto *manualFields=new QHBoxLayout;
    m_name=new QLineEdit(manualPage); m_name->setObjectName("bookkeepingNameEdit"); m_name->setPlaceholderText(QStringLiteral("消费名称，比如：午饭")); m_name->setMaxLength(100); m_amount=amountField(manualPage,"bookkeepingAmountSpinBox"); m_amount->setMaximumWidth(180); manualFields->addWidget(m_name,1); manualFields->addWidget(m_amount); manualLayout->addLayout(manualFields);
    m_category=new ModernComboBox(manualPage); m_category->setObjectName("bookkeepingCategoryComboBox"); fillCategories(m_category); manualLayout->addWidget(m_category);
    m_manualNote=new QPlainTextEdit(manualPage); m_manualNote->setObjectName("bookkeepingNoteEdit"); m_manualNote->setPlaceholderText(QStringLiteral("可选备注，最多 1000 字")); m_manualNote->setFixedHeight(65); m_manualNote->hide(); auto *manualNoteToggle=new QPushButton(QStringLiteral("添加备注 ▾"),manualPage); manualNoteToggle->setProperty("billingQuiet",true); manualLayout->addWidget(manualNoteToggle,0,Qt::AlignLeft); manualLayout->addWidget(m_manualNote); connect(manualNoteToggle,&QPushButton::clicked,m_manualNote,[this] { m_manualNote->setVisible(m_manualNote->isHidden()); }); manualLayout->addStretch(); m_modes->addWidget(aiPage); m_modes->addWidget(manualPage); layout->addWidget(m_modes);
    auto *actions=new QHBoxLayout; m_upload=new QPushButton(QStringLiteral("上传截图"),this); m_upload->setObjectName("bookkeepingUploadButton"); m_upload->setProperty("billingQuiet",true); actions->addWidget(m_upload);
    m_date=new DatePicker(QDate::currentDate(),this); m_date->setObjectName("bookkeepingDateEdit"); m_date->setToolTip(QStringLiteral("非今日的补记不自动补造具体时间，可在详情中编辑。")); m_date->setMaximumWidth(145); actions->addWidget(m_date); actions->addStretch(); m_cancel=new QPushButton(QStringLiteral("取消等待"),this); m_cancel->setObjectName("cancelBookkeepingAiButton"); m_cancel->setProperty("billingQuiet",true); actions->addWidget(m_cancel);
    m_parse=new QPushButton(QStringLiteral("AI 解析 →"),this); m_parse->setObjectName("parseBookkeepingButton"); m_parse->setProperty("primary",true); m_manualSave=new QPushButton(QStringLiteral("保存这笔"),this); m_manualSave->setObjectName("saveBookkeepingManualButton"); m_manualSave->setProperty("primary",true); actions->addWidget(m_parse); actions->addWidget(m_aiSave); actions->addWidget(m_manualSave); layout->addLayout(actions);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("bookkeepingErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    connect(m_aiMode,&QPushButton::clicked,this,[this] { setMode(false); }); connect(m_manualMode,&QPushButton::clicked,this,[this] { setMode(true); }); connect(m_parse,&QPushButton::clicked,this,&ExpenseComposer::analyze); connect(m_aiSave,&QPushButton::clicked,this,[this] { save(true); }); connect(m_manualSave,&QPushButton::clicked,this,[this] { save(false); }); connect(m_upload,&QPushButton::clicked,this,[this] { const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("选择支付截图"),{},QStringLiteral("图片 (*.png *.jpg *.jpeg *.webp *.bmp)")); if (!path.isEmpty()) setScreenshot(path); });
    connect(m_cancel,&QPushButton::clicked,this,[this] { m_ai.cancel(); m_busy=false; m_error->setText(QStringLiteral("已取消等待，输入和截图已保留。")); m_error->show(); updateControls(); });
    connect(m_text,&QPlainTextEdit::textChanged,this,[this] { if (!m_busy) { m_hasAiDraft=false; m_confirmation->hide(); } updateControls(); }); connect(m_name,&QLineEdit::textChanged,this,&ExpenseComposer::updateControls); connect(m_amount,&QDoubleSpinBox::valueChanged,this,&ExpenseComposer::updateControls);
    connect(&m_ai,&AIService::parsed,this,[this](const ParsedProduct &product) { m_busy=false; m_hasAiDraft=true; m_aiName->setText(product.name); m_aiAmount->setValue(double(product.amountCents)/100); m_aiCategory->setCurrentIndex(m_aiCategory->findData(product.category)); m_aiNote->setPlainText(product.note); m_aiNote->setVisible(!product.note.isEmpty()); m_confirmation->show(); m_error->hide(); updateControls(); });
    connect(&m_ai,&AIService::failed,this,[this](const QString &message) { m_busy=false; m_error->setText(message); m_error->show(); updateControls(); });
    auto *shortcut=new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_Return),this); shortcut->setContext(Qt::WidgetWithChildrenShortcut); connect(shortcut,&QShortcut::activated,this,[this] { if (m_modes->currentIndex()==0 && m_hasAiDraft && m_aiSave->isEnabled()) save(true); else if (m_modes->currentIndex()==0 && m_parse->isEnabled()) analyze(); else if (m_modes->currentIndex()==1 && m_manualSave->isEnabled()) save(false); }); setFocusProxy(m_text); updateControls();
}
void ExpenseComposer::setShowExamples(bool show) { m_showExamples=show; updateControls(); }
void ExpenseComposer::setDatabaseAvailable(bool available) { m_available=available; if (!available) { m_ai.cancel(); m_busy=false; } updateControls(); }
bool ExpenseComposer::hasDraft() const { return !m_text->toPlainText().trimmed().isEmpty() || !m_image.isEmpty() || !m_name->text().trimmed().isEmpty() || m_amount->value()>0 || m_hasAiDraft; }
void ExpenseComposer::suggestDate(const QDate &date) { if (date.isValid() && !hasDraft()) m_date->setDate(date); }
void ExpenseComposer::setMode(bool manual) { if (m_busy) return; setFocusProxy(manual?static_cast<QWidget *>(m_name):m_text); m_modes->setCurrentIndex(manual?1:0); m_aiMode->setChecked(!manual); m_manualMode->setChecked(manual); m_error->hide(); updateControls(); }
void ExpenseComposer::updateControls()
{
    const bool enabled=m_available && !m_busy && !m_saving; m_aiMode->setEnabled(enabled); m_manualMode->setEnabled(enabled); m_text->setEnabled(enabled); m_name->setEnabled(enabled); m_amount->setEnabled(enabled); m_category->setEnabled(enabled); m_date->setEnabled(enabled); m_confirmation->setEnabled(enabled); m_upload->setVisible(m_modes->currentIndex()==0); m_upload->setEnabled(enabled); m_parse->setVisible(m_modes->currentIndex()==0 && !m_hasAiDraft); m_aiSave->setVisible(m_modes->currentIndex()==0 && m_hasAiDraft); m_examples->setVisible(m_showExamples && m_text->toPlainText().trimmed().isEmpty() && m_image.isEmpty() && !m_hasAiDraft); m_parse->setEnabled(enabled && (!m_text->toPlainText().trimmed().isEmpty() || !m_image.isEmpty())); m_manualSave->setVisible(m_modes->currentIndex()==1); m_manualSave->setEnabled(enabled && !m_name->text().trimmed().isEmpty() && m_amount->value()>0); m_aiSave->setEnabled(enabled && m_hasAiDraft); m_cancel->setVisible(m_busy); m_parse->setText(m_busy?QStringLiteral("解析中…"):QStringLiteral("识别这一笔 →")); m_modes->updateGeometry();
}
void ExpenseComposer::analyze()
{
    if (!m_parse->isEnabled()) return; m_busy=true; m_error->hide(); updateControls(); if (!m_ai.analyze(AISettingsStore::load(m_settingsPath),m_text->toPlainText(),m_image)) { m_busy=false; updateControls(); }
}
bool ExpenseComposer::setScreenshot(const QString &path)
{
    if (!m_available || m_busy) return false; QImageReader reader(path); reader.setAutoTransform(true); const auto size=reader.size(); QString error;
    if (!QFileInfo(path).isFile() || QFileInfo(path).size()>20*1024*1024 || !size.isValid() || qint64(size.width())*size.height()>25000000) error=QStringLiteral("请选择不超过 20 MB、2500 万像素的图片。");
    QByteArray bytes; if (error.isEmpty()) { reader.setScaledSize(size.scaled(1600,1600,Qt::KeepAspectRatio)); const auto image=reader.read(); if (image.isNull()) error=QStringLiteral("无法读取这张截图。"); else { QImage canvas(image.size(),QImage::Format_RGB32); canvas.fill(Qt::white); { QPainter painter(&canvas); painter.drawImage(0,0,image); } QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); canvas.save(&buffer,"JPEG",80); if (bytes.isEmpty() || bytes.size()>2*1024*1024) error=QStringLiteral("截图压缩后仍过大，请裁剪商品区域。"); } }
    if (!error.isEmpty()) { m_error->setText(error); m_error->show(); return false; } m_image=bytes; m_file->setText(QStringLiteral("已选择：%1 · 解析时发送给你设置的 AI 服务").arg(QFileInfo(path).fileName())); m_file->show(); m_hasAiDraft=false; m_confirmation->hide(); updateControls(); return true;
}
void ExpenseComposer::save(bool ai)
{
    if (!m_available || m_busy || m_saving || (ai && !m_hasAiDraft)) return; auto *amount=ai?m_aiAmount:m_amount; if (!amount->hasAcceptableInput()) { m_error->setText(QStringLiteral("请输入有效金额，最多两位小数。")); m_error->show(); return; } amount->interpretText();
    const auto category=(ai?m_aiCategory:m_category)->currentData().toString(); const auto date=m_date->date(); const QString name=(ai?m_aiName:m_name)->text().trimmed(),note=(ai?m_aiNote:m_manualNote)->toPlainText(); const auto time=date==QDate::currentDate()?QTime::currentTime():QTime();
    Transaction record(0,name,qRound64(amount->value()*100),CategoryCatalog::legacyCategory(category),date,time,note,category,m_image); const auto validation=record.validationError(); if (!validation.isEmpty()) { m_error->setText(validation); m_error->show(); return; }
    m_saving=true; updateControls(); TransactionRepository repository(m_database); if (!repository.add(record)) { m_saving=false; m_error->setText(repository.lastError()); m_error->show(); updateControls(); return; }
    m_saving=false; clearDraft(); emit expenseSaved(record.id(),record.date());
}
void ExpenseComposer::clearDraft() { m_hasAiDraft=false; m_text->clear(); m_name->clear(); m_amount->setValue(0); m_manualNote->clear(); m_aiNote->clear(); m_image.clear(); m_file->clear(); m_file->hide(); m_confirmation->hide(); m_manualNote->hide(); m_aiNote->hide(); m_error->hide(); updateControls(); }
void ExpenseComposer::resetAfterRestore() { m_ai.cancel(); m_busy=false; clearDraft(); }
