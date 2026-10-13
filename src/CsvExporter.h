#pragma once
#include "Transaction.h"
#include <QList>

class CsvExporter
{
public:
    static bool write(const QString &path, const QList<Transaction> &records, QString &error);
};
