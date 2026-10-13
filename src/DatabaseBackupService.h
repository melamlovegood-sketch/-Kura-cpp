#pragma once
#include <QString>
#include <memory>
class DatabaseManager;
class QTemporaryDir;

struct BackupInfo
{
    qsizetype expenseCount = 0;
    qsizetype budgetCount = 0;
    qsizetype wishCount = 0;
};

class DatabaseBackupService
{
public:
    explicit DatabaseBackupService(DatabaseManager &database);
    ~DatabaseBackupService();
    bool backup(const QString &destination, QString &error);
    // Snapshots and validates the selected file without modifying it.
    bool prepareRestore(const QString &source, BackupInfo &info, QString &error);
    bool restorePrepared(QString &safetyBackup, QString &error);
    void discardPrepared();
    bool isProtectedPath(const QString &path) const;
private:
    bool snapshot(const QString &source, const QString &destination, QString &error);
    DatabaseManager &m_database;
    std::unique_ptr<QTemporaryDir> m_preparedDirectory;
    QString m_preparedPath;
};
