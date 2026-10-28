#include "EmptyAmountSpinBox.h"
#include "ModernSelectors.h"
#include "AppTheme.h"
#include "BudgetWidget.h"
#include "Transaction.h"

#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

BudgetWidget::BudgetWidget(DatabaseManager &database, QWidget *parent)
    : QWidget(parent), m_repository(database)
{
    setObjectName(QStringLiteral("monthlyBudgetWidget"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *group = new QGroupBox(QStringLiteral("月度预算"), this);
    auto *layout = new QVBoxLayout(group);
    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("月份"), group));
    m_monthEdit = new MonthPicker(MonthlyBudget::firstDay(QDate::currentDate()), group);
    m_monthEdit->setObjectName(QStringLiteral("budgetMonthEdit"));
    m_monthEdit->setDateRange(QDate(1, 1, 1), QDate(9999, 12, 31));
    m_monthEdit->setDisplayFormat(QStringLiteral("yyyy-MM"));
    m_monthEdit->setCalendarPopup(true);
    controls->addWidget(m_monthEdit);
    controls->addStretch();
    auto *amounts = new QHBoxLayout;
    amounts->addWidget(new QLabel(QStringLiteral("必要生活预算"), group));
    m_basicAmountEdit = new EmptyAmountSpinBox(group,QStringLiteral("0.00（元）"));
    m_basicAmountEdit->setObjectName(QStringLiteral("basicBudgetAmountSpinBox"));
    m_basicAmountEdit->setToolTip(QStringLiteral("为基本吃饭、通勤、日用品等预留；例如每月 1000 元。"));
    m_basicAmountEdit->setDecimals(2); m_basicAmountEdit->setRange(0.0, 99999999.99);
    m_basicAmountEdit->setSingleStep(100.0);
    amounts->addWidget(m_basicAmountEdit,1);
    amounts->addWidget(new QLabel(QStringLiteral("自由消费预算"), group));
    m_amountEdit = new EmptyAmountSpinBox(group,QStringLiteral("0.00（元）"));
    m_amountEdit->setObjectName(QStringLiteral("budgetAmountSpinBox"));
    m_amountEdit->setToolTip(QStringLiteral("在必要生活之外，自己决定如何花的钱，也可以留着不花。两项预算分别计算，不自动挪用。"));
    m_amountEdit->setLocale(QLocale(QLocale::Chinese, QLocale::China));
    m_amountEdit->setDecimals(2);
    m_amountEdit->setRange(0.0, static_cast<double>(MonthlyBudget::MaximumLimitCents) / 100.0);
    m_amountEdit->setSingleStep(100.0);
    amounts->addWidget(m_amountEdit,1);
    m_saveButton = new QPushButton(QStringLiteral("保存预算"), group);
    m_saveButton->setObjectName(QStringLiteral("saveBudgetButton"));
    controls->addWidget(m_saveButton);
    layout->addLayout(controls);
    layout->addLayout(amounts);
    m_splitLabel = new QLabel(group); m_splitLabel->setWordWrap(true); m_splitLabel->setProperty("muted",true); m_splitLabel->setObjectName("budgetSplitLabel");
    layout->addWidget(m_splitLabel);

    auto *figures = new QHBoxLayout;
    m_limitLabel = new QLabel(QStringLiteral("预算：—"), group);
    m_limitLabel->setObjectName(QStringLiteral("budgetLimitLabel"));
    m_spentLabel = new QLabel(QStringLiteral("已消费：—"), group);
    m_spentLabel->setObjectName(QStringLiteral("budgetSpentLabel"));
    m_remainingLabel = new QLabel(QStringLiteral("剩余额度：—"), group);
    m_remainingLabel->setObjectName(QStringLiteral("budgetRemainingLabel"));
    figures->addWidget(m_limitLabel);
    figures->addWidget(m_spentLabel);
    figures->addWidget(m_remainingLabel);
    layout->addLayout(figures);
    m_statusLabel = new QLabel(QStringLiteral("预算暂不可用。"), group);
    m_statusLabel->setObjectName(QStringLiteral("budgetStatusLabel"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);
    m_errorLabel = new QLabel(group);
    m_errorLabel->setObjectName(QStringLiteral("budgetErrorLabel"));
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setProperty("error", true);
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);
    outer->addWidget(group);

    connect(m_monthEdit, &QDateEdit::dateChanged, this, [this] {
        loadSelectedMonth(true);
    });
    connect(m_saveButton, &QPushButton::clicked, this, &BudgetWidget::saveBudget);
    updateControls();
}

void BudgetWidget::setDatabaseAvailable(bool available)
{
    if (m_databaseAvailable == available)
        return;
    m_databaseAvailable = available;
    if (available)
        loadSelectedMonth(true);
    else {
        m_hasLoaded = false;
        m_limitLabel->setText(QStringLiteral("预算：—"));
        m_spentLabel->setText(QStringLiteral("已消费：—"));
        m_remainingLabel->setText(QStringLiteral("剩余额度：—"));
        m_statusLabel->setText(QStringLiteral("预算暂不可用。"));
    }
    updateControls();
}

bool BudgetWidget::refresh()
{
    return loadSelectedMonth(false);
}

void BudgetWidget::setMonth(const QDate &month)
{
    if (MonthlyBudget(month, 0).validationError().isEmpty())
        m_monthEdit->setDate(MonthlyBudget::firstDay(month));
}

bool BudgetWidget::loadSelectedMonth(bool resetAmount)
{
    if (!m_databaseAvailable)
        return false;
    MonthlyBudgetStatus loaded;
    const QDate requested = MonthlyBudget::firstDay(m_monthEdit->date());
    if (!m_repository.loadStatus(requested, loaded)) {
        m_errorLabel->setText(QStringLiteral("加载预算失败：%1").arg(m_repository.lastError()));
        m_errorLabel->show();
        if (m_hasLoaded) {
            const QSignalBlocker blocker(m_monthEdit);
            m_monthEdit->setDate(m_status.month);
        }
        updateControls();
        return false;
    }
    const bool changedMonth = !m_hasLoaded || m_status.month != loaded.month;
    m_status = loaded;
    m_hasLoaded = true;
    {
        const QSignalBlocker blocker(m_monthEdit);
        m_monthEdit->setDate(loaded.month);
    }
    if (resetAmount || changedMonth) {
        m_amountEdit->setValue(loaded.hasSplit ? double(loaded.discretionaryLimitCents)/100 : loaded.hasBudget ? double(loaded.limitCents)/100 : 0.0);
        m_basicAmountEdit->setValue(loaded.hasSplit ? double(loaded.basicLimitCents)/100 : 0.0);
    }
    renderStatus();
    m_errorLabel->hide();
    updateControls();
    return true;
}

void BudgetWidget::saveBudget()
{
    if (!m_databaseAvailable || !m_hasLoaded)
        return;
    if (!m_amountEdit->hasAcceptableInput() || !m_basicAmountEdit->hasAcceptableInput()) {
        m_errorLabel->setText(QStringLiteral("请输入有效预算金额，最多保留两位小数。"));
        m_errorLabel->show();
        return;
    }
    m_amountEdit->interpretText();
    m_basicAmountEdit->interpretText();
    m_saveButton->setEnabled(false);
    if (!m_repository.saveSplit(m_monthEdit->date(),qRound64(m_basicAmountEdit->value()*100),qRound64(m_amountEdit->value()*100))) {
        m_errorLabel->setText(QStringLiteral("保存预算失败：%1").arg(m_repository.lastError()));
        m_errorLabel->show();
        updateControls();
        return;
    }
    if (!loadSelectedMonth(true)) {
        m_errorLabel->setText(QStringLiteral("预算已保存，但更新显示失败：%1。请稍后刷新。")
                             .arg(m_repository.lastError()));
        m_errorLabel->show();
    }
    updateControls();
    emit budgetSaved();
}

void BudgetWidget::renderStatus()
{
    m_splitLabel->setText(m_status.hasBudget && !m_status.hasSplit
        ? QStringLiteral("旧总预算尚未分配，暂填入自由消费预算。请确认两项金额后保存，原预算在保存前保持不变。")
        : QStringLiteral("必要生活已花 %1 · 自由消费已花 %2。两项预算分别计算，余额不自动挪用。")
            .arg(Transaction::formatAmount(m_status.basicSpentCents),Transaction::formatAmount(m_status.discretionarySpentCents)));
    m_spentLabel->setText(QStringLiteral("已消费：%1").arg(Transaction::formatAmount(m_status.spentCents)));
    m_limitLabel->setText(m_status.hasBudget
        ? QStringLiteral("预算：%1").arg(Transaction::formatAmount(m_status.limitCents))
        : QStringLiteral("预算：未设置"));
    m_remainingLabel->setText(m_status.hasBudget
        ? QStringLiteral("剩余额度：%1").arg(Transaction::formatAmount(m_status.remainingCents()))
        : QStringLiteral("剩余额度：—"));
    AppTheme::setTone(m_remainingLabel, m_status.isOverBudget() ? QStringLiteral("danger") : QString());
    if (!m_status.hasBudget) {
        m_statusLabel->setText(QStringLiteral("该月尚未设置预算。"));
        AppTheme::setTone(m_statusLabel, QStringLiteral("muted"));
    } else if (m_status.isOverBudget()) {
        m_statusLabel->setText(QStringLiteral("已超支 %1，请调整消费计划。")
                              .arg(Transaction::formatAmount(m_status.overspentCents())));
        AppTheme::setTone(m_statusLabel, QStringLiteral("danger"));
    } else if (m_status.remainingCents() == 0) {
        m_statusLabel->setText(QStringLiteral("预算已用完。"));
        AppTheme::setTone(m_statusLabel, QStringLiteral("warning"));
    } else if (m_status.hasSplit && (m_status.basicSpentCents>m_status.basicLimitCents || m_status.discretionarySpentCents>m_status.discretionaryLimitCents)) {
        m_statusLabel->setText(QStringLiteral("%1预算已超支，请检查两项剩余额度。")
            .arg(m_status.basicSpentCents>m_status.basicLimitCents ? QStringLiteral("必要生活") : QStringLiteral("自由消费")));
        AppTheme::setTone(m_statusLabel,QStringLiteral("danger"));
    } else {
        m_statusLabel->setText(QStringLiteral("当前支出在预算内。"));
        AppTheme::setTone(m_statusLabel, QStringLiteral("success"));
    }
}

void BudgetWidget::updateControls()
{
    m_monthEdit->setEnabled(m_databaseAvailable);
    m_amountEdit->setEnabled(m_databaseAvailable && m_hasLoaded);
    m_basicAmountEdit->setEnabled(m_databaseAvailable && m_hasLoaded);
    m_saveButton->setEnabled(m_databaseAvailable && m_hasLoaded);
}
