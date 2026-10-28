#include "MonthlyTrendChart.h"

#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

MonthlyTrendChart::MonthlyTrendChart(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("monthlyTrendChart"));
    setMinimumHeight(230);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize MonthlyTrendChart::minimumSizeHint() const { return {620, 230}; }

void MonthlyTrendChart::setStatistics(const YearlyExpenseStatistics &statistics)
{
    m_statistics = statistics;
    m_hasSnapshot = true;
    update();
}

QRectF MonthlyTrendChart::plotArea() const
{
    qint64 maximum = 0;
    for (const auto &month : m_statistics.months)
        maximum = qMax(maximum, month.totalCents);
    const int left = fontMetrics().horizontalAdvance(Transaction::formatAmount(maximum)) + 16;
    return QRectF(rect()).adjusted(left, 18, -14, -34);
}

void MonthlyTrendChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // QSS does not paint this custom chart. Use the window colors explicitly so
    // switching to dark mode cannot leave a white plot behind light labels.
    const auto chartPalette = window()->palette();
    const QColor ink = chartPalette.windowText().color();
    painter.fillRect(rect(), chartPalette.window());
    painter.setPen(ink);
    if (!m_hasSnapshot) {
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("统计暂未加载。"));
        return;
    }
    if (m_statistics.totalCents == 0) {
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("本年度暂无消费记录。"));
        return;
    }
    const QRectF plot = plotArea();
    if (plot.width() <= 0 || plot.height() <= 0 || m_statistics.months.isEmpty())
        return;
    qint64 maximum = 0;
    for (const auto &month : m_statistics.months)
        maximum = qMax(maximum, month.totalCents);
    for (int tick = 0; tick <= 4; ++tick) {
        const qreal y = plot.bottom() - plot.height() * tick / 4.0;
        QColor grid = ink; grid.setAlpha(45);
        painter.setPen(grid);
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        const qint64 cents = maximum / 4 * tick + maximum % 4 * tick / 4;
        painter.setPen(ink);
        painter.drawText(QRectF(0, y - 10, plot.left() - 8, 20),
                         Qt::AlignRight | Qt::AlignVCenter, Transaction::formatAmount(cents));
    }
    const qreal slot = plot.width() / m_statistics.months.size();
    const qreal barWidth = qMin(34.0, slot * 0.55);
    const int peak = m_statistics.peakMonthIndex();
    for (qsizetype i = 0; i < m_statistics.months.size(); ++i) {
        const auto &month = m_statistics.months[i];
        const qreal x = plot.left() + slot * (i + 0.5);
        if (month.totalCents > 0) {
            const qreal height = qMax(1.0, plot.height() * static_cast<double>(month.totalCents) / maximum);
            QColor bar = ink; bar.setAlpha(i == peak ? 255 : 140);
            painter.fillRect(QRectF(x - barWidth / 2, plot.bottom() - height, barWidth, height), bar);
        }
        painter.setPen(ink);
        painter.drawText(QRectF(x - slot / 2, plot.bottom() + 8, slot, 20), Qt::AlignCenter,
                         QStringLiteral("%1月").arg(month.month.month()));
    }
}

int MonthlyTrendChart::monthAt(const QPoint &position) const
{
    const auto plot = plotArea();
    if (!m_hasSnapshot || m_statistics.totalCents == 0 || m_statistics.months.isEmpty()
        || !plot.contains(position) || plot.width() <= 0)
        return -1;
    const int index = static_cast<int>((position.x() - plot.left()) / plot.width() * m_statistics.months.size());
    return index >= 0 && index < m_statistics.months.size() ? index : -1;
}

bool MonthlyTrendChart::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int index = monthAt(help->pos());
        if (index >= 0) {
            const auto &month = m_statistics.months[index];
            const QString text = QStringLiteral("%1\n支出：%2\n%3 笔消费")
                .arg(month.month.toString(QStringLiteral("yyyy-MM")))
                .arg(Transaction::formatAmount(month.totalCents)).arg(month.recordCount);
            setToolTip(text);
            QToolTip::showText(help->globalPos(), text, this);
        } else {
            setToolTip(QString());
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(event);
}

void MonthlyTrendChart::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int index = monthAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index >= 0)
        emit monthActivated(m_statistics.months[index].month);
    else
        QWidget::mouseDoubleClickEvent(event);
}
