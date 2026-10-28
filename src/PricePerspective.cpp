#include "PricePerspective.h"
#include "Transaction.h"
#include <QSettings>
#include <QVariantMap>
#include <cmath>

namespace {
QString number(long double value) {
    const auto text=QString::number(static_cast<double>(value),'f',1);
    return text.endsWith(".0")?text.left(text.size()-2):text;
}
QString smallNumber(long double value) { return value<0.1L?QStringLiteral("不足 0.1"):number(value); }
}
QString PricePerspective::validationError() const {
    if (mode!=Off && mode!=Student && mode!=Worker) return QStringLiteral("请选择有效的换算模式。");
    if (dailyFoodCents<0 || monthlyIncomeCents<0 || dailyFoodCents>Transaction::MaximumAmountCents || monthlyIncomeCents>Transaction::MaximumAmountCents)
        return QStringLiteral("金额须在 0 至 99,999,999.99 元之间。");
    if (mealsPerDay<1 || mealsPerDay>10) return QStringLiteral("每天餐数须为 1 至 10 顿。");
    if (!std::isfinite(workDaysPerMonth) || workDaysPerMonth<1 || workDaysPerMonth>31 || !std::isfinite(dailyWorkHours) || dailyWorkHours<0.5 || dailyWorkHours>24)
        return QStringLiteral("每月工作天数须为 1 至 31 天，每天工时须为 0.5 至 24 小时。");
    return {};
}
bool PricePerspective::ready() const { return validationError().isEmpty() && ((mode==Student && dailyFoodCents>0) || (mode==Worker && monthlyIncomeCents>0)); }
QString PricePerspective::reference() const {
    if (!ready()) return {};
    const double rate=mode==Student?double(dailyFoodCents)/100/mealsPerDay:double(monthlyIncomeCents)/100/workDaysPerMonth/dailyWorkHours;
    const auto money=rate<0.01?QStringLiteral("不足¥0.01"):QStringLiteral("¥%1").arg(QString::number(rate,'f',2));
    return mode==Student?QStringLiteral("参考餐费：%1／顿").arg(money):QStringLiteral("参考时薪：%1／小时").arg(money);
}
PriceQuote PricePerspective::quote(qint64 amountCents) const {
    PriceQuote result; if (!ready() || amountCents<=0 || amountCents>Transaction::MaximumAmountCents) return result;
    if (mode==Student) {
        const long double meals=static_cast<long double>(amountCents)*mealsPerDay/dailyFoodCents;
        const auto count=smallNumber(meals);
        result.primary=QStringLiteral("约相当于你 %1 顿饭的预算").arg(count);
        result.compact=QStringLiteral("约 %1 顿饭的预算").arg(count);
        if (meals>=mealsPerDay*3) result.secondary=QStringLiteral("按你的设置，约 %1 天伙食费").arg(number(static_cast<long double>(amountCents)/dailyFoodCents));
        result.basis=QStringLiteral("按每日伙食费 %1、每天 %2 顿平均换算。餐费仅作价格参考。").arg(Transaction::formatAmount(dailyFoodCents)).arg(mealsPerDay);
    } else {
        const long double hours=static_cast<long double>(amountCents)/monthlyIncomeCents*workDaysPerMonth*dailyWorkHours;
        const auto time=hours<1?smallNumber(hours*60)+QStringLiteral(" 分钟"):number(hours)+QStringLiteral(" 小时");
        result.primary=QStringLiteral("约相当于你工作 %1").arg(time); result.compact=QStringLiteral("约 %1工作").arg(time);
        if (hours>=dailyWorkHours) result.secondary=QStringLiteral("按你的设置，约 %1 个工作日").arg(number(hours/dailyWorkHours));
        result.basis=QStringLiteral("按月到手 %1、每月工作 %2 天、每天 %3 小时平均换算。").arg(Transaction::formatAmount(monthlyIncomeCents),number(workDaysPerMonth),number(dailyWorkHours));
    }
    return result;
}
QString PricePerspectiveStore::settingsPath(const QString &databasePath) { return databasePath==":memory:"?QString():databasePath+QStringLiteral(".ui.ini"); }
PricePerspective PricePerspectiveStore::load(const QString &path) {
    PricePerspective p; if (path.isEmpty()) return p;
    QSettings settings(path,QSettings::IniFormat); settings.beginGroup("pricePerspective");
    const auto mode=settings.value("mode","off").toString(); p.mode=mode=="student"?PricePerspective::Student:mode=="worker"?PricePerspective::Worker:PricePerspective::Off;
    p.dailyFoodCents=settings.value("dailyFoodCents",0).toLongLong(); p.monthlyIncomeCents=settings.value("monthlyIncomeCents",0).toLongLong();
    p.mealsPerDay=settings.value("mealsPerDay",3).toInt(); p.workDaysPerMonth=settings.value("workDaysPerMonth",22).toDouble(); p.dailyWorkHours=settings.value("dailyWorkHours",8).toDouble();
    // Invalid preferences never generate a misleading cost estimate.
    if (!p.validationError().isEmpty()) p.mode=PricePerspective::Off;
    return p;
}
bool PricePerspectiveStore::save(const QString &path,const PricePerspective &p,QString &error) {
    error=p.validationError(); if (!error.isEmpty()) return false;
    if (path.isEmpty()) { error=QStringLiteral("当前未提供设置保存位置。"); return false; }
    QSettings settings(path,QSettings::IniFormat); settings.beginGroup("pricePerspective");
    const auto oldValues=[&] { QVariantMap result; for (const auto &key:settings.childKeys()) result.insert(key,settings.value(key)); return result; }();
    settings.setValue("mode",p.mode==PricePerspective::Student?"student":p.mode==PricePerspective::Worker?"worker":"off");
    settings.setValue("dailyFoodCents",p.dailyFoodCents); settings.setValue("monthlyIncomeCents",p.monthlyIncomeCents);
    settings.setValue("mealsPerDay",p.mealsPerDay); settings.setValue("workDaysPerMonth",p.workDaysPerMonth); settings.setValue("dailyWorkHours",p.dailyWorkHours); settings.endGroup(); settings.sync();
    if (settings.status()!=QSettings::NoError) {
        // Restore the shared QSettings cache too, so a failed save cannot leak into later price refreshes.
        settings.beginGroup("pricePerspective"); settings.remove(""); for (auto it=oldValues.cbegin();it!=oldValues.cend();++it) settings.setValue(it.key(),it.value()); settings.endGroup(); settings.sync();
        error=QStringLiteral("价格换算设置保存失败，请检查文件权限。"); return false;
    }
    return true;
}
