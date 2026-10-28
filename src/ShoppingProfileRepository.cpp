#include "ShoppingProfileRepository.h"
#include "CategoryCatalog.h"
#include "WorkflowStorage.h"
#include <QSqlQuery>
#include <QVariant>

bool ShoppingProfileRepository::loadRules(QStringList &rules)
{
    if (!m_database.database().isOpen()) { m_error=QStringLiteral("数据库未打开。"); return false; }
    QSqlQuery query(m_database.database());
    if (!query.exec("SELECT content FROM shopping_rules ORDER BY sort_order,id")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    QStringList loaded; while (query.next()) loaded << query.value(0).toString();
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    rules=loaded; m_error.clear(); return true;
}
bool ShoppingProfileRepository::saveRules(const QStringList &rules)
{
    m_error.clear();
    if (rules.size()>30) { m_error=QStringLiteral("最多保存 30 条购物原则。"); return false; }
    for (const auto &rule:rules) if (rule.trimmed().isEmpty() || rule.trimmed().size()>500) { m_error=QStringLiteral("每条购物原则须为 1 至 500 字。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database());
    if (!query.exec("DELETE FROM shopping_rules")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    for (int i=0;i<rules.size();++i) {
        query.prepare("INSERT INTO shopping_rules(content,sort_order) VALUES(:content,:order)"); query.bindValue(":content",rules[i].trimmed()); query.bindValue(":order",i);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    }
    return transaction.commit();
}
bool ShoppingProfileRepository::loadBrands(QList<BrandEntry> &brands,const QString &category)
{
    if (!m_database.database().isOpen()) { m_error=QStringLiteral("数据库未打开。"); return false; }
    QSqlQuery query(m_database.database());
    query.prepare("SELECT b.id,b.name,b.detail_category,b.weight,b.note,b.active,"
        "(SELECT COUNT(*) FROM review_tasks r WHERE r.brand_id=b.id AND r.completed_ms IS NOT NULL AND r.worthiness=1),"
        "(SELECT COUNT(*) FROM review_tasks r WHERE r.brand_id=b.id AND r.completed_ms IS NOT NULL AND r.worthiness=-1) "
        "FROM brands b WHERE b.active=1 AND (:category='' OR b.detail_category=:category) ORDER BY b.weight DESC,b.name_key,b.id");
    query.bindValue(":category",category.isNull()?QStringLiteral(""):category);
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    QList<BrandEntry> loaded;
    while (query.next()) loaded.append({query.value(0).toLongLong(),query.value(1).toString(),query.value(2).toString(),query.value(4).toString(),query.value(3).toInt(),query.value(5).toInt()==1,query.value(6).toInt(),query.value(7).toInt()});
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    brands=loaded; m_error.clear(); return true;
}
bool ShoppingProfileRepository::saveBrand(BrandEntry &brand)
{
    m_error.clear(); const auto name=brand.name.trimmed(), note=brand.note.trimmed();
    if (brand.id<0 || name.isEmpty() || name.size()>80 || note.size()>500 || !CategoryCatalog::isValid(brand.category) || brand.weight < -100 || brand.weight>100) { m_error=QStringLiteral("请检查品牌名称、品类、权重和备注。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database()); qint64 id=brand.id; bool restored=false;
    if (!id) {
        query.prepare("SELECT id,active FROM brands WHERE detail_category=:category AND name_key=:key"); query.bindValue(":category",brand.category); query.bindValue(":key",WorkflowStorage::nameKey(name));
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
        if (query.next()) {
            if (query.value(1).toInt()==1) { m_error=QStringLiteral("这个品类已有同名品牌，请选中后修改。"); return false; }
            id=query.value(0).toLongLong(); restored=true;
        } query.finish();
    }
    if (id) query.prepare(restored?"UPDATE brands SET active=1,name=:name,name_key=:key,note=:note WHERE id=:id AND active=0":"UPDATE brands SET name=:name,name_key=:key,detail_category=:category,weight=:weight,note=:note WHERE id=:id AND active=1");
    else query.prepare("INSERT INTO brands(name,name_key,detail_category,weight,note) VALUES(:name,:key,:category,:weight,:note)");
    query.bindValue(":name",name); query.bindValue(":key",WorkflowStorage::nameKey(name)); query.bindValue(":note",note.isNull()?QStringLiteral(""):note);
    if (id) query.bindValue(":id",id);
    if (!restored) { query.bindValue(":category",brand.category); query.bindValue(":weight",brand.weight); }
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=query.lastError().isValid()?WorkflowStorage::error(query.lastError()):QStringLiteral("品牌已不存在或已停用，请刷新。"); return false; }
    if (!id) id=query.lastInsertId().toLongLong(); query.finish();
    if (!transaction.commit()) return false;
    brand.id=id; brand.name=name; brand.note=note; return true;
}
bool ShoppingProfileRepository::archiveBrand(qint64 id)
{
    m_error.clear();
    if (!m_database.database().isOpen() || id<=0) { m_error=QStringLiteral("请选择有效品牌。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("UPDATE brands SET active=0 WHERE id=:id AND active=1"); query.bindValue(":id",id);
    if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("品牌停用失败，请刷新后重试。"); return false; } return true;
}
