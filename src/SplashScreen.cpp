#include "SplashScreen.h"
#include "AppTypography.h"

#include <QEasingCurve>
#include <QEvent>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {
qreal bezier(qreal value, const QPointF &first, const QPointF &second)
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(first, second, QPointF(1, 1));
    return curve.valueForProgress(qBound(qreal(0), value, qreal(1)));
}

qreal easeOut(qreal value) { return bezier(value, {0, 0}, {0.58, 1}); }
qreal easeIn(qreal value) { return bezier(value, {0.42, 0}, {1, 1}); }
qreal progress(int elapsed, int delay, int duration)
{
    return qBound(qreal(0), qreal(elapsed - delay) / duration, qreal(1));
}

qreal acornOffset(int elapsed)
{
    const qreal t = progress(elapsed, 200, 800);
    if (t < .62) return -80 + 80 * bezier(t / .62, {.55, 0}, {1, .45});
    if (t < .73) return -10 * easeOut((t - .62) / .11);
    if (t < .83) return -10 + 10 * easeIn((t - .73) / .10);
    if (t < .91) return -4 * easeOut((t - .83) / .08);
    return -4 + 4 * easeIn((t - .91) / .09);
}

QPainterPath squirrelBody()
{
    // Exact SVG path from Kura/src/components/SplashScreen.tsx, including its
    // internal translate(8,22). The small ear/arm stroke is outside that transform.
    QPainterPath path;
    path.moveTo(103,87); path.cubicTo(99,88,93,90,89,91);
    path.lineTo(88,95); path.cubicTo(91,96,95,98,98,105);
    path.lineTo(91,115); path.cubicTo(91,115,95,115,100,115);
    path.cubicTo(104,116,103,122,103,122); path.lineTo(58,122);
    path.cubicTo(58,122,54,97,74,80); path.cubicTo(73,70,74,63,78,59);
    path.lineTo(78,48); path.lineTo(87,55); path.cubicTo(96,54,102,63,103,70);
    path.lineTo(92,74); path.lineTo(91,81); path.lineTo(99,80); path.lineTo(102,75);
    path.cubicTo(109,77,111,85,103,87); path.closeSubpath();
    path.moveTo(49,122); path.cubicTo(38,120,31,114,31,102);
    path.cubicTo(31,88,39,59,16,63); path.lineTo(15,60);
    path.cubicTo(19,51,27,42,40,42); path.cubicTo(54,42,61,51,61,68);
    path.cubicTo(61,88,48,89,49,122); path.closeSubpath();
    return QTransform::fromTranslate(8,22).map(path);
}

void textLine(QPainter &painter, const QString &text, const QFont &font,
              const QColor &color, qreal top, qreal height, qreal width, qreal opacity)
{
    painter.save();
    painter.setOpacity(painter.opacity() * opacity);
    painter.setFont(font); painter.setPen(color);
    const QFontMetricsF metrics(font);
    painter.drawText(QPointF((width - metrics.horizontalAdvance(text)) / 2,
                            top + (height - metrics.height()) / 2 + metrics.ascent()), text);
    painter.restore();
}
}

SplashScreen::SplashScreen(QWidget *window)
    : QWidget(window), m_animation(new QVariantAnimation(this)),
      m_previousFocus(window->focusWidget())
{
    setObjectName(QStringLiteral("kuraSplashScreen"));
    setAccessibleName(QStringLiteral("KURA · Buy less, own more."));
    setFocusPolicy(Qt::StrongFocus);
    setGeometry(window->rect());
    window->installEventFilter(this);
    AppTypography::initialize();
    m_animation->setObjectName(QStringLiteral("splashTimeline"));
    m_animation->setStartValue(0); m_animation->setEndValue(2800);
    m_animation->setDuration(2800);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_elapsed = value.toInt(); update();
    });
    connect(m_animation, &QVariantAnimation::finished, this, [this] {
        hide();
        if (m_previousFocus) m_previousFocus->setFocus(Qt::OtherFocusReason);
        deleteLater();
    });
}

SplashScreen *SplashScreen::present(QWidget *window)
{
    if (window->property("reducedMotion").toBool()) return nullptr;
    // A second request during startup must not create overlapping openings.
    if (auto *existing = window->findChild<SplashScreen *>(QStringLiteral("kuraSplashScreen")))
        return existing;
    auto *splash = new SplashScreen(window);
    splash->show(); splash->raise(); splash->setFocus(Qt::OtherFocusReason);
    splash->m_animation->start();
    return splash;
}

bool SplashScreen::eventFilter(QObject *object, QEvent *event)
{
    if (object == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(object, event);
}

void SplashScreen::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setOpacity(1 - easeOut(progress(m_elapsed, 2500, 300)));
    painter.fillRect(rect(), QColor(QStringLiteral("#F5F0E8")));

    const qreal top = (height() - 296.) / 2;
    painter.save();
    painter.translate((width() - 150.) / 2, top);
    painter.setBrush(Qt::NoBrush);
    const QColor ink(QStringLiteral("#5C5448"));
    painter.save();
    painter.translate(0, acornOffset(m_elapsed));
    painter.setPen(QPen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawEllipse(QPointF(62,36),11,13);
    QPainterPath cap; cap.moveTo(51,37); cap.cubicTo(53,30,71,30,73,37);
    painter.drawPath(cap);
    painter.setPen(QPen(ink, 1.8, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(62,23), QPointF(62,18));
    QPainterPath stem; stem.moveTo(62,18); stem.cubicTo(62,15,65,13,67,15);
    painter.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(stem); painter.restore();

    static const QPainterPath body = squirrelBody();
    static const QPainterPath stroke = [] {
        QPainterPath path; path.moveTo(78,48); path.cubicTo(73,43,66,41,64,44); return path;
    }();
    const QRectF bounds = body.boundingRect().united(stroke.boundingRect());
    const QPointF pivot(bounds.center().x(), bounds.top() + bounds.height() * .9);
    const qreal reach = progress(m_elapsed, 750, 650);
    const qreal angle = reach < .4
        ? -7 * bezier(reach / .4, {.42,0}, {.58,1})
        : -7 + 7 * bezier((reach - .4) / .6, {.42,0}, {.58,1});
    painter.translate(pivot); painter.rotate(angle); painter.translate(-pivot);
    painter.setPen(QPen(ink, 3.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(body);
    painter.setPen(QPen(ink, 2, Qt::SolidLine, Qt::RoundCap)); painter.drawPath(stroke);
    painter.restore();

    auto title = AppTypography::serif(64);
    title.setLetterSpacing(QFont::AbsoluteSpacing, -.64);
    const qreal titleProgress = easeOut(progress(m_elapsed, 1050, 450));
    textLine(painter, QStringLiteral("KURA"), title, QColor("#3C3028"),
             top + 173 + 14 * (1 - titleProgress), 64, width(), titleProgress);
    auto tagline = AppTypography::serif(16); tagline.setItalic(true);
    textLine(painter, QStringLiteral("Buy less, own more."), tagline, ink,
             top + 243, 24, width(), easeOut(progress(m_elapsed, 1400, 400)));
    auto label = AppTypography::sans(10); label.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    textLine(painter, QStringLiteral("SAVE FOR WHAT STAYS"), label, QColor("#9C9080"),
             top + 281, 15, width(), easeOut(progress(m_elapsed, 1700, 400)));
    textLine(painter, QStringLiteral("Made by Melam"), AppTypography::sans(12, QFont::Light),
             QColor("#C8BFB0"), height() - 46, 18, width(), easeOut(progress(m_elapsed, 2000, 350)));
}
