#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

class Transaction
{
public:
    static constexpr qint64 MaximumAmountCents = 9999999999LL;

    Transaction(qint64 id, const QString &description, qint64 amountCents,
                const QString &category, const QDate &date);

    qint64 id() const;
    QString description() const;
    qint64 amountCents() const;
    double amount() const;
    QString category() const;
    QDate date() const;

    QString validationError() const;
    static QStringList categories();
    static QString formatAmount(qint64 cents);

private:
    qint64 m_id;
    QString m_description;
    qint64 m_amountCents;
    QString m_category;
    QDate m_date;
};
