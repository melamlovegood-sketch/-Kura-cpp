#include "WishPoolRepository.h"
#include "Transaction.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

WishPoolRepository::WishPoolRepository(DatabaseManager &database) : m_database(database) {}
QString WishPoolRepository::lastError() const { return m_error; }
bool WishPoolRepository::fail(const QString &error) { m_error = error; return false; }

bool WishPoolRepository::load(WishPoolSnapshot &snapshot)
{
    if (!m_database.database().isOpen()) return fail(QStringLiteral("数据库未打开。"));
    QSqlQuery query(m_database.database());
    // One SQL statement keeps goal, its savings and unassigned savings together.
    if (!query.exec(QStringLiteral("SELECT p.id,p.wish_id,p.name,p.target_cents,COALESCE((SELECT SUM(amount_cents) FROM savings_records WHERE pool_id=p.id),0),(SELECT COALESCE(SUM(amount_cents),0) FROM savings_records WHERE pool_id IS NULL) FROM (SELECT 1) LEFT JOIN wish_pools p ON p.closed_ms IS NULL")) || !query.next())
        return fail(QStringLiteral("无法读取许愿池：%1").arg(query.lastError().text()));
    WishPoolSnapshot loaded;
    loaded.id = query.value(0).toLongLong(); loaded.wishId = query.value(1).toLongLong();
    loaded.name = query.value(2).toString(); loaded.targetCents = query.value(3).toLongLong();
    bool savedOk = false, pendingOk = false;
    loaded.savedCents = query.value(4).toLongLong(&savedOk); loaded.pendingCents = query.value(5).toLongLong(&pendingOk);
    if (!savedOk || !pendingOk || loaded.savedCents < 0 || loaded.pendingCents < 0)
        return fail(QStringLiteral("许愿池金额无效或超过支持范围。"));
    if (loaded.id && (loaded.id < 0 || loaded.wishId <= 0 || loaded.name.trimmed().isEmpty() || loaded.name.size()>100
        || loaded.targetCents < 1 || loaded.targetCents > Transaction::MaximumAmountCents))
        return fail(QStringLiteral("许愿池目标无效。"));
    if (query.next() || query.lastError().isValid()) return fail(QStringLiteral("许愿池目标数据不一致。"));
    snapshot = loaded; m_error.clear(); return true;
}

bool WishPoolRepository::setGoal(qint64 wishId, bool replace)
{
    if (wishId <= 0) return fail(QStringLiteral("请选择有效的待决定愿望。"));
    auto db = m_database.database();
    if (!db.isOpen()) return fail(QStringLiteral("数据库未打开。"));
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE"))) return fail(QStringLiteral("数据库正在被占用，无法设置目标。"));
    const auto rollback = [&](const QString &error) { query.finish(); db.rollback(); return fail(error); };
    query.prepare(QStringLiteral("SELECT name,estimated_cents FROM wishlist WHERE id=:id AND status=0")); query.bindValue(":id", wishId);
    if (!query.exec() || !query.next()) return rollback(QStringLiteral("愿望已不存在或已经完成，请刷新清单。"));
    const auto name = query.value(0).toString(); const auto target = query.value(1).toLongLong(); query.finish();
    if (name.trimmed().isEmpty() || name.size()>100 || target<1 || target>Transaction::MaximumAmountCents)
        return rollback(QStringLiteral("愿望名称或价格无效。"));
    if (!query.exec(QStringLiteral("SELECT wish_id FROM wish_pools WHERE closed_ms IS NULL"))) return rollback(query.lastError().text());
    if (query.next()) {
        if (query.value(0).toLongLong() == wishId) { query.finish(); if (!db.commit()) return rollback(db.lastError().text()); m_error.clear(); return true; }
        if (!replace) return rollback(QStringLiteral("已有许愿目标，请先确认是否切换。"));
    }
    query.finish(); const qint64 now = QDateTime::currentMSecsSinceEpoch();
    query.prepare(QStringLiteral("UPDATE wish_pools SET closed_ms=MAX(:now,created_ms) WHERE closed_ms IS NULL")); query.bindValue(":now", now);
    if (!query.exec()) return rollback(query.lastError().text()); query.finish();
    query.prepare(QStringLiteral("INSERT INTO wish_pools(wish_id,name,target_cents,created_ms) VALUES(:wish,:name,:target,:now)"));
    query.bindValue(":wish", wishId); query.bindValue(":name", name); query.bindValue(":target", target); query.bindValue(":now", now);
    if (!query.exec()) return rollback(query.lastError().text());
    const auto id = query.lastInsertId().toLongLong(); query.finish();
    query.prepare(QStringLiteral("UPDATE savings_records SET pool_id=:id WHERE pool_id IS NULL")); query.bindValue(":id", id);
    if (!query.exec()) return rollback(query.lastError().text()); query.finish();
    if (!db.commit()) return rollback(db.lastError().text()); m_error.clear(); return true;
}

bool WishPoolRepository::addSaving(qint64 cents, const QString &description, const QString &token)
{
    const QString text = description.trimmed();
    if (cents < 1 || cents > Transaction::MaximumAmountCents || text.isEmpty() || text.size()>1000 || token.isEmpty() || token.size()>64)
        return fail(QStringLiteral("请确认忍住的金额与名称。"));
    auto db = m_database.database();
    if (!db.isOpen()) return fail(QStringLiteral("数据库未打开。"));
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE"))) return fail(QStringLiteral("数据库正在被占用，请稍后重试。"));
    const auto rollback = [&](const QString &error) { query.finish(); db.rollback(); return fail(error); };
    query.prepare(QStringLiteral("SELECT amount_cents,description FROM savings_records WHERE request_token=:token")); query.bindValue(":token", token);
    if (!query.exec()) return rollback(query.lastError().text());
    if (query.next()) {
        const bool same = query.value(0).toLongLong()==cents && query.value(1).toString()==text; query.finish();
        if (!same) return rollback(QStringLiteral("这次操作已经保存，请关闭后重新录入。"));
        if (!db.commit()) return rollback(db.lastError().text()); m_error.clear(); return true;
    }
    query.finish();
    query.prepare(QStringLiteral("INSERT INTO savings_records(pool_id,amount_cents,description,created_ms,request_token) VALUES((SELECT id FROM wish_pools WHERE closed_ms IS NULL),:amount,:text,:now,:token)"));
    query.bindValue(":amount", cents); query.bindValue(":text", text); query.bindValue(":now", QDateTime::currentMSecsSinceEpoch()); query.bindValue(":token", token);
    if (!query.exec()) return rollback(QStringLiteral("忍住记录保存失败：%1").arg(query.lastError().text())); query.finish();
    // Reject integer SUM overflow before committing, preserving the old state.
    WishPoolSnapshot snapshot; if (!load(snapshot)) return rollback(m_error);
    if (!db.commit()) return rollback(db.lastError().text()); m_error.clear(); return true;
}

bool WishPoolRepository::history(QList<SavingRecord> &records)
{
    if (!m_database.database().isOpen()) return fail(QStringLiteral("数据库未打开。"));
    QSqlQuery query(m_database.database());
    if (!query.exec(QStringLiteral("SELECT s.id,s.amount_cents,s.description,s.created_ms,p.name FROM savings_records s LEFT JOIN wish_pools p ON s.pool_id=p.id ORDER BY s.created_ms DESC,s.id DESC LIMIT 200"))) return fail(query.lastError().text());
    QList<SavingRecord> loaded;
    while (query.next()) loaded.append({query.value(0).toLongLong(),query.value(1).toLongLong(),query.value(2).toString(),query.value(4).toString(),QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong())});
    if (query.lastError().isValid()) return fail(query.lastError().text());
    records=loaded; m_error.clear(); return true;
}
