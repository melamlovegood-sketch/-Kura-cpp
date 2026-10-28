#pragma once
#include <QSqlDatabase>
#include <QList>
#include <QString>

struct UserCategory { qint64 id=0; QString key,name; bool active=true; };
class CategoryRepository {
public:
    explicit CategoryRepository(QSqlDatabase database):m_database(database) {}
    bool load(QList<UserCategory> &categories,bool includeInactive=false);
    bool add(const QString &name);
    bool rename(const QString &key,const QString &name);
    bool setActive(const QString &key,bool active);
    QString lastError() const { return m_error; }
    static bool upgrade(QSqlDatabase database,QString &error);
private:
    QString validateName(const QString &name,const QString &except={});
    QSqlDatabase m_database;
    QString m_error;
};
