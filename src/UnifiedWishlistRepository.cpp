#include "UnifiedWishlistRepository.h"
#include "WishlistRepository.h"
#include "PriceRepository.h"
#include "ProductRepository.h"
#include "CategoryCatalog.h"
#include "WorkflowStorage.h"
#include <QSet>
#include <QSqlQuery>
#include <QVariant>

QList<WorkflowTable> UnifiedWishlistSchema::tables()
{
    return {
        {"wishlist_price_links","CREATE TABLE IF NOT EXISTS wishlist_price_links(id INTEGER PRIMARY KEY,wish_id INTEGER NOT NULL CHECK(wish_id>=0),legacy_archived INTEGER NOT NULL DEFAULT 0 CHECK(legacy_archived IN (0,1)))",{"id","wish_id","legacy_archived"},{"id","wish_id","legacy_archived"},{}},
        {"wishlist_order","CREATE TABLE IF NOT EXISTS wishlist_order(id INTEGER PRIMARY KEY,position INTEGER NOT NULL CHECK(position>0))",{"id","position"},{"id","position"},{}}
    };
}

bool UnifiedWishlistRepository::state(QHash<qint64,qint64> &tracks,QHash<qint64,qint64> &positions,QSet<qint64> *legacyArchives)
{
    m_error.clear(); QSqlQuery query(m_database.database()); QHash<qint64,qint64> loadedTracks,loadedOrder; QSet<qint64> archived;
    if (!query.exec("SELECT id,wish_id,legacy_archived FROM wishlist_price_links WHERE wish_id>0")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) { loadedTracks.insert(query.value(1).toLongLong(),query.value(0).toLongLong()); if (query.value(2).toInt()) archived.insert(query.value(1).toLongLong()); }
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    if (!query.exec("SELECT id,position FROM wishlist_order")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) loadedOrder.insert(query.value(0).toLongLong(),query.value(1).toLongLong());
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    tracks=loadedTracks; positions=loadedOrder; if (legacyArchives) *legacyArchives=archived; return true;
}

bool UnifiedWishlistRepository::synchronize()
{
    m_error.clear(); QSqlQuery query(m_database.database());
    if (!query.exec("SELECT id FROM price_tracks WHERE NOT EXISTS(SELECT 1 FROM wishlist_price_links l WHERE l.id=price_tracks.id)")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    QSet<qint64> missing; while (query.next()) missing.insert(query.value(0).toLongLong());
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    if (missing.isEmpty()) return true;
    WorkflowStorage::Transaction write(m_database.database(),m_error); if (!write.ok()) return false;
    // Recheck under the write transaction; another window may have imported the same tracks.
    if (!query.exec("SELECT id FROM wishlist_price_links")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) missing.remove(query.value(0).toLongLong()); query.finish();
    QList<PriceTrack> prices; PriceRepository priceRepository(m_database);
    QList<WishItem> wishes; WishlistRepository wishRepository(m_database);
    QHash<qint64,qint64> links,positions,wishProfiles,priceProfiles;
    if (!priceRepository.load(prices,true)) { m_error=priceRepository.lastError(); return false; }
    if (!wishRepository.findAll(wishes)) { m_error=wishRepository.lastError(); return false; }
    if (!state(links,positions)) return false;
    if (!query.exec("SELECT kind,entity_id,product_id FROM product_links WHERE kind IN ('wish','price')")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) (query.value(0).toString()=="wish"?wishProfiles:priceProfiles).insert(query.value(1).toLongLong(),query.value(2).toLongLong()); query.finish();
    for (const auto &price:prices) {
        if (!missing.contains(price.id)) continue;
        QList<qint64> candidates,profileMatches;
        for (const auto &wish:wishes) {
            if (links.contains(wish.id) || (wish.status==WishStatus::Waiting)!=price.active) continue;
            const auto wp=wishProfiles.value(wish.id),pp=priceProfiles.value(price.id);
            if (wp && pp && wp!=pp) continue;
            const auto category=wish.detailCategory.isEmpty()?CategoryCatalog::fromLegacy(wish.category):wish.detailCategory;
            if (wp && wp==pp) profileMatches.append(wish.id);
            else if (category==price.category && WorkflowStorage::nameKey(wish.name)==WorkflowStorage::nameKey(price.name)) candidates.append(wish.id);
        }
        if (!profileMatches.isEmpty()) candidates=profileMatches;
        qint64 wishId=candidates.size()==1?candidates.first():0;
        const bool importedArchive=!wishId && !price.active;
        if (!wishId) {
            query.prepare("SELECT created_ms FROM price_tracks WHERE id=:id"); query.bindValue(":id",price.id);
            if (!query.exec() || !query.next()) { m_error=QStringLiteral("原价格商品已变化，请刷新后重试。"); return false; }
            const auto created=query.value(0).toLongLong(); query.finish();
            query.prepare("INSERT INTO wishlist(name,estimated_cents,category,cooling_days,reason,created_ms,ready_ms,status,closed_ms,transaction_id,priority,detail_category,source_image) VALUES(:name,:amount,:category,3,'',:created,:created,:status,:closed,NULL,2,:detail,X'')");
            query.bindValue(":name",price.name); query.bindValue(":amount",price.latestCents); query.bindValue(":category",CategoryCatalog::legacyCategory(price.category)); query.bindValue(":detail",price.category);
            query.bindValue(":created",created); query.bindValue(":status",price.active?0:2); query.bindValue(":closed",price.active?QVariant():QVariant(qMax(created,QDateTime::currentMSecsSinceEpoch())));
            if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
            wishId=query.lastInsertId().toLongLong(); query.finish();
        }
        query.prepare("INSERT INTO wishlist_price_links(id,wish_id,legacy_archived) VALUES(:track,:wish,:archived)"); query.bindValue(":track",price.id); query.bindValue(":wish",wishId); query.bindValue(":archived",importedArchive?1:0);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish(); links.insert(wishId,price.id);
        if (!wishProfiles.value(wishId)) {
            ProductRepository products(m_database);
            if (!products.carry("price",price.id,"wish",wishId)) { m_error=products.lastError(); return false; }
        }
    }
    return write.commit();
}

bool UnifiedWishlistRepository::saveOrder(const QList<qint64> &ids)
{
    m_error.clear(); WorkflowStorage::Transaction write(m_database.database(),m_error); if (!write.ok()) return false;
    QSqlQuery query(m_database.database()); QSet<qint64> expected;
    if (!query.exec("SELECT id FROM wishlist WHERE status=0")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) expected.insert(query.value(0).toLongLong()); query.finish();
    if (expected!=QSet<qint64>(ids.begin(),ids.end()) || expected.size()!=ids.size()) { m_error=QStringLiteral("清单已变化，请刷新后重新调整顺序。"); return false; }
    if (!query.exec("DELETE FROM wishlist_order")) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    for (int i=0;i<ids.size();++i) {
        query.prepare("INSERT INTO wishlist_order(id,position) VALUES(:id,:position)"); query.bindValue(":id",ids[i]); query.bindValue(":position",i+1);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    }
    return write.commit();
}

bool UnifiedWishlistRepository::recordPrice(qint64 wishId,qint64 cents,const QString &source,const QString &token,const QDateTime &observedAt,qint64 &trackId)
{
    m_error.clear(); WorkflowStorage::Transaction write(m_database.database(),m_error); if (!write.ok()) return false;
    QSqlQuery query(m_database.database());
    query.prepare("SELECT name,detail_category FROM wishlist WHERE id=:id AND status=0"); query.bindValue(":id",wishId);
    if (!query.exec() || !query.next()) { m_error=QStringLiteral("商品已完成或不存在，请刷新后重试。"); return false; }
    const auto name=query.value(0).toString(),category=query.value(1).toString(); query.finish();
    query.prepare("SELECT id FROM wishlist_price_links WHERE wish_id=:wish"); query.bindValue(":wish",wishId);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    qint64 id=query.next()?query.value(0).toLongLong():0; query.finish();
    if (!id) {
        query.prepare("INSERT INTO price_tracks(name,name_key,detail_category,created_ms) VALUES(:name,:key,:category,:created)");
        query.bindValue(":name",name); query.bindValue(":key",WorkflowStorage::nameKey(name)); query.bindValue(":category",category); query.bindValue(":created",QDateTime::currentMSecsSinceEpoch());
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } id=query.lastInsertId().toLongLong(); query.finish();
        query.prepare("INSERT INTO wishlist_price_links(id,wish_id) VALUES(:id,:wish)"); query.bindValue(":id",id); query.bindValue(":wish",wishId);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    }
    PriceRepository prices(m_database);
    if (!prices.record(id,cents,source,token,observedAt)) { m_error=prices.lastError(); return false; }
    ProductRepository products(m_database);
    if (!products.carry("wish",wishId,"price",id)) { m_error=products.lastError(); return false; }
    if (!write.commit()) return false; trackId=id; return true;
}

bool UnifiedWishlistRepository::closeWish(qint64 wishId,bool removing)
{
    m_error.clear(); WorkflowStorage::Transaction write(m_database.database(),m_error); if (!write.ok()) return false;
    QSqlQuery query(m_database.database());
    query.prepare("UPDATE price_tracks SET active=0 WHERE id IN (SELECT id FROM wishlist_price_links WHERE wish_id=:wish)"); query.bindValue(":wish",wishId);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    if (!removing) return write.commit();
    // A zero wish_id is a tombstone: deleting a product must not import its prices again.
    query.prepare("UPDATE wishlist_price_links SET wish_id=0 WHERE wish_id=:wish"); query.bindValue(":wish",wishId);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    query.prepare("DELETE FROM wishlist_order WHERE id=:wish"); query.bindValue(":wish",wishId);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    query.finish(); return write.commit();
}
