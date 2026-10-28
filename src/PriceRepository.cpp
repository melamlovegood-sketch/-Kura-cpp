#include "ProductRepository.h"
#include "PriceRepository.h"
#include "CategoryCatalog.h"
#include "Transaction.h"
#include "WorkflowStorage.h"
#include <QSqlQuery>
#include <QVariant>
#include <QUrl>

bool PriceRepository::validLink(const QString &link)
{
    if (link.trimmed().isEmpty()) return true;
    const QUrl url(link.trimmed()); return link.size()<=2000 && url.isValid() && (url.scheme()=="https" || url.scheme()=="http") && !url.host().isEmpty() && url.userInfo().isEmpty();
}
bool PriceRepository::load(QList<PriceTrack> &tracks,bool includeArchived)
{
    m_error.clear(); if (!m_database.database().isOpen()) { m_error=QStringLiteral("数据库未打开。"); return false; }
    QSqlQuery query(m_database.database());
    query.prepare("SELECT t.id,t.name,t.detail_category,t.target_cents,t.source_url,t.active,t.dismissed_target_record_id,t.dismissed_drop_record_id,p.id,p.amount_cents,p.source,p.observed_ms,(SELECT amount_cents FROM price_records WHERE track_id=t.id ORDER BY observed_ms DESC,id DESC LIMIT 1 OFFSET 1),(SELECT COUNT(*) FROM price_records WHERE track_id=t.id),(SELECT MIN(amount_cents) FROM price_records WHERE track_id=t.id) FROM price_tracks t LEFT JOIN price_records p ON p.id=(SELECT id FROM price_records WHERE track_id=t.id ORDER BY observed_ms DESC,id DESC LIMIT 1) WHERE (:all=1 OR t.active=1) ORDER BY p.observed_ms DESC,t.id DESC"); query.bindValue(":all",includeArchived?1:0);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    QList<PriceTrack> loaded;
    while (query.next()) {
        PriceTrack track; track.id=query.value(0).toLongLong(); track.name=query.value(1).toString(); track.category=query.value(2).toString(); track.targetCents=query.value(3).toLongLong(); track.sourceUrl=query.value(4).toString(); track.active=query.value(5).toInt()==1;
        track.latestRecordId=query.value(8).toLongLong(); track.latestCents=query.value(9).toLongLong(); track.source=query.value(10).toString(); track.observedAt=QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong(),Qt::UTC); track.previousCents=query.value(12).toLongLong(); track.recordCount=query.value(13).toLongLong(); track.lowestCents=query.value(14).toLongLong();
        if (track.id<=0 || track.name.trimmed().isEmpty() || !CategoryCatalog::isValid(track.category) || track.latestRecordId<=0 || track.latestCents<1 || track.latestCents>Transaction::MaximumAmountCents || !WorkflowStorage::validTime(track.observedAt) || !validLink(track.sourceUrl)) { m_error=QStringLiteral("价格记录数据无效。"); return false; }
        track.targetHit=track.active && track.targetCents>0 && track.latestCents<=track.targetCents && query.value(6).toLongLong()!=track.latestRecordId;
        track.priceDrop=track.active && track.previousCents>track.latestCents && query.value(7).toLongLong()!=track.latestRecordId;
        loaded.append(track);
    }
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    tracks=loaded; return true;
}
bool PriceRepository::history(qint64 id,QList<PriceRecord> &records)
{
    m_error.clear(); if (!m_database.database().isOpen() || id<=0) { m_error=QStringLiteral("请选择有效商品。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("SELECT id,track_id,amount_cents,source,observed_ms FROM price_records WHERE track_id=:id ORDER BY observed_ms,id"); query.bindValue(":id",id);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    QList<PriceRecord> loaded; while (query.next()) loaded.append({query.value(0).toLongLong(),query.value(1).toLongLong(),query.value(2).toLongLong(),query.value(3).toString(),QDateTime::fromMSecsSinceEpoch(query.value(4).toLongLong(),Qt::UTC)});
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } records=loaded; return true;
}
bool PriceRepository::intake(const QString &name,const QString &category,qint64 cents,const QString &source,const QString &token,qint64 &trackId,const QDateTime &observedAt,qint64 productId)
{
    m_error.clear(); const auto title=name.trimmed();
    if (title.isEmpty() || title.size()>100 || !CategoryCatalog::isValid(category)) { m_error=QStringLiteral("请填写商品名和有效品类。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database());
    // A retry must return the original product, even if it was since archived.
    query.prepare("SELECT t.id,t.name_key,t.detail_category FROM price_records p JOIN price_tracks t ON p.track_id=t.id WHERE p.request_token=:token"); query.bindValue(":token",token);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    qint64 id=0;
    if (query.next()) {
        if (query.value(1).toString()!=WorkflowStorage::nameKey(title) || query.value(2).toString()!=category) { m_error=QStringLiteral("这次价格操作已保存，请重新开始录入。"); return false; }
        id=query.value(0).toLongLong();
    } query.finish();
    if (!id) {
        query.prepare("SELECT id FROM price_tracks WHERE name_key=:key AND detail_category=:category AND active=1"); query.bindValue(":key",WorkflowStorage::nameKey(title)); query.bindValue(":category",category);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
        if (query.next()) id=query.value(0).toLongLong(); query.finish();
    }
    if (!id) {
        query.prepare("INSERT INTO price_tracks(name,name_key,detail_category,created_ms) VALUES(:name,:key,:category,:now)"); query.bindValue(":name",title); query.bindValue(":key",WorkflowStorage::nameKey(title)); query.bindValue(":category",category); query.bindValue(":now",QDateTime::currentMSecsSinceEpoch());
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } id=query.lastInsertId().toLongLong(); query.finish();
    }
    if (!record(id,cents,source,token,observedAt)) return false;
    ProductRepository products(m_database);
    if (productId && !products.attach("price",id,productId)) { m_error=products.lastError(); return false; }
    if (!transaction.commit()) return false; trackId=id; return true;
}
bool PriceRepository::record(qint64 id,qint64 cents,const QString &source,const QString &token,const QDateTime &observedAt)
{
    m_error.clear();
    if (id<=0 || cents<1 || cents>Transaction::MaximumAmountCents || source.trimmed().isEmpty() || source.trimmed().size()>80 || token.isEmpty() || token.size()>64 || !WorkflowStorage::validTime(observedAt)) { m_error=QStringLiteral("请确认金额、来源和记录时间。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database()); query.prepare("SELECT track_id,amount_cents,source FROM price_records WHERE request_token=:token"); query.bindValue(":token",token);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    if (query.next()) {
        if (query.value(0).toLongLong()!=id || query.value(1).toLongLong()!=cents || query.value(2).toString()!=source.trimmed()) { m_error=QStringLiteral("这次价格操作已保存，请重新开始录入。"); return false; }
        query.finish(); return transaction.commit();
    } query.finish();
    query.prepare("INSERT INTO price_records(track_id,amount_cents,source,observed_ms,request_token) SELECT id,:amount,:source,:time,:token FROM price_tracks WHERE id=:id AND active=1"); query.bindValue(":id",id); query.bindValue(":amount",cents); query.bindValue(":source",source.trimmed()); query.bindValue(":time",observedAt.toMSecsSinceEpoch()); query.bindValue(":token",token);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("商品已结束追踪或价格保存失败，请刷新后重试。"); return false; } query.finish(); return transaction.commit();
}
bool PriceRepository::configure(qint64 id,qint64 target,const QString &sourceUrl)
{
    m_error.clear(); if (id<=0 || target<0 || target>Transaction::MaximumAmountCents || !validLink(sourceUrl) || !m_database.database().isOpen()) { m_error=QStringLiteral("请检查目标价和 HTTP／HTTPS 商品链接。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("UPDATE price_tracks SET target_cents=:target,source_url=:url,dismissed_target_record_id=NULL WHERE id=:id AND active=1"); query.bindValue(":id",id); query.bindValue(":target",target?QVariant(target):QVariant()); query.bindValue(":url",sourceUrl.trimmed().isNull()?QStringLiteral(""):sourceUrl.trimmed());
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("价格设置保存失败，商品可能已结束追踪。"); return false; } return true;
}
bool PriceRepository::archive(qint64 id)
{
    m_error.clear(); if (!m_database.database().isOpen() || id<=0) { m_error=QStringLiteral("请选择有效商品。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("UPDATE price_tracks SET active=0 WHERE id=:id AND active=1"); query.bindValue(":id",id);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("结束追踪失败，请刷新后重试。"); return false; } return true;
}
bool PriceRepository::dismiss(qint64 id,const QString &kind,qint64 recordId)
{
    m_error.clear(); if (!m_database.database().isOpen() || id<=0 || recordId<=0 || (kind!="target" && kind!="drop")) { m_error=QStringLiteral("价格提醒无效。"); return false; }
    const auto column=kind=="target"?QStringLiteral("dismissed_target_record_id"):QStringLiteral("dismissed_drop_record_id");
    QSqlQuery query(m_database.database()); query.prepare("UPDATE price_tracks SET "+column+"=:record WHERE id=:id AND EXISTS(SELECT 1 FROM price_records p WHERE p.id=:record AND p.track_id=:id)"); query.bindValue(":id",id); query.bindValue(":record",recordId);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("提醒状态保存失败，请重试。"); return false; } return true;
}
