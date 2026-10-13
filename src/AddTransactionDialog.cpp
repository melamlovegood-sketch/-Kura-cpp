#include "AddTransactionDialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

AddTransactionDialog::AddTransactionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("记一笔"));
    setObjectName(QStringLiteral("addTransactionDialog"));
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *form = new QFormLayout;
    form->setSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setObjectName(QStringLiteral("descriptionEdit"));
    m_descriptionEdit->setPlaceholderText(QStringLiteral("例如：午饭、地铁、教材"));
    m_descriptionEdit->setMaxLength(100);
    form->addRow(QStringLiteral("消费名称"), m_descriptionEdit);

    m_amountSpinBox = new QDoubleSpinBox(this);
    m_amountSpinBox->setObjectName(QStringLiteral("amountSpinBox"));
    m_amountSpinBox->setLocale(QLocale(QLocale::Chinese, QLocale::China));
    m_amountSpinBox->setDecimals(2);
    m_amountSpinBox->setRange(0.0, 99999999.99);
    m_amountSpinBox->setSingleStep(1.0);
    m_amountSpinBox->setSuffix(QStringLiteral(" 元"));
    form->addRow(QStringLiteral("金额"), m_amountSpinBox);

    m_categoryComboBox = new QComboBox(this);
    m_categoryComboBox->setObjectName(QStringLiteral("categoryComboBox"));
    m_categoryComboBox->addItems({
        QStringLiteral("餐饮"), QStringLiteral("交通"), QStringLiteral("购物"),
        QStringLiteral("娱乐"), QStringLiteral("学习"), QStringLiteral("其他")
    });
    form->addRow(QStringLiteral("分类"), m_categoryComboBox);

    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setObjectName(QStringLiteral("dateEdit"));
    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateEdit->setCalendarPopup(true);
    form->addRow(QStringLiteral("日期"), m_dateEdit);
    layout->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("validationErrorLabel"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #b42318;"));
    m_errorLabel->setMinimumHeight(m_errorLabel->fontMetrics().height());
    layout->addWidget(m_errorLabel);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Save)->setObjectName(QStringLiteral("saveButton"));
    buttons->button(QDialogButtonBox::Save)->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("cancelButton"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AddTransactionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AddTransactionDialog::reject);
    layout->addWidget(buttons);

    m_descriptionEdit->setFocus();
}

QString AddTransactionDialog::description() const
{
    return m_descriptionEdit->text().trimmed();
}

double AddTransactionDialog::amount() const
{
    return m_amountSpinBox->value();
}

QString AddTransactionDialog::category() const
{
    return m_categoryComboBox->currentText();
}

QDate AddTransactionDialog::date() const
{
    return m_dateEdit->date();
}

void AddTransactionDialog::accept()
{
    if (description().isEmpty()) {
        m_errorLabel->setText(QStringLiteral("请填写消费名称。"));
        m_descriptionEdit->setFocus();
        return;
    }

    if (!m_amountSpinBox->hasAcceptableInput()) {
        m_errorLabel->setText(QStringLiteral("请输入有效金额，最多保留两位小数。"));
        m_amountSpinBox->setFocus();
        return;
    }

    // Commit any text still being edited before reading the numeric value.
    m_amountSpinBox->interpretText();
    if (amount() <= 0.0) {
        m_errorLabel->setText(QStringLiteral("金额必须大于 0 元。"));
        m_amountSpinBox->setFocus();
        m_amountSpinBox->selectAll();
        return;
    }

    if (!m_dateEdit->hasAcceptableInput() || !date().isValid()) {
        m_errorLabel->setText(QStringLiteral("请选择有效的消费日期。"));
        m_dateEdit->setFocus();
        return;
    }

    m_dateEdit->interpretText();
    m_errorLabel->clear();
    QDialog::accept();
}
