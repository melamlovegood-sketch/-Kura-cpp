#pragma once

#include "WishItem.h"
#include <QList>
class DatabaseManager;

class WishlistRepository
{
public:
    explicit WishlistRepository(DatabaseManager &database);
    bool add(WishItem &item, const QDateTime &now = QDateTime::currentDateTimeUtc(),qint64 productId=0);
    // Editing an open wish restarts its cooling period; completed history is immutable.
    bool update(const WishItem &item, const QDateTime &now = QDateTime::currentDateTimeUtc());
    bool remove(qint64 id);
    bool abandon(qint64 id, const QDateTime &now = QDateTime::currentDateTimeUtc());
    // Both the expense and the wish state commit together; expense is unchanged on failure.
    bool purchase(qint64 id, Transaction &expense,
                  const QDateTime &now = QDateTime::currentDateTimeUtc(), qint64 brandId = 0);
    bool findAll(QList<WishItem> &items);
    QString lastError() const;
private:
    bool connectionReady();
    DatabaseManager &m_database;
    QString m_lastError;
};
