#pragma once
#include "DatabaseManager.h"
#include <QDateTime>
struct WishPoolSnapshot {
    qint64 id = 0, wishId = 0, targetCents = 0, savedCents = 0, pendingCents = 0;
    QString name;
};
struct SavingRecord { qint64 id = 0, amountCents = 0; QString description, goalName; QDateTime createdAt; };
class WishPoolRepository {
public:
    explicit WishPoolRepository(DatabaseManager &database);
    bool load(WishPoolSnapshot &snapshot);
    bool setGoal(qint64 wishId, bool replaceExisting = false);
    bool addSaving(qint64 cents, const QString &description, const QString &requestToken);
    bool history(QList<SavingRecord> &records);
    QString lastError() const;
private:
    bool fail(const QString &error);
    DatabaseManager &m_database;
    QString m_error;
};
