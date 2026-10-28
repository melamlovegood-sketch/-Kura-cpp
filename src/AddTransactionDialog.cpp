#include "EmptyAmountSpinBox.h"
#include "ModernSelectors.h"
#include "AddTransactionDialog.h"
#include "PricePerspectiveHint.h"
#include "ExpenseClassification.h"
#include "CategoryCatalog.h"
#include "ImagePreview.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTimeEdit>
#include <QVBoxLayout>

AddTransactionDialog::AddTransactionDialog(QWidget *parent,const QString &settingsPath)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("记一笔"));
    setObjectName(QStringLiteral("addTransactionDialog"));
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *form = new QFormLayout;
    m_form=form;
    form->setSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_descriptionEdit = new QLineEdit(this);
    m_descriptionEdit->setObjectName(QStringLiteral("descriptionEdit"));
    m_descriptionEdit->setPlaceholderText(QStringLiteral("例如：午饭、地铁、教材"));
    m_descriptionEdit->setMaxLength(100);
    form->addRow(QStringLiteral("消费名称（选填）"), m_descriptionEdit);

    m_amountSpinBox = new EmptyAmountSpinBox(this);
    m_amountSpinBox->setObjectName(QStringLiteral("amountSpinBox"));
    m_amountSpinBox->setLocale(QLocale(QLocale::Chinese, QLocale::China));
    m_amountSpinBox->setDecimals(2);
    m_amountSpinBox->setRange(0.0, 99999999.99);
    m_amountSpinBox->setSingleStep(1.0);
    form->addRow(QStringLiteral("金额（元）"), m_amountSpinBox);
    auto *costHint=new PricePerspectiveHint(settingsPath,this); costHint->setObjectName("transactionCostHintLabel"); form->addRow(costHint);
    connect(m_amountSpinBox,&QDoubleSpinBox::valueChanged,costHint,[costHint](double amount) { costHint->setAmount(qRound64(amount*100)); });

    m_classification=new ExpenseClassification(settingsPath,this);
    m_categoryComboBox=m_classification->purposeCombo();
    m_categoryComboBox->setObjectName(QStringLiteral("categoryComboBox"));
    form->addRow(m_classification);

    m_dateEdit = new DatePicker(QDate::currentDate(), this);
    m_dateEdit->setObjectName(QStringLiteral("dateEdit"));
    m_dateEdit->setDateRange(QDate(1, 1, 1), QDate(9999, 12, 31));
    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateEdit->setCalendarPopup(true);
    form->addRow(QStringLiteral("日期"), m_dateEdit);
    auto *timeRow = new QWidget(this);
    auto *timeLayout = new QHBoxLayout(timeRow);
    timeLayout->setContentsMargins(0, 0, 0, 0);
    m_timeEdit = new QTimeEdit(QTime::currentTime(), timeRow);
    m_timeEdit->setObjectName(QStringLiteral("timeEdit"));
    m_timeEdit->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    timeLayout->addWidget(m_timeEdit);
    m_timeKnownCheckBox = new QCheckBox(QStringLiteral("补填旧记录时间"), timeRow);
    m_timeKnownCheckBox->setObjectName(QStringLiteral("timeKnownCheckBox"));
    m_timeKnownCheckBox->setChecked(true);
    m_timeKnownCheckBox->hide();
    m_timeKnownCheckBox->setToolTip(QStringLiteral("原记录未保存具体时间；仅在知道实际时间时补填。"));
    connect(m_timeKnownCheckBox, &QCheckBox::toggled, m_timeEdit, &QTimeEdit::setEnabled);
    timeLayout->addWidget(m_timeKnownCheckBox);
    form->addRow(QStringLiteral("具体时间"), timeRow);
    m_noteEdit = new QPlainTextEdit(this);
    m_noteEdit->setObjectName(QStringLiteral("noteEdit"));
    m_noteEdit->setPlaceholderText(QStringLiteral("选填，最多 1000 个字符"));
    m_noteEdit->setMaximumHeight(100);
    m_noteEdit->setTabChangesFocus(true);
    form->addRow(QStringLiteral("备注"), m_noteEdit);
    layout->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("validationErrorLabel"));
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setProperty("error", true);
    m_errorLabel->setMinimumHeight(m_errorLabel->fontMetrics().height());
    layout->addWidget(m_errorLabel);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    m_saveButton = buttons->button(QDialogButtonBox::Save);
    m_saveButton->setText(QStringLiteral("保存"));
    m_saveButton->setObjectName(QStringLiteral("saveButton"));
    m_saveButton->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName(QStringLiteral("cancelButton"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AddTransactionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AddTransactionDialog::reject);
    layout->addWidget(buttons);

    m_descriptionEdit->setFocus();
}

AddTransactionDialog::AddTransactionDialog(const Transaction &transaction, QWidget *parent,const QString &settingsPath)
    : AddTransactionDialog(parent,settingsPath)
{
    m_transactionId = transaction.id();
    m_originalDetail=transaction.detailCategory();
    setWindowTitle(QStringLiteral("修改消费"));
    m_descriptionEdit->setText(transaction.description());
    m_amountSpinBox->setValue(transaction.amount());
    m_classification->setValues(transaction.purposeCategory(),transaction.budgetScope());
    m_sourceImage=transaction.sourceImage();
    if (!m_sourceImage.isEmpty()) qobject_cast<QVBoxLayout *>(layout())->insertWidget(1,screenshotButton(this,m_sourceImage));
    m_dateEdit->setDate(transaction.date());
    m_noteEdit->setPlainText(transaction.note());
    const bool unknownExistingTime = transaction.id() > 0 && !transaction.time().isValid();
    m_timeKnownCheckBox->setVisible(unknownExistingTime);
    m_timeKnownCheckBox->setChecked(!unknownExistingTime);
    m_timeEdit->setTime(transaction.time().isValid() ? transaction.time()
        : (unknownExistingTime ? QTime(0, 0) : QTime::currentTime()));
}

QString AddTransactionDialog::description() const
{
    return m_descriptionEdit->text().trimmed();
}

double AddTransactionDialog::amount() const
{
    return m_amountSpinBox->value();
}

QString AddTransactionDialog::detailCategory() const
{
    return !m_originalDetail.isEmpty() && CategoryCatalog::purposeFromDetail(m_originalDetail)==m_classification->purpose()?m_originalDetail:CategoryCatalog::detailForPurpose(m_classification->purpose());
}

QString AddTransactionDialog::category() const
{
    return CategoryCatalog::legacyCategory(detailCategory());
}

QDate AddTransactionDialog::date() const
{
    return m_dateEdit->date();
}

QTime AddTransactionDialog::time() const
{
    return m_timeKnownCheckBox->isChecked() ? m_timeEdit->time() : QTime();
}

QString AddTransactionDialog::note() const
{
    return m_noteEdit->toPlainText();
}

void AddTransactionDialog::accept()
{
    if (!m_saveButton->isEnabled())
        return;

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
    if (m_timeKnownCheckBox->isChecked()) {
        if (!m_timeEdit->hasAcceptableInput()) {
            showSaveError(QStringLiteral("请输入有效的具体时间（时:分:秒）。"));
            m_timeEdit->setFocus();
            return;
        }
        m_timeEdit->interpretText();
    }
    const QString error = transaction().validationError();
    if (!error.isEmpty()) {
        showSaveError(error);
        return;
    }
    m_errorLabel->clear();
    m_saveButton->setEnabled(false);
    emit saveRequested();
}

Transaction AddTransactionDialog::transaction() const
{
    return Transaction(m_transactionId, description().isEmpty()?QStringLiteral("一笔支出"):description(), qRound64(amount() * 100.0), category(), date(), time(), note(), detailCategory(), m_sourceImage, m_classification->purpose(), m_classification->scope());
}

void AddTransactionDialog::showSaveError(const QString &message)
{
    m_errorLabel->setText(message);
    m_saveButton->setEnabled(true);
}

void AddTransactionDialog::completeSave()
{
    QDialog::accept();
}
