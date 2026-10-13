#include "WishDialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

WishDialog::WishDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("wishDialog"));
    setWindowTitle(QStringLiteral("加入愿望清单"));
    setMinimumWidth(460);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *form = new QFormLayout;
    form->setSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_name = new QLineEdit(this);
    m_name->setObjectName(QStringLiteral("wishNameEdit"));
    m_name->setMaxLength(100);
    m_name->setPlaceholderText(QStringLiteral("例如：耳机、键盘、书"));
    form->addRow(QStringLiteral("想买什么"), m_name);
    m_amount = new QDoubleSpinBox(this);
    m_amount->setObjectName(QStringLiteral("wishAmountSpinBox"));
    m_amount->setLocale(QLocale(QLocale::Chinese, QLocale::China));
    m_amount->setDecimals(2);
    m_amount->setRange(0, Transaction::MaximumAmountCents / 100.0);
    m_amount->setSuffix(QStringLiteral(" 元"));
    form->addRow(QStringLiteral("预计价格"), m_amount);
    m_category = new QComboBox(this);
    m_category->setObjectName(QStringLiteral("wishCategoryComboBox"));
    m_category->addItems(Transaction::categories());
    m_category->setCurrentText(QStringLiteral("购物"));
    form->addRow(QStringLiteral("分类"), m_category);
    m_priority = new QComboBox(this);
    m_priority->setObjectName(QStringLiteral("wishPriorityComboBox"));
    m_priority->addItems({QStringLiteral("高"), QStringLiteral("中"), QStringLiteral("低")});
    m_priority->setCurrentIndex(1);
    form->addRow(QStringLiteral("优先级"), m_priority);
    m_days = new QSpinBox(this);
    m_days->setObjectName(QStringLiteral("wishDaysSpinBox"));
    m_days->setRange(1, 30);
    m_days->setValue(3);
    m_days->setSuffix(QStringLiteral(" 天"));
    form->addRow(QStringLiteral("冷静期"), m_days);
    m_reason = new QLineEdit(this);
    m_reason->setObjectName(QStringLiteral("wishReasonEdit"));
    m_reason->setMaxLength(500);
    m_reason->setPlaceholderText(QStringLiteral("可选：为什么想买，是否真的需要"));
    form->addRow(QStringLiteral("购买理由"), m_reason);
    layout->addLayout(form);
    auto *hint = new QLabel(QStringLiteral("保存后开始等待，每天按 24 小时计算。冷静期结束后可以购买；等待期间也可以放弃。"), this);
    hint->setObjectName(QStringLiteral("wishCoolingHint"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("wishValidationError"));
    m_error->setTextFormat(Qt::PlainText);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("color: #b42318;"));
    layout->addWidget(m_error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    m_save = buttons->button(QDialogButtonBox::Save);
    m_save->setObjectName(QStringLiteral("saveWishButton"));
    m_save->setText(QStringLiteral("保存"));
    m_save->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("cancelWishButton"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, &WishDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &WishDialog::reject);
    layout->addWidget(buttons);
}

WishDialog::WishDialog(const WishItem &wish, QWidget *parent) : WishDialog(parent)
{
    m_original = wish;
    setWindowTitle(QStringLiteral("修改愿望"));
    m_name->setText(wish.name);
    m_amount->setValue(wish.estimatedCents / 100.0);
    m_category->setCurrentText(wish.category);
    m_days->setValue(wish.coolingDays);
    m_priority->setCurrentIndex(wish.priority - 1);
    m_reason->setText(wish.reason);
    findChild<QLabel *>(QStringLiteral("wishCoolingHint"))->setText(
        QStringLiteral("修改并保存会从现在重新开始冷静期，原来的创建时间保留。"));
}

WishItem WishDialog::item() const
{
    WishItem wish = m_original;
    wish.name = m_name->text().trimmed();
    wish.estimatedCents = qRound64(m_amount->value() * 100.0);
    wish.category = m_category->currentText();
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
    m_error->setText(message);
    m_save->setEnabled(true);
}

void WishDialog::completeSave() { QDialog::accept(); }
