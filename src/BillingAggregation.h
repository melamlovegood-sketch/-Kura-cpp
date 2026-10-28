#pragma once
#include "Transaction.h"
struct BillingDay { QDate date; qint64 cents=0,count=0; };
struct BillingMonth { QDate month; qint64 cents=0,count=0; };
struct BillingSnapshot {
    QDate month;
    QList<Transaction> records;
    QList<BillingDay> days;
    QList<BillingMonth> trend;
    qint64 totalCents=0,basicCents=0,discretionaryCents=0,todayCents=0;
};
namespace BillingAggregation {
bool calculate(const QList<Transaction> &records,const QDate &month,BillingSnapshot &snapshot,QString &error,const QDate &today=QDate::currentDate());
QString money(qint64 cents,bool trimZeroDecimals=false);
}
