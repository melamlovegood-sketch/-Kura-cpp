#include "BillingWorkspace.h"
#include "ProductDetailsDialog.h"
#include "ExpenseComposer.h"
#include "ModernSelectors.h"
#include "BillingCharts.h"
#include "BudgetRepository.h"
#include "CategoryCatalog.h"
#include "TransactionRepository.h"
#include "WorkflowStorage.h"
#include "ShellWidgets.h"
#include "AppTheme.h"
#include "AppTypography.h"
#include "ImagePreview.h"
#include <QDialog>
#include <QFrame>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <algorithm>

namespace {
class ContentStack:public QStackedWidget {
public:
    explicit ContentStack(QWidget *parent):QStackedWidget(parent) { connect(this,&QStackedWidget::currentChanged,this,[this] { updateGeometry(); }); }
    QSize sizeHint() const override { return currentWidget()?currentWidget()->sizeHint():QSize(0,0); }
    QSize minimumSizeHint() const override { return currentWidget()?currentWidget()->minimumSizeHint():QSize(0,0); }
};
class RecentButton:public QPushButton {
public:
    RecentButton(const Transaction &record,QWidget *parent):QPushButton(parent),m_record(record) { setObjectName(QStringLiteral("billingRecentRecord%1").arg(record.id())); setProperty("recordId",record.id()); setFixedHeight(44); setCursor(Qt::PointingHandCursor); setAccessibleName(record.description()+" "+BillingAggregation::money(record.amountCents())); setToolTip(record.date().toString(Qt::ISODate)+(record.time().isValid()?" "+record.time().toString("HH:mm:ss"):QStringLiteral(" · 时间未记录"))); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); if (underMouse() || isDown()) p.fillRect(rect(),AppTheme::color(this,"inset")); p.setPen(AppTheme::color(this,"border")); p.drawLine(0,0,width(),0);
        const int moneyRight=width()-37,moneyLeft=moneyRight-106,dayLeft=moneyLeft-105,categoryLeft=dayLeft-82; const bool compact=width()<540; auto font=this->font(); font.setPixelSize(13); p.setFont(font); p.setPen(AppTheme::color(this,"ink")); const int nameWidth=compact?moneyLeft-28:categoryLeft-25; p.drawText(QRect(16,0,qMax(50,nameWidth),height()),Qt::AlignVCenter,p.fontMetrics().elidedText(m_record.description(),Qt::ElideRight,qMax(50,nameWidth)));
        font.setPixelSize(11); p.setFont(font); p.setPen(AppTheme::color(this,"muted")); if (!compact) { p.drawText(QRect(categoryLeft,0,79,height()),Qt::AlignVCenter,p.fontMetrics().elidedText(CategoryCatalog::label(m_record.detailCategory()),Qt::ElideRight,76)); p.drawText(QRect(dayLeft,0,102,height()),Qt::AlignVCenter,m_record.date().toString("MM/dd")+(m_record.time().isValid()?" "+m_record.time().toString("HH:mm"):QString())); }
        p.setFont(AppTypography::serif(15)); p.setPen(AppTheme::color(this,"ink")); p.drawText(QRect(moneyLeft,0,106,height()),Qt::AlignRight|Qt::AlignVCenter,BillingAggregation::money(m_record.amountCents())); p.setFont(AppTypography::sans(17)); p.setPen(AppTheme::color(this,"muted")); p.drawText(QRect(width()-29,0,22,height()),Qt::AlignCenter,QStringLiteral("···")); if (hasFocus()) { p.setPen(AppTheme::color(this,"accent")); p.setBrush(Qt::NoBrush); p.drawRoundedRect(rect().adjusted(2,2,-3,-3),6,6); }
    }
private: Transaction m_record;
};
void clearLayout(QVBoxLayout *layout) { while (auto *item=layout->takeAt(0)) { if (item->widget()) { item->widget()->hide(); item->widget()->deleteLater(); } delete item; } }
QLabel *amountLabel(QWidget *parent,const char *name) { auto *label=ShellWidgets::label(QStringLiteral("—"),parent); label->setObjectName(QLatin1String(name)); label->setProperty("billingAmount",true); return label; }
QPushButton *segment(const QString &text,const char *name,QWidget *parent) { auto *button=new QPushButton(text,parent); button->setObjectName(QLatin1String(name)); button->setProperty("billingSegment",true); button->setCheckable(true); return button; }
}
BillingWorkspace::BillingWorkspace(DatabaseManager &database,const QString &settingsPath,QWidget *parent):QWidget(parent),m_database(database),m_month(QDate::currentDate().year(),QDate::currentDate().month(),1)
{
    setObjectName("billingWorkspace"); setProperty("page",true); auto *layout=new QVBoxLayout(this); layout->setContentsMargins(28,18,28,18); layout->setSpacing(12);
    auto *heading=new QHBoxLayout; auto *title=ShellWidgets::label(QStringLiteral("账单"),this); title->setObjectName("billingPageTitle"); heading->addWidget(title); m_monthPicker=new MonthPicker(m_month,this); m_monthPicker->setObjectName("billingMonthPicker"); m_monthPicker->setMaximumWidth(157); heading->addWidget(m_monthPicker); heading->addStretch();
    m_import=new QPushButton(QStringLiteral("↓ 导入账单"),this); m_import->setObjectName("billingImportButton"); m_import->setProperty("billingQuiet",true); heading->addWidget(m_import); m_quickRecord=new QPushButton(QStringLiteral("＋ 记一笔"),this); m_quickRecord->setObjectName("billingQuickRecordButton"); m_quickRecord->setProperty("billingQuiet",true); m_quickRecord->hide(); heading->addWidget(m_quickRecord); layout->addLayout(heading);
    auto *viewRow=new QHBoxLayout; auto *switches=new QFrame(this); switches->setProperty("billingInset",true); auto *switchLayout=new QHBoxLayout(switches); switchLayout->setContentsMargins(4,4,4,4); switchLayout->setSpacing(3); m_recordTab=segment(QStringLiteral("记账"),"billingRecordViewButton",switches); m_chartTab=segment(QStringLiteral("图表"),"billingChartsViewButton",switches); switchLayout->addWidget(m_recordTab); switchLayout->addWidget(m_chartTab); m_recordTab->setChecked(true); viewRow->addWidget(switches); viewRow->addStretch();
    m_headerStats=new QWidget(this);
    auto *statsLayout=new QHBoxLayout(m_headerStats); statsLayout->setContentsMargins(0,0,0,0); statsLayout->setSpacing(10);
    m_total=amountLabel(m_headerStats,"billingMonthTotalLabel"); m_today=amountLabel(m_headerStats,"billingTodayTotalLabel");
    m_count=ShellWidgets::label(QStringLiteral("— 笔"),m_headerStats,"muted"); m_count->setObjectName("billingCountLabel"); m_count->hide();
    statsLayout->addWidget(ShellWidgets::label(QStringLiteral("本月支出"),m_headerStats,"muted")); statsLayout->addWidget(m_total);
    statsLayout->addWidget(ShellWidgets::label(QStringLiteral("今日"),m_headerStats,"muted")); statsLayout->addWidget(m_today); statsLayout->addWidget(m_count);
    viewRow->addWidget(m_headerStats); layout->addLayout(viewRow);
    m_workspaceLayout=new QHBoxLayout; m_workspaceLayout->setSpacing(20); m_views=new ContentStack(this); m_views->setObjectName("billingViews");
    auto *recordPage=new QWidget; auto *recordLayout=new QVBoxLayout(recordPage); recordLayout->setContentsMargins(0,0,0,0); recordLayout->setSpacing(14); m_composer=new ExpenseComposer(database,settingsPath,recordPage); recordLayout->addWidget(m_composer);
    auto *recent=new QFrame(recordPage); recent->setObjectName("billingRecentCard"); recent->setProperty("billingCard",true); auto *recentLayout=new QVBoxLayout(recent); recentLayout->setContentsMargins(0,0,0,0); recentLayout->setSpacing(0); auto *recentHeader=new QHBoxLayout; recentHeader->setContentsMargins(18,13,18,10); recentHeader->addWidget(ShellWidgets::label(QStringLiteral("最近记录"),recent)); m_recentCount=ShellWidgets::label(QStringLiteral("— 笔"),recent,"muted"); m_recentCount->setObjectName("billingRecentCountLabel"); m_recentCount->hide(); recentHeader->addStretch(); auto *all=new QPushButton(QStringLiteral("查看当月全部 ↗"),recent); all->setObjectName("billingAllRecordsButton"); all->setProperty("billingQuiet",true); recentHeader->addWidget(all); recentLayout->addLayout(recentHeader); m_recentRows=new QVBoxLayout; m_recentRows->setContentsMargins(0,0,0,0); m_recentRows->setSpacing(0); recentLayout->addLayout(m_recentRows); auto *foot=ShellWidgets::label(QStringLiteral("点击记录查看详情"),recent,"muted"); foot->setObjectName("billingRecentFootnote"); recentLayout->addWidget(foot); recordLayout->addWidget(recent);
    auto *budgetSummaryRow=new QHBoxLayout;
    budgetSummaryRow->setContentsMargins(4,0,4,0); budgetSummaryRow->setSpacing(12);
    m_budgetSummary=ShellWidgets::label(QStringLiteral("预算尚未加载"),recordPage,"muted");
    m_budgetSummary->setObjectName("billingBudgetSummaryLabel"); budgetSummaryRow->addWidget(m_budgetSummary,1);
    m_budgetSummaryButton=new QPushButton(QStringLiteral("预算详情 →"),recordPage);
    m_budgetSummaryButton->setObjectName("billingBudgetSummaryButton"); m_budgetSummaryButton->setProperty("billingQuiet",true);
    budgetSummaryRow->addWidget(m_budgetSummaryButton); recordLayout->addLayout(budgetSummaryRow);
    recordLayout->addStretch(); m_views->addWidget(recordPage);
    auto *chartsPage=new QFrame; chartsPage->setProperty("billingCard",true); chartsPage->setObjectName("billingChartsCard");
    auto *chartsLayout=new QVBoxLayout(chartsPage); chartsLayout->setContentsMargins(20,20,20,18); chartsLayout->setSpacing(16);
    auto *chartHeader=new QHBoxLayout;
    m_chartTitle=ShellWidgets::label(QStringLiteral("消费日历"),chartsPage); m_chartTitle->setObjectName("billingChartTitleLabel"); m_chartTitle->setProperty("billingSubtitle",true);
    chartHeader->addWidget(m_chartTitle,1);
    auto *chartNumbers=new QVBoxLayout; chartNumbers->setSpacing(3);
    m_chartAmount=amountLabel(chartsPage,"billingChartAmountLabel"); m_chartAmount->setProperty("billingBalance",true); m_chartAmount->setAlignment(Qt::AlignRight);
    m_chartHint=ShellWidgets::label(QString(),chartsPage,"muted"); m_chartHint->setObjectName("billingChartRangeLabel"); m_chartHint->setAlignment(Qt::AlignRight); m_chartHint->setWordWrap(false);
    chartNumbers->addWidget(m_chartAmount); chartNumbers->addWidget(m_chartHint); chartHeader->addLayout(chartNumbers); chartsLayout->addLayout(chartHeader);
    auto *chartTabs=new QHBoxLayout; chartTabs->setSpacing(6);
    m_calendarTab=segment(QStringLiteral("日历"),"billingCalendarViewButton",chartsPage);
    m_trendTab=segment(QStringLiteral("本月趋势"),"billingTrendViewButton",chartsPage);
    m_six=segment(QStringLiteral("近半年"),"billingSixMonthTrendButton",chartsPage);
    m_calendarTab->setChecked(true); chartTabs->addWidget(m_calendarTab); chartTabs->addWidget(m_trendTab); chartTabs->addWidget(m_six); chartTabs->addStretch(); chartsLayout->addLayout(chartTabs);
    m_chartViews=new ContentStack(chartsPage); m_chartViews->setObjectName("billingChartViews");
    auto *calendarPage=new QWidget; auto *calendarLayout=new QVBoxLayout(calendarPage); calendarLayout->setContentsMargins(0,0,0,0); calendarLayout->setSpacing(12);
    m_calendar=new BillingCalendar(calendarPage); calendarLayout->addWidget(m_calendar);
    auto *calendarHint=ShellWidgets::label(QStringLiteral("颜色越深，支出越多 · 点击日期查看记录"),calendarPage,"muted"); calendarLayout->addWidget(calendarHint);
    auto *dayDivider=new QFrame(calendarPage); dayDivider->setProperty("billingDivider",true); dayDivider->setFixedHeight(1); calendarLayout->addWidget(dayDivider);
    m_dayRows=new QVBoxLayout; m_dayRows->setContentsMargins(0,0,0,0); m_dayRows->setSpacing(4); calendarLayout->addLayout(m_dayRows); m_chartViews->addWidget(calendarPage);
    auto *trendPage=new QWidget; auto *trendLayout=new QVBoxLayout(trendPage); trendLayout->setContentsMargins(0,0,0,0); trendLayout->setSpacing(12);
    m_trend=new BillingTrendChart(trendPage); trendLayout->addWidget(m_trend);
    auto *selection=new QHBoxLayout;
    m_trendReadout=ShellWidgets::label(QStringLiteral("点击图中位置查看记录"),trendPage,"muted"); m_trendReadout->setObjectName("billingTrendReadout"); selection->addWidget(m_trendReadout,1);
    m_trendRecords=new QPushButton(QStringLiteral("查看记录 →"),trendPage); m_trendRecords->setObjectName("billingTrendRecordsButton"); m_trendRecords->setProperty("billingQuiet",true); m_trendRecords->hide(); selection->addWidget(m_trendRecords); trendLayout->addLayout(selection);
    m_trendSummary=ShellWidgets::label(QString(),trendPage,"muted"); m_trendSummary->setObjectName("billingTrendSummaryLabel"); trendLayout->addWidget(m_trendSummary); m_chartViews->addWidget(trendPage);
    chartsLayout->addWidget(m_chartViews);
    m_detailsToggle=new QPushButton(QStringLiteral("查看分类与预算"),chartsPage); m_detailsToggle->setObjectName("billingChartDetailsButton"); m_detailsToggle->setProperty("billingQuiet",true); m_detailsToggle->setCheckable(true); chartsLayout->addWidget(m_detailsToggle,0,Qt::AlignLeft);
    m_views->addWidget(chartsPage); m_workspaceLayout->addWidget(m_views,1);
    m_sidebar=new QWidget(chartsPage); m_sidebar->setObjectName("billingSidebar"); m_sidebar->hide(); m_sidebarLayout=new QVBoxLayout(m_sidebar); m_sidebarLayout->setContentsMargins(0,0,0,0); m_sidebarLayout->setSpacing(18);
    auto *budget=new QFrame(m_sidebar); budget->setProperty("billingCard",true); auto *budgetLayout=new QVBoxLayout(budget); budgetLayout->setContentsMargins(18,18,18,18); budgetLayout->setSpacing(9); auto *budgetHeader=new QHBoxLayout; budgetHeader->addWidget(ShellWidgets::label(QStringLiteral("月度预算"),budget)); budgetHeader->addStretch(); auto *budgetMonth=ShellWidgets::label(QString(),budget,"muted"); budgetMonth->setObjectName("billingBudgetMonthLabel"); budgetHeader->addWidget(budgetMonth); budgetLayout->addLayout(budgetHeader); m_budgetState=ShellWidgets::label(QStringLiteral("尚未加载"),budget,"muted"); m_budgetState->setObjectName("billingBudgetStateLabel"); budgetLayout->addSpacing(4); budgetLayout->addWidget(m_budgetState); m_budgetBalance=amountLabel(budget,"billingBudgetBalanceLabel"); m_budgetBalance->setProperty("billingBalance",true); budgetLayout->addWidget(m_budgetBalance); m_budgetTotal=ShellWidgets::label(QStringLiteral("—"),budget,"muted"); m_budgetTotal->setObjectName("billingBudgetTotalLabel"); budgetLayout->addWidget(m_budgetTotal);
    const auto scope=[&](const QString &title,const char *remainingName,const char *usedName,const char *barName,QLabel *&remaining,QLabel *&used,QProgressBar *&bar) { auto *row=new QHBoxLayout; row->addWidget(ShellWidgets::label(title,budget)); row->addStretch(); remaining=ShellWidgets::label(QStringLiteral("—"),budget,"muted"); remaining->setObjectName(QLatin1String(remainingName)); row->addWidget(remaining); budgetLayout->addSpacing(6); budgetLayout->addLayout(row); bar=new QProgressBar(budget); bar->setObjectName(QLatin1String(barName)); bar->setProperty("billingProgress",true); bar->setRange(0,1000); bar->setTextVisible(false); budgetLayout->addWidget(bar); used=ShellWidgets::label(QStringLiteral("—"),budget,"muted"); used->setObjectName(QLatin1String(usedName)); budgetLayout->addWidget(used); };
    scope(QStringLiteral("基础生活"),"billingBasicRemainingLabel","billingBasicUsedLabel","billingBasicProgress",m_basicRemaining,m_basicUsed,m_basicBar); scope(QStringLiteral("可支配"),"billingDiscRemainingLabel","billingDiscUsedLabel","billingDiscProgress",m_discRemaining,m_discUsed,m_discBar); m_budgetEdit=new QPushButton(QStringLiteral("调整预算 →"),budget); m_budgetEdit->setObjectName("billingEditBudgetButton"); m_budgetEdit->setProperty("billingQuiet",true); budgetLayout->addSpacing(6); budgetLayout->addWidget(m_budgetEdit); m_sidebarLayout->addWidget(budget);
    auto *categories=new QFrame(m_sidebar); categories->setProperty("billingCard",true); auto *categoryLayout=new QVBoxLayout(categories); categoryLayout->setContentsMargins(18,16,18,16); categoryLayout->setSpacing(12); categoryLayout->addWidget(ShellWidgets::label(QStringLiteral("本月分类"),categories)); m_categoryRows=new QVBoxLayout; m_categoryRows->setContentsMargins(0,0,0,0); m_categoryRows->setSpacing(10); categoryLayout->addLayout(m_categoryRows); m_sidebarLayout->addWidget(categories,1); m_sidebarLayout->setStretch(0,1); chartsLayout->addWidget(m_sidebar); layout->addLayout(m_workspaceLayout); layout->addStretch();
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("billingLoadErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    connect(m_recordTab,&QPushButton::clicked,this,[this] { showView(false); }); connect(m_chartTab,&QPushButton::clicked,this,[this] { showView(true); });
    connect(m_calendarTab,&QPushButton::clicked,this,[this] { showChart(0); }); connect(m_trendTab,&QPushButton::clicked,this,[this] { showChart(1); }); connect(m_six,&QPushButton::clicked,this,[this] { showChart(2); });
    connect(m_detailsToggle,&QPushButton::toggled,this,[this](bool expanded) { m_sidebar->setVisible(expanded); m_detailsToggle->setText(expanded?QStringLiteral("收起分类与预算"):QStringLiteral("查看分类与预算")); m_views->updateGeometry(); });
    connect(m_trendRecords,&QPushButton::clicked,this,[this] {
        if (!m_trendSelection.isValid()) return;
        const auto first=m_trendSelection;
        const auto last=m_six->isChecked()?QDate(first.year(),first.month(),first.daysInMonth()):first;
        emit recordsRequested(first,last);
    });
    connect(m_monthPicker,&QDateEdit::dateChanged,this,[this](const QDate &month) { setMonth(month); }); connect(m_calendar,&BillingCalendar::daySelected,this,&BillingWorkspace::renderDay); connect(m_trend,&BillingTrendChart::pointSelected,this,[this](const QDate &date,qint64 cents,qint64 count) { m_trendSelection=date; m_trendReadout->setText(QStringLiteral("%1 · %2 笔 · %3").arg(date.toString(m_six->isChecked()?QStringLiteral("yyyy 年 M 月"):QStringLiteral("M 月 d 日"))).arg(count).arg(BillingAggregation::money(cents))); m_trendRecords->setText(m_six->isChecked()?QStringLiteral("查看当月记录 →"):QStringLiteral("查看当天记录 →")); m_trendRecords->setVisible(count>0); });
    connect(m_import,&QPushButton::clicked,this,&BillingWorkspace::importRequested); connect(all,&QPushButton::clicked,this,[this] { emit recordsRequested(m_month,QDate(m_month.year(),m_month.month(),m_month.daysInMonth())); }); connect(m_budgetEdit,&QPushButton::clicked,this,[this] { emit budgetRequested(m_month); }); connect(m_budgetSummaryButton,&QPushButton::clicked,this,[this] { emit budgetRequested(m_month); }); connect(m_composer,&ExpenseComposer::settingsRequested,this,&BillingWorkspace::settingsRequested); connect(m_quickRecord,&QPushButton::clicked,this,[this] { showView(false); m_composer->setFocus(); });
    connect(m_composer,&ExpenseComposer::expenseSaved,this,[this](qint64 id,const QDate &date) { m_month=QDate(date.year(),date.month(),1); { const QSignalBlocker blocker(m_monthPicker); m_monthPicker->setDate(m_month); } emit monthChanged(m_month); emit expenseSaved(id); });
    auto *timer=new QTimer(this); timer->setInterval(60000); connect(timer,&QTimer::timeout,this,[this] { if (isVisible() && m_available) refresh(); }); timer->start();
}
void BillingWorkspace::setDatabaseAvailable(bool available) { m_available=available; m_composer->setDatabaseAvailable(available); for (auto *widget:QList<QWidget *>{m_monthPicker,m_recordTab,m_chartTab,m_budgetEdit,m_budgetSummaryButton,m_import}) widget->setEnabled(available); if (available) refresh(); setMaximumWidth(m_chartTab->isChecked()?960:820); if (!available) showLoadError(QStringLiteral("暂时无法读取本机账单，请检查数据保存状态。")); }
bool BillingWorkspace::refresh()
{
    if (!m_available) return false; QString error; WorkflowStorage::Transaction read(m_database.database(),error); if (!read.ok()) { showLoadError(error); return false; }
    QList<Transaction> records; TransactionRepository expenses(m_database); BudgetRepository budgets(m_database); MonthlyBudgetStatus budget; BillingSnapshot snapshot;
    if (!expenses.findAll(records) || !BillingAggregation::calculate(records,m_month,snapshot,error) || !budgets.loadStatus(m_month,budget)) { showLoadError(!expenses.lastError().isEmpty()?expenses.lastError():!error.isEmpty()?error:budgets.lastError()); return false; }
    if (!read.commit()) { showLoadError(error); return false; } m_records=records; m_snapshot=snapshot; m_budget=budget; m_hasSnapshot=true; m_error->hide(); render(); return true;
}
bool BillingWorkspace::setMonth(const QDate &value)
{
    if (!value.isValid()) return false; const QDate candidate(value.year(),value.month(),1),previous=m_month; m_month=candidate;
    if (m_available && !refresh()) { m_month=previous; const QSignalBlocker blocker(m_monthPicker); m_monthPicker->setDate(previous); return false; }
    { const QSignalBlocker blocker(m_monthPicker); m_monthPicker->setDate(candidate); } const auto today=QDate::currentDate(); m_composer->suggestDate(today.year()==candidate.year() && today.month()==candidate.month()?today:candidate); if (previous!=candidate) emit monthChanged(candidate); return true;
}
void BillingWorkspace::showLoadError(const QString &message) { m_error->setText(message+ (m_hasSnapshot?QStringLiteral(" 仍显示上次成功的数据。"):QString())); m_error->show(); }
void BillingWorkspace::resetAfterRestore() { m_composer->resetAfterRestore(); refresh(); }
void BillingWorkspace::showView(bool charts)
{
    m_views->setCurrentIndex(charts?1:0); m_recordTab->setChecked(!charts); m_chartTab->setChecked(charts);
    m_quickRecord->setVisible(charts); m_import->setVisible(!charts); m_headerStats->setVisible(!charts);
    setMaximumWidth(charts?960:820); updateGeometry();
}
void BillingWorkspace::showChart(int view)
{
    m_chartViews->setCurrentIndex(view==0?0:1); m_calendarTab->setChecked(view==0); m_trendTab->setChecked(view==1); m_six->setChecked(view==2);
    m_trend->setSixMonths(view==2); renderChartSummary(); m_chartViews->updateGeometry(); m_views->updateGeometry(); updateGeometry();
}
void BillingWorkspace::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event); m_sidebarLayout->setDirection(width()>=700?QBoxLayout::LeftToRight:QBoxLayout::TopToBottom);
}
void BillingWorkspace::renderChartSummary()
{
    if (!m_hasSnapshot) return;
    const bool six=m_six->isChecked(); qint64 total=m_snapshot.totalCents,count=m_snapshot.records.size(),highest=0; QDate peak;
    if (six) { total=0; count=0; }
    for (const auto &point:m_trend->points()) {
        if (six) { total+=point.cents; count+=point.count; }
        if (point.cents>highest) { highest=point.cents; peak=point.month; }
    }
    m_chartTitle->setText(m_calendarTab->isChecked()?QStringLiteral("消费日历"):six?QStringLiteral("近半年趋势"):QStringLiteral("每日支出趋势"));
    m_chartAmount->setText(BillingAggregation::money(total));
    const auto range=six?m_snapshot.trend.first().month.toString("yyyy.MM")+QStringLiteral(" — ")+m_month.toString("yyyy.MM"):m_month.toString(QStringLiteral("yyyy 年 M 月"));
    m_chartHint->setText(QStringLiteral("%1 · %2 笔支出").arg(range).arg(count));
    m_chartHint->setMinimumWidth(m_chartHint->fontMetrics().horizontalAdvance(m_chartHint->text())+8);
    m_trendSummary->setText(highest?QStringLiteral("%1 %2 · %3").arg(six?QStringLiteral("最高月份"):QStringLiteral("最高单日"),peak.toString(six?QStringLiteral("yyyy 年 M 月"):QStringLiteral("M 月 d 日")),BillingAggregation::money(highest)):QString());
    m_trendSelection=QDate(); m_trendReadout->setText(total>0?QStringLiteral("点击图中位置查看记录"):QStringLiteral("选择其他月份，或先记下一笔。")); m_trendRecords->hide();
}
void BillingWorkspace::render()
{
    if (!m_hasSnapshot) return; m_total->setText(BillingAggregation::money(m_snapshot.totalCents)); m_today->setText(BillingAggregation::money(m_snapshot.todayCents)); m_count->setText(QStringLiteral("%1 笔").arg(m_snapshot.records.size())); m_recentCount->setText(QStringLiteral("%1 笔").arg(m_snapshot.records.size())); m_calendar->setSnapshot(m_snapshot); m_trend->setSnapshot(m_snapshot); m_composer->setShowExamples(m_records.isEmpty()); renderRecent(); renderBudget(); renderDay(m_calendar->selectedDate());
    renderChartSummary();
}
void BillingWorkspace::renderRecent()
{
    clearLayout(m_recentRows); const auto recent=m_snapshot.records.mid(0,3); for (const auto &record:recent) { auto *button=new RecentButton(record,this); m_recentRows->addWidget(button); connect(button,&QPushButton::clicked,this,[this,id=record.id()] { showDetails(id); }); } if (recent.isEmpty()) { auto *empty=ShellWidgets::label(QStringLiteral("这个月还没有记录，先记下第一笔。"),this,"muted"); empty->setObjectName("billingRecentEmptyLabel"); empty->setContentsMargins(18,12,18,12); empty->setMinimumHeight(44); m_recentRows->addWidget(empty); empty->show(); }
    findChild<QLabel *>("billingRecentFootnote")->setVisible(!recent.isEmpty());
}
void BillingWorkspace::renderBudget()
{
    if (!m_budget.hasBudget) {
        m_budgetSummary->setText(QStringLiteral("还没有设置本月预算"));
        m_budgetSummaryButton->setText(QStringLiteral("设置预算 →"));
        AppTheme::setTone(m_budgetSummary,"muted");
    } else {
        const auto remaining=m_budget.remainingCents();
        m_budgetSummary->setText((remaining<0?QStringLiteral("本月预算已超出 "):QStringLiteral("本月预算剩余 "))+BillingAggregation::money(qAbs(remaining)));
        m_budgetSummaryButton->setText(QStringLiteral("预算详情 →"));
        AppTheme::setTone(m_budgetSummary,remaining<0?"danger":"muted");
    }
    findChild<QLabel *>("billingBudgetMonthLabel")->setText(QStringLiteral("%1 月").arg(m_month.month()));
    if (!m_budget.hasBudget) { m_budgetState->setText(QStringLiteral("本月尚未设置预算")); m_budgetBalance->setText(QStringLiteral("—")); m_budgetTotal->setText(QStringLiteral("已记支出 %1").arg(BillingAggregation::money(m_snapshot.totalCents))); AppTheme::setTone(m_budgetBalance,""); }
    else { const auto remaining=m_budget.remainingCents(); m_budgetState->setText(remaining<0?QStringLiteral("已超出总预算"):QStringLiteral("本月还可使用")); m_budgetBalance->setText(BillingAggregation::money(qAbs(remaining))); AppTheme::setTone(m_budgetState,remaining<0?"danger":"muted"); AppTheme::setTone(m_budgetBalance,remaining<0?"danger":""); m_budgetTotal->setText(QStringLiteral("已花 %1 / 额度 %2").arg(BillingAggregation::money(m_budget.spentCents,true),BillingAggregation::money(m_budget.limitCents,true))); }
    const auto scope=[&](QLabel *remainingLabel,QLabel *usedLabel,QProgressBar *bar,qint64 used,qint64 limit) { const auto remaining=limit-used; remainingLabel->setText(m_budget.hasSplit?(remaining<0?QStringLiteral("超 "):QStringLiteral("余 "))+BillingAggregation::money(qAbs(remaining),true):QStringLiteral("未分配")); usedLabel->setText(QStringLiteral("已用 %1%2").arg(BillingAggregation::money(used,true),m_budget.hasSplit?QStringLiteral(" / ")+BillingAggregation::money(limit,true):QString())); bar->setVisible(m_budget.hasSplit); bar->setValue(limit>0?int(qBound(0.0,double(used)/limit*1000,1000.0)):(used>0?1000:0)); AppTheme::setTone(remainingLabel,m_budget.hasSplit && remaining<0?"danger":"muted"); bar->setProperty("overBudget",m_budget.hasSplit && remaining<0); bar->style()->unpolish(bar); bar->style()->polish(bar); };
    scope(m_basicRemaining,m_basicUsed,m_basicBar,m_budget.basicSpentCents,m_budget.basicLimitCents); scope(m_discRemaining,m_discUsed,m_discBar,m_budget.discretionarySpentCents,m_budget.discretionaryLimitCents); if (m_budget.hasBudget && !m_budget.hasSplit) m_budgetState->setText(QStringLiteral("旧总预算尚未分配，请调整预算"));
    clearLayout(m_categoryRows); QList<QPair<QString,qint64>> categories; for (const auto &category:Transaction::categories()) { qint64 cents=0; for (const auto &record:m_snapshot.records) if (record.category()==category) cents+=record.amountCents(); if (cents>0) categories.append({category,cents}); } std::stable_sort(categories.begin(),categories.end(),[](const auto &a,const auto &b) { return a.second>b.second; });
    for (const auto &category:categories.mid(0,3)) { auto *row=new QWidget(this); auto *line=new QHBoxLayout(row); line->setContentsMargins(0,0,0,0); line->setSpacing(8); auto *name=ShellWidgets::label(category.first,row); name->setMinimumWidth(29); line->addWidget(name); auto *bar=new QProgressBar(row); bar->setProperty("billingProgress",true); bar->setTextVisible(false); bar->setRange(0,1000); bar->setValue(m_snapshot.totalCents?int(double(category.second)/m_snapshot.totalCents*1000):0); line->addWidget(bar,1); auto *value=amountLabel(row,"billingCategoryAmount"); value->setText(BillingAggregation::money(category.second)); line->addWidget(value); m_categoryRows->addWidget(row); } if (categories.isEmpty()) m_categoryRows->addWidget(ShellWidgets::label(QStringLiteral("还没有分类支出。"),this,"muted"));
}
void BillingWorkspace::renderDay(const QDate &date)
{
    clearLayout(m_dayRows); if (!m_hasSnapshot || !date.isValid()) return;
    qint64 count=0,cents=0;
    for (const auto &record:m_snapshot.records) if (record.date()==date) { ++count; cents+=record.amountCents(); }
    auto *row=new QWidget; auto *line=new QHBoxLayout(row); line->setContentsMargins(0,0,0,0); line->setSpacing(12);
    auto *heading=ShellWidgets::label(QStringLiteral("%1 · %2 笔 · %3").arg(date.toString(QStringLiteral("M 月 d 日"))).arg(count).arg(BillingAggregation::money(cents)),row);
    heading->setObjectName("billingDaySummaryLabel"); line->addWidget(heading,1);
    if (count>0) {
        auto *open=new QPushButton(QStringLiteral("查看当天记录 →"),row); open->setObjectName("billingDayRecordsButton"); open->setProperty("billingQuiet",true); line->addWidget(open);
        connect(open,&QPushButton::clicked,this,[this,date] { emit recordsRequested(date,date); });
    }
    m_dayRows->addWidget(row); row->show();
}
void BillingWorkspace::showDetails(qint64 id)
{
    if (!m_available) return; const auto found=std::find_if(m_records.cbegin(),m_records.cend(),[id](const auto &record) { return record.id()==id; }); if (found==m_records.cend()) return; const auto record=*found;
    auto *dialog=new QDialog(this,Qt::Dialog|Qt::FramelessWindowHint); dialog->setObjectName("billingTransactionDetailsDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setAttribute(Qt::WA_TranslucentBackground); dialog->setStyleSheet("QDialog#billingTransactionDetailsDialog { background: transparent; }");
    auto *layout=new QVBoxLayout(dialog); layout->setContentsMargins(0,0,0,0); auto *frame=new QFrame(dialog); frame->setProperty("billingCard",true); auto *column=new QVBoxLayout(frame); column->setContentsMargins(24,22,24,22); column->setSpacing(15); auto *header=new QHBoxLayout; auto *title=ShellWidgets::label(record.description(),frame); title->setProperty("billingSubtitle",true); header->addWidget(title,1); auto *close=new QPushButton(QStringLiteral("×"),frame); close->setProperty("billingQuiet",true); header->addWidget(close); column->addLayout(header);
    auto *amount=amountLabel(frame,"billingDetailAmountLabel"); amount->setProperty("billingBalance",true); amount->setText(BillingAggregation::money(record.amountCents())); column->addWidget(amount); column->addWidget(ShellWidgets::label(QStringLiteral("%1\n%2\n%3 · %4").arg(record.date().toString(Qt::ISODate),record.time().isValid()?record.time().toString("HH:mm:ss"):QStringLiteral("时间未记录"),CategoryCatalog::label(record.detailCategory()),CategoryCatalog::isBasic(record.detailCategory())?QStringLiteral("基础生活"):QStringLiteral("可支配")),frame,"muted"));
    auto *note=ShellWidgets::label(record.note().isEmpty()?QStringLiteral("未填写备注"):record.note(),frame); note->setTextInteractionFlags(Qt::TextSelectableByMouse); column->addWidget(note); if (!record.sourceImage().isEmpty()) column->addWidget(screenshotButton(frame,record.sourceImage())); column->addStretch(); auto *remove=new QPushButton(QStringLiteral("删除这笔"),frame); remove->setObjectName("billingDetailDeleteButton"); auto *edit=new QPushButton(QStringLiteral("修改这笔"),frame); edit->setObjectName("billingDetailEditButton"); edit->setProperty("primary",true); auto *buttons=new QHBoxLayout; buttons->addWidget(remove); buttons->addStretch(); buttons->addWidget(edit); column->addLayout(buttons); layout->addWidget(frame); connect(close,&QPushButton::clicked,dialog,&QDialog::reject); connect(edit,&QPushButton::clicked,dialog,[this,dialog,id] { dialog->accept(); emit editRequested(id); }); connect(remove,&QPushButton::clicked,dialog,[this,dialog,id] { dialog->accept(); emit deleteRequested(id); });
    if (record.category()!=QStringLiteral("餐饮") && record.category()!=QStringLiteral("交通")) {
    auto *product=new QPushButton(QStringLiteral("商品详情与 AI 讨论 →"),frame); product->setObjectName("billingProductDetailsButton"); product->setProperty("billingQuiet",true); column->insertWidget(1,product);
    connect(product,&QPushButton::clicked,dialog,[this,id,dialog] { ProductDetailsDialog::openFor(m_database,"transaction",id,dialog); });
    }
    auto *owner=window(); const auto area=owner->rect(); const int height=qMax(380,qMin(640,area.height()-105)); dialog->resize(410,height); const QPoint position=owner->mapToGlobal(QPoint(qMax(10,area.width()-430),75)); dialog->move(position); dialog->open();
}
