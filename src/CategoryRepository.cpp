#include "CategoryRepository.h"
#include "CategoryCatalog.h"
#include "WorkflowStorage.h"
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QSet>

bool CategoryRepository::load(QList<UserCategory> &categories,bool includeInactive)
{
    m_error.clear(); QSqlQuery q(m_database);
    if(!q.exec(QStringLiteral("SELECT id,category_key,name,active FROM user_categories %1 ORDER BY id").arg(includeInactive?QString():QStringLiteral("WHERE active=1")))) {m_error=q.lastError().text();return false;}
    QList<UserCategory> loaded;QSet<QString> names;
    for(const auto &key:CategoryCatalog::purposeKeys())names.insert(CategoryCatalog::purposeLabel(key).toCaseFolded());
    while(q.next()) {
        UserCategory c{q.value(0).toLongLong(),q.value(1).toString(),q.value(2).toString(),q.value(3).toBool()};
        if(c.id<=0 || !CategoryCatalog::isCustom(c.key) || CategoryCatalog::customKey(c.name)!=c.key || (q.value(3).toInt()!=0 && q.value(3).toInt()!=1) || names.contains(c.name.toCaseFolded()) || loaded.size()>=40) {m_error=QStringLiteral("自定义分类数据无效。");return false;}
        names.insert(c.name.toCaseFolded());loaded.append(c);
    }
    if(q.lastError().isValid()){m_error=q.lastError().text();return false;}
    categories=loaded;return true;
}
QString CategoryRepository::validateName(const QString &value,const QString &except)
{
    const auto name=value.trimmed().normalized(QString::NormalizationForm_C);
    if(!CategoryCatalog::isCustom(CategoryCatalog::customKey(name))) return QStringLiteral("分类名称请填写 1–20 个可见字符。");
    for(const auto &key:CategoryCatalog::purposeKeys()) if(CategoryCatalog::purposeLabel(key).compare(name,Qt::CaseInsensitive)==0) return QStringLiteral("这个名称已用于默认分类，请换一个名称。");
    QList<UserCategory> categories;if(!load(categories,true)) return m_error;
    for(const auto &c:categories) if(c.key!=except && c.name.compare(name,Qt::CaseInsensitive)==0) return QStringLiteral("已有同名分类；已停用的分类可以重新启用。");
    if(except.isEmpty() && categories.size()>=40) return QStringLiteral("最多保留 40 个自定义分类。");
    return {};
}
bool CategoryRepository::add(const QString &value)
{
    m_error=validateName(value);if(!m_error.isEmpty())return false;
    QSqlQuery q(m_database);q.prepare("INSERT INTO user_categories(category_key,name,active) VALUES(:key,:name,1)");
    q.bindValue(":key",CategoryCatalog::customKey(value));q.bindValue(":name",value.trimmed().normalized(QString::NormalizationForm_C));
    if(q.exec())return true;m_error=q.lastError().text();return false;
}
bool CategoryRepository::rename(const QString &key,const QString &value)
{
    m_error.clear();if(!CategoryCatalog::isCustom(key)){m_error=QStringLiteral("默认分类保留原名称，请新增自己的分类。");return false;}
    m_error=validateName(value,key);if(!m_error.isEmpty())return false;
    WorkflowStorage::Transaction transaction(m_database,m_error);if(!transaction.ok())return false;
    const auto updated=CategoryCatalog::customKey(value);
    QSqlQuery q(m_database);q.prepare("UPDATE user_categories SET category_key=:new,name=:name WHERE category_key=:old");
    q.bindValue(":new",updated);q.bindValue(":name",value.trimmed().normalized(QString::NormalizationForm_C));q.bindValue(":old",key);
    if(!q.exec() || q.numRowsAffected()!=1){m_error=q.lastError().isValid()?q.lastError().text():QStringLiteral("分类不存在，请刷新后重试。");return false;}q.finish();
    const QList<QPair<QString,QString>> columns={{"transactions","detail_category"},{"transactions","purpose_category"},{"wishlist","detail_category"},{"brands","detail_category"},{"execution_sessions","detail_category"},{"review_tasks","detail_category"},{"price_tracks","detail_category"},{"product_profiles","detail_category"}};
    for(const auto &column:columns){
        q.prepare("UPDATE "+column.first+" SET "+column.second+"=:new WHERE "+column.second+"=:old");
        q.bindValue(":new",updated);q.bindValue(":old",key);
        if(!q.exec()){m_error=q.lastError().text();return false;}q.finish();
    }
    return transaction.commit();
}
bool CategoryRepository::setActive(const QString &key,bool active)
{
    m_error.clear();if(!CategoryCatalog::isCustom(key)){m_error=QStringLiteral("默认五类始终保留。");return false;}
    QSqlQuery q(m_database);q.prepare("UPDATE user_categories SET active=:active WHERE category_key=:key");q.bindValue(":active",active?1:0);q.bindValue(":key",key);
    if(q.exec() && q.numRowsAffected()==1)return true;m_error=q.lastError().isValid()?q.lastError().text():QStringLiteral("分类不存在。");return false;
}
bool CategoryRepository::upgrade(QSqlDatabase database,QString &error)
{
    // Called inside the schema transaction. Preserve every column, index, trigger
    // and the AUTOINCREMENT high-water mark when opening a pre-custom-category DB.
    QSqlQuery q(database);if(!q.exec("SELECT sql FROM sqlite_master WHERE type='table' AND name='transactions'") || !q.next()){error=q.lastError().text();return false;}
    QString sql=q.value(0).toString();q.finish();if(sql.contains("custom:"))return true;
    const QRegularExpression oldCheck("CHECK\\s*\\(\\s*purpose_category\\s+IN\\s*\\([^)]*\\)\\s*\\)",QRegularExpression::CaseInsensitiveOption);
    if(!sql.contains(oldCheck)){error=QStringLiteral("无法识别旧分类约束，原数据保持不变。");return false;}
    sql.replace(oldCheck,"CHECK(purpose_category='' OR "+CategoryCatalog::validSql("purpose_category",true)+")");
    sql.replace(QRegularExpression("^CREATE TABLE\\s+(?:IF NOT EXISTS\\s+)?[\"`\\[]?transactions[\"`\\]]?",QRegularExpression::CaseInsensitiveOption),"CREATE TABLE transactions_category_upgrade");
    QStringList objects;
    if(!q.exec("SELECT sql FROM sqlite_master WHERE tbl_name='transactions' AND type IN ('index','trigger') AND sql IS NOT NULL")){error=q.lastError().text();return false;}
    while(q.next())objects<<q.value(0).toString();q.finish();
    qint64 sequence=0;if(!q.exec("SELECT seq FROM sqlite_sequence WHERE name='transactions'")){error=q.lastError().text();return false;}if(q.next())sequence=q.value(0).toLongLong();q.finish();
    QStringList columns;if(!q.exec("PRAGMA table_info(transactions)")){error=q.lastError().text();return false;}while(q.next())columns<<'"'+q.value(1).toString()+'"';q.finish();
    const QStringList changes={sql,"INSERT INTO transactions_category_upgrade("+columns.join(',')+") SELECT "+columns.join(',')+" FROM transactions","DROP TABLE transactions","ALTER TABLE transactions_category_upgrade RENAME TO transactions"};
    for(const auto &change:changes)if(!q.exec(change)){error=q.lastError().text();return false;}
    for(const auto &object:objects)if(!q.exec(object)){error=q.lastError().text();return false;}
    q.prepare("UPDATE sqlite_sequence SET seq=max(seq,:seq) WHERE name='transactions'");q.bindValue(":seq",sequence);
    if(!q.exec()){error=q.lastError().text();return false;}
    if(q.numRowsAffected()==0){q.prepare("INSERT INTO sqlite_sequence(name,seq) VALUES('transactions',:seq)");q.bindValue(":seq",sequence);if(!q.exec()){error=q.lastError().text();return false;}}
    return true;
}
