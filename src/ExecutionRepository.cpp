#include "ExecutionRepository.h"
#include "ProductRepository.h"
#include "CategoryCatalog.h"
#include "WorkflowStorage.h"
#include "Transaction.h"
#include "WishlistRepository.h"
#include <QSqlQuery>
#include <QVariant>

bool ExecutionRepository::loadActive(ExecutionSession &session)
{
    if (!m_database.database().isOpen()) { m_error=QStringLiteral("数据库未打开。"); return false; }
    QSqlQuery query(m_database.database());
    if (!query.exec("SELECT id,name,detail_category,estimated_cents,mode,duration_seconds,started_ms,deadline_ms,notes,advice FROM execution_sessions WHERE state='active'")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    ExecutionSession loaded;
    if (query.next()) {
        loaded.id=query.value(0).toLongLong(); loaded.name=query.value(1).toString(); loaded.category=query.value(2).toString(); loaded.estimatedCents=query.value(3).toLongLong(); loaded.mode=query.value(4).toString(); loaded.durationSeconds=query.value(5).toInt();
        loaded.startedAt=QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong(),Qt::UTC); loaded.deadline=QDateTime::fromMSecsSinceEpoch(query.value(7).toLongLong(),Qt::UTC); loaded.notes=query.value(8).toString(); loaded.advice=query.value(9).toString();
        if (loaded.id<=0 || loaded.name.trimmed().isEmpty() || !CategoryCatalog::isValid(loaded.category) || !WorkflowStorage::validTime(loaded.startedAt) || !WorkflowStorage::validTime(loaded.deadline)
            || loaded.deadline!=loaded.startedAt.addSecs(loaded.durationSeconds) || loaded.durationSeconds<60 || loaded.durationSeconds>7200 || (loaded.mode!="fast" && loaded.mode!="research")) { m_error=QStringLiteral("执行会话数据无效。"); return false; }
        if (query.next()) { m_error=QStringLiteral("存在多个进行中的决策，请检查数据。"); return false; }
    }
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    session=loaded; m_error.clear(); return true;
}
bool ExecutionRepository::start(ExecutionSession &session,const QDateTime &now,qint64 productId)
{
    m_error.clear();
    if (session.id!=0 || session.name.trimmed().isEmpty() || session.name.trimmed().size()>100 || !CategoryCatalog::isValid(session.category)
        || session.estimatedCents<0 || session.estimatedCents>Transaction::MaximumAmountCents || session.durationSeconds<60 || session.durationSeconds>7200
        || (session.mode!="fast" && session.mode!="research") || !WorkflowStorage::validTime(now) || !WorkflowStorage::validTime(now.addSecs(session.durationSeconds))) { m_error=QStringLiteral("请填写商品、品类和 1 至 120 分钟的决策时长。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database());
    query.prepare("INSERT INTO execution_sessions(name,detail_category,estimated_cents,mode,duration_seconds,started_ms,deadline_ms) VALUES(:name,:category,:amount,:mode,:seconds,:start,:end)");
    query.bindValue(":name",session.name.trimmed()); query.bindValue(":category",session.category); query.bindValue(":amount",session.estimatedCents); query.bindValue(":mode",session.mode); query.bindValue(":seconds",session.durationSeconds); query.bindValue(":start",now.toMSecsSinceEpoch()); query.bindValue(":end",now.addSecs(session.durationSeconds).toMSecsSinceEpoch());
    if (!query.exec()) { m_error=QStringLiteral("无法开始决策，可能已有进行中的会话或数据库被占用。"); return false; }
    const auto id=query.lastInsertId().toLongLong(); query.finish();
    ProductRepository products(m_database);
    if (productId && !products.attach("execution",id,productId)) { m_error=products.lastError(); return false; }
    if (!transaction.commit()) return false;
    session.id=id; session.name=session.name.trimmed(); session.startedAt=now.toUTC(); session.deadline=now.addSecs(session.durationSeconds).toUTC(); return true;
}
bool ExecutionRepository::saveNotes(qint64 id,const QString &notes,const QString &advice)
{
    m_error.clear(); if (id<=0 || notes.size()>4000 || advice.size()>ExecutionSession::MaximumAdviceLength || !m_database.database().isOpen()) { m_error=QStringLiteral("研究记录或对话内容过长，无法保存。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("UPDATE execution_sessions SET notes=:notes,advice=:advice WHERE id=:id AND state='active'"); query.bindValue(":id",id); query.bindValue(":notes",notes.isNull()?QStringLiteral(""):notes); query.bindValue(":advice",advice.isNull()?QStringLiteral(""):advice);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("记录保存失败，决策可能已结束，请刷新后重试。"); return false; } return true;
}
bool ExecutionRepository::finish(qint64 id,const QString &decision,const QDateTime &now)
{
    m_error.clear(); if (id<=0 || (decision!="skipped" && decision!="undecided") || !WorkflowStorage::validTime(now) || !m_database.database().isOpen()) { m_error=QStringLiteral("决策结果无效。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("UPDATE execution_sessions SET state=:state,closed_ms=MAX(:now,started_ms) WHERE id=:id AND state='active'"); query.bindValue(":state",decision); query.bindValue(":now",now.toMSecsSinceEpoch()); query.bindValue(":id",id);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("决策已结束或保存失败，请刷新后重试。"); return false; } return true;
}

bool ExecutionRepository::updateProduct(qint64 id,const QString &name,const QString &category,qint64 estimatedCents)
{
    m_error.clear();
    if (id<=0 || name.trimmed().isEmpty() || name.trimmed().size()>100 || !CategoryCatalog::isValid(category)
        || estimatedCents<0 || estimatedCents>Transaction::MaximumAmountCents) {
        m_error=QStringLiteral("请填写有效的商品名称、品类和预计金额。"); return false;
    }
    QSqlQuery query(m_database.database());
    query.prepare("UPDATE execution_sessions SET name=:name,detail_category=:category,estimated_cents=:amount WHERE id=:id AND state='active'");
    query.bindValue(":name",name.trimmed()); query.bindValue(":category",category); query.bindValue(":amount",estimatedCents); query.bindValue(":id",id);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("商品修改未保存，决策可能已结束，请刷新后重试。"); return false; }
    return true;
}

bool ExecutionRepository::deferToWishlist(qint64 id,WishItem &item,qint64 productId)
{
    m_error.clear();
    if (id<0) { m_error=QStringLiteral("决策编号无效。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error);
    if (!transaction.ok()) return false;
    WishlistRepository wishes(m_database);
    auto saved=item;
    if (!wishes.add(saved,QDateTime::currentDateTimeUtc(),productId)) { m_error=wishes.lastError(); return false; }
    if (id>0 && !finish(id,QStringLiteral("undecided"))) return false;
    ProductRepository products(m_database);
    if (id>0 && !products.carry("execution",id,"wish",saved.id)) { m_error=products.lastError(); return false; }
    if (!transaction.commit()) return false;
    item=saved;
    return true;
}

bool ExecutionRepository::setMode(qint64 id,const QString &mode)
{
    m_error.clear();
    if (id<=0 || (mode!="fast" && mode!="research")) { m_error=QStringLiteral("请选择有效的决策模式。"); return false; }
    QSqlQuery query(m_database.database());
    query.prepare("UPDATE execution_sessions SET mode=:mode WHERE id=:id AND state='active'");
    query.bindValue(":mode",mode); query.bindValue(":id",id);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("模式切换未保存，决策可能已结束，请刷新后重试。"); return false; }
    return true;
}
