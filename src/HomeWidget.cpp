#include "AppTheme.h"
#include "HomeWidget.h"
#include "WishPoolWidget.h"
#include "WorkflowOverview.h"
#include "ShellWidgets.h"
#include "Transaction.h"
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

HomeWidget::HomeWidget(DatabaseManager &database, QWidget *parent)
    : QWidget(parent), m_budgets(database), m_wishes(database)
{
    setObjectName(QStringLiteral("homeWidget"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);
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
    m_progress = new QProgressBar(budgetCard);
    m_progress->setObjectName(QStringLiteral("homeBudgetProgress"));
    m_progress->setRange(0, 1000); m_progress->setTextVisible(false);
    m_budgetDetail = ShellWidgets::label(QStringLiteral("预算暂未加载"), budgetCard, "muted");
    m_budgetDetail->setObjectName(QStringLiteral("homeBudgetDetailLabel"));
    auto *scopes=new QHBoxLayout;
    const auto addScope=[&](QLabel *&label,QProgressBar *&progress,const char *name) {
        auto *column=new QVBoxLayout;
        column->setSpacing(6);
        label=ShellWidgets::label(QStringLiteral("—"),budgetCard,"muted"); label->setObjectName(QLatin1String(name)); column->addWidget(label);
        progress=new QProgressBar(budgetCard); progress->setRange(0,1000); progress->setTextVisible(false); column->addWidget(progress); scopes->addLayout(column,1);
    };
    scopes->setSpacing(20);
    addScope(m_discretionaryBudget,m_discretionaryProgress,"homeDiscretionaryBudgetLabel"); addScope(m_basicBudget,m_basicProgress,"homeBasicBudgetLabel");
    budget->addLayout(scopes);
    budget->addWidget(m_progress);
    budget->addWidget(m_budgetDetail);
    layout->addWidget(budgetCard);

    m_pool=new WishPoolWidget(database,this); layout->addWidget(m_pool);
    connect(m_pool,&WishPoolWidget::wishlistRequested,this,&HomeWidget::wishlistRequested);
    m_overview=new WorkflowOverview(database,this); layout->addWidget(m_overview);
    connect(m_overview,&WorkflowOverview::reviewRequested,this,&HomeWidget::reviewRequested);
    connect(m_overview,&WorkflowOverview::pricesRequested,this,&HomeWidget::pricesRequested);
    connect(m_overview,&WorkflowOverview::attentionChanged,this,&HomeWidget::attentionChanged);

    QFrame *wishCard;
    auto *wishes = ShellWidgets::card(this, &wishCard);
    wishCard->setObjectName(QStringLiteral("homeWishCard"));
    wishes->addWidget(ShellWidgets::label(QStringLiteral("清单里的心动"), wishCard, "eyebrow"));
    m_wishCount = ShellWidgets::label(QStringLiteral("—"), wishCard, "heading");
    m_wishCount->setObjectName(QStringLiteral("homeWishCountLabel"));
    wishes->addWidget(m_wishCount);
    m_wishDetail = ShellWidgets::label(QStringLiteral("让想买的东西，经过一点时间。"), wishCard, "muted");
    m_wishDetail->setObjectName(QStringLiteral("homeWishDetailLabel"));
    wishes->addWidget(m_wishDetail);
    auto *wishButton = new QPushButton(QStringLiteral("去清单看看  →"), wishCard);
    wishButton->setObjectName(QStringLiteral("homeWishlistButton"));
    wishButton->setProperty("link", true);
    wishes->addWidget(wishButton, 0, Qt::AlignLeft);
    layout->addWidget(wishCard);
    m_error = ShellWidgets::label(QString(), this, "error");
    m_error->setObjectName(QStringLiteral("homeErrorLabel"));
    m_error->hide(); layout->addWidget(m_error);
    layout->addStretch(1);
    connect(m_budgetButton, &QPushButton::clicked, this, &HomeWidget::budgetRequested);
    connect(wishButton, &QPushButton::clicked, this, &HomeWidget::wishlistRequested);
    auto *timer = new QTimer(this);
    timer->setInterval(60000);
    connect(timer, &QTimer::timeout, this, [this] { if (isVisible()) refresh(); });
    timer->start();
    setDatabaseAvailable(false);
}

void HomeWidget::setDatabaseAvailable(bool available)
{
    m_available = available;
    m_pool->setDatabaseAvailable(available);
    m_overview->setDatabaseAvailable(available);
    m_budgetButton->setEnabled(available);
    if (available) refresh();
    else { m_error->setText(QStringLiteral("暂时无法读取本机数据，请在账单页查看错误。")); m_error->show(); }
}

bool HomeWidget::refresh()
{
    if (!m_available) return false;
    m_pool->refresh();
    m_overview->refresh();
    const auto today = QDate::currentDate();
    MonthlyBudgetStatus status;
    QList<WishItem> wishes;
    // Keep the last successful home snapshot if either read fails.
    if (!m_budgets.loadStatus(today, status) || !m_wishes.findAll(wishes)) {
        const auto error = !m_budgets.lastError().isEmpty() ? m_budgets.lastError() : m_wishes.lastError();
        m_error->setText(QStringLiteral("首页更新失败：%1").arg(error)); m_error->show(); return false;
    }
    m_month->setText(status.hasBudget ? QStringLiteral("还能花") : QStringLiteral("本月预算"));
    m_month->setToolTip(today.toString(QStringLiteral("yyyy 年 M 月")));
    m_remaining->setText(status.hasBudget ? Transaction::formatAmount(status.remainingCents()) : QStringLiteral("还没有设置预算"));
    m_remaining->setProperty("empty", !status.hasBudget);
    m_budgetButton->setText(status.hasBudget ? QStringLiteral("修改") : QStringLiteral("设置预算"));
    m_progress->setVisible(status.hasBudget);
    const int used = status.limitCents > 0
        ? static_cast<int>(qBound(0.0, double(status.spentCents) / status.limitCents * 1000, 1000.0))
        : (status.spentCents > 0 ? 1000 : 0);
    m_progress->setValue(used);
    m_progress->setVisible(status.hasBudget && !status.hasSplit);
    const auto scopeText=[&](const QString &name,qint64 spent,qint64 limit) {
        return status.hasSplit ? QStringLiteral("%1  %2").arg(name,Transaction::formatAmount(limit-spent))
                               : QStringLiteral("%1 · 已花 %2，额度未分配").arg(name,Transaction::formatAmount(spent));
    };
    m_basicBudget->setText(scopeText(QStringLiteral("基础生活"),status.basicSpentCents,status.basicLimitCents));
    m_discretionaryBudget->setText(scopeText(QStringLiteral("可支配"),status.discretionarySpentCents,status.discretionaryLimitCents));
    m_basicBudget->setToolTip(QStringLiteral("已花 %1 / 额度 %2").arg(Transaction::formatAmount(status.basicSpentCents),Transaction::formatAmount(status.basicLimitCents)));
    m_discretionaryBudget->setToolTip(QStringLiteral("已花 %1 / 额度 %2").arg(Transaction::formatAmount(status.discretionarySpentCents),Transaction::formatAmount(status.discretionaryLimitCents)));
    m_basicBudget->setVisible(status.hasBudget); m_discretionaryBudget->setVisible(status.hasBudget);
    AppTheme::setTone(m_basicBudget,status.hasSplit && status.basicSpentCents>status.basicLimitCents?QStringLiteral("danger"):QString());
    AppTheme::setTone(m_discretionaryBudget,status.hasSplit && status.discretionarySpentCents>status.discretionaryLimitCents?QStringLiteral("danger"):QString());
    m_basicProgress->setVisible(status.hasSplit); m_discretionaryProgress->setVisible(status.hasSplit);
    const auto ratio=[](qint64 spent,qint64 limit) { return limit>0?int(qBound(0.0,double(spent)/limit*1000,1000.0)):(spent>0?1000:0); };
    m_basicProgress->setValue(ratio(status.basicSpentCents,status.basicLimitCents)); m_discretionaryProgress->setValue(ratio(status.discretionarySpentCents,status.discretionaryLimitCents));
    m_remaining->setProperty("overBudget", status.isOverBudget());
    AppTheme::setTone(m_remaining, status.isOverBudget() ? QStringLiteral("danger") : QString());
    m_budgetDetail->setText(status.hasBudget
        ? QStringLiteral("已花 %1  /  预算 %2%3").arg(Transaction::formatAmount(status.spentCents),
            Transaction::formatAmount(status.limitCents), status.isOverBudget() ? QStringLiteral(" · 已超支") : QString())
        : QStringLiteral("本月已花 %1，给接下来的消费留一点余地。").arg(Transaction::formatAmount(status.spentCents)));
    const auto now = QDateTime::currentDateTimeUtc();
    int waiting = 0, ready = 0;
    QString firstReady;
    for (const auto &wish : wishes) {
        if (wish.status != WishStatus::Waiting) continue;
        if (wish.canPurchase(now)) { ++ready; if (firstReady.isEmpty()) firstReady = wish.name; }
        else ++waiting;
    }
    m_wishCount->setText(waiting + ready > 0 ? QStringLiteral("%1 件，慢慢决定").arg(waiting + ready) : QStringLiteral("给心动留个位置"));
    m_wishDetail->setText(ready > 0
        ? QStringLiteral("%1 件已结束冷静期，包括“%2”。现在还想要吗？").arg(ready).arg(firstReady)
        : waiting > 0 ? QStringLiteral("%1 件正在冷静期。不急着决定，也是一种选择。").arg(waiting)
                      : QStringLiteral("把想买的先放进清单，等一等，再看看它是否真的适合你。"));
    m_error->hide(); return true;
}
