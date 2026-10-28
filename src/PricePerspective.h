#pragma once
#include <QString>
#include <QtGlobal>

struct PriceQuote {
    QString primary,secondary,compact,basis;
    bool isEmpty() const { return primary.isEmpty(); }
};
struct PricePerspective {
    enum Mode { Off,Student,Worker };
    Mode mode=Off;
    qint64 dailyFoodCents=0,monthlyIncomeCents=0;
    int mealsPerDay=3;
    double workDaysPerMonth=22,dailyWorkHours=8;
    QString validationError() const;
    bool ready() const;
    QString reference() const;
    PriceQuote quote(qint64 amountCents) const;
};
namespace PricePerspectiveStore {
PricePerspective load(const QString &path);
bool save(const QString &path,const PricePerspective &value,QString &error);
QString settingsPath(const QString &databasePath);
}
