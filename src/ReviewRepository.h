#pragma once
#include "DatabaseManager.h"
#include <QDateTime>
struct ReviewTask {
    qint64 id=0,transactionId=0,amountCents=0,brandId=0;
    QString name,category,brandName,note;
    int day=7,frequency=-1,worthiness=0;
    QDateTime purchasedAt,dueAt,completedAt;
};
class ReviewRepository {
public:
    explicit ReviewRepository(DatabaseManager &database):m_database(database) {}
    bool schedule(qint64 transactionId,qint64 brandId=0,const QDateTime &purchaseTime={});
    bool list(QList<ReviewTask> &tasks,bool completed=false,const QDateTime &now=QDateTime::currentDateTimeUtc(),bool includeFuture=false);
    bool dueCount(qint64 &count,const QDateTime &now=QDateTime::currentDateTimeUtc());
    bool complete(qint64 taskId,int frequency,int worthiness,const QString &note,const QDateTime &now=QDateTime::currentDateTimeUtc());
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database; QString m_error;
};
