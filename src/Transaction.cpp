#include "Transaction.h"

Transaction::Transaction(qint64 id, const QString &description, qint64 amountCents,
                         const QString &category, const QDate &date,
                         const QTime &time, const QString &note)
    : m_id(id), m_description(description.trimmed()), m_amountCents(amountCents),
      m_category(category), m_date(date), m_time(time.isValid() ? QTime(time.hour(), time.minute(), time.second()) : QTime()),
      m_note(note)
{
}

qint64 Transaction::id() const { return m_id; }
QString Transaction::description() const { return m_description; }
qint64 Transaction::amountCents() const { return m_amountCents; }
double Transaction::amount() const { return static_cast<double>(m_amountCents) / 100.0; }
QString Transaction::category() const { return m_category; }
QDate Transaction::date() const { return m_date; }
QTime Transaction::time() const { return m_time; }
QString Transaction::note() const { return m_note; }

QString Transaction::validationError() const
{
    if (m_id < 0)
        return QStringLiteral("消费记录编号无效。");
    if (m_description.isEmpty() || m_description.size() > 100)
        return QStringLiteral("消费名称须为 1 至 100 个字符。");
    if (m_amountCents <= 0 || m_amountCents > MaximumAmountCents)
        return QStringLiteral("金额须在 0.01 至 99,999,999.99 元之间。");
    if (!categories().contains(m_category))
        return QStringLiteral("请选择有效的消费分类。");
    if (!m_date.isValid() || m_date.year() < 1 || m_date.year() > 9999)
        return QStringLiteral("请选择有效的消费日期。");
    if (m_note.size() > MaximumNoteLength)
        return QStringLiteral("备注最多填写 1000 个字符。");
    return {};
}

QStringList Transaction::categories()
{
    return {QStringLiteral("餐饮"), QStringLiteral("交通"), QStringLiteral("购物"),
            QStringLiteral("娱乐"), QStringLiteral("学习"), QStringLiteral("其他")};
}

QString Transaction::formatAmount(qint64 cents)
{
    return QStringLiteral("%1¥%2.%3")
        .arg(cents < 0 ? QStringLiteral("-") : QString())
        .arg(qAbs(cents / 100))
        .arg(qAbs(cents % 100), 2, 10, QLatin1Char('0'));
}
