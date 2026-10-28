#include "WishDialog.h"
#include "CategoryCatalog.h"
#include "ModernSelectors.h"
#include <QHBoxLayout>
#include <QStyle>
#include "ImagePreview.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

WishDialog::WishDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("wishDialog"));
    setWindowTitle(QStringLiteral("加入清单"));
    resize(540,450);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24,24,24,24); layout->setSpacing(15);
    auto *form=new QFormLayout; form->setRowWrapPolicy(QFormLayout::DontWrapRows); form->setVerticalSpacing(9);
    m_name=new QLineEdit(this); m_name->setObjectName("wishNameEdit"); m_name->setMaxLength(100);
    m_name->setPlaceholderText(QStringLiteral("例如：一双球鞋")); form->addRow(QStringLiteral("想买什么"),m_name);
    m_amount=new QDoubleSpinBox(this); m_amount->setObjectName("wishAmountSpinBox");
    m_amount->setLocale(QLocale(QLocale::Chinese,QLocale::China)); m_amount->setDecimals(2);
    m_amount->setRange(0,Transaction::MaximumAmountCents/100.0); m_amount->setPrefix(QStringLiteral("¥ "));
    m_amount->setButtonSymbols(QAbstractSpinBox::NoButtons); m_amount->setSpecialValueText(QStringLiteral("填写预计价格"));
    form->addRow(QStringLiteral("预计价格"),m_amount); layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("给自己多久考虑"),this));
    auto *cooling=new QHBoxLayout; cooling->setSpacing(8);
    m_days=new QSpinBox(this); m_days->setObjectName("wishDaysSpinBox"); m_days->setRange(1,30); m_days->setValue(3);
    m_days->setSuffix(QStringLiteral(" 天")); m_days->setButtonSymbols(QAbstractSpinBox::NoButtons); m_days->setFixedWidth(85); m_days->hide();
    auto *custom=new QPushButton(QStringLiteral("自定义"),this); custom->setObjectName("wishCoolingCustomButton"); custom->setCheckable(true); custom->setProperty("coolingPreset",true);
    for(int days:{3,7,14}) {
        auto *preset=new QPushButton(QStringLiteral("%1 天").arg(days),this); preset->setObjectName(QStringLiteral("wishCoolingPreset%1").arg(days));
        preset->setProperty("coolingPreset",true); preset->setCheckable(true); preset->setChecked(days==3); cooling->addWidget(preset);
        connect(preset,&QPushButton::clicked,this,[this,custom,preset,days] { m_customCooling=false; m_days->setValue(days); preset->setChecked(true); custom->setChecked(false); m_days->hide(); });
        connect(m_days,&QSpinBox::valueChanged,preset,[this,preset,days](int value) { preset->setChecked(!m_customCooling && value==days); });
    }
    cooling->addWidget(custom); cooling->addStretch(); cooling->addWidget(m_days); layout->addLayout(cooling);
    connect(custom,&QPushButton::clicked,this,[this,custom] { m_customCooling=true; custom->setChecked(true); for(int days:{3,7,14}) findChild<QPushButton *>(QStringLiteral("wishCoolingPreset%1").arg(days))->setChecked(false); m_days->show(); m_days->setFocus(); m_days->selectAll(); });
    connect(m_days,&QSpinBox::valueChanged,this,[this,custom](int value) { m_customCooling=m_customCooling || (value!=3 && value!=7 && value!=14); custom->setChecked(m_customCooling); m_days->setVisible(m_customCooling); });
    auto *more=new QPushButton(QStringLiteral("更多选项"),this); more->setObjectName("wishMoreOptionsButton"); more->setProperty("link",true); more->setCheckable(true); layout->addWidget(more,0,Qt::AlignLeft);
    auto *options=new QWidget(this); options->setObjectName("wishMoreOptions"); auto *optionForm=new QFormLayout(options); optionForm->setContentsMargins(0,0,0,0); optionForm->setSpacing(10);
    m_category=new ModernComboBox(options); m_category->setObjectName("wishCategoryComboBox");
    for(const auto &key:CategoryCatalog::keys()) m_category->addItem(CategoryCatalog::label(key),key);
    m_category->setCurrentIndex(m_category->findData("online_shopping"));
    m_priority=new ModernComboBox(options); m_priority->setObjectName("wishPriorityComboBox"); m_priority->addItems({QStringLiteral("高"),QStringLiteral("中"),QStringLiteral("低")}); m_priority->setCurrentIndex(1);
    m_reason=new QLineEdit(options); m_reason->setObjectName("wishReasonEdit"); m_reason->setMaxLength(500); m_reason->setPlaceholderText(QStringLiteral("为什么想买？选填"));
    optionForm->addRow(QStringLiteral("分类"),m_category); optionForm->addRow(QStringLiteral("优先级"),m_priority); optionForm->addRow(QStringLiteral("购买理由"),m_reason);
    options->hide(); layout->addWidget(options);
    connect(more,&QPushButton::toggled,this,[options,more](bool open) { options->setVisible(open); more->setText(open?QStringLiteral("收起选项"):QStringLiteral("更多选项")); });
    connect(more,&QPushButton::toggled,this,[this,options](bool open) { if(open) QTimer::singleShot(0,this,[this,options] { if(auto *scroll=findChild<QScrollArea *>("appDialogScroll")) scroll->ensureWidgetVisible(options,0,8); }); });
    auto *hint=new QLabel(this); hint->setObjectName("wishCoolingHint"); hint->setWordWrap(true); hint->setProperty("dialogCaption",true);
    const auto updateHint=[this,hint] { hint->setText(m_original.id>0?QStringLiteral("保存修改会重新开始冷静期。"):QStringLiteral("保存后开始 %1 天冷静期，到期再决定是否购买。").arg(m_days->value())); m_days->setToolTip(hint->text()); };
    connect(m_days,&QSpinBox::valueChanged,this,updateHint); updateHint(); layout->addWidget(hint);
    hint->hide();
    m_error=new QLabel(this); m_error->setObjectName("wishValidationError"); m_error->setTextFormat(Qt::PlainText); m_error->setWordWrap(true); m_error->setProperty("error",true); m_error->hide(); layout->insertWidget(1,m_error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this);
    m_save=buttons->button(QDialogButtonBox::Save); m_save->setObjectName("saveWishButton"); m_save->setText(QStringLiteral("加入清单")); m_save->setProperty("primary",true); m_save->setDefault(true);
    auto *cancel=buttons->button(QDialogButtonBox::Cancel); cancel->setObjectName("cancelWishButton"); cancel->setText(QStringLiteral("取消"));
    connect(buttons,&QDialogButtonBox::accepted,this,&WishDialog::accept); connect(buttons,&QDialogButtonBox::rejected,this,&WishDialog::reject); layout->addWidget(buttons);
    m_name->setFocus();
    connect(m_name,&QLineEdit::textChanged,this,[this] { m_error->clear(); m_error->hide(); m_name->setProperty("fieldError",false); m_name->style()->unpolish(m_name); m_name->style()->polish(m_name); });
    connect(m_amount,&QDoubleSpinBox::valueChanged,this,[this] { m_error->clear(); m_error->hide(); m_amount->setProperty("fieldError",false); m_amount->style()->unpolish(m_amount); m_amount->style()->polish(m_amount); });
}

WishDialog::WishDialog(const WishItem &wish, QWidget *parent) : WishDialog(parent)
{
    m_original = wish;
    setWindowTitle(wish.id>0?QStringLiteral("修改清单商品"):QStringLiteral("加入清单"));
    m_save->setText(wish.id>0?QStringLiteral("保存修改"):QStringLiteral("加入清单"));
    m_name->setText(wish.name);
    m_amount->setValue(wish.estimatedCents / 100.0);
    m_category->setCurrentIndex(m_category->findData(wish.detailCategory.isEmpty()?CategoryCatalog::fromLegacy(wish.category):wish.detailCategory));
    if (!wish.sourceImage.isEmpty()) qobject_cast<QVBoxLayout *>(layout())->insertWidget(1,screenshotButton(this,wish.sourceImage));
    m_days->setValue(wish.coolingDays);
    m_priority->setCurrentIndex(wish.priority - 1);
    m_reason->setText(wish.reason);
    findChild<QLabel *>(QStringLiteral("wishCoolingHint"))->setText(
        QStringLiteral("保存修改会重新开始冷静期。"));
    if(wish.id>0) { auto *hint=findChild<QLabel *>("wishCoolingHint"); qobject_cast<QVBoxLayout *>(layout())->insertWidget(0,hint); hint->show(); }
}

WishItem WishDialog::item() const
{
    WishItem wish = m_original;
    wish.name = m_name->text().trimmed();
    wish.estimatedCents = qRound64(m_amount->value() * 100.0);
    wish.detailCategory=m_category->currentData().toString();
    wish.category = CategoryCatalog::legacyCategory(wish.detailCategory);
    wish.coolingDays = m_days->value();
    wish.priority = m_priority->currentIndex() + 1;
    wish.reason = m_reason->text().trimmed();
    return wish;
}

void WishDialog::accept()
{
    if (!m_save->isEnabled())
        return;
    if (!m_amount->hasAcceptableInput() || !m_days->hasAcceptableInput()) {
        showSaveError(QStringLiteral("请输入有效价格和冷静期天数。"));
        return;
    }
    m_amount->interpretText();
    m_days->interpretText();
    const QString error = item().inputError();
    if (!error.isEmpty()) {
        showSaveError(error);
        return;
    }
    m_error->clear();
    m_save->setEnabled(false);
    emit saveRequested();
}

void WishDialog::showSaveError(const QString &message)
{
    m_error->setText(message); m_error->show();
    QWidget *invalid=m_name->text().trimmed().isEmpty()?static_cast<QWidget *>(m_name):m_amount->value()<=0?static_cast<QWidget *>(m_amount):nullptr;
    if(invalid) { invalid->setProperty("fieldError",true); invalid->style()->unpolish(invalid); invalid->style()->polish(invalid); invalid->setFocus(); }
    m_save->setEnabled(true);
}

void WishDialog::completeSave() { QDialog::accept(); }
