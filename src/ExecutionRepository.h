#pragma once
#include "DatabaseManager.h"
#include "WishItem.h"
#include <QDateTime>
struct ExecutionSession {
    static constexpr int MaximumAdviceLength=128000;
    qint64 id=0,estimatedCents=0,transactionId=0;
    QString name,category,mode=QStringLiteral("fast"),state=QStringLiteral("active"),notes,advice;
    int durationSeconds=900;
    QDateTime startedAt,deadline,closedAt;
};
class ExecutionRepository {
public:
    explicit ExecutionRepository(DatabaseManager &database):m_database(database) {}
    bool loadActive(ExecutionSession &session);
    bool start(ExecutionSession &session,const QDateTime &now=QDateTime::currentDateTimeUtc(),qint64 productId=0);
    bool saveNotes(qint64 id,const QString &notes,const QString &advice);
    bool updateProduct(qint64 id,const QString &name,const QString &category,qint64 estimatedCents);
    bool setMode(qint64 id,const QString &mode);
    bool deferToWishlist(qint64 id,WishItem &item,qint64 productId=0);
    bool finish(qint64 id,const QString &decision,const QDateTime &now=QDateTime::currentDateTimeUtc());
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database; QString m_error;
};
