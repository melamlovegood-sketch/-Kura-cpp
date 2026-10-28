#include "BillingCharts.h"
#include "AppTheme.h"
#include "AppTypography.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace {
QColor mix(const QColor &a,const QColor &b,qreal ratio) { return QColor::fromRgbF(a.redF()*(1-ratio)+b.redF()*ratio,a.greenF()*(1-ratio)+b.greenF()*ratio,a.blueF()*(1-ratio)+b.blueF()*ratio); }
QFont sizedFont(const QWidget *,int pixels,bool medium=false) { return AppTypography::sans(pixels,medium?QFont::Medium:QFont::Normal); }
}
BillingCalendar::BillingCalendar(QWidget *parent):QWidget(parent) { setObjectName("billingCalendar"); setMouseTracking(true); setFocusPolicy(Qt::StrongFocus); setAccessibleName(QStringLiteral("消费日历")); setMinimumHeight(330); }
QSize BillingCalendar::minimumSizeHint() const { return {380,330}; }
void BillingCalendar::setSnapshot(const BillingSnapshot &snapshot)
{
    if (!m_selected.isValid() || m_selected.year()!=snapshot.month.year() || m_selected.month()!=snapshot.month.month()) { const auto today=QDate::currentDate(); m_selected=snapshot.month.year()==today.year() && snapshot.month.month()==today.month()?today:snapshot.month; }
    m_snapshot=snapshot; m_hasSnapshot=true; const int weeks=(snapshot.month.dayOfWeek()-1+snapshot.month.daysInMonth()+6)/7; setFixedHeight(29+weeks*61); update();
}
QRectF BillingCalendar::cellRect(int day) const
{
    if (!m_hasSnapshot || day<1 || day>m_snapshot.days.size()) return {}; const int index=m_snapshot.month.dayOfWeek()-1+day-1; const qreal gap=6,cellWidth=(width()-gap*6)/7.0; return {index%7*(cellWidth+gap),29+(index/7)*61.0,cellWidth,55};
}
void BillingCalendar::paintEvent(QPaintEvent *)
{
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); const auto ink=AppTheme::color(this,"ink"),muted=AppTheme::color(this,"muted"),card=AppTheme::color(this,"card"),accent=AppTheme::color(this,"accent"),inset=AppTheme::color(this,"inset");
    if (!m_hasSnapshot) { painter.setPen(muted); painter.drawText(rect(),Qt::AlignCenter,QStringLiteral("尚未加载账单。")); return; }
    painter.setFont(sizedFont(this,11)); painter.setPen(muted); const QStringList weekdays={QStringLiteral("一"),QStringLiteral("二"),QStringLiteral("三"),QStringLiteral("四"),QStringLiteral("五"),QStringLiteral("六"),QStringLiteral("日")};
    for (int column=0;column<7;++column) painter.drawText(QRectF(column*width()/7.0,0,width()/7.0,20),Qt::AlignCenter,weekdays[column]);
    qint64 maximum=1; for (const auto &point:m_snapshot.days) maximum=qMax(maximum,point.cents);
    for (const auto &point:m_snapshot.days) {
        const auto box=cellRect(point.date.day()); const qreal ratio=double(point.cents)/maximum; const bool strong=point.cents>0 && ratio>.65; const auto background=point.cents==0?mix(card,inset,.70):strong?accent:mix(card,accent,ratio>.20?.42:.19); const auto foreground=strong?AppTheme::color(this,"onAccent"):ink;
        painter.setOpacity(point.date>QDate::currentDate() && point.count==0?.45:1); painter.setPen(Qt::NoPen); painter.setBrush(background); painter.drawRoundedRect(box,9,9); painter.setPen(foreground); painter.setFont(sizedFont(this,11)); painter.drawText(box.adjusted(9,6,-5,-5),Qt::AlignLeft|Qt::AlignTop,QString::number(point.date.day()));
        painter.setFont(sizedFont(this,11,true)); painter.setPen(point.count?foreground:muted); const auto amount=point.count?BillingAggregation::money(point.cents,true):QStringLiteral("—"); painter.drawText(box.adjusted(8,5,-5,-7),Qt::AlignLeft|Qt::AlignBottom,painter.fontMetrics().elidedText(amount,Qt::ElideRight,box.width()-13));
        if (point.date==m_selected) { painter.setBrush(Qt::NoBrush); painter.setPen(QPen(strong?foreground:accent,1)); painter.drawRoundedRect(box.adjusted(3,3,-3,-3),6,6); } painter.setOpacity(1);
    }
}
void BillingCalendar::selectDay(int day) { if (!m_hasSnapshot || day<1 || day>m_snapshot.days.size()) return; m_selected=m_snapshot.days[day-1].date; setAccessibleDescription(QStringLiteral("已选择 %1，%2 笔，%3").arg(m_selected.toString(Qt::ISODate)).arg(m_snapshot.days[day-1].count).arg(BillingAggregation::money(m_snapshot.days[day-1].cents))); update(); emit daySelected(m_selected); }
void BillingCalendar::mousePressEvent(QMouseEvent *event) { if (event->button()!=Qt::LeftButton) return; for (int day=1;day<=m_snapshot.days.size();++day) if (cellRect(day).contains(event->position())) { setFocus(); selectDay(day); break; } }
void BillingCalendar::mouseMoveEvent(QMouseEvent *event) { for (const auto &point:m_snapshot.days) if (cellRect(point.date.day()).contains(event->position())) { QToolTip::showText(event->globalPosition().toPoint(),QStringLiteral("%1 · %2 笔 · %3").arg(point.date.toString(Qt::ISODate)).arg(point.count).arg(BillingAggregation::money(point.cents)),this); return; } QToolTip::hideText(); }
void BillingCalendar::keyPressEvent(QKeyEvent *event) { int day=m_selected.isValid()?m_selected.day():1; switch(event->key()) { case Qt::Key_Left:--day;break;case Qt::Key_Right:++day;break;case Qt::Key_Up:day-=7;break;case Qt::Key_Down:day+=7;break;case Qt::Key_Home:day=1;break;case Qt::Key_End:day=m_snapshot.days.size();break;case Qt::Key_Return:case Qt::Key_Enter:break;default:QWidget::keyPressEvent(event);return; } if (!m_snapshot.days.isEmpty()) selectDay(qBound(1,day,int(m_snapshot.days.size()))); event->accept(); }

BillingTrendChart::BillingTrendChart(QWidget *parent):QWidget(parent) { setObjectName("billingTrendChart"); setMouseTracking(true); setFocusPolicy(Qt::StrongFocus); setMinimumHeight(285); setAccessibleName(QStringLiteral("支出折线趋势")); }
QSize BillingTrendChart::minimumSizeHint() const { return {380,285}; }
void BillingTrendChart::setSnapshot(const BillingSnapshot &snapshot) { m_snapshot=snapshot; m_hasSnapshot=true; m_hover=-1; update(); }
void BillingTrendChart::setSixMonths(bool sixMonths) { m_six=sixMonths; m_hover=-1; update(); }
QList<BillingMonth> BillingTrendChart::points() const
{
    if (!m_hasSnapshot) return {}; if (m_six) return m_snapshot.trend; int limit=0; const auto today=QDate::currentDate(); if (m_snapshot.month<QDate(today.year(),today.month(),1)) limit=m_snapshot.days.size(); else if (m_snapshot.month.year()==today.year() && m_snapshot.month.month()==today.month()) limit=today.day(); for (const auto &point:m_snapshot.days) if (point.count>0) limit=qMax(limit,point.date.day()); QList<BillingMonth> result; for (int i=0;i<limit;++i) result.append({m_snapshot.days[i].date,m_snapshot.days[i].cents,m_snapshot.days[i].count}); return result;
}
QRectF BillingTrendChart::plotRect() const
{
    qint64 highest=0; for (const auto &point:points()) highest=qMax(highest,point.cents); return QRectF(rect()).adjusted(qMax(55,fontMetrics().horizontalAdvance(BillingAggregation::money(highest,true))+12),30,-32,-37);
}
void BillingTrendChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing); const auto ink=AppTheme::color(this,"ink"),muted=AppTheme::color(this,"muted"),border=AppTheme::color(this,"border"),accent=AppTheme::color(this,"accent"); painter.setFont(sizedFont(this,11)); painter.setPen(muted);
    const auto values=points(); qint64 highest=0; for (const auto &point:values) highest=qMax(highest,point.cents); if (!m_hasSnapshot || highest==0 || values.isEmpty()) { painter.drawText(rect(),Qt::AlignCenter,m_hasSnapshot?QStringLiteral("这个范围还没有记账记录。"):QStringLiteral("尚未加载账单。")); return; }
    const auto plot=plotRect(); if (plot.width()<=0 || plot.height()<=0) return; const auto x=[&](int i) { return values.size()==1?plot.center().x():plot.left()+plot.width()*i/(values.size()-1); }; const auto y=[&](qint64 cents) { return plot.bottom()-plot.height()*double(cents)/highest; };
    for (const qint64 amount:{qint64(0),highest/2,highest}) { painter.setPen(QPen(border,1)); painter.drawLine(QPointF(plot.left(),y(amount)),QPointF(plot.right(),y(amount))); painter.setPen(muted); painter.drawText(QRectF(0,y(amount)-9,plot.left()-9,19),Qt::AlignRight|Qt::AlignVCenter,BillingAggregation::money(amount,true)); }
    QPainterPath line; int peak=0; for (int i=0;i<values.size();++i) { if (values[i].cents>values[peak].cents) peak=i; const QPointF point(x(i),y(values[i].cents)); if (i==0) line.moveTo(point); else line.lineTo(point); }
    QPainterPath area=line; area.lineTo(x(values.size()-1),plot.bottom()); area.lineTo(x(0),plot.bottom()); area.closeSubpath(); auto fill=accent; fill.setAlpha(14); painter.fillPath(area,fill); painter.setPen(QPen(accent,2)); painter.setBrush(Qt::NoBrush); if (values.size()>1) painter.drawPath(line); painter.setBrush(accent);
    for (int i=0;i<values.size();++i) if (values[i].cents>0 || values.size()==1) painter.drawEllipse(QPointF(x(i),y(values[i].cents)),3,3);
    painter.setPen(ink); const auto peakText=BillingAggregation::money(highest); const bool right=peak>values.size()/2; painter.drawText(QRectF(right?x(peak)-150:x(peak),y(highest)-25,150,20),right?Qt::AlignRight:Qt::AlignLeft,peakText);
    QList<int> ticks; if (values.size()<=6) for (int i=0;i<values.size();++i) ticks << i; else for (int i:{0,6,12,19,int(values.size()-1)}) if (i<values.size() && !ticks.contains(i)) ticks << i;
    painter.setPen(muted); for (const int i:ticks) { const auto text=values[i].month.toString(m_six?QStringLiteral("M 月"):QStringLiteral("d 日")); const auto align=i==0?Qt::AlignLeft:i==values.size()-1?Qt::AlignRight:Qt::AlignHCenter; QRectF label(x(i)-30,plot.bottom()+12,60,20); if (i==0) label.moveLeft(plot.left()); if (i==values.size()-1) label.moveRight(plot.right()); painter.drawText(label,align|Qt::AlignVCenter,text); }
    if (m_hover>=0 && m_hover<values.size()) { auto guide=muted; guide.setAlpha(110); painter.setPen(QPen(guide,1,Qt::DashLine)); painter.drawLine(QPointF(x(m_hover),plot.top()),QPointF(x(m_hover),plot.bottom())); painter.setBrush(accent); painter.setPen(Qt::NoPen); painter.drawEllipse(QPointF(x(m_hover),y(values[m_hover].cents)),4,4); }
}
void BillingTrendChart::selectPoint(int index) { const auto values=points(); if (values.isEmpty()) return; m_hover=qBound(0,index,int(values.size()-1)); update(); emit pointSelected(values[m_hover].month,values[m_hover].cents,values[m_hover].count); }
void BillingTrendChart::mouseMoveEvent(QMouseEvent *event) { const auto values=points(); const auto plot=plotRect(); if (values.isEmpty() || !plot.contains(event->position())) return; selectPoint(qRound((event->position().x()-plot.left())/plot.width()*(values.size()-1))); }
void BillingTrendChart::mousePressEvent(QMouseEvent *event) { if (event->button()!=Qt::LeftButton) return; setFocus(); mouseMoveEvent(event); }
void BillingTrendChart::keyPressEvent(QKeyEvent *event) { if (event->key()==Qt::Key_Left || event->key()==Qt::Key_Right) { selectPoint(qMax(0,m_hover)+(event->key()==Qt::Key_Right?1:-1)); event->accept(); } else QWidget::keyPressEvent(event); }
