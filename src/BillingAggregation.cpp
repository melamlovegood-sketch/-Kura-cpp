#include "BillingAggregation.h"
#include "CategoryCatalog.h"
#include <algorithm>
#include <limits>

QString BillingAggregation::money(qint64 cents,bool trimZeroDecimals)
{
    QString whole=QString::number(qAbs(cents/100)); for (int position=whole.size()-3;position>0;position-=3) whole.insert(position,',');
    return (cents<0?QStringLiteral("-¥"):QStringLiteral("¥"))+whole+(trimZeroDecimals && cents%100==0?QString():QStringLiteral(".%1").arg(qAbs(cents%100),2,10,QChar('0')));
}

bool BillingAggregation::calculate(const QList<Transaction> &records,const QDate &month,BillingSnapshot &snapshot,QString &error,const QDate &today)
{
    error.clear(); if (!month.isValid() || month.year()<1 || month.year()>9999) { error=QStringLiteral("请选择有效月份。"); return false; }
    BillingSnapshot loaded; loaded.month=QDate(month.year(),month.month(),1); for (int day=1;day<=loaded.month.daysInMonth();++day) loaded.days.append({QDate(month.year(),month.month(),day),0,0});
    for (int offset=-5;offset<=0;++offset) { const auto date=loaded.month.addMonths(offset); if (date.isValid()) loaded.trend.append({date,0,0}); }
    const auto add=[&](qint64 &total,qint64 value) { if (total>std::numeric_limits<qint64>::max()-value) { error=QStringLiteral("账单金额合计超过支持范围。"); return false; } total+=value; return true; };
    for (const auto &record:records) {
        if (!record.validationError().isEmpty() || record.id()<=0) { error=QStringLiteral("账单中存在无效记录。"); return false; }
        if (record.date()==today && !add(loaded.todayCents,record.amountCents())) return false;
        for (auto &point:loaded.trend) if (record.date().year()==point.month.year() && record.date().month()==point.month.month()) { if (!add(point.cents,record.amountCents())) return false; ++point.count; break; }
        if (record.date().year()!=month.year() || record.date().month()!=month.month()) continue;
        if (!add(loaded.totalCents,record.amountCents()) || !add(record.isBasic()?loaded.basicCents:loaded.discretionaryCents,record.amountCents())) return false;
        auto &day=loaded.days[record.date().day()-1]; if (!add(day.cents,record.amountCents())) return false; ++day.count; loaded.records.append(record);
    }
    std::sort(loaded.records.begin(),loaded.records.end(),[](const Transaction &a,const Transaction &b) { if (a.date()!=b.date()) return a.date()>b.date(); if (a.time()!=b.time()) return a.time().isValid() && (!b.time().isValid() || a.time()>b.time()); return a.id()>b.id(); }); snapshot=loaded; return true;
}
