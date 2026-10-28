#pragma once
#include "DatabaseManager.h"
#include "MonthlyBudget.h"
#include <QDateTime>
#include <QJsonObject>
struct MonthlyReviewExpense { qint64 id=0,cents=0; QString name,category,note; QDate date; QTime time; int tag=0; QString purchaseNote,purposeCategory,budgetScope; };
struct MonthlyCategory { QString key; qint64 cents=0,previousCents=0,count=0; };
struct MonthlyReviewData {
    QDate month;
    qint64 totalCents=0,basicCents=0,discretionaryCents=0,previousCents=0,count=0,savingsCents=0,savingsCount=0,abandonedCount=0,worthCount=0,regretCount=0,biggestSavingCents=0;
    QString biggestSaving,factsHash,factsJson,localStory,localPersona;
    QList<MonthlyCategory> categories;
    QList<MonthlyReviewExpense> expenses;
    MonthlyBudgetStatus budget;
    bool hasData() const { return count>0 || savingsCount>0 || abandonedCount>0; }
};
struct MonthlyStory { qint64 id=0; QDate month; QString factsHash,factsJson,story,persona,source; QDateTime generatedAt; bool stale=false; };
class MonthlyReviewRepository {
public:
    explicit MonthlyReviewRepository(DatabaseManager &database):m_database(database) {}
    bool load(const QDate &month,MonthlyReviewData &data);
    bool loadStory(const MonthlyReviewData &data,MonthlyStory &story);
    bool saveStory(const QDate &month,const QString &expectedHash,const QString &story,const QString &persona,const QString &source);
    bool setTag(qint64 expenseId,int tag,const QString &note);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database; QString m_error;
};
