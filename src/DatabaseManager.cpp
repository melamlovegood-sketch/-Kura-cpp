#include "DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

DatabaseManager::DatabaseManager(const QString &databasePath)
    : m_databasePath(databasePath.isEmpty() ? defaultDatabasePath() : databasePath),
      m_connectionName(QStringLiteral("kura-%1").arg(QUuid::createUuid().toString()))
{
}

DatabaseManager::~DatabaseManager()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            auto connection = database();
            connection.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QString DatabaseManager::defaultDatabasePath()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return directory.isEmpty() ? QString() : QDir(directory).filePath(QStringLiteral("kura.sqlite"));
}

bool DatabaseManager::initialize()
{
    m_lastError.clear();
    if (m_databasePath.isEmpty()) {
        m_lastError = QStringLiteral("无法确定本机数据目录。");
        return false;
    }
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        m_lastError = QStringLiteral("缺少 SQLite 驱动，请安装 Qt SQL 或补齐 sqldrivers/qsqlite 插件。");
        return false;
    }
    if (m_databasePath != QStringLiteral(":memory:")) {
        const QString directory = QFileInfo(m_databasePath).absolutePath();
        if (!QDir().mkpath(directory)) {
            m_lastError = QStringLiteral("无法创建数据目录：%1。请检查目录权限。").arg(directory);
            return false;
        }
    }

    auto connection = QSqlDatabase::contains(m_connectionName)
        ? database() : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    if (!connection.isOpen()) {
        connection.setDatabaseName(m_databasePath);
        connection.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=1000"));
    }
    if (!connection.isOpen() && !connection.open()) {
        m_lastError = QStringLiteral("无法打开数据库：%1。%2")
            .arg(m_databasePath, connection.lastError().text());
        return false;
    }

    QSqlQuery query(connection);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS transactions ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "description TEXT NOT NULL CHECK(length(trim(description)) BETWEEN 1 AND 100), "
            "amount_cents INTEGER NOT NULL CHECK(typeof(amount_cents) = 'integer' "
            "AND amount_cents BETWEEN 1 AND 9999999999), "
            "category TEXT NOT NULL CHECK(length(category) > 0), "
            "date TEXT NOT NULL CHECK(length(date) = 10))"))) {
        m_lastError = QStringLiteral("无法初始化消费记录表：%1").arg(query.lastError().text());
        return false;
    }
    return true;
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::contains(m_connectionName)
        ? QSqlDatabase::database(m_connectionName, false) : QSqlDatabase();
}

QString DatabaseManager::databasePath() const { return m_databasePath; }
QString DatabaseManager::lastError() const { return m_lastError; }
