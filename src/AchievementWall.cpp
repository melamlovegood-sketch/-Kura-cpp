#include "AchievementWall.h"
#include "AchievementArt.h"
#include "AppTheme.h"
#include "ShellWidgets.h"
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QTimer>

namespace {
qreal textScale(const QWidget *widget)
{
    for(const QWidget *ancestor=widget;ancestor;ancestor=ancestor->parentWidget()) {
        const int scale=ancestor->property("fontScale").toInt();
        if(scale==115 || scale==130)return scale/100.0;
    }
    return 1;
}

}

class AchievementBadge:public QAbstractButton {
public:
    Achievement badge;
    int stamp;
    explicit AchievementBadge(int index,QWidget *parent):QAbstractButton(parent),stamp(index) {
        setCheckable(true);setFocusPolicy(Qt::StrongFocus);setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setFixedHeight(112);
    }
    QSize sizeHint() const override { return {qMax(120,minimumSizeHint().width()),112}; }
    QSize minimumSizeHint() const override { return {qMax(96,qRound(14*textScale(this))*6+12),112}; }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const auto paper=AppTheme::color(this,"card"),soft=AppTheme::color(this,"inset");
        const auto accent=AppTheme::color(this,"accent"),ink=AppTheme::color(this,"ink");
        const auto secondary=AppTheme::color(this,"secondary");
        const qreal scale=textScale(this);
        const QRectF halo((width()-60)/2.0,2,60,60);
        if(isChecked() || underMouse()) {
            p.setPen(isChecked()?QPen(accent,1.2):QPen(Qt::NoPen));
            p.setBrush(soft);p.drawEllipse(halo);
        }
        p.save();p.translate((width()-48)/2.0,8);p.scale(.75,.75);
        AchievementArt::chestnut(p,stamp,badge.unlocked(),badge.unlocked()?accent:secondary,soft,paper);p.restore();
        auto titleFont=font();titleFont.setPixelSize(qRound(14*scale));titleFont.setWeight(QFont::Medium);p.setFont(titleFont);p.setPen(ink);
        p.drawText(QRect(5,64,width()-10,24),Qt::AlignCenter,badge.name);
        auto small=font();small.setPixelSize(qRound(11*scale));p.setFont(small);
        p.setPen(badge.unlocked()?accent:secondary);
        if(badge.unlocked()) {
            p.setPen(QPen(accent,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            QPainterPath check;check.moveTo(width()/2.0-4,99);check.lineTo(width()/2.0-1,102);check.lineTo(width()/2.0+5,96);p.drawPath(check);
        } else p.drawText(QRect(6,90,width()-12,20),Qt::AlignCenter,QStringLiteral("%1 / %2").arg(badge.progress).arg(badge.target));
        if (hasFocus()) { p.setBrush(Qt::NoBrush);p.setPen(QPen(accent,1,Qt::DotLine));p.drawEllipse(halo.adjusted(-3,-3,3,3)); }
    }
};

AchievementWall::AchievementWall(DatabaseManager &database,QWidget *parent):QFrame(parent),m_repository(database),m_badges(AchievementRepository::definitions())
{
    setObjectName("achievementWall");setProperty("card",true);setAccessibleName(QStringLiteral("栗子收藏室成就墙"));
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    setMinimumWidth(240);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(22,18,22,18);layout->setSpacing(12);layout->setAlignment(Qt::AlignTop);
    auto *heading=new QHBoxLayout;heading->setSpacing(10);
    auto *title=ShellWidgets::label(QStringLiteral("栗子收藏室"),this,"heading");title->setObjectName("achievementWallTitle");
    heading->addWidget(title,1);
    m_count=ShellWidgets::label(QStringLiteral("已收藏 0 / 12 枚"),this,"muted");m_count->setObjectName("achievementCountLabel");heading->addWidget(m_count);layout->addLayout(heading);
    m_count->setToolTip(QStringLiteral("累计收藏 · 跨月保留 · 中断不清零"));
    layout->addWidget(ShellWidgets::label(QStringLiteral("把每一次认真选择，藏成一颗小栗子。"),this,"muted"));
    m_error=ShellWidgets::label(QString(),this,"error");m_error->setObjectName("achievementErrorLabel");m_error->hide();layout->addWidget(m_error);
    m_grid=new QGridLayout;m_grid->setSpacing(12);layout->addLayout(m_grid);
    auto *group=new QButtonGroup(this);group->setExclusive(true);
    for (int i=0;i<m_badges.size();++i) {
        auto *button=new AchievementBadge(i,this);button->badge=m_badges[i];button->setObjectName("achievementBadge_"+m_badges[i].key);
        button->setAccessibleName(m_badges[i].name);button->setToolTip(m_badges[i].group+QStringLiteral(" · ")+m_badges[i].condition);group->addButton(button);m_buttons<<button;
        connect(button,&QAbstractButton::clicked,this,[this,i]{select(i);});
    }
    auto *line=new QFrame(this);line->setProperty("reviewDivider",true);line->setFixedHeight(1);layout->addWidget(line);
    auto *detailHeading=new QHBoxLayout;detailHeading->setSpacing(10);
    m_detailTitle=ShellWidgets::label(QString(),this);m_detailTitle->setObjectName("achievementDetailTitle");detailHeading->addWidget(m_detailTitle,1);
    m_status=ShellWidgets::label(QString(),this,"muted");m_status->setObjectName("achievementStatusLabel");m_status->setWordWrap(false);m_status->setAlignment(Qt::AlignRight);detailHeading->addWidget(m_status);layout->addLayout(detailHeading);
    m_condition=ShellWidgets::label(QString(),this);m_condition->setObjectName("achievementConditionLabel");layout->addWidget(m_condition);
    reflow();select(0);setDatabaseAvailable(false);
}

void AchievementWall::setDatabaseAvailable(bool available)
{
    m_available=available;
    for(auto *button:m_buttons)button->setEnabled(available);
    if(!available){m_count->setText(QStringLiteral("收藏暂不可用"));m_status->setText(QStringLiteral("暂不可用"));}
}

bool AchievementWall::refresh()
{
    if(!m_available)return false;
    QList<Achievement> badges;
    if(!m_repository.load(badges)){m_error->setText(QStringLiteral("收藏未能更新：")+m_repository.lastError());m_error->show();return false;}
    m_error->hide();m_badges=badges;int unlocked=0;
    for(int i=0;i<m_buttons.size();++i){auto *button=m_buttons[i];button->badge=m_badges[i];
        button->setProperty("achievementUnlocked",m_badges[i].unlocked());button->setProperty("achievementProgress",m_badges[i].progress);
        button->setAccessibleDescription((m_badges[i].unlocked()?QStringLiteral("已收藏。"):QStringLiteral("待收藏。"))+m_badges[i].group+QStringLiteral("。")+m_badges[i].condition+QStringLiteral(" 当前进度 %1 / %2。").arg(m_badges[i].progress).arg(m_badges[i].target));
        button->update();if(m_badges[i].unlocked())++unlocked;
    }
    m_count->setText(QStringLiteral("已收藏 %1 / %2 枚").arg(unlocked).arg(m_badges.size()));select(m_selected);return true;
}

void AchievementWall::select(int index)
{
    m_selected=index;const auto &badge=m_badges[index];m_buttons[index]->setChecked(true);
    m_detailTitle->setText(badge.name);m_condition->setText(badge.group+QStringLiteral(" · ")+badge.condition+QStringLiteral("  “%1”").arg(badge.message));
    m_status->setText(badge.unlocked()?QStringLiteral("已收藏 · %1").arg(badge.unlockedAt.toLocalTime().toString(QStringLiteral("yyyy/M/d"))):QStringLiteral("待收藏 · %1 / %2").arg(badge.progress).arg(badge.target));
}

void AchievementWall::resizeEvent(QResizeEvent *event){QFrame::resizeEvent(event);reflow();}
void AchievementWall::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if(event->type()==QEvent::FontChange || event->type()==QEvent::StyleChange)
        QTimer::singleShot(0,this,[this]{reflow();update();});
}
void AchievementWall::reflow()
{
    int minimum=96;for(auto *button:m_buttons)minimum=qMax(minimum,button->minimumSizeHint().width());
    const int available=qMax(0,width()-44);
    int columns=1;for(int candidate:{6,4,3,2})if(candidate*minimum+(candidate-1)*12<=available){columns=candidate;break;}
    if(columns==m_columns)return;
    while(auto *item=m_grid->takeAt(0))delete item;
    for(int i=0;i<6;++i)m_grid->setColumnStretch(i,0);
    m_columns=columns;
    for(int i=0;i<m_buttons.size();++i)m_grid->addWidget(m_buttons[i],i/columns,i%columns);
    for(int i=0;i<columns;++i)m_grid->setColumnStretch(i,1);
    updateGeometry();
}
