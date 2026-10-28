#pragma once
#include "AppTheme.h"
#include <QPainter>
#include <QToolButton>

// Draw the controls with the theme palette, independent of emoji fonts.
class WishActionButton : public QToolButton {
public:
    enum Kind { Wand, Trash };
    WishActionButton(Kind kind,QWidget *parent):QToolButton(parent),m_kind(kind) {
        setFixedSize(36,36); setCursor(Qt::PointingHandCursor);
        setAutoRaise(true); setCheckable(kind==Wand);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const auto color=AppTheme::color(this,isChecked()?"accent":"muted");
        if (isChecked() || underMouse() || hasFocus()) {
            auto background=color; background.setAlpha(isChecked()?32:18);
            p.setPen(Qt::NoPen); p.setBrush(background); p.drawRoundedRect(rect().adjusted(1,1,-1,-1),9,9);
        }
        p.setBrush(Qt::NoBrush); p.setPen(QPen(color,1.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if (m_kind==Wand) {
            p.save(); p.translate(18,19); p.rotate(40);
            p.drawRoundedRect(QRectF(-2,-9,4,19),1,1); p.drawLine(QPointF(-2,-4),QPointF(2,-4)); p.restore();
            for (const auto &point:{QPointF(12,9),QPointF(25,10),QPointF(27,21)}) {
                p.drawLine(point+QPointF(-2,0),point+QPointF(2,0)); p.drawLine(point+QPointF(0,-2),point+QPointF(0,2));
            }
        } else {
            p.drawLine(10,12,26,12); p.drawRoundedRect(QRectF(15,8,6,4),1,1);
            p.drawRoundedRect(QRectF(12,12,12,16),2,2); p.drawLine(16,16,16,24); p.drawLine(20,16,20,24);
        }
    }
private:
    Kind m_kind;
};
