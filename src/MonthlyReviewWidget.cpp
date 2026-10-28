#include "ModernSelectors.h"
#include "MonthlyReviewWidget.h"
#include "ProductDetailsDialog.h"
#include "ProductList.h"
#include "ShellWidgets.h"
#include "AppTheme.h"
#include "CategoryCatalog.h"
#include "Transaction.h"
#include "ReviewRepository.h"
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolButton>
#include <QTextBrowser>

MonthlyReviewWidget::MonthlyReviewWidget(DatabaseManager &database,const QString &settingsPath,QWidget *parent):QWidget(parent),m_database(database),m_repository(database),m_ai(this),m_settingsPath(settingsPath)
{
    setObjectName("monthlyReviewWidget");
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(28,28,28,26); layout->setSpacing(20);
    auto *heading=new QHBoxLayout;
    m_title=ShellWidgets::label(QStringLiteral("复盘"),this);
    m_title->setObjectName("monthlyReviewTitleLabel"); heading->addWidget(m_title,1);
    const auto monthButton=[&](const QString &text,const char *name,const QString &accessible) {
        auto *button=new QPushButton(text,this); button->setObjectName(QLatin1String(name));
        button->setProperty("reviewMonthArrow",true); button->setFixedSize(30,36);
        button->setAccessibleName(accessible); button->setToolTip(accessible); return button;
    };
    m_prev=monthButton(QStringLiteral("‹"),"previousReviewMonthButton",QStringLiteral("上个月"));
    m_next=monthButton(QStringLiteral("›"),"nextReviewMonthButton",QStringLiteral("下个月"));
    m_month=new QDateEdit(MonthlyBudget::firstDay(QDate::currentDate()),this);
    m_month->setObjectName("monthlyReviewMonthEdit"); m_month->setDateRange(QDate(1,1,1),QDate(9999,12,31));
    m_month->setDisplayFormat(QStringLiteral("yyyy年M月")); m_month->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_month->setAlignment(Qt::AlignCenter); m_month->setFixedWidth(122); m_month->setKeyboardTracking(false);
    m_month->setAccessibleName(QStringLiteral("复盘月份"));
    m_month->setToolTip(QStringLiteral("可直接修改年份或月份，左右箭头切换月份。"));
    heading->setSpacing(5); heading->addWidget(m_prev); heading->addWidget(m_month); heading->addWidget(m_next);
    layout->addLayout(heading);
    m_dueReviews=new QPushButton(this); m_dueReviews->setObjectName("monthlyDueReviewsButton");
    m_dueReviews->setProperty("reviewLink",true); m_dueReviews->hide();
    layout->addWidget(m_dueReviews,0,Qt::AlignLeft);
    connect(m_dueReviews,&QPushButton::clicked,this,&MonthlyReviewWidget::purchaseReviewRequested);

    QFrame *story; auto *storyLayout=ShellWidgets::card(this,&story);
    story->setObjectName("monthlyStoryCard"); storyLayout->setContentsMargins(28,22,28,24); storyLayout->setSpacing(12);
    auto *storyHeading=new QHBoxLayout;
    m_personaBadge=ShellWidgets::label(QStringLiteral("本月故事"),story);
    m_personaBadge->setObjectName("monthlyStoryPersonaLabel"); storyHeading->addWidget(m_personaBadge,1);
    m_generate=new QPushButton(QStringLiteral("生成本月故事"),story); m_generate->setObjectName("generateMonthlyStoryButton");
    m_generate->setProperty("reviewLink",true);
    m_generate->setToolTip(QStringLiteral("使用已配置 AI 生成动物人格卡；发送月度汇总、个人标记和最多12笔消费简要信息。"));
    m_cancel=new QPushButton(QStringLiteral("取消生成"),story); m_cancel->setObjectName("cancelMonthlyStoryButton"); m_cancel->setProperty("reviewLink",true);
    storyHeading->addWidget(m_generate); storyHeading->addWidget(m_cancel);
    m_more=new QToolButton(story); m_more->setObjectName("monthlyStoryMoreButton"); m_more->setText(QStringLiteral("···"));
    m_more->setAccessibleName(QStringLiteral("更多故事操作")); m_more->setToolTip(QStringLiteral("更多故事操作"));
    m_more->setPopupMode(QToolButton::InstantPopup);
    auto *menu=new QMenu(m_more);
    m_regenerate=menu->addAction(QStringLiteral("重新生成故事（AI）")); m_regenerate->setObjectName("regenerateMonthlyStoryAction");
    m_local=menu->addAction(QStringLiteral("生成本地总结（无需 AI）"));
    m_local->setObjectName("saveLocalMonthlyStoryAction"); m_more->setMenu(menu); storyHeading->addWidget(m_more);
    storyLayout->addLayout(storyHeading);
    m_storyTitle=ShellWidgets::label(QStringLiteral("这个月，还没有记录。"),story);
    m_storyTitle->setObjectName("monthlyStoryTitleLabel"); storyLayout->addWidget(m_storyTitle);
    m_storyText=ShellWidgets::label(QStringLiteral("记下一笔消费或购买选择，就能开启本月人格故事。"),story);
    m_storyText->setObjectName("monthlyStoryText"); m_storyText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_storyText->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum); storyLayout->addWidget(m_storyText);
    m_interlude=ShellWidgets::label(QString(),story); m_interlude->setObjectName("monthlyStoryInterludeLabel"); storyLayout->addWidget(m_interlude); m_interlude->hide();
    m_nextTask=ShellWidgets::label(QString(),story); m_nextTask->setObjectName("monthlyStoryNextStepLabel"); storyLayout->addWidget(m_nextTask); m_nextTask->hide();
    m_expandStory=new QPushButton(QStringLiteral("展开全文"),story); m_expandStory->setObjectName("monthlyStoryExpandButton");
    m_expandStory->setProperty("reviewLink",true); m_expandStory->setCheckable(true); m_expandStory->hide();
    storyLayout->addWidget(m_expandStory,0,Qt::AlignLeft);
    connect(m_expandStory,&QPushButton::toggled,this,&MonthlyReviewWidget::renderStoryBody);
    m_previousStory=new QPushButton(QStringLiteral("查看上次故事"),story); m_previousStory->setObjectName("monthlyPreviousStoryButton"); m_previousStory->setProperty("reviewLink",true);
    storyLayout->addWidget(m_previousStory,0,Qt::AlignLeft); m_previousStory->hide();
    connect(m_previousStory,&QPushButton::clicked,this,&MonthlyReviewWidget::showPreviousStory);
    m_source=ShellWidgets::label(QString(),story,"muted"); m_source->setObjectName("monthlyStorySourceLabel");
    storyLayout->addWidget(m_source); layout->addWidget(story);

    auto *stats=new QWidget(this); stats->setObjectName("monthlyReviewMetrics");
    auto *statsLayout=new QHBoxLayout(stats); statsLayout->setContentsMargins(0,0,0,0); statsLayout->setSpacing(24);
    const auto figure=[&](const QString &caption,const char *name,const QString &unit=QString()) {
        auto *column=new QVBoxLayout; column->setSpacing(5);
        column->addWidget(ShellWidgets::label(caption,stats,"eyebrow"));
        auto *value=ShellWidgets::label(QStringLiteral("—"),stats); value->setObjectName(QLatin1String(name));
        value->setProperty("reviewFigure",true); value->setWordWrap(false);
        auto *number=new QHBoxLayout; number->setSpacing(5); number->addWidget(value,0,Qt::AlignBaseline);
        if (!unit.isEmpty()) number->addWidget(ShellWidgets::label(unit,stats,"muted"),0,Qt::AlignBaseline);
        number->addStretch(); column->addLayout(number);
        statsLayout->addLayout(column,1); return value;
    };
    const auto divider=[&] { auto *line=new QFrame(stats); line->setProperty("reviewDivider",true); line->setFixedWidth(1); statsLayout->addWidget(line); };
    m_spent=figure(QStringLiteral("本月支出"),"monthlySpentLabel"); divider();
    m_resists=figure(QStringLiteral("忍住了"),"monthlyResistCountLabel",QStringLiteral("次")); divider();
    m_saved=figure(QStringLiteral("忍住的金额"),"monthlyResistedAmountLabel");
    stats->setToolTip(QStringLiteral("按消费日期统计支出；忍住记录按本机时区统计，其金额不代表实际收入。"));
    layout->addWidget(stats);

    QFrame *boards; auto *boardsLayout=ShellWidgets::card(this,&boards);
    boards->setObjectName("monthlyReviewBoardsCard"); boardsLayout->setContentsMargins(26,16,26,8); boardsLayout->setSpacing(12);
    auto *boardHeading=new QHBoxLayout; boardHeading->setSpacing(6);
    auto *tabs=new QButtonGroup(this); tabs->setExclusive(true);
    m_worthTab=new QPushButton(QStringLiteral("值得  0"),boards); m_worthTab->setObjectName("monthlyWorthTabButton");
    m_regretTab=new QPushButton(QStringLiteral("后悔  0"),boards); m_regretTab->setObjectName("monthlyRegretTabButton");
    for (auto *button:{m_worthTab,m_regretTab}) { button->setCheckable(true); button->setProperty("reviewTab",true); tabs->addButton(button); boardHeading->addWidget(button); }
    m_worthTab->setChecked(true); boardHeading->addStretch();
    m_tags=new QPushButton(QStringLiteral("＋ 标记一笔"),boards); m_tags->setObjectName("manageMonthlyTagsButton"); m_tags->setProperty("reviewLink",true);
    boardHeading->addWidget(m_tags); boardsLayout->addLayout(boardHeading);
    auto *boardLine=new QFrame(boards); boardLine->setProperty("reviewDivider",true); boardLine->setFixedHeight(1); boardsLayout->addWidget(boardLine);
    // Hiding the inactive list lets the card shrink to the selected content.
    m_worth=new QWidget(boards); m_worth->setObjectName("monthlyWorthBoard");
    m_regret=new QWidget(boards); m_regret->setObjectName("monthlyRegretBoard");
    for (auto *page:{m_worth,m_regret}) { auto *rows=new QVBoxLayout(page); rows->setContentsMargins(0,0,0,0); rows->setSpacing(0); boardsLayout->addWidget(page); }
    layout->addWidget(boards);
    const auto selectBoard=[this](int index) {
        m_worth->setVisible(index==0); m_regret->setVisible(index==1);
    };
    connect(m_worthTab,&QPushButton::clicked,this,[selectBoard] { selectBoard(0); });
    connect(m_regretTab,&QPushButton::clicked,this,[selectBoard] { selectBoard(1); }); selectBoard(0);

    auto *links=new QHBoxLayout; links->addStretch(); links->setSpacing(20);
    auto *purchase=new QPushButton(QStringLiteral("购买后复盘 ↗"),this); purchase->setObjectName("monthlyPurchaseReviewButton"); purchase->setProperty("reviewLink",true);
    auto *statistics=new QPushButton(QStringLiteral("年度与分类统计 ↗"),this); statistics->setObjectName("monthlyStatisticsButton"); statistics->setProperty("reviewLink",true);
    links->addWidget(purchase); links->addWidget(statistics); layout->addLayout(links);
    connect(purchase,&QPushButton::clicked,this,&MonthlyReviewWidget::purchaseReviewRequested);
    connect(statistics,&QPushButton::clicked,this,[this] { emit statisticsRequested(m_month->date()); });
    m_error=ShellWidgets::label(QString(),story,"error"); m_error->setObjectName("monthlyReviewErrorLabel"); m_error->hide(); storyLayout->addWidget(m_error);
    connect(m_prev,&QPushButton::clicked,this,[this] { const auto date=MonthlyBudget::firstDay(m_month->date()).addMonths(-1); if (date.isValid()) m_month->setDate(date); });
    connect(m_next,&QPushButton::clicked,this,[this] { const auto date=MonthlyBudget::firstDay(m_month->date()).addMonths(1); if (date.isValid()) m_month->setDate(date); });
    connect(m_month,&QDateEdit::dateChanged,this,[this] { cancelAnalysis(); refresh(); });
    connect(m_local,&QAction::triggered,this,[this] { generate(false); });
    connect(m_generate,&QPushButton::clicked,this,[this] { generate(true); });
    connect(m_regenerate,&QAction::triggered,this,[this] { generate(true); });
    connect(m_cancel,&QPushButton::clicked,this,&MonthlyReviewWidget::cancelAnalysis);
    connect(m_tags,&QPushButton::clicked,this,&MonthlyReviewWidget::manageTags);
    connect(&m_ai,&AIService::failed,this,[this](const QString &error) { m_busy=false; m_error->setText(error); m_error->show(); updateControls(); });
    connect(&m_ai,&AIService::monthlyReady,this,[this](const MonthlyAnalysis &analysis) { m_busy=false; if (!m_repository.saveStory(m_requestedMonth,m_requestedHash,analysis.story,analysis.persona,"ai")) { m_error->setText(m_repository.lastError()); m_error->show(); updateControls(); return; } refresh(); }); updateControls();
}
void MonthlyReviewWidget::setDatabaseAvailable(bool available) { m_available=available; if (available) refresh(); else cancelAnalysis(); updateControls(); }
void MonthlyReviewWidget::cancelAnalysis() { m_ai.cancel(); m_busy=false; updateControls(); }
bool MonthlyReviewWidget::refresh()
{
    if (!m_available) return false; MonthlyReviewData data; MonthlyStory story;
    if (!m_repository.load(m_month->date(),data) || !m_repository.loadStory(data,story)) { m_error->setText(m_repository.lastError()+QStringLiteral(" 仍保留上次成功结果。")); m_error->show(); if (m_hasSnapshot) { const QSignalBlocker block(m_month); m_month->setDate(m_data.month); } updateControls(); return false; }
    const bool changed=m_busy && (data.month!=m_requestedMonth || data.factsHash!=m_requestedHash);
    if (changed) cancelAnalysis();
    if (data.month!=m_data.month || story.story!=m_story.story || story.stale!=m_story.stale) {
        const QSignalBlocker block(m_expandStory); m_expandStory->setChecked(false);
    }
    m_data=data; m_story=story; m_hasSnapshot=true; const QSignalBlocker block(m_month); m_month->setDate(data.month); render();
    if (changed) { m_error->setText(QStringLiteral("数据已变化，已取消旧快照的生成，请重新生成。")); m_error->show(); } else m_error->hide(); updateControls(); return true;
}
void MonthlyReviewWidget::updateControls()
{
    const bool ready=m_available && m_hasSnapshot;
    const auto first=MonthlyBudget::firstDay(m_month->date());
    m_month->setEnabled(m_available);
    m_prev->setEnabled(m_available && first.addMonths(-1) >= m_month->minimumDate());
    m_next->setEnabled(m_available && first.addMonths(1).isValid() && first.addMonths(1) <= m_month->maximumDate());
    m_local->setEnabled(ready && !m_busy && m_data.hasData());
    m_more->setVisible(ready && m_data.hasData()); m_more->setEnabled(!m_busy);
    m_generate->setEnabled(ready && !m_busy && m_data.hasData() && m_data.month<=MonthlyBudget::firstDay(QDate::currentDate()));
    m_regenerate->setEnabled(m_generate->isEnabled()); m_regenerate->setVisible(m_story.id!=0);
    m_cancel->setVisible(m_busy); m_cancel->setEnabled(m_busy);
    m_tags->setEnabled(ready && !m_data.expenses.isEmpty());
    m_generate->setText(m_busy?QStringLiteral("生成中…"):m_story.id?QStringLiteral("重新生成"):QStringLiteral("生成本月故事"));
    m_generate->setVisible(m_busy || (ready && m_data.hasData() && !m_story.id));
    m_dueReviews->setEnabled(ready);
}
void MonthlyReviewWidget::render()
{
    m_spent->setText(Transaction::formatAmount(m_data.totalCents));
    m_resists->setText(QString::number(m_data.savingsCount));
    m_saved->setText(Transaction::formatAmount(m_data.savingsCents));
    m_personaBadge->setText(QStringLiteral("本月故事")); m_previousStory->hide();
    m_fullStory.clear(); m_storyInterlude.clear(); m_storyNextStep.clear();
    ReviewRepository reviews(m_database); qint64 due=0;
    if (reviews.dueCount(due)) {
        m_dueReviews->setText(QStringLiteral("%1 项购买后复盘到期 · 去复盘 →").arg(due));
        m_dueReviews->setVisible(due>0);
    } else m_dueReviews->hide();
    if (m_data.hasData()) {
        const auto preview=MonthlyStoryTemplate::local(m_data);
        m_personaBadge->setText(MonthlyStoryTemplate::personaSymbol(preview.personaId)+" "+MonthlyStoryTemplate::personaCaption(preview));
    }
    const auto body=[this](const QString &text) {
        m_fullStory=text;
    };
    ReviewStoryContent content; QString formatError;
    const bool structured=m_story.id && MonthlyStoryTemplate::decode(m_story.story,content,formatError);
    const bool legacyPending=structured && content.personaId=="pending" && m_data.hasData();
    if (legacyPending) content=MonthlyStoryTemplate::local(m_data);
    if (m_story.id && m_story.stale) {
        m_storyTitle->setText(QStringLiteral("记录翻页了，故事也该更新了。"));
        body(QStringLiteral("本月的消费或评价有了变化。重新生成，看看现在的你会是哪位动物主角。"));
        m_previousStory->setText(QStringLiteral("查看上次故事")); m_previousStory->show();
    } else if (structured) {
        m_personaBadge->setText(MonthlyStoryTemplate::personaSymbol(content.personaId)+" "+MonthlyStoryTemplate::personaCaption(content));
        m_storyTitle->setText(content.title); body(content.story);
        m_storyInterlude=content.interlude; m_storyNextStep=content.nextStep;
        if (legacyPending) m_previousStory->show();
    } else if (m_story.id) {
        m_storyTitle->setText(m_data.month.toString(QStringLiteral("M月的消费故事")));
        const int firstBreak=m_story.story.indexOf('\n');
        const QString headline=firstBreak>0?m_story.story.left(firstBreak).trimmed():QString();
        if (!headline.isEmpty() && headline.size()<=40 && !m_story.story.mid(firstBreak+1).trimmed().isEmpty()) {
            m_storyTitle->setText(headline);
            body(m_story.story.mid(firstBreak+1).trimmed());
        } else body(m_story.story.startsWith('{')?QStringLiteral("这是一份旧格式的故事，可以重新生成动物人格卡。"):m_story.story);
        m_previousStory->setText(QStringLiteral("查看完整旧版故事")); m_previousStory->setVisible(m_story.story.startsWith('{'));
    } else {
        m_storyTitle->setText(m_data.hasData()?QStringLiteral("这个月，你会是哪位动物主角？"):QStringLiteral("这个月，还没有记录。"));
        body(m_data.hasData()?QStringLiteral("从真实消费里挑一两个小片段，写成你的本月人格故事。"):QStringLiteral("记下一笔消费或购买选择，就能开启本月人格故事。"));
    }
    if (m_story.id) {
        m_source->setText(QStringLiteral("%1 · %2").arg(m_story.source=="ai"?QStringLiteral("AI 解读"):QStringLiteral("本地事实总结"),m_story.stale?QStringLiteral("数据已变化，待更新"):QStringLiteral("与当前数据一致")));
        if (m_data.hasData() && (m_story.stale || legacyPending)) m_source->setText(m_story.stale?QStringLiteral("本地人格预览 · 数据已变化，请更新故事"):QStringLiteral("本地人格预览 · 可重新生成故事"));
        if (!structured && !m_story.stale) m_source->setText(m_source->text()+QStringLiteral(" · 旧版故事，可重新生成"));
        m_source->setToolTip(m_story.generatedAt.toLocalTime().toString("yyyy-MM-dd HH:mm")+"\n"+m_story.persona);
        AppTheme::setTone(m_source,m_story.stale?"warning":"muted");
        m_source->show();
    } else {
        m_source->setText(m_data.hasData()?QStringLiteral("本地人格预览 · 点击生成本月故事"):QString());
        m_source->setVisible(m_data.hasData());
        AppTheme::setTone(m_source,"muted");
    }
    renderStoryBody(); renderBoards();
}
void MonthlyReviewWidget::renderStoryBody()
{
    const bool canExpand=m_fullStory.size()>100 || !m_storyInterlude.isEmpty() || !m_storyNextStep.isEmpty();
    const bool expanded=canExpand && m_expandStory->isChecked();
    const QString text=expanded || m_fullStory.size()<=100?m_fullStory:m_fullStory.left(100).trimmed()+QStringLiteral("…");
    m_storyText->setTextFormat(Qt::RichText);
    m_storyText->setText(QStringLiteral("<div style='line-height:160%'>%1</div>").arg(text.toHtmlEscaped().replace('\n',QStringLiteral("<br>"))));
    m_interlude->setText(QStringLiteral("本月小插曲 · ")+m_storyInterlude);
    m_nextTask->setText(QStringLiteral("下个月的小任务 · ")+m_storyNextStep);
    m_interlude->setVisible(expanded && !m_storyInterlude.isEmpty());
    m_nextTask->setVisible(expanded && !m_storyNextStep.isEmpty());
    m_expandStory->setVisible(canExpand);
    m_expandStory->setText(expanded?QStringLiteral("收起全文"):QStringLiteral("展开全文"));
    updateGeometry();
}
void MonthlyReviewWidget::renderBoards()
{
    m_worthTab->setText(QStringLiteral("值得  %1").arg(m_data.worthCount));
    m_regretTab->setText(QStringLiteral("后悔  %1").arg(m_data.regretCount));
    for (int board=0;board<2;++board) {
        auto *page=board==0?m_worth:m_regret;
        auto *rows=qobject_cast<QVBoxLayout *>(page->layout());
        while (auto *item=rows->takeAt(0)) { delete item->widget(); delete item; }
        int count=0;
        for (const auto &expense:m_data.expenses) {
            if (expense.tag!=(board==0?1:-1) || count>=5) continue;
            if (count++) { auto *line=new QFrame(page); line->setProperty("reviewDivider",true); line->setFixedHeight(1); rows->addWidget(line); }
            auto *row=new QWidget(page); row->setObjectName("monthlyReviewExpenseRow");
            auto *rowLayout=new QHBoxLayout(row); rowLayout->setContentsMargins(0,13,0,13); rowLayout->setSpacing(24);
            auto *left=new QVBoxLayout; left->setSpacing(5);
            auto *name=new ProductNameLabel(expense.name,row); name->setWordWrap(true); name->setTextFormat(Qt::PlainText); name->setObjectName("monthlyReviewExpenseNameLabel"); name->setCursor(Qt::PointingHandCursor); name->setToolTip(QStringLiteral("打开商品详情")); name->activated=[this,id=expense.id] { ProductDetailsDialog::openFor(m_database,"transaction",id,this); }; left->addWidget(name);
            if (!expense.note.isEmpty()) { auto *note=ShellWidgets::label(expense.note,row,"muted"); note->setObjectName("monthlyReviewExpenseNoteLabel"); left->addWidget(note); }
            rowLayout->addLayout(left,1);
            auto *right=new QVBoxLayout; right->setSpacing(5);
            auto *price=ShellWidgets::label(Transaction::formatAmount(expense.cents),row); price->setProperty("reviewPrice",true); price->setWordWrap(false); price->setAlignment(Qt::AlignRight); right->addWidget(price);
            auto *date=ShellWidgets::label(expense.date.toString(QStringLiteral("M月d日")),row,"eyebrow"); date->setAlignment(Qt::AlignRight); date->setWordWrap(false); right->addWidget(date);
            rowLayout->addLayout(right); rows->addWidget(row);
        }
        if (!count) { auto *empty=ShellWidgets::label(board==0?QStringLiteral("还没有标记值得的消费。"):QStringLiteral("还没有标记后悔的消费。"),page,"muted"); empty->setObjectName("monthlyReviewEmptyBoardLabel"); empty->setContentsMargins(0,20,0,24); rows->addWidget(empty); }
    }
}
void MonthlyReviewWidget::generate(bool ai)
{
    if (!m_available || m_busy || !refresh()) return;
    if (!ai) {
        const auto content=MonthlyStoryTemplate::local(m_data);
        if (!m_repository.saveStory(m_data.month,m_data.factsHash,MonthlyStoryTemplate::encode(content),MonthlyStoryTemplate::personaCaption(content),"local")) { m_error->setText(m_repository.lastError()); m_error->show(); return; }
        refresh(); return;
    }
    if (!m_data.hasData() || m_data.month>MonthlyBudget::firstDay(QDate::currentDate())) return; m_requestedHash=m_data.factsHash; m_requestedMonth=m_data.month; m_busy=true; m_error->hide(); updateControls(); if (!m_ai.monthly(AISettingsStore::load(m_settingsPath),m_data.factsJson)) { m_busy=false; updateControls(); }
}
void MonthlyReviewWidget::showPreviousStory()
{
    if (!m_story.id) return;
    auto *dialog=new QDialog(this); dialog->setObjectName("previousMonthlyStoryDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("上次保存的故事")); dialog->resize(640,480);
    auto *layout=new QVBoxLayout(dialog); layout->setContentsMargins(24,22,24,22); layout->setSpacing(14);
    layout->addWidget(ShellWidgets::label(m_story.generatedAt.toLocalTime().toString(QStringLiteral("yyyy年M月d日 HH:mm 保存"))+(m_story.stale?QStringLiteral(" · 对应旧记录"):QString()),dialog,"muted"));
    ReviewStoryContent content; QString error;
    auto *text=new QTextBrowser(dialog); text->setObjectName("previousMonthlyStoryText"); text->setOpenExternalLinks(false);
    text->setPlainText(MonthlyStoryTemplate::decode(m_story.story,content,error)?MonthlyStoryTemplate::plainText(content):m_story.story);
    layout->addWidget(text,1); auto *close=new QPushButton(QStringLiteral("关闭"),dialog); layout->addWidget(close,0,Qt::AlignRight);
    connect(close,&QPushButton::clicked,dialog,&QDialog::accept); dialog->open();
}
void MonthlyReviewWidget::manageTags()
{
    if (!m_hasSnapshot || !m_available) return; auto *dialog=new QDialog(this); dialog->setObjectName("monthlyTagsDialog"); dialog->setWindowTitle(QStringLiteral("本月值得／后悔标记")); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(760,570);
    auto *layout=new QVBoxLayout(dialog); layout->addWidget(ShellWidgets::label(QStringLiteral("只记录你的评价，不重复改变品牌权重。"),dialog,"muted")); auto *table=new QTableWidget(m_data.expenses.size(),4,dialog); table->setObjectName("monthlyTagTransactionsTable"); table->setHorizontalHeaderLabels({QStringLiteral("日期"),QStringLiteral("消费"),QStringLiteral("金额"),QStringLiteral("当前标记")}); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->verticalHeader()->hide(); table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i=0;i<m_data.expenses.size();++i) { const auto &expense=m_data.expenses[i]; const QStringList values={expense.date.toString(Qt::ISODate),expense.name,Transaction::formatAmount(expense.cents),expense.tag==1?QStringLiteral("值得"):expense.tag==-1?QStringLiteral("后悔"):QStringLiteral("未标记")}; for (int j=0;j<values.size();++j) table->setItem(i,j,new QTableWidgetItem(values[j])); table->item(i,0)->setData(Qt::UserRole,expense.id); table->item(i,0)->setData(Qt::UserRole+1,expense.tag); table->item(i,0)->setData(Qt::UserRole+2,expense.note); } layout->addWidget(table,1);
    auto *tag=new ModernComboBox(dialog); tag->setObjectName("monthlyTagComboBox"); tag->addItem(QStringLiteral("值得"),1); tag->addItem(QStringLiteral("后悔"),-1); tag->addItem(QStringLiteral("清除标记"),0); auto *note=new QLineEdit(dialog); note->setObjectName("monthlyTagNoteEdit"); note->setMaxLength(500); auto *form=new QFormLayout; form->addRow(QStringLiteral("个人标记"),tag); form->addRow(QStringLiteral("理由（可选）"),note); layout->addLayout(form); auto *status=ShellWidgets::label(QString(),dialog); status->setObjectName("monthlyTagStatusLabel"); layout->addWidget(status); auto *save=new QPushButton(QStringLiteral("保存所选消费的标记"),dialog); save->setObjectName("saveMonthlyTagButton"); save->setEnabled(false); auto *close=new QPushButton(QStringLiteral("完成"),dialog); auto *buttons=new QHBoxLayout; buttons->addWidget(close); buttons->addStretch(); buttons->addWidget(save); layout->addLayout(buttons); connect(close,&QPushButton::clicked,dialog,&QDialog::accept);
    connect(table,&QTableWidget::itemSelectionChanged,dialog,[table,tag,note,save] { const int row=table->currentRow(); const bool selected=row>=0 && table->item(row,0) && table->item(row,0)->isSelected(); save->setEnabled(selected); if (!selected) return; tag->setCurrentIndex(tag->findData(table->item(row,0)->data(Qt::UserRole+1))); note->setText(table->item(row,0)->data(Qt::UserRole+2).toString()); });
    connect(save,&QPushButton::clicked,dialog,[this,table,tag,note,status] { const int row=table->currentRow(); if (row<0 || !table->item(row,0)->isSelected()) return; const auto id=table->item(row,0)->data(Qt::UserRole).toLongLong(); const int value=tag->currentData().toInt(); if (!m_repository.setTag(id,value,note->text())) { status->setText(m_repository.lastError()); AppTheme::setTone(status,"danger"); return; } table->item(row,0)->setData(Qt::UserRole+1,value); table->item(row,0)->setData(Qt::UserRole+2,note->text()); table->item(row,3)->setText(value==1?QStringLiteral("值得"):value==-1?QStringLiteral("后悔"):QStringLiteral("未标记")); status->setText(QStringLiteral("已保存，月度故事如已过期会提示更新。")); AppTheme::setTone(status,"success"); refresh(); emit tagsChanged(); }); dialog->open();
}
