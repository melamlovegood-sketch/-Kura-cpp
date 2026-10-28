#pragma once
#include "DatabaseManager.h"
#include "WorkflowSchema.h"
#include <QDateTime>
#include <QHash>
#include <QSet>

namespace UnifiedWishlistSchema {
QList<WorkflowTable> tables();
}

class UnifiedWishlistRepository {
public:
    explicit UnifiedWishlistRepository(DatabaseManager &database):m_database(database) {}
    bool synchronize();
    bool state(QHash<qint64,qint64> &tracks,QHash<qint64,qint64> &positions,QSet<qint64> *legacyArchives=nullptr);
    bool saveOrder(const QList<qint64> &ids);
    bool recordPrice(qint64 wishId,qint64 cents,const QString &source,const QString &token,
                     const QDateTime &observedAt,qint64 &trackId);
    bool closeWish(qint64 wishId,bool removing=false);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database;
    QString m_error;
};
