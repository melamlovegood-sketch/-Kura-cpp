#include "WishlistRepository.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString queryError(const QSqlError &error)
{
    const int code = error.nativeErrorCode().toInt() & 0xff;
    if (code == 5 || code == 6)
        return QStringLiteral("数据库正在被占用，请稍后重试。");
    if (code == 8)
        return QStringLiteral("数据库只读，请检查文件和目录权限。");
    return QStringLiteral("数据库操作失败：%1").arg(error.text());
}
bool validNow(const QDateTime &now)
{
    return now.isValid() && now.date().year() >= 1 && now.date().year() <= 9999;
}
void bindInput(QSqlQuery &query, const WishItem &item)
{
    query.bindValue(QStringLiteral(":name"), item.name.trimmed());
    query.bindValue(QStringLiteral(":amount"), item.estimatedCents);
    query.bindValue(QStringLiteral(":category"), item.category);
    query.bindValue(QStringLiteral(":days"), item.coolingDays);
    query.bindValue(QStringLiteral(":priority"), item.priority);
    const QString reason = item.reason.trimmed();
    // An omitted optional QString must be stored as empty text, not SQL NULL.
    query.bindValue(QStringLiteral(":reason"), reason.isNull() ? QStringLiteral("") : reason);
}
}

WishlistRepository::WishlistRepository(DatabaseManager &database) : m_database(database) {}
QString WishlistRepository::lastError() const { return m_lastError; }

bool WishlistRepository::connectionReady()
{
    if (m_database.database().isOpen())
        return true;
    m_lastError = QStringLiteral("数据库未打开，请检查数据目录后重新启动程序。");
    return false;
}

bool WishlistRepository::add(WishItem &item, const QDateTime &now)
{
    m_lastError = item.inputError();
    if (!m_lastError.isEmpty())
        return false;
    if (item.id != 0 || !validNow(now) || !validNow(now.toUTC().addSecs(item.coolingDays * 86400LL))) {
        m_lastError = QStringLiteral("新愿望的编号或当前时间无效。");
        return false;
    }
    if (!connectionReady()) return false;
    WishItem saved = item;
    saved.name = saved.name.trimmed();
    saved.reason = saved.reason.trimmed();
    saved.createdAt = now.toUTC();
    saved.readyAt = saved.createdAt.addSecs(saved.coolingDays * 86400LL);
    saved.status = WishStatus::Waiting;
    saved.closedAt = {};
    saved.transactionId = 0;
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("INSERT INTO wishlist(name, estimated_cents, category, cooling_days, reason, "
        "created_ms, ready_ms, status,priority) VALUES(:name,:amount,:category,:days,:reason,:created,:ready,0,:priority)"));
    bindInput(query, saved);
    query.bindValue(QStringLiteral(":created"), saved.createdAt.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":ready"), saved.readyAt.toMSecsSinceEpoch());
    if (!query.exec()) {
        m_lastError = queryError(query.lastError());
        return false;
    }
    saved.id = query.lastInsertId().toLongLong();
    item = saved;
    return true;
}

bool WishlistRepository::update(const WishItem &item, const QDateTime &now)
{
    m_lastError = item.inputError();
    if (!m_lastError.isEmpty())
        return false;
    const auto ready = now.toUTC().addSecs(item.coolingDays * 86400LL);
    if (item.id <= 0 || !validNow(now) || !validNow(ready)) {
        m_lastError = QStringLiteral("愿望编号或当前时间无效。");
        return false;
    }
    if (!connectionReady()) return false;
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("UPDATE wishlist SET name=:name,estimated_cents=:amount,category=:category,"
        "cooling_days=:days,priority=:priority,reason=:reason,ready_ms=:ready WHERE id=:id AND status=0 AND created_ms<=:now"));
    bindInput(query, item);
    query.bindValue(QStringLiteral(":id"), item.id);
    query.bindValue(QStringLiteral(":ready"), ready.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":now"), now.toMSecsSinceEpoch());
    if (!query.exec()) {
        m_lastError = queryError(query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_lastError = QStringLiteral("愿望已不存在、已经完成，或系统时间早于创建时间。请刷新后重试。");
        return false;
    }
    return true;
}

bool WishlistRepository::remove(qint64 id)
{
    m_lastError.clear();
    if (id <= 0) {
        m_lastError = QStringLiteral("请选择有效的愿望。");
        return false;
    }
    if (!connectionReady()) return false;
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("DELETE FROM wishlist WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        m_lastError = queryError(query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_lastError = QStringLiteral("这条愿望已不存在，请刷新后重试。");
        return false;
    }
    return true;
}

bool WishlistRepository::abandon(qint64 id, const QDateTime &now)
{
    m_lastError.clear();
    if (id <= 0 || !validNow(now)) {
        m_lastError = QStringLiteral("愿望编号或当前时间无效。");
        return false;
    }
    if (!connectionReady()) return false;
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("UPDATE wishlist SET status=2,closed_ms=:now "
                                 "WHERE id=:id AND status=0 AND created_ms<=:now"));
    query.bindValue(QStringLiteral(":id"), id);
    query.bindValue(QStringLiteral(":now"), now.toMSecsSinceEpoch());
    if (!query.exec()) {
        m_lastError = queryError(query.lastError());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_lastError = QStringLiteral("愿望已不存在、已经完成，或系统时间早于创建时间。请刷新后重试。");
        return false;
    }
    return true;
}

bool WishlistRepository::purchase(qint64 id, Transaction &expense, const QDateTime &now)
{
    m_lastError = expense.validationError();
    if (!m_lastError.isEmpty())
        return false;
    if (id <= 0 || expense.id() != 0 || !validNow(now)) {
        m_lastError = QStringLiteral("愿望编号、消费编号或当前时间无效。");
        return false;
    }
    if (!connectionReady()) return false;
    auto connection = m_database.database();
    if (!connection.transaction()) {
        m_lastError = queryError(connection.lastError());
        return false;
    }
    // Claim the still-open, eligible wish before inserting an expense. A second
    // connection cannot purchase the same wish, even if its UI snapshot is stale.
    QSqlQuery claim(connection);
    claim.prepare(QStringLiteral("UPDATE wishlist SET status=1,closed_ms=:now "
        "WHERE id=:id AND status=0 AND ready_ms<=:now"));
    claim.bindValue(QStringLiteral(":id"), id);
    claim.bindValue(QStringLiteral(":now"), now.toMSecsSinceEpoch());
    if (!claim.exec() || claim.numRowsAffected() != 1) {
        m_lastError = claim.lastError().isValid() ? queryError(claim.lastError())
            : QStringLiteral("冷静期尚未结束，或愿望已不存在／已经完成。请刷新后重试。");
        claim.finish();
        connection.rollback();
        return false;
    }
    claim.finish();
    Transaction saved = expense;
    TransactionRepository expenses(m_database);
    if (!expenses.add(saved)) {
        m_lastError = expenses.lastError();
        connection.rollback();
        return false;
    }
    QSqlQuery link(connection);
    link.prepare(QStringLiteral("UPDATE wishlist SET transaction_id=:expense WHERE id=:id"));
    link.bindValue(QStringLiteral(":expense"), saved.id());
    link.bindValue(QStringLiteral(":id"), id);
    if (!link.exec()) {
        m_lastError = queryError(link.lastError());
        link.finish();
        connection.rollback();
        return false;
    }
    link.finish();
    if (!connection.commit()) {
        m_lastError = queryError(connection.lastError());
        connection.rollback();
        return false;
    }
    expense = saved;
    return true;
}

bool WishlistRepository::findAll(QList<WishItem> &items)
{
    m_lastError.clear();
    if (!connectionReady()) return false;
    QSqlQuery query(m_database.database());
    if (!query.exec(QStringLiteral("SELECT id,name,estimated_cents,category,cooling_days,reason,"
        "created_ms,ready_ms,status,closed_ms,transaction_id,priority FROM wishlist ORDER BY created_ms DESC,id DESC"))) {
        m_lastError = queryError(query.lastError());
        return false;
    }
    QList<WishItem> loaded;
    while (query.next()) {
        WishItem item;
        item.id = query.value(0).toLongLong();
        item.name = query.value(1).toString();
        item.estimatedCents = query.value(2).toLongLong();
        item.category = query.value(3).toString();
        item.coolingDays = query.value(4).toInt();
        item.reason = query.value(5).toString();
        item.createdAt = QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong()).toUTC();
        item.readyAt = QDateTime::fromMSecsSinceEpoch(query.value(7).toLongLong()).toUTC();
        item.status = static_cast<WishStatus>(query.value(8).toInt());
        if (!query.value(9).isNull())
            item.closedAt = QDateTime::fromMSecsSinceEpoch(query.value(9).toLongLong()).toUTC();
        item.transactionId = query.value(10).toLongLong();
        item.priority = query.value(11).toInt();
        if (item.id <= 0 || !item.validationError().isEmpty()) {
            m_lastError = QStringLiteral("数据库中编号 %1 的愿望无效，请检查数据文件。").arg(item.id);
            return false;
        }
        loaded.append(item);
    }
    if (query.lastError().isValid()) {
        m_lastError = queryError(query.lastError());
        return false;
    }
    items = loaded;
    return true;
}
