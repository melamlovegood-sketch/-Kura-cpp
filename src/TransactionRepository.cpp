#include "TransactionRepository.h"
#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

TransactionRepository::TransactionRepository(DatabaseManager &database)
    : m_database(database)
{
}

bool TransactionRepository::add(Transaction &transaction)
{
    m_lastError = transaction.validationError();
    if (!m_lastError.isEmpty())
        return false;
    if (transaction.id() != 0) {
        m_lastError = QStringLiteral("这条记录已经保存，请使用修改操作。");
        return false;
    }
    auto connection = m_database.database();
    if (!connection.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，请检查数据目录后重新启动程序。");
        return false;
    }
    QSqlQuery query(connection);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO transactions(description, amount_cents, category, date) "
            "VALUES (:description, :amount, :category, :date)"))) {
        m_lastError = QStringLiteral("无法准备保存操作：%1").arg(query.lastError().text());
        return false;
    }
    query.bindValue(QStringLiteral(":description"), transaction.description());
    query.bindValue(QStringLiteral(":amount"), transaction.amountCents());
    query.bindValue(QStringLiteral(":category"), transaction.category());
    query.bindValue(QStringLiteral(":date"), transaction.date().toString(Qt::ISODate));
    if (!query.exec()) {
        const int code = query.lastError().nativeErrorCode().toInt() & 0xff;
        if (code == 5 || code == 6)
            m_lastError = QStringLiteral("数据库正在被占用，请稍后重试。");
        else if (code == 8)
            m_lastError = QStringLiteral("数据库只读，请检查文件和目录权限。");
        else
            m_lastError = QStringLiteral("无法写入消费记录：%1。请检查文件权限或稍后重试。")
                .arg(query.lastError().text());
        return false;
    }
    transaction = Transaction(query.lastInsertId().toLongLong(), transaction.description(),
                              transaction.amountCents(), transaction.category(), transaction.date());
    m_lastError.clear();
    return true;
}

bool TransactionRepository::findAll(QList<Transaction> &transactions)
{
    m_lastError.clear();
    auto connection = m_database.database();
    if (!connection.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，无法读取消费记录。");
        return false;
    }
    QSqlQuery query(connection);
    if (!query.exec(QStringLiteral(
            "SELECT id, description, amount_cents, category, date FROM transactions "
            "ORDER BY date DESC, id DESC"))) {
        m_lastError = QStringLiteral("无法读取消费记录：%1").arg(query.lastError().text());
        return false;
    }
    QList<Transaction> loaded;
    while (query.next()) {
        Transaction transaction(query.value(0).toLongLong(), query.value(1).toString(),
                                query.value(2).toLongLong(), query.value(3).toString(),
                                QDate::fromString(query.value(4).toString(), Qt::ISODate));
        if (transaction.id() <= 0 || !transaction.validationError().isEmpty()) {
            m_lastError = QStringLiteral("数据库中编号 %1 的记录无效，请检查数据文件。")
                .arg(transaction.id());
            return false;
        }
        loaded.append(transaction);
    }
    if (query.lastError().isValid()) {
        m_lastError = QStringLiteral("读取消费记录时发生错误：%1").arg(query.lastError().text());
        return false;
    }
    transactions = loaded;
    return true;
}

QString TransactionRepository::lastError() const { return m_lastError; }
