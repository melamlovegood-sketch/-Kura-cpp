#pragma once

#include <QSqlDatabase>
#include <QString>

// Owns one named connection. Repository queries must finish before destruction.
class DatabaseManager
{
public:
    explicit DatabaseManager(const QString &databasePath = {});
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool initialize();
    QSqlDatabase database() const;
    QString databasePath() const;
    QString lastError() const;
    static QString defaultDatabasePath();

private:
    QString m_databasePath;
    QString m_connectionName;
    QString m_lastError;
};
