#include "DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

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
            "date TEXT NOT NULL CHECK(length(date) = 10), "
            "occurred_time TEXT NOT NULL DEFAULT '' CHECK(occurred_time = '' OR length(occurred_time) = 8), "
            "note TEXT NOT NULL DEFAULT '' CHECK(length(note) <= 1000))"))) {
        m_lastError = QStringLiteral("无法初始化消费记录表：%1").arg(query.lastError().text());
        return false;
    }
    // Add fields without rebuilding the table or changing existing ids and dates.
    if (!query.exec(QStringLiteral("PRAGMA table_info(transactions)"))) {
        m_lastError = QStringLiteral("无法检查消费记录表结构：%1").arg(query.lastError().text());
        return false;
    }
    QSet<QString> columns;
    while (query.next())
        columns.insert(query.value(1).toString());
    if (query.lastError().isValid()) {
        m_lastError = QStringLiteral("无法读取消费记录表结构：%1").arg(query.lastError().text());
        return false;
    }
    query.finish();
    if (!columns.contains(QStringLiteral("occurred_time")) || !columns.contains(QStringLiteral("note"))) {
        if (!connection.transaction()) {
            m_lastError = QStringLiteral("无法开始消费记录升级：%1").arg(connection.lastError().text());
            return false;
        }
        const auto migrate = [&](const QString &column, const QString &statement) {
            if (columns.contains(column) || query.exec(statement))
                return true;
            m_lastError = QStringLiteral("无法升级消费记录表：%1").arg(query.lastError().text());
            query.finish();
            connection.rollback();
            return false;
        };
        if (!migrate(QStringLiteral("occurred_time"), QStringLiteral(
                "ALTER TABLE transactions ADD COLUMN occurred_time TEXT NOT NULL DEFAULT '' "
                "CHECK(occurred_time = '' OR length(occurred_time) = 8)"))
            || !migrate(QStringLiteral("note"), QStringLiteral(
                "ALTER TABLE transactions ADD COLUMN note TEXT NOT NULL DEFAULT '' CHECK(length(note) <= 1000)")))
            return false;
        query.finish();
        if (!connection.commit()) {
            m_lastError = QStringLiteral("无法提交消费记录升级：%1").arg(connection.lastError().text());
            connection.rollback();
            return false;
        }
    }
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS monthly_budgets ("
            "month TEXT PRIMARY KEY NOT NULL CHECK(length(month) = 7), "
            "limit_cents INTEGER NOT NULL CHECK(typeof(limit_cents) = 'integer' "
            "AND limit_cents BETWEEN 0 AND 9999999999))"))) {
        m_lastError = QStringLiteral("无法初始化月度预算表：%1").arg(query.lastError().text());
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
