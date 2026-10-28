#include "TransactionRepository.h"
#include "DatabaseManager.h"
#include "WorkflowStorage.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString writeError(const QSqlError &error)
{
    const int code = error.nativeErrorCode().toInt() & 0xff;
    if (code == 5 || code == 6)
        return QStringLiteral("数据库正在被占用，请稍后重试。");
    if (code == 8)
        return QStringLiteral("数据库只读，请检查文件和目录权限。");
    return QStringLiteral("数据库操作失败：%1。请检查文件权限或稍后重试。").arg(error.text());
}
}

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
            "INSERT INTO transactions(description, amount_cents, category, date, occurred_time, note, detail_category, source_image) "
            "VALUES (:description, :amount, :category, :date, :time, :note, :detail, :image)"))) {
        m_lastError = QStringLiteral("无法准备保存操作：%1").arg(query.lastError().text());
        return false;
    }
    query.bindValue(QStringLiteral(":description"), transaction.description());
    query.bindValue(QStringLiteral(":amount"), transaction.amountCents());
    query.bindValue(QStringLiteral(":category"), transaction.category());
    query.bindValue(QStringLiteral(":date"), transaction.date().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":time"), transaction.time().isValid()
        ? transaction.time().toString(QStringLiteral("HH:mm:ss")) : QStringLiteral(""));
    query.bindValue(QStringLiteral(":note"), transaction.note().isNull() ? QStringLiteral("") : transaction.note());
    query.bindValue(QStringLiteral(":detail"), transaction.detailCategory());
    query.bindValue(QStringLiteral(":image"), transaction.sourceImage().isNull() ? QByteArray("") : transaction.sourceImage());
    if (!query.exec()) {
        m_lastError = writeError(query.lastError());
        return false;
    }
    transaction = Transaction(query.lastInsertId().toLongLong(), transaction.description(),
                              transaction.amountCents(), transaction.category(), transaction.date(),
                              transaction.time(), transaction.note(), transaction.detailCategory(), transaction.sourceImage());
    m_lastError.clear();
    return true;
}

bool TransactionRepository::update(const Transaction &transaction)
{
    m_lastError = transaction.validationError();
    if (!m_lastError.isEmpty())
        return false;
    if (transaction.id() <= 0) {
        m_lastError = QStringLiteral("请选择已经保存的消费记录。");
        return false;
    }
    auto connection = m_database.database();
    if (!connection.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，无法修改记录。");
        return false;
    }
    QSqlQuery query(connection);
    if (!query.prepare(QStringLiteral(
            "UPDATE transactions SET description=:description, amount_cents=:amount, "
            "category=:category, date=:date, occurred_time=:time, note=:note, detail_category=:detail, source_image=:image WHERE id=:id"))) {
        m_lastError = writeError(query.lastError());
        return false;
    }
    query.bindValue(QStringLiteral(":id"), transaction.id());
    query.bindValue(QStringLiteral(":description"), transaction.description());
    query.bindValue(QStringLiteral(":amount"), transaction.amountCents());
    query.bindValue(QStringLiteral(":category"), transaction.category());
    query.bindValue(QStringLiteral(":date"), transaction.date().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":time"), transaction.time().isValid()
        ? transaction.time().toString(QStringLiteral("HH:mm:ss")) : QStringLiteral(""));
    query.bindValue(QStringLiteral(":note"), transaction.note().isNull() ? QStringLiteral("") : transaction.note());
    query.bindValue(QStringLiteral(":detail"), transaction.detailCategory());
    query.bindValue(QStringLiteral(":image"), transaction.sourceImage().isNull() ? QByteArray("") : transaction.sourceImage());
    if (!query.exec()) {
        m_lastError = writeError(query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_lastError = QStringLiteral("这条记录已不存在，请刷新列表后重试。");
        return false;
    }
    m_lastError.clear();
    return true;
}

bool TransactionRepository::remove(qint64 id)
{
    m_lastError.clear();
    if (id <= 0) {
        m_lastError = QStringLiteral("请选择有效的消费记录。");
        return false;
    }
    auto connection = m_database.database();
    if (!connection.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，无法删除记录。");
        return false;
    }
    WorkflowStorage::Transaction removal(connection,m_lastError); if (!removal.ok()) return false;
    QSqlQuery query(connection);
    for (const auto &sql:{"DELETE FROM transaction_reviews WHERE transaction_id=:id","UPDATE bill_import_keys SET transaction_id=NULL WHERE transaction_id=:id"}) { query.prepare(sql); query.bindValue(":id",id); if (!query.exec()) { m_lastError=writeError(query.lastError()); return false; } query.finish(); }
    query.prepare("DELETE FROM review_tasks WHERE transaction_id=:id AND completed_ms IS NULL"); query.bindValue(":id",id);
    if (!query.exec()) { m_lastError=writeError(query.lastError()); return false; } query.finish();
    if (!query.prepare(QStringLiteral("DELETE FROM transactions WHERE id=:id"))) {
        m_lastError = writeError(query.lastError());
        return false;
    }
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        m_lastError = writeError(query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_lastError = QStringLiteral("这条记录已不存在，请刷新列表后重试。");
        return false;
    }
    query.finish(); return removal.commit();
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
            "SELECT id, description, amount_cents, category, date, occurred_time, note, detail_category, source_image FROM transactions "
            "ORDER BY date DESC, occurred_time DESC, id DESC"))) {
        m_lastError = QStringLiteral("无法读取消费记录：%1").arg(query.lastError().text());
        return false;
    }
    QList<Transaction> loaded;
    while (query.next()) {
        const QString timeText = query.value(5).toString();
        const QTime time = QTime::fromString(timeText, QStringLiteral("HH:mm:ss"));
        Transaction transaction(query.value(0).toLongLong(), query.value(1).toString(),
                                query.value(2).toLongLong(), query.value(3).toString(),
                                QDate::fromString(query.value(4).toString(), Qt::ISODate), time,
                                query.value(6).toString(), query.value(7).toString(), query.value(8).toByteArray());
        if (transaction.id() <= 0 || !transaction.validationError().isEmpty()
            || (!timeText.isEmpty() && (!time.isValid() || time.toString(QStringLiteral("HH:mm:ss")) != timeText))) {
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
