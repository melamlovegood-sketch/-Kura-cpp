#include "ReviewRepository.h"
#include "WorkflowStorage.h"
#include "CategoryCatalog.h"
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>

bool ReviewRepository::schedule(qint64 transactionId,qint64 brandId,const QDateTime &purchaseTime)
{
    m_error.clear();
    if (transactionId<=0 || brandId<0) { m_error=QStringLiteral("购买记录或品牌编号无效。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database());
    query.prepare("SELECT description,amount_cents,category,detail_category,date,occurred_time FROM transactions WHERE id=:id"); query.bindValue(":id",transactionId);
    if (!query.exec() || !query.next()) { m_error=QStringLiteral("消费记录已不存在，无法安排复盘。"); return false; }
    const QString name=query.value(0).toString(); const qint64 amount=query.value(1).toLongLong();
    const QString category=query.value(3).toString().isEmpty()?CategoryCatalog::fromLegacy(query.value(2).toString()):query.value(3).toString();
    QTime time=QTime::fromString(query.value(5).toString(),"HH:mm:ss"); if (!time.isValid()) time=QTime(12,0);
    const auto date=QDate::fromString(query.value(4).toString(),Qt::ISODate);
    const QDateTime purchased=purchaseTime.isValid()?purchaseTime.toUTC():QDateTime(date,time,QTimeZone::systemTimeZone()).toUTC(); query.finish();
    if (!WorkflowStorage::validTime(purchased) || !WorkflowStorage::validTime(purchased.addDays(30))) { m_error=QStringLiteral("购买日期超出可安排复盘的范围。"); return false; }
    QString brandName=QStringLiteral("");
    if (brandId) {
        query.prepare("SELECT name FROM brands WHERE id=:id AND active=1 AND detail_category=:category"); query.bindValue(":id",brandId); query.bindValue(":category",category);
        if (!query.exec() || !query.next()) { m_error=QStringLiteral("品牌已停用、已不存在或品类不匹配，请重新选择。"); return false; }
        brandName=query.value(0).toString(); query.finish();
    }
    for (int day:{7,30}) {
        query.prepare("INSERT INTO review_tasks(transaction_id,name,amount_cents,detail_category,brand_id,brand_name,review_day,purchased_ms,due_ms) VALUES(:expense,:name,:amount,:category,:brand,:brandName,:day,:purchased,:due) ON CONFLICT(transaction_id,review_day) DO NOTHING");
        query.bindValue(":expense",transactionId); query.bindValue(":name",name); query.bindValue(":amount",amount); query.bindValue(":category",category);
        query.bindValue(":brand",brandId?QVariant(brandId):QVariant()); query.bindValue(":brandName",brandName); query.bindValue(":day",day); query.bindValue(":purchased",purchased.toMSecsSinceEpoch()); query.bindValue(":due",purchased.addDays(day).toMSecsSinceEpoch());
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    }
    return transaction.commit();
}
bool ReviewRepository::list(QList<ReviewTask> &tasks,bool completed,const QDateTime &now,bool includeFuture)
{
    m_error.clear(); if (!m_database.database().isOpen() || !WorkflowStorage::validTime(now)) { m_error=QStringLiteral("数据库或当前时间不可用。"); return false; }
    QSqlQuery query(m_database.database());
    query.prepare("SELECT id,transaction_id,name,amount_cents,detail_category,brand_id,brand_name,review_day,purchased_ms,due_ms,frequency,worthiness,note,completed_ms FROM review_tasks WHERE "+QString(completed?"completed_ms IS NOT NULL ORDER BY completed_ms DESC,id DESC":includeFuture?"completed_ms IS NULL ORDER BY due_ms,id":"completed_ms IS NULL AND due_ms<=:now ORDER BY due_ms,id")+" LIMIT 200");
    if (!completed && !includeFuture) query.bindValue(":now",now.toMSecsSinceEpoch());
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    QList<ReviewTask> loaded;
    while (query.next()) {
        ReviewTask task; task.id=query.value(0).toLongLong(); task.transactionId=query.value(1).toLongLong(); task.name=query.value(2).toString(); task.amountCents=query.value(3).toLongLong(); task.category=query.value(4).toString(); task.brandId=query.value(5).toLongLong(); task.brandName=query.value(6).toString(); task.day=query.value(7).toInt();
        task.purchasedAt=QDateTime::fromMSecsSinceEpoch(query.value(8).toLongLong(),Qt::UTC); task.dueAt=QDateTime::fromMSecsSinceEpoch(query.value(9).toLongLong(),Qt::UTC); task.frequency=query.value(10).isNull()?-1:query.value(10).toInt(); task.worthiness=query.value(11).toInt(); task.note=query.value(12).toString(); if (!query.value(13).isNull()) task.completedAt=QDateTime::fromMSecsSinceEpoch(query.value(13).toLongLong(),Qt::UTC);
        if (task.id<=0 || !WorkflowStorage::validTime(task.dueAt) || task.name.isEmpty() || (task.day!=7 && task.day!=30)) { m_error=QStringLiteral("复盘任务数据无效。"); return false; }
        loaded.append(task);
    }
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    tasks=loaded; return true;
}
bool ReviewRepository::dueCount(qint64 &count,const QDateTime &now)
{
    m_error.clear(); if (!m_database.database().isOpen() || !WorkflowStorage::validTime(now)) { m_error=QStringLiteral("无法读取待复盘数量。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("SELECT COUNT(*) FROM review_tasks WHERE completed_ms IS NULL AND due_ms<=:now"); query.bindValue(":now",now.toMSecsSinceEpoch());
    if (!query.exec() || !query.next()) { m_error=WorkflowStorage::error(query.lastError()); return false; } count=query.value(0).toLongLong(); return true;
}
bool ReviewRepository::complete(qint64 id,int frequency,int worthiness,const QString &note,const QDateTime &now)
{
    m_error.clear();
    if (id<=0 || frequency<0 || frequency>2 || worthiness < -1 || worthiness>1 || note.size()>500 || !WorkflowStorage::validTime(now)) { m_error=QStringLiteral("请选择使用频率和值得度，备注最多 500 字。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database()); query.prepare("SELECT id FROM review_tasks WHERE id=:id AND completed_ms IS NULL AND due_ms<=:now"); query.bindValue(":id",id); query.bindValue(":now",now.toMSecsSinceEpoch());
    if (!query.exec() || !query.next()) { m_error=QStringLiteral("复盘尚未到期、已完成或已经取消，请刷新。"); return false; }
    query.finish();
    query.prepare("UPDATE review_tasks SET frequency=:frequency,worthiness=:worth,note=:note,completed_ms=:now WHERE id=:id AND completed_ms IS NULL AND due_ms<=:now");
    query.bindValue(":id",id); query.bindValue(":frequency",frequency); query.bindValue(":worth",worthiness); query.bindValue(":note",note.trimmed().isNull()?QStringLiteral(""):note.trimmed()); query.bindValue(":now",now.toMSecsSinceEpoch());
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("复盘保存失败或已被处理，请刷新后重试。"); return false; } query.finish();
    return transaction.commit();
}
