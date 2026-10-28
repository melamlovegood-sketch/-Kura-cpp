#pragma once
#include "DatabaseBackupService.h"
#include <QDateTime>

class AutoBackupService {
public:
    AutoBackupService(DatabaseManager &database, const QString &settingsPath);
    bool enabled() const;
    int keepCount() const;
    bool configure(bool enabled, int keepCount, QString &error);
    bool runIfDue(QString &error, const QDateTime &now = QDateTime::currentDateTime());
    QString directory() const;
    QString lastBackupText() const;
    bool recordManualBackup(QString &error);
private:
    DatabaseManager &m_database;
    QString m_settingsPath;
};
