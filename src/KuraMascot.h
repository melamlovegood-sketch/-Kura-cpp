#pragma once
#include "AppTheme.h"
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

// The original Kura squirrel, kept as vectors so it stays crisp at large sizes.
class KuraMascot final : public QWidget {
public:
    explicit KuraMascot(QWidget *parent=nullptr):QWidget(parent) {
        setAccessibleName(QStringLiteral("抱着栗子的小松鼠 Kura"));
        setMinimumSize(40,44); setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const qreal scale=qMin(width()/150.,height()/165.);
        p.translate((width()-150*scale)/2,(height()-165*scale)/2); p.scale(scale,scale);
        const auto ink=AppTheme::color(this,"ink"),accent=AppTheme::color(this,"accent");
        p.setPen(QPen(ink,3.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p.setBrush(Qt::NoBrush); p.drawEllipse(QRectF(51,23,22,26));
        QPainterPath cap; cap.moveTo(51,37); cap.cubicTo(53,30,71,30,73,37);
        cap.moveTo(62,23); cap.lineTo(62,18); cap.cubicTo(62,15,65,13,67,15); p.drawPath(cap);
        p.translate(8,22);
        QPainterPath body;
        body.moveTo(103,87); body.cubicTo(99,88,93,90,89,91); body.lineTo(88,95);
        body.cubicTo(91,96,95,98,98,105); body.lineTo(91,115); body.cubicTo(91,115,95,115,100,115);
        body.cubicTo(104,116,103,122,103,122); body.lineTo(58,122); body.cubicTo(58,122,54,97,74,80);
        body.cubicTo(73,70,74,63,78,59); body.lineTo(78,48); body.lineTo(87,55);
        body.cubicTo(96,54,102,63,103,70); body.lineTo(92,74); body.lineTo(91,81); body.lineTo(99,80);
        body.lineTo(102,75); body.cubicTo(109,77,111,85,103,87); body.closeSubpath();
        body.moveTo(49,122); body.cubicTo(38,120,31,114,31,102); body.cubicTo(31,88,39,59,16,63);
        body.lineTo(15,60); body.cubicTo(19,51,27,42,40,42); body.cubicTo(54,42,61,51,61,68);
        body.cubicTo(61,88,48,89,49,122); body.closeSubpath();
        QColor fill=accent; fill.setAlpha(20); p.setBrush(fill); p.drawPath(body);
        p.setBrush(ink); p.setPen(Qt::NoPen); p.drawEllipse(QPointF(93,64),1.8,1.8);
        QColor blush=accent; blush.setAlpha(100); p.setBrush(blush); p.drawEllipse(QPointF(89,70),3,2);
        p.setPen(QPen(ink,2,Qt::SolidLine,Qt::RoundCap));
        QPainterPath arm; arm.moveTo(78,48); arm.cubicTo(73,43,66,41,64,44); p.drawPath(arm);
    }
};
