#include "AutoBackupService.h"
#include "DatabaseManager.h"
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>

AutoBackupService::AutoBackupService(DatabaseManager &database, const QString &settingsPath)
    : m_database(database), m_settingsPath(settingsPath) {}

bool AutoBackupService::enabled() const
{
    if (m_settingsPath.isEmpty()) return false;
    return QSettings(m_settingsPath,QSettings::IniFormat).value("backup/enabled",false).toBool();
}
int AutoBackupService::keepCount() const
{
    if (m_settingsPath.isEmpty()) return 7;
    return qBound(1,QSettings(m_settingsPath,QSettings::IniFormat).value("backup/keepCount",7).toInt(),30);
}
bool AutoBackupService::configure(bool enabled, int count, QString &error)
{
    error.clear();
    if (m_settingsPath.isEmpty() || m_database.databasePath()==":memory:") {
        error=QStringLiteral("临时内存数据不支持自动备份。"); return false;
    }
    QSettings settings(m_settingsPath,QSettings::IniFormat);
    settings.setValue("backup/enabled",enabled); settings.setValue("backup/keepCount",qBound(1,count,30)); settings.sync();
    if (settings.status()!=QSettings::NoError) { error=QStringLiteral("自动备份设置未能保存，请检查目录权限。"); return false; }
    return true;
}
QString AutoBackupService::directory() const { return m_database.databasePath()+QStringLiteral(".backups"); }
QString AutoBackupService::lastBackupText() const
{
    if (m_settingsPath.isEmpty()) return QStringLiteral("尚未备份");
    const auto date=QSettings(m_settingsPath,QSettings::IniFormat).value("backup/lastSuccess").toDateTime();
    return date.isValid()?QStringLiteral("最近备份 %1").arg(date.toLocalTime().toString("MM-dd HH:mm")):QStringLiteral("尚未备份");
}
bool AutoBackupService::recordManualBackup(QString &error)
{
    if (m_settingsPath.isEmpty()) return true;
    QSettings settings(m_settingsPath,QSettings::IniFormat);
    settings.setValue("backup/lastSuccess",QDateTime::currentDateTime()); settings.sync();
    if (settings.status()!=QSettings::NoError) { error=QStringLiteral("备份文件已保存，但备份时间未能记录。"); return false; }
    return true;
}
bool AutoBackupService::runIfDue(QString &error, const QDateTime &now)
{
    error.clear();
    if (!enabled()) return true;
    if (!now.isValid() || !m_database.database().isOpen()) { error=QStringLiteral("数据尚不可用，自动备份未完成。"); return false; }
    QDir folder(directory());
    if (!folder.mkpath(".")) { error=QStringLiteral("无法创建自动备份文件夹。"); return false; }
    QLockFile lock(folder.filePath("auto-backup.lock"));
    if (!lock.tryLock(0)) { error=QStringLiteral("其他窗口正在备份，请稍后重试。"); return false; }
    QSettings settings(m_settingsPath,QSettings::IniFormat);
    if (settings.value("backup/lastAutoSuccess").toDateTime().toLocalTime().date()==now.toLocalTime().date()
        && QFileInfo::exists(settings.value("backup/lastAutoPath").toString())) return true;
    const auto name=QStringLiteral("kura-auto-%1-%2.sqlite")
        .arg(now.toLocalTime().toString("yyyyMMdd-HHmmsszzz"),QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto path=folder.filePath(name);
    DatabaseBackupService backup(m_database);
    if (!backup.backup(path,error)) return false;
    settings.setValue("backup/lastAutoSuccess",now); settings.setValue("backup/lastAutoPath",path);
    settings.setValue("backup/lastSuccess",now); settings.sync();
    if (settings.status()!=QSettings::NoError) { error=QStringLiteral("自动备份已生成，但备份时间未能记录。"); return false; }
    // Rotate only our own generated files; manual backups and links are untouched.
    const QRegularExpression pattern(QStringLiteral("^kura-auto-[0-9]{8}-[0-9]{9}-[0-9a-f-]{36}\\.sqlite$"));
    QStringList files;
    for (const auto &item:folder.entryInfoList(QDir::Files|QDir::NoSymLinks,QDir::Name|QDir::Reversed))
        if (pattern.match(item.fileName()).hasMatch()) files.append(item.fileName());
    int retained=0;
    // Keep the successful snapshot even if the system clock moved backwards.
    files.removeAll(name); files.prepend(name);
    for (const auto &file:files) {
        if (++retained<=keepCount()) continue;
        if (!folder.remove(file)) { error=QStringLiteral("自动备份已完成，部分旧备份未能清理。"); return false; }
    }
    return true;
}
