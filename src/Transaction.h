#pragma once

#include <QDate>
#include <QTime>
#include <QString>
#include <QStringList>

class Transaction
{
public:
    static constexpr qint64 MaximumAmountCents = 9999999999LL;
    static constexpr int MaximumNoteLength = 1000;

    Transaction(qint64 id, const QString &description, qint64 amountCents,
                const QString &category, const QDate &date,
                const QTime &time = {}, const QString &note = {});

    qint64 id() const;
    QString description() const;
    qint64 amountCents() const;
    double amount() const;
    QString category() const;
    QDate date() const;
    QTime time() const; // Invalid means the original record did not include a time.
    QString note() const;

    QString validationError() const;
    static QStringList categories();
    static QString formatAmount(qint64 cents);

private:
    qint64 m_id;
    QString m_description;
    qint64 m_amountCents;
    QString m_category;
    QDate m_date;
    QTime m_time;
    QString m_note;
};
