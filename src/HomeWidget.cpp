#include "AppTheme.h"
#include "HomeWidget.h"
#include "WishPoolWidget.h"
#include "ProductList.h"
#include "ShellWidgets.h"
#include "Transaction.h"
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QLocale>
#include <algorithm>

HomeWidget::HomeWidget(DatabaseManager &database, QWidget *parent)
    : QWidget(parent), m_budgets(database), m_wishes(database), m_execution(database)
{
    setObjectName(QStringLiteral("homeWidget"));
    auto *layout = new QVBoxLayout(this);
    ShellWidgets::pageLayout(layout);
    m_heading = new QHBoxLayout;
    auto *title = new QVBoxLayout;
    title->setSpacing(6);
    title->addWidget(ShellWidgets::pageTitle(QStringLiteral("给每一次心动，留一点余地。"), this, "homePageTitle"));
    m_date = ShellWidgets::label(QLocale(QLocale::Chinese).toString(QDate::currentDate(), QStringLiteral("M 月 d 日 · dddd")), this, "muted");
    title->addWidget(m_date);
    m_heading->addLayout(title, 1);
    layout->addLayout(m_heading);
    QFrame *budgetCard;
    auto *budget = ShellWidgets::card(this, &budgetCard);
    budgetCard->setObjectName(QStringLiteral("homeBudgetCard"));
    budget->setSpacing(8);
    auto *budgetHeader = new QHBoxLayout;
    m_month = ShellWidgets::label(QString(), budgetCard, "eyebrow");
    m_budgetButton = new QPushButton(QStringLiteral("调整预算"), budgetCard);
    m_budgetButton->setObjectName(QStringLiteral("homeBudgetButton"));
    m_budgetButton->setProperty("link", true);
    budgetHeader->addWidget(m_month, 1); budgetHeader->addWidget(m_budgetButton);
    budget->addLayout(budgetHeader);
    m_remaining = ShellWidgets::label(QStringLiteral("—"), budgetCard, "figure");
    m_remaining->setObjectName(QStringLiteral("homeBudgetRemainingLabel"));
    budget->addWidget(m_remaining);
    budget->addWidget(ShellWidgets::label(QStringLiteral("按已记账支出计算"), budgetCard, "muted"));
    m_progress = new QProgressBar(budgetCard);
    m_progress->setObjectName(QStringLiteral("homeBudgetProgress"));
    m_progress->setRange(0, 1000); m_progress->setTextVisible(false);
    m_budgetDetail = ShellWidgets::label(QStringLiteral("预算暂未加载"), budgetCard, "muted");
    m_budgetDetail->setObjectName(QStringLiteral("homeBudgetDetailLabel"));
    auto *scopes=new QVBoxLayout;
    const auto addScope=[&](QLabel *&label,const char *name) {
        label=ShellWidgets::label(QStringLiteral("—"),budgetCard,"muted");
        label->setObjectName(QLatin1String(name)); scopes->addWidget(label);
    };
    scopes->setSpacing(6);
    addScope(m_basicBudget,"homeBasicBudgetLabel");
    // The main figure already shows this balance; keep the secondary breakdown compact.
    addScope(m_discretionaryBudget,"homeDiscretionaryBudgetLabel");
    budget->addLayout(scopes);
    budget->addWidget(m_progress);
    budget->addWidget(m_budgetDetail);
    m_sections = new QBoxLayout(QBoxLayout::TopToBottom);
    m_sections->setSpacing(ShellWidgets::PageSpacing);
    m_sections->addWidget(budgetCard, 5);
    m_pool=new WishPoolWidget(database,this); m_sections->addWidget(m_pool, 4);
    connect(m_pool,&WishPoolWidget::wishlistRequested,this,&HomeWidget::wishlistRequested);
    layout->addLayout(m_sections);

    QFrame *resume;
    auto *resumeLayout = ShellWidgets::card(this, &resume);
    m_resume = resume;
    m_resume->setObjectName("homeResumeCard");
    auto *resumeRow = new QHBoxLayout;
    auto *resumeText = new QVBoxLayout;
    resumeText->addWidget(ShellWidgets::label(QStringLiteral("继续上次分析"), resume, "eyebrow"));
    m_resumeName = ShellWidgets::label(QString(), resume, "heading");
    m_resumeName->setObjectName("homeResumeNameLabel");
    resumeText->addWidget(m_resumeName);
    resumeRow->addLayout(resumeText, 1);
    m_continue = new QPushButton(QStringLiteral("继续分析  →"), resume);
    m_continue->setObjectName("homeContinueButton");
    resumeRow->addWidget(m_continue);
    resumeLayout->addLayout(resumeRow);
    layout->addWidget(resume);
    resume->hide();
    connect(m_continue, &QPushButton::clicked, this, &HomeWidget::executionRequested);

    m_decisions = new QWidget(this);
    m_decisions->setObjectName("homeReadySection");
    auto *decisions = new QVBoxLayout(m_decisions);
    decisions->setContentsMargins(0, 0, 0, 0);
    decisions->setSpacing(8);
    auto *readyHeading = new QHBoxLayout;
    readyHeading->addWidget(ShellWidgets::label(QStringLiteral("等过之后，再看一眼"), m_decisions, "heading"), 1);
    m_allReady = new QPushButton(QStringLiteral("查看全部  →"), m_decisions);
    m_allReady->setObjectName("homeReadyAllButton");
    m_allReady->setProperty("link", true);
    readyHeading->addWidget(m_allReady);
    decisions->addLayout(readyHeading);
    m_readySummary = ShellWidgets::label(QString(), m_decisions, "muted");
    m_readySummary->setObjectName("homeReadySummaryLabel");
    decisions->addWidget(m_readySummary);
    for (auto &row : m_rows) {
        auto *frame = new QFrame(m_decisions);
        frame->setObjectName("homeReadyRow");
        row.widget = frame;
        auto *line = new QHBoxLayout(frame);
        line->setContentsMargins(0, 14, 0, 14);
        line->setSpacing(16);
        auto *badge = new ProductStateBadge(frame);
        badge->setState(false, false);
        line->addWidget(badge);
        auto *text = new QVBoxLayout;
        text->setSpacing(4);
        row.name = ShellWidgets::label(QString(), frame);
        row.name->setObjectName("homeReadyNameLabel");
        row.detail = ShellWidgets::label(QString(), frame, "muted");
        row.detail->setObjectName("homeReadyDetailLabel");
        text->addWidget(row.name); text->addWidget(row.detail);
        line->addLayout(text, 1);
        row.action = new QPushButton(QStringLiteral("现在还想要吗？"), frame);
        row.action->setObjectName("homeReadyWishButton");
        line->addWidget(row.action);
        connect(row.action, &QPushButton::clicked, this, [this, button = row.action] {
            emit readyWishesRequested(button->property("wishId").toLongLong());
        });
        decisions->addWidget(frame);
        frame->hide();
    }
    layout->addWidget(m_decisions);
    m_decisions->hide();
    connect(m_allReady, &QPushButton::clicked, this, [this] { emit readyWishesRequested(); });
    m_error = ShellWidgets::label(QString(), this, "error");
    m_error->setObjectName(QStringLiteral("homeErrorLabel"));
    m_error->hide(); layout->addWidget(m_error);
    layout->addStretch(1);
    connect(m_budgetButton, &QPushButton::clicked, this, &HomeWidget::budgetRequested);
    auto *timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, &QTimer::timeout, this, [this] { if (isVisible()) refresh(); });
    timer->start();
    setDatabaseAvailable(false);
}

void HomeWidget::setPrimaryAction(QWidget *action)
{
    m_heading->addWidget(action);
}

void HomeWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_sections->setDirection(width() >= 880 ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
}

void HomeWidget::setDatabaseAvailable(bool available)
{
    m_available = available;
    m_pool->setDatabaseAvailable(available);
    m_budgetButton->setEnabled(available);
    m_allReady->setEnabled(available);
    m_continue->setEnabled(available);
    for (const auto &row : m_rows) row.action->setEnabled(available);
    if (available) refresh();
    else { m_error->setText(QStringLiteral("暂时无法读取本机数据，请在账单页查看错误。")); m_error->show(); }
}

bool HomeWidget::refresh()
{
    if (!m_available) return false;
    m_pool->refresh();
    const auto today = QDate::currentDate();
    m_date->setText(QLocale(QLocale::Chinese).toString(today, QStringLiteral("M 月 d 日 · dddd")));
    MonthlyBudgetStatus status;
    QList<WishItem> wishes;
    ExecutionSession session;
    // Keep the last successful home snapshot if either read fails.
    if (!m_budgets.loadStatus(today, status) || !m_wishes.findAll(wishes) || !m_execution.loadActive(session)) {
        const auto error = !m_budgets.lastError().isEmpty() ? m_budgets.lastError()
            : !m_wishes.lastError().isEmpty() ? m_wishes.lastError() : m_execution.lastError();
        m_error->setText(QStringLiteral("首页更新失败：%1").arg(error)); m_error->show(); return false;
    }
    m_month->setText(status.hasBudget ? (status.hasSplit?QStringLiteral("本月自由消费剩余"):QStringLiteral("本月预算总剩余")) : QStringLiteral("本月预算"));
    m_month->setToolTip(today.toString(QStringLiteral("yyyy 年 M 月")));
    m_remaining->setText(status.hasBudget ? Transaction::formatAmount(status.hasSplit?(status.discretionaryLimitCents-status.discretionarySpentCents):status.remainingCents()) : QStringLiteral("还没有设置预算"));
    m_remaining->setProperty("empty", !status.hasBudget);
    m_budgetButton->setText(status.hasBudget ? QStringLiteral("调整预算") : QStringLiteral("设置预算"));
    m_progress->setVisible(status.hasBudget);
    const int used = status.limitCents > 0
        ? static_cast<int>(qBound(0.0, double(status.spentCents) / status.limitCents * 1000, 1000.0))
        : (status.spentCents > 0 ? 1000 : 0);
    m_progress->setValue(used);
    m_progress->setVisible(status.hasBudget && !status.hasSplit);
    const auto scopeText=[&](const QString &name,qint64 spent,qint64 limit) {
        return status.hasSplit ? QStringLiteral("%1剩余  %2").arg(name,Transaction::formatAmount(limit-spent))
                               : QStringLiteral("%1 · 已花 %2，额度未分配").arg(name,Transaction::formatAmount(spent));
    };
    m_basicBudget->setText(scopeText(QStringLiteral("必要生活"),status.basicSpentCents,status.basicLimitCents));
    m_discretionaryBudget->setText(scopeText(QStringLiteral("自由消费"),status.discretionarySpentCents,status.discretionaryLimitCents));
    m_basicBudget->setToolTip(QStringLiteral("已花 %1 / 额度 %2").arg(Transaction::formatAmount(status.basicSpentCents),Transaction::formatAmount(status.basicLimitCents)));
    m_discretionaryBudget->setToolTip(QStringLiteral("已花 %1 / 额度 %2").arg(Transaction::formatAmount(status.discretionarySpentCents),Transaction::formatAmount(status.discretionaryLimitCents)));
    m_basicBudget->setVisible(status.hasBudget);
    m_discretionaryBudget->setVisible(status.hasBudget && !status.hasSplit);
    AppTheme::setTone(m_basicBudget,status.hasSplit && status.basicSpentCents>status.basicLimitCents?QStringLiteral("danger"):QString());
    AppTheme::setTone(m_discretionaryBudget,status.hasSplit && status.discretionarySpentCents>status.discretionaryLimitCents?QStringLiteral("danger"):QString());
    const bool balanceOver = status.hasSplit ? status.discretionarySpentCents > status.discretionaryLimitCents : status.isOverBudget();
    m_remaining->setProperty("overBudget", balanceOver);
    AppTheme::setTone(m_remaining, balanceOver ? QStringLiteral("danger") : QString());
    m_budgetDetail->setText(status.hasBudget
        ? QStringLiteral("合计已花 %1  /  总预算 %2%3").arg(Transaction::formatAmount(status.spentCents),
            Transaction::formatAmount(status.limitCents), status.isOverBudget() ? QStringLiteral(" · 已超支") : QString())
        : QStringLiteral("本月已花 %1，给接下来的消费留一点余地。").arg(Transaction::formatAmount(status.spentCents)));
    const auto now = QDateTime::currentDateTimeUtc();
    QList<WishItem> ready;
    for (const auto &wish : wishes) {
        if (wish.status != WishStatus::Waiting) continue;
        if (wish.canPurchase(now)) ready.append(wish);
    }
    std::sort(ready.begin(), ready.end(), [](const WishItem &a, const WishItem &b) {
        if (a.priority!=b.priority) return a.priority<b.priority;
        return a.readyAt == b.readyAt ? a.id < b.id : a.readyAt < b.readyAt;
    });
    m_readySummary->setText(QStringLiteral("%1 件已结束冷静期 · 到期后可以重新考虑").arg(ready.size()));
    for (size_t i = 0; i < m_rows.size(); ++i) {
        auto &row = m_rows[i];
        row.widget->setVisible(i < size_t(ready.size()));
        if (i >= size_t(ready.size())) { row.action->setProperty("wishId", 0); continue; }
        const auto &wish = ready[int(i)];
        row.name->setText(wish.name);
        row.detail->setText(QStringLiteral("%1 · 预计 %2 · 已结束冷静期").arg(wish.priorityText(),Transaction::formatAmount(wish.estimatedCents)));
        row.action->setProperty("wishId", wish.id);
        row.action->setAccessibleName(QStringLiteral("重新考虑：%1").arg(wish.name));
    }
    m_decisions->setVisible(!ready.isEmpty());
    m_resumeName->setText(session.id ? QStringLiteral("%1 · 预计 %2").arg(session.name, Transaction::formatAmount(session.estimatedCents)) : QString());
    m_resume->setVisible(session.id > 0);
    m_error->hide(); return true;
}
