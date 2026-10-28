#pragma once
#include "DatabaseManager.h"
#include <QDateTime>
#include <QList>

struct Achievement {
    QString key,name,group,condition,message;
    int target=1,progress=0;
    QDateTime unlockedAt;
    bool unlocked() const { return progress>=target; }
};

class AchievementRepository {
public:
    explicit AchievementRepository(DatabaseManager &database):m_database(database) {}
    static QList<Achievement> definitions();
    // Capture distinct evidence up to each goal. Evidence survives source edits/deletion.
    // Existing records participate on first load; month selection never limits collection.
    bool load(QList<Achievement> &achievements);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database;
    QString m_error;
};
