#include "OnboardingTour.h"
#include "AppTheme.h"
#include "ShellWidgets.h"
#include "KuraMascot.h"
#include <QAbstractButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QTimer>
#include <climits>

namespace {
// Page numbers are the public MainWindow::Page values. Only navigation and the
// manual-input mode are activated; the tour never submits a financial form.
struct Step { int page; const char *target; const char *fallback; const char *title; const char *body; bool click; };
const Step steps[]={
    {4,"homeBudgetCard","homePageTitle","先看看我们的小窝", "主页会告诉你还能自由花多少，也放着许愿目标。忍住一次小冲动，就离真正喜欢的东西近一点点。",false},
    {4,"billingNavButton","","第一站，记下花出去的小栗子", "点一下亮着的「账单」，跟我去看看怎么记下第一笔。",true},
    {0,"bookkeepingManualModeButton","billingPageTitle","不用 AI 也能轻松记账", "点一下「手动」试试。填名称、金额、用途和预算归属，再保存，就记好啦。现在只是看看，不会替你记一笔。",true},
    {0,"billingBudgetCard","billingPageTitle","给生活和心动各留一份", "必要生活留给吃饭、通勤这些日常，自由消费留给喜欢的东西。想调整本月额度，就来这张预算卡片。",false},
    {0,"wishlistNavButton","","下一站，心动也有等候区", "点一下「清单」。暂时拿不定主意的宝贝，先来这里坐一会儿。",true},
    {2,"planningTitleLabel","wishlistWidget","想买，不必马上买", "这里按优先级放好你的心愿。点商品名看资料，记价格、等冷静期；小魔法棒可以把它变成许愿目标。",false},
    {2,"executionNavButton","","让我陪你想一想", "点一下「执行」。真要做决定时，我们在这里慢慢拆开看。",true},
    {5,"executionInputCard","executionHeading","喜欢它，也问问自己为什么", "从商品开始，整理用途、预算和替代品，再选快速思考或认真研究。你的购物原则也放在这里；已有分析可以接着做。",false},
    {5,"reviewNavButton","","最后，回头看看收获", "点一下「复盘」。每个月花过的钱，都能留下一点自己的经验。",true},
    {1,"monthlyStoryCard","monthlyReviewTitleLabel","真香和后悔，都值得记住", "把消费标成「值得」或「后悔」，写下原因，再看看这个月的故事。底下还有慢慢收集的小栗子，等你来发现。",false},
    {1,"settingsButton","","小窝的工具箱在这里", "点一下右上角的设置。显示、AI 和备份，都收在这里。",true},
    {6,"settingsHome","settingsTitleLabel","好啦，我把路认熟啦", "想改价格换算、配置 AI 或备份账本，都能来这里。要是忘了路，在「关于与使用说明」叫我，就能重看引导。现在去装下喜欢的生活吧！",false}
};
constexpr int count=sizeof(steps)/sizeof(steps[0]);
}

OnboardingTour::OnboardingTour(QWidget *parent):QWidget(parent)
{
    setObjectName("onboardingTour"); setMouseTracking(true); setFocusPolicy(Qt::StrongFocus);
    m_bubble=new QFrame(this); m_bubble->setObjectName("onboardingBubble");
    const auto color=[this](const char *role) { return AppTheme::color(this,role).name(); };
    m_bubble->setStyleSheet(QString("QFrame#onboardingBubble { background:%1; border:1px solid %2; border-radius:18px; }").arg(color("card"),color("border")));
    auto *layout=new QVBoxLayout(m_bubble); layout->setContentsMargins(20,16,20,18); layout->setSpacing(10);
    auto *heading=new QHBoxLayout;
    auto *mascot=new KuraMascot(m_bubble); mascot->setFixedSize(40,44); heading->addWidget(mascot);
    m_progress=ShellWidgets::label({},m_bubble,"muted"); m_progress->setObjectName("tourProgress"); heading->addWidget(m_progress,1);
    auto *skip=new QPushButton(QStringLiteral("结束导览"),m_bubble); skip->setObjectName("tourSkipButton"); skip->setProperty("link",true); heading->addWidget(skip); layout->addLayout(heading);
    m_title=ShellWidgets::label({},m_bubble,"heading"); m_title->setObjectName("tourTitle"); layout->addWidget(m_title);
    m_body=ShellWidgets::label({},m_bubble); m_body->setObjectName("tourBody"); layout->addWidget(m_body);
    auto *actions=new QHBoxLayout;
    m_previous=new QPushButton(QStringLiteral("上一步"),m_bubble); m_previous->setObjectName("tourPreviousButton"); actions->addWidget(m_previous); actions->addStretch();
    m_next=new QPushButton(m_bubble); m_next->setObjectName("tourNextButton"); m_next->setProperty("primary",true); actions->addWidget(m_next); layout->addLayout(actions);
    connect(skip,&QPushButton::clicked,this,[this] { emit finished(false); });
    connect(m_previous,&QPushButton::clicked,this,[this] { showStep(m_step-1); });
    connect(m_next,&QPushButton::clicked,this,&OnboardingTour::advance);
    auto *enter=new QShortcut(QKeySequence(Qt::Key_Return),this); enter->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enter,&QShortcut::activated,this,&OnboardingTour::advance);
}

void OnboardingTour::start() { setGeometry(parentWidget()->rect()); show(); raise(); setFocus(); showStep(0); }

void OnboardingTour::showStep(int step)
{
    m_step=qBound(0,step,count-1); const auto &s=steps[m_step];
    emit pageRequested(s.page);
    m_progress->setText(QStringLiteral("Kura 带你逛一圈 · %1 / %2").arg(m_step+1).arg(count));
    m_title->setText(QString::fromUtf8(s.title)); m_body->setText(QString::fromUtf8(s.body));
    m_previous->setEnabled(m_step>0);
    m_next->setText(m_step==count-1?QStringLiteral("出发吧！"):s.click?QStringLiteral("点亮处试试 →"):QStringLiteral("下一站 →"));
    m_target=window()->findChild<QWidget *>(QString::fromLatin1(s.target));
    if (!m_target || !m_target->isVisible()) m_target=window()->findChild<QWidget *>(QString::fromLatin1(s.fallback));
    if (m_target) for (auto *p=m_target->parentWidget();p;p=p->parentWidget()) if (auto *scroll=qobject_cast<QScrollArea *>(p)) { scroll->ensureWidgetVisible(m_target,12,12); break; }
    m_pressed=false; positionBubble();
    QTimer::singleShot(0,this,[this] { positionBubble(); });
}

void OnboardingTour::advance()
{
    if (steps[m_step].click) {
        auto *button=qobject_cast<QAbstractButton *>(m_target.data());
        if (!button || !button->isVisible() || !button->isEnabled()) {
            m_body->setText(QStringLiteral("这一处现在暂时不可用，点「结束导览」先自由探索，之后可以在设置里再叫我。")); return;
        }
        button->click();
    }
    if (m_step==count-1) emit finished(true); else showStep(m_step+1);
}

void OnboardingTour::positionBubble()
{
    m_spotlight={};
    if (m_target && m_target->isVisible()) {
        m_spotlight=QRect(mapFromGlobal(m_target->mapToGlobal(QPoint())),m_target->size());
        for (auto *p=m_target->parentWidget();p && p!=window();p=p->parentWidget())
            m_spotlight=m_spotlight.intersected(QRect(mapFromGlobal(p->mapToGlobal(QPoint())),p->size()));
        m_spotlight=m_spotlight.adjusted(-5,-5,5,5).intersected(rect().adjusted(3,3,-3,-3));
    }
    const int w=qMin(400,qMax(280,width()-32));
    m_body->setMinimumHeight(qMax(0,m_body->heightForWidth(w-40)));
    m_title->setMinimumHeight(qMax(0,m_title->heightForWidth(w-40)));
    m_bubble->layout()->invalidate();
    m_bubble->setFixedWidth(w); m_bubble->layout()->activate();
    const int desired=m_bubble->layout()->hasHeightForWidth()?m_bubble->layout()->heightForWidth(w):m_bubble->sizeHint().height();
    const int h=qMin(height()-24,desired);
    m_bubble->resize(w,h);
    const auto area=rect().adjusted(12,12,-12,-12);
    const auto target=m_spotlight.isEmpty()?QRect(width()/2,height()/2,1,1):m_spotlight;
    const QList<QPoint> positions={QPoint(target.center().x()-w/2,target.top()-h-18),QPoint(target.center().x()-w/2,target.bottom()+18),QPoint(target.right()+18,target.center().y()-h/2),QPoint(target.left()-w-18,target.center().y()-h/2)};
    QPoint chosen(area.left(),area.top()); int best=INT_MAX;
    for (auto p:positions) {
        p.setX(qBound(area.left(),p.x(),qMax(area.left(),area.right()-w+1)));
        p.setY(qBound(area.top(),p.y(),qMax(area.top(),area.bottom()-h+1)));
        const QRect candidate(p,QSize(w,h)); const QRect overlap=candidate.intersected(target);
        const int score=overlap.width()*overlap.height();
        if (score<best) { best=score; chosen=p; }
    }
    m_bubble->move(chosen); m_bubble->show(); update();
}

void OnboardingTour::paintEvent(QPaintEvent *)
{
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath shade; shade.addRect(rect());
    if (!m_spotlight.isEmpty()) shade.addRoundedRect(m_spotlight,10,10);
    shade.setFillRule(Qt::OddEvenFill); painter.fillPath(shade,QColor(24,20,16,148));
    if (!m_spotlight.isEmpty()) {
        painter.setBrush(Qt::NoBrush); painter.setPen(QPen(AppTheme::color(this,"accent"),2)); painter.drawRoundedRect(m_spotlight,10,10);
        const auto bubble=m_bubble->geometry(); const auto c=m_spotlight.center();
        QPolygon triangle;
        if (bubble.bottom()<m_spotlight.top()) { const int x=qBound(bubble.left()+22,c.x(),bubble.right()-22); triangle << QPoint(x-9,bubble.bottom()-1) << QPoint(x+9,bubble.bottom()-1) << QPoint(x,bubble.bottom()+11); }
        else if (bubble.top()>m_spotlight.bottom()) { const int x=qBound(bubble.left()+22,c.x(),bubble.right()-22); triangle << QPoint(x-9,bubble.top()+1) << QPoint(x+9,bubble.top()+1) << QPoint(x,bubble.top()-11); }
        if (!triangle.isEmpty()) { painter.setPen(Qt::NoPen); painter.setBrush(AppTheme::color(this,"card")); painter.drawPolygon(triangle); }
    }
}
void OnboardingTour::resizeEvent(QResizeEvent *) { positionBubble(); }
void OnboardingTour::mousePressEvent(QMouseEvent *event) { m_pressed=event->button()==Qt::LeftButton && m_spotlight.contains(event->position().toPoint()); event->accept(); }
void OnboardingTour::mouseReleaseEvent(QMouseEvent *event) { const bool activate=m_pressed && event->button()==Qt::LeftButton && m_spotlight.contains(event->position().toPoint()); m_pressed=false; if (activate && steps[m_step].click) advance(); event->accept(); }
void OnboardingTour::mouseMoveEvent(QMouseEvent *event) { setCursor(steps[m_step].click && m_spotlight.contains(event->position().toPoint())?Qt::PointingHandCursor:Qt::ArrowCursor); }
