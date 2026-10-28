#include "ProductRepository.h"
#include "UnifiedWishlistRepository.h"
#include "CategoryCatalog.h"
#include "Transaction.h"
#include "WorkflowStorage.h"
#include <QBuffer>
#include <QImageReader>
#include <QJsonDocument>
#include <QMap>
#include <QSqlQuery>
#include <QVariant>

QList<WorkflowTable> ProductSchema::tables()
{
    return QList<WorkflowTable>{
        {"product_profiles","CREATE TABLE IF NOT EXISTS product_profiles(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,detail_category TEXT NOT NULL,fields_json TEXT NOT NULL DEFAULT '{}',user_notes TEXT NOT NULL DEFAULT '',conversation TEXT NOT NULL DEFAULT '',updated_ms INTEGER NOT NULL)",{"id","name","detail_category","fields_json","user_notes","conversation","updated_ms"},{"id","updated_ms"},{}},
        {"product_links","CREATE TABLE IF NOT EXISTS product_links(id INTEGER PRIMARY KEY AUTOINCREMENT,product_id INTEGER NOT NULL,kind TEXT NOT NULL,entity_id INTEGER NOT NULL,UNIQUE(kind,entity_id))",{"id","product_id","kind","entity_id"},{"id","product_id","entity_id"},{}},
        {"product_evidence","CREATE TABLE IF NOT EXISTS product_evidence(id INTEGER PRIMARY KEY AUTOINCREMENT,product_id INTEGER NOT NULL,source_image BLOB NOT NULL,extracted_json TEXT NOT NULL,created_ms INTEGER NOT NULL)",{"id","product_id","source_image","extracted_json","created_ms"},{"id","product_id","created_ms"},{}}
    } + UnifiedWishlistSchema::tables();
}
bool ProductSchema::upgrade(const QSqlDatabase &database,QString &error)
{
    QSqlQuery query(database);
    for (const auto &table:tables()) if (!query.exec(table.createSql)) { error=WorkflowStorage::error(query.lastError()); return false; }
    if (!query.exec("PRAGMA table_info(wishlist_price_links)")) { error=WorkflowStorage::error(query.lastError()); return false; }
    bool hasArchive=false; while (query.next()) hasArchive|=query.value(1).toString()=="legacy_archived"; query.finish();
    if (!hasArchive && !query.exec("ALTER TABLE wishlist_price_links ADD COLUMN legacy_archived INTEGER NOT NULL DEFAULT 0 CHECK(legacy_archived IN (0,1))")) { error=WorkflowStorage::error(query.lastError()); return false; }
    if (!query.exec("CREATE UNIQUE INDEX IF NOT EXISTS one_price_track_per_wish ON wishlist_price_links(wish_id) WHERE wish_id>0")) { error=WorkflowStorage::error(query.lastError()); return false; }
    return true;
}
namespace {
bool validImage(const QByteArray &image)
{
    if (image.isEmpty() || image.size()>2097152) return false;
    QBuffer buffer; buffer.setData(image); buffer.open(QIODevice::ReadOnly); QImageReader reader(&buffer);
    const auto size=reader.size(); return size.isValid() && qint64(size.width())*size.height()<=25000000 && reader.canRead();
}
bool validKind(const QString &kind) { return kind=="wish" || kind=="price" || kind=="transaction" || kind=="execution"; }
QString json(const QJsonObject &value) { return QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Compact)); }
}
bool ProductSchema::validate(const QSqlDatabase &database,QString &error)
{
    QSqlQuery query(database);
    const auto reject=[&](const QString &sql) {
        if (!query.exec(sql) || query.next() || query.lastError().isValid()) { error=QStringLiteral("备份中的商品详情或截图数据无效。"); return false; }
        query.finish(); return true;
    };
    for (const auto &table:tables()) {
        QStringList checks;
        for (const auto &column:table.columns) checks << "typeof("+column+")!='"+(column=="source_image"?QStringLiteral("blob"):table.integers.contains(column)?QStringLiteral("integer"):QStringLiteral("text"))+"'";
        if (!reject("SELECT 1 FROM "+table.name+" WHERE "+checks.join(" OR ")+" LIMIT 1")) return false;
    }
    if (!reject("SELECT 1 FROM product_links l WHERE id<=0 OR entity_id<=0 OR kind NOT IN ('wish','price','transaction','execution') OR NOT EXISTS(SELECT 1 FROM product_profiles p WHERE p.id=l.product_id) LIMIT 1")
        || !reject("SELECT 1 FROM product_links GROUP BY kind,entity_id HAVING COUNT(*)>1")
        || !reject("SELECT 1 FROM product_evidence e WHERE id<=0 OR length(source_image) NOT BETWEEN 1 AND 2097152 OR created_ms NOT BETWEEN -62135596800000 AND 253402300799999 OR NOT EXISTS(SELECT 1 FROM product_profiles p WHERE p.id=e.product_id) LIMIT 1")
        || !reject("SELECT 1 FROM product_evidence GROUP BY product_id HAVING COUNT(*)>12")) return false;
    if (!query.exec("SELECT id,name,detail_category,fields_json,user_notes,conversation,updated_ms FROM product_profiles")) { error=query.lastError().text(); return false; }
    while (query.next()) {
        const auto fields=QJsonDocument::fromJson(query.value(3).toString().toUtf8());
        if (query.value(0).toLongLong()<=0 || query.value(1).toString().trimmed().isEmpty() || query.value(1).toString().size()>100
            || !CategoryCatalog::isValid(query.value(2).toString()) || !fields.isObject() || !ProductRepository::validFields(fields.object())
            || query.value(4).toString().size()>2000 || !ProductRepository::validConversation(query.value(5).toString())
            || !WorkflowStorage::validTime(QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong()))) { error=QStringLiteral("备份中的商品资料或对话格式无效。"); return false; }
    }
    if (query.lastError().isValid()) { error=query.lastError().text(); return false; } query.finish();
    if (!query.exec("SELECT source_image,extracted_json FROM product_evidence")) { error=query.lastError().text(); return false; }
    while (query.next()) if (!validImage(query.value(0).toByteArray()) || query.value(1).toString().size()>16000 || !QJsonDocument::fromJson(query.value(1).toString().toUtf8()).isObject()) { error=QStringLiteral("备份中的商品原始截图无效。"); return false; }
    if (query.lastError().isValid()) { error=query.lastError().text(); return false; }
    if (!reject("SELECT 1 FROM wishlist_price_links l WHERE id<=0 OR wish_id<0 OR legacy_archived NOT IN (0,1) OR NOT EXISTS(SELECT 1 FROM price_tracks p WHERE p.id=l.id) OR (wish_id>0 AND NOT EXISTS(SELECT 1 FROM wishlist w WHERE w.id=l.wish_id)) LIMIT 1")
        || !reject("SELECT 1 FROM wishlist_price_links WHERE wish_id>0 GROUP BY wish_id HAVING COUNT(*)>1")
        || !reject("SELECT 1 FROM wishlist_order o WHERE id<=0 OR position<=0 OR NOT EXISTS(SELECT 1 FROM wishlist w WHERE w.id=o.id) LIMIT 1")) return false;
    error.clear(); return true;
}
QList<QPair<QString,QString>> ProductRepository::fieldLabels()
{
    return {{"brand",QStringLiteral("品牌")},{"model",QStringLiteral("型号／货号")},{"materials",QStringLiteral("材质／成分")},
        {"specification",QStringLiteral("规格／可选尺码")},{"season",QStringLiteral("适用／上市季节")},
        {"features",QStringLiteral("特点／参数")},{"care",QStringLiteral("保养／使用说明")},{"other",QStringLiteral("其他／待补充")}};
}
bool ProductRepository::validFields(const QJsonObject &fields)
{
    QSet<QString> allowed; for (const auto &field:fieldLabels()) allowed.insert(field.first);
    allowed.insert("purchase_reason"); allowed.insert("alternatives");
    for (auto it=fields.begin();it!=fields.end();++it) if (!allowed.contains(it.key()) || !it.value().isString() || it.value().toString().size()>(it.key()=="purchase_reason"?500:1000)) return false;
    return true;
}
QString ProductRepository::purchaseReason(const ProductProfile &profile,const ProductContext &context)
{
    return profile.fields.contains("purchase_reason")?profile.fields.value("purchase_reason").toString():context.purchaseReason;
}
bool ProductRepository::validConversation(const QString &conversation)
{
    if (conversation.isEmpty()) return true;
    if (conversation.size()>64000) return false;
    const auto document=QJsonDocument::fromJson(conversation.toUtf8()); if (!document.isArray() || document.array().size()>40) return false;
    const auto messages=document.array(); if (messages.size()%2) return false;
    for (int i=0;i<messages.size();++i) {
        const auto object=messages[i].toObject(); const auto text=object.value("content");
        if (object.value("role").toString()!=(i%2?QStringLiteral("assistant"):QStringLiteral("user")) || !text.isString() || text.toString().trimmed().isEmpty() || text.toString().size()>(i%2?8000:2000)) return false;
    }
    return true;
}
bool ProductRepository::context(const QString &kind,qint64 entityId,ProductContext &result)
{
    m_error.clear(); if (!validKind(kind) || entityId<=0) { m_error=QStringLiteral("请先选择商品。"); return false; }
    const QMap<QString,QString> sql={
        {"wish","SELECT name,detail_category,estimated_cents,source_image,reason FROM wishlist WHERE id=:id"},
        {"transaction","SELECT description,detail_category,amount_cents,source_image,COALESCE((SELECT reason FROM wishlist WHERE transaction_id=t.id ORDER BY id LIMIT 1),'') FROM transactions t WHERE id=:id"},
        {"price","SELECT name,detail_category,COALESCE((SELECT amount_cents FROM price_records WHERE track_id=t.id ORDER BY observed_ms DESC,id DESC LIMIT 1),0),X'' FROM price_tracks t WHERE id=:id"},
        {"execution","SELECT name,detail_category,estimated_cents,X'' FROM execution_sessions WHERE id=:id"}};
    QSqlQuery query(m_database.database()); query.prepare(sql.value(kind)); query.bindValue(":id",entityId);
    if (!query.exec() || !query.next()) { m_error=QStringLiteral("商品已不存在，请刷新后重试。"); return false; }
    result={kind,query.value(0).toString(),query.value(1).toString(),entityId,query.value(2).toLongLong(),query.value(3).toByteArray(),
        kind=="wish" || kind=="transaction"?query.value(4).toString():QString()}; return true;
}
bool ProductRepository::load(qint64 id,ProductProfile &profile)
{
    m_error.clear(); QSqlQuery query(m_database.database()); query.prepare("SELECT id,name,detail_category,fields_json,user_notes,conversation,updated_ms FROM product_profiles WHERE id=:id"); query.bindValue(":id",id);
    if (!query.exec() || !query.next()) { m_error=QStringLiteral("商品详情不可用，请重新打开。"); return false; }
    const auto fields=QJsonDocument::fromJson(query.value(3).toString().toUtf8());
    if (!fields.isObject() || !validFields(fields.object()) || !validConversation(query.value(5).toString())) { m_error=QStringLiteral("商品详情格式无效。"); return false; }
    profile.id=query.value(0).toLongLong(); profile.name=query.value(1).toString(); profile.category=query.value(2).toString(); profile.fields=fields.object(); profile.notes=query.value(4).toString(); profile.conversation=query.value(5).toString(); profile.revision=query.value(6).toLongLong(); return true;
}
bool ProductRepository::ensure(const ProductContext &context,ProductProfile &profile)
{
    m_error.clear();
    if (context.name.trimmed().isEmpty() || context.name.size()>100 || context.purchaseReason.size()>500 || !CategoryCatalog::isValid(context.category) || context.entityId<0 || (context.entityId && !validKind(context.kind))) { m_error=QStringLiteral("请先填写有效的商品名与品类，想买理由最多500字。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database());
    if (context.entityId) {
        query.prepare("SELECT product_id FROM product_links WHERE kind=:kind AND entity_id=:id"); query.bindValue(":kind",context.kind); query.bindValue(":id",context.entityId);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
        if (query.next()) { const auto id=query.value(0).toLongLong(); query.finish(); return load(id,profile) && transaction.commit(); } query.finish();
        // Resolve known lifecycle relationships, including records created before this feature.
        if (context.kind=="transaction") {
            query.prepare("SELECT l.product_id FROM product_links l WHERE (l.kind='wish' AND l.entity_id IN (SELECT id FROM wishlist WHERE transaction_id=:id)) OR (l.kind='execution' AND l.entity_id IN (SELECT id FROM execution_sessions WHERE transaction_id=:id)) LIMIT 1"); query.bindValue(":id",context.entityId);
        } else if (context.kind=="wish") {
            query.prepare("SELECT l.product_id FROM product_links l JOIN wishlist w ON l.kind='transaction' AND l.entity_id=w.transaction_id WHERE w.id=:id"); query.bindValue(":id",context.entityId);
        } else if (context.kind=="execution") {
            query.prepare("SELECT l.product_id FROM product_links l JOIN execution_sessions e ON l.kind='transaction' AND l.entity_id=e.transaction_id WHERE e.id=:id"); query.bindValue(":id",context.entityId);
        }
        if (context.kind!="price") {
            if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
            if (query.next()) { const auto id=query.value(0).toLongLong(); query.finish(); return attach(context.kind,context.entityId,id) && load(id,profile) && transaction.commit(); } query.finish();
        }
    }
    query.prepare("INSERT INTO product_profiles(name,detail_category,fields_json,updated_ms) VALUES(:name,:category,:fields,:now)");
    QJsonObject initialFields; if (!context.purchaseReason.isEmpty()) initialFields.insert("purchase_reason",context.purchaseReason);
    query.bindValue(":fields",json(initialFields));
    query.bindValue(":name",context.name.trimmed()); query.bindValue(":category",context.category); query.bindValue(":now",QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    const auto id=query.lastInsertId().toLongLong(); query.finish();
    if (context.entityId && !attach(context.kind,context.entityId,id)) return false;
    return load(id,profile) && transaction.commit();
}
bool ProductRepository::attach(const QString &kind,qint64 entityId,qint64 productId)
{
    m_error.clear(); if (!validKind(kind) || entityId<=0 || productId<=0) { m_error=QStringLiteral("商品关联无效。"); return false; }
    ProductContext checked; if (!context(kind,entityId,checked)) return false;
    QSqlQuery query(m_database.database()); query.prepare("INSERT INTO product_links(product_id,kind,entity_id) SELECT id,:kind,:entity FROM product_profiles WHERE id=:product ON CONFLICT(kind,entity_id) DO UPDATE SET product_id=excluded.product_id");
    query.bindValue(":kind",kind); query.bindValue(":entity",entityId); query.bindValue(":product",productId);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("无法关联商品资料。"); return false; } return true;
}
bool ProductRepository::carry(const QString &fromKind,qint64 fromId,const QString &toKind,qint64 toId)
{
    m_error.clear(); QSqlQuery query(m_database.database()); query.prepare("SELECT product_id FROM product_links WHERE kind=:kind AND entity_id=:id"); query.bindValue(":kind",fromKind); query.bindValue(":id",fromId);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    if (!query.next()) return true;
    const auto id=query.value(0).toLongLong(); query.finish(); return attach(toKind,toId,id);
}
bool ProductRepository::list(QList<ProductProfile> &profiles)
{
    m_error.clear(); QSqlQuery query(m_database.database());
    if (!query.exec("SELECT id FROM product_profiles p WHERE fields_json!='{}' OR user_notes!='' OR conversation!='' OR EXISTS(SELECT 1 FROM product_evidence e WHERE e.product_id=p.id) ORDER BY updated_ms DESC,id DESC LIMIT 200")) { m_error=query.lastError().text(); return false; }
    QList<ProductProfile> loaded; while (query.next()) { ProductProfile profile; if (!load(query.value(0).toLongLong(),profile)) return false; loaded << profile; }
    if (query.lastError().isValid()) { m_error=query.lastError().text(); return false; } profiles=loaded; return true;
}
bool ProductRepository::save(ProductProfile &profile,const QByteArray &image,const QJsonObject &extracted)
{
    m_error.clear(); if (profile.id<=0 || !validFields(profile.fields) || profile.notes.size()>2000 || (!image.isEmpty() && (!validImage(image) || json(extracted).size()>16000))) { m_error=QStringLiteral("资料或截图过长、无效，请检查后保存。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database());
    const auto revision=qMax(QDateTime::currentMSecsSinceEpoch(),profile.revision+1);
    query.prepare("UPDATE product_profiles SET fields_json=:fields,user_notes=:notes,updated_ms=:now WHERE id=:id AND updated_ms=:previous");
    query.bindValue(":fields",json(profile.fields)); query.bindValue(":notes",profile.notes.isNull()?QStringLiteral(""):profile.notes); query.bindValue(":now",revision); query.bindValue(":id",profile.id); query.bindValue(":previous",profile.revision);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("商品资料已在其他窗口更新，请重新打开后核对；当前输入未保存。"); return false; } query.finish();
    if (profile.fields.contains("purchase_reason")) {
        query.prepare("UPDATE wishlist SET reason=:reason WHERE status=0 AND id IN (SELECT entity_id FROM product_links WHERE kind='wish' AND product_id=:id)");
        query.bindValue(":reason",profile.fields.value("purchase_reason").toString()); query.bindValue(":id",profile.id);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    }
    if (!image.isEmpty()) {
        query.prepare("SELECT COUNT(*) FROM product_evidence WHERE product_id=:id"); query.bindValue(":id",profile.id);
        if (!query.exec() || !query.next() || query.value(0).toInt()>=12) { m_error=QStringLiteral("每件商品最多保存 12 张详情截图。"); return false; } query.finish();
        query.prepare("INSERT INTO product_evidence(product_id,source_image,extracted_json,created_ms) VALUES(:id,:image,:json,:now)"); query.bindValue(":id",profile.id); query.bindValue(":image",image); query.bindValue(":json",json(extracted)); query.bindValue(":now",QDateTime::currentMSecsSinceEpoch());
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    }
    if (!transaction.commit()) return false; profile.revision=revision; return true;
}
bool ProductRepository::evidence(qint64 productId,QList<ProductEvidence> &images)
{
    m_error.clear(); QSqlQuery query(m_database.database()); query.prepare("SELECT id,created_ms,source_image,extracted_json FROM product_evidence WHERE product_id=:id ORDER BY id"); query.bindValue(":id",productId);
    if (!query.exec()) { m_error=query.lastError().text(); return false; }
    QList<ProductEvidence> loaded; while (query.next()) loaded << ProductEvidence{query.value(0).toLongLong(),query.value(1).toLongLong(),query.value(2).toByteArray(),QJsonDocument::fromJson(query.value(3).toString().toUtf8()).object()};
    if (query.lastError().isValid()) { m_error=query.lastError().text(); return false; } images=loaded; return true;
}
bool ProductRepository::saveConversation(qint64 id,const QString &conversation,const QString &previous)
{
    m_error.clear(); if (!validConversation(conversation)) { m_error=QStringLiteral("对话过长或格式无效，请保留最近的讨论。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("UPDATE product_profiles SET conversation=:chat WHERE id=:id AND conversation=:previous"); query.bindValue(":id",id); query.bindValue(":chat",conversation.isNull()?QStringLiteral(""):conversation); query.bindValue(":previous",previous.isNull()?QStringLiteral(""):previous);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("对话已在其他窗口更新，本次回答未保存，请重新打开详情。"); return false; } return true;
}
QString ProductRepository::aiContext(qint64 productId,const ProductContext &context)
{
    ProductProfile profile; if (!load(productId,profile)) return {};
    QJsonArray observations; QSqlQuery query(m_database.database());
    query.prepare("SELECT r.amount_cents,r.source,r.observed_ms FROM price_records r JOIN product_links l ON l.kind='price' AND l.entity_id=r.track_id WHERE l.product_id=:id ORDER BY observed_ms DESC,r.id DESC LIMIT 12"); query.bindValue(":id",productId);
    if (!query.exec()) { m_error=query.lastError().text(); return {}; }
    while (query.next()) observations << QJsonObject{{"amount",Transaction::formatAmount(query.value(0).toLongLong())},{"source",query.value(1).toString()},{"observed_at",QDateTime::fromMSecsSinceEpoch(query.value(2).toLongLong()).toString(Qt::ISODate)}};
    if (query.lastError().isValid()) { m_error=query.lastError().text(); return {}; }
    QJsonArray sourceFacts; QList<ProductEvidence> shots; if (!evidence(productId,shots)) return {};
    for (const auto &shot:shots.mid(qMax(0,int(shots.size())-2))) sourceFacts << QJsonObject{{"captured_at",QDateTime::fromMSecsSinceEpoch(shot.createdMs).toString(Qt::ISODate)},
        {"recognition_note",shot.extracted.value("note").toString().left(300)},
        {"merchant_material_excerpt",shot.extracted.value("fields").toObject().value("materials").toString().left(300)}};
    auto details=profile.fields; details.remove("purchase_reason"); details.remove("alternatives");
    return json(QJsonObject{{"product",profile.name},{"current_record_name",context.name},{"category",profile.category},{"confirmed_details",details},
        {"user_purchase_reason",purchaseReason(profile,context)},{"user_alternatives",profile.fields.value("alternatives").toString()},
        {"user_needs_and_notes",profile.notes},{"current_record_price",context.amountCents?Transaction::formatAmount(context.amountCents):QStringLiteral("未知")},
        {"price_history",observations},{"merchant_screenshot_extractions",sourceFacts}});
}
