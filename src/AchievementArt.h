#pragma once
#include <QPainter>
#include <QPainterPath>

namespace AchievementArt {
// Native chestnut stamps remain sharp at any display scale, in all three themes.
inline void chestnut(QPainter &p,int stamp,bool unlocked,const QColor &ink,const QColor &soft,const QColor &paper)
{
    p.setPen(Qt::NoPen); p.setBrush(soft); p.drawEllipse(QRectF(0,0,64,64));
    QPainterPath nut;
    nut.moveTo(32,10); nut.cubicTo(28,20,12,22,12,37);
    nut.cubicTo(12,55,52,55,52,37); nut.cubicTo(52,22,36,20,32,10);
    p.setPen(QPen(ink,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    p.setBrush(unlocked?ink:paper); p.drawPath(nut);
    p.setBrush(paper); p.drawEllipse(QRectF(17,39,30,10));
    p.setPen(QPen(unlocked?paper:ink,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    const auto line=[&](qreal x,qreal y,qreal a,qreal b){p.drawLine(QPointF(x,y),QPointF(a,b));};
    switch(stamp) {
    case 0: line(26,31,30,35); line(30,35,39,26); break;
    case 1: case 2: { auto font=p.font();font.setPixelSize(14);font.setWeight(QFont::Medium);p.setFont(font);p.drawText(QRectF(19,22,26,19),Qt::AlignCenter,stamp==1?QStringLiteral("7"):QStringLiteral("30")); break; }
    case 3: p.drawRoundedRect(QRectF(24,25,16,12),2,2);line(22,24,42,24);line(29,29,35,29);break;
    case 4: line(26,24,38,24);line(26,37,38,37);line(27,24,37,37);line(37,24,27,37);break;
    case 5: { QPainterPath leaf;leaf.moveTo(25,36);leaf.cubicTo(20,25,32,24,40,24);leaf.cubicTo(39,36,30,40,25,36);p.setBrush(Qt::NoBrush);p.drawPath(leaf);line(25,36,35,28);break; }
    case 6: { QPainterPath heart;heart.moveTo(32,37);heart.cubicTo(15,28,28,20,32,27);heart.cubicTo(37,20,49,28,32,37);p.setBrush(Qt::NoBrush);p.drawPath(heart);break; }
    case 7: line(32,37,32,27);p.drawArc(QRectF(22,23,10,9),0,180*16);p.drawArc(QRectF(32,21,10,9),0,180*16);break;
    case 8: p.drawRoundedRect(QRectF(24,24,16,13),1,1);line(32,24,32,37);break;
    case 9: { QPainterPath star;star.moveTo(32,22);star.lineTo(35,28);star.lineTo(42,29);star.lineTo(37,33);star.lineTo(38,39);star.lineTo(32,36);star.lineTo(26,39);star.lineTo(27,33);star.lineTo(22,29);star.lineTo(29,28);star.closeSubpath();p.setBrush(Qt::NoBrush);p.drawPath(star);break; }
    case 10: line(23,30,32,23);line(32,23,41,30);line(26,29,26,38);line(26,38,38,38);line(38,38,38,29);break;
    default: p.drawEllipse(QRectF(24,23,16,16));line(29,34,35,27);line(35,27,36,32);break;
    }
}
}
