#include "SupplementSchema.h"
#include "AchievementRepository.h"
#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

QList<WorkflowTable> SupplementSchema::tables()
{
    return {
        {"achievement_marks","CREATE TABLE IF NOT EXISTS achievement_marks(id INTEGER PRIMARY KEY AUTOINCREMENT,badge_key TEXT NOT NULL,fact_key TEXT NOT NULL,observed_ms INTEGER NOT NULL,UNIQUE(badge_key,fact_key))",{"id","badge_key","fact_key","observed_ms"},{"id","observed_ms"},{}},
        {"bill_import_keys","CREATE TABLE IF NOT EXISTS bill_import_keys(id INTEGER PRIMARY KEY AUTOINCREMENT,import_key TEXT NOT NULL UNIQUE,payload_hash TEXT NOT NULL,fingerprint TEXT NOT NULL,platform TEXT NOT NULL,external_id TEXT NOT NULL,file_hash TEXT NOT NULL,source_file TEXT NOT NULL,original_name TEXT NOT NULL,transaction_id INTEGER,imported_ms INTEGER NOT NULL)",{"id","import_key","payload_hash","fingerprint","platform","external_id","file_hash","source_file","original_name","transaction_id","imported_ms"},{"id","transaction_id","imported_ms"},{"transaction_id"}},
        {"transaction_reviews","CREATE TABLE IF NOT EXISTS transaction_reviews(id INTEGER PRIMARY KEY AUTOINCREMENT,transaction_id INTEGER NOT NULL UNIQUE,tag INTEGER NOT NULL CHECK(tag IN (-1,1)),note TEXT NOT NULL DEFAULT '',updated_ms INTEGER NOT NULL)",{"id","transaction_id","tag","note","updated_ms"},{"id","transaction_id","tag","updated_ms"},{}},
        {"monthly_stories","CREATE TABLE IF NOT EXISTS monthly_stories(id INTEGER PRIMARY KEY AUTOINCREMENT,month TEXT NOT NULL UNIQUE,facts_hash TEXT NOT NULL,facts_json TEXT NOT NULL,story TEXT NOT NULL,persona TEXT NOT NULL,source TEXT NOT NULL,generated_ms INTEGER NOT NULL)",{"id","month","facts_hash","facts_json","story","persona","source","generated_ms"},{"id","generated_ms"},{}}
    };
}
bool SupplementSchema::validate(const QSqlDatabase &database,QString &error)
{
    QSqlQuery query(database);
    const auto reject=[&](const QString &sql) { if (!query.exec(sql) || query.next() || query.lastError().isValid()) { error=QStringLiteral("备份中的导入记录、月度标记或故事数据无效。"); query.finish(); return false; } query.finish(); return true; };
    for (const auto &table:tables()) { QStringList invalid; for (const auto &column:table.columns) { const auto check="typeof("+column+")!='"+(table.integers.contains(column)?QStringLiteral("integer"):QStringLiteral("text"))+"'"; invalid << (table.nullable.contains(column)?"("+column+" IS NOT NULL AND "+check+")":check); } if (!reject("SELECT 1 FROM "+table.name+" WHERE "+invalid.join(" OR ")+" LIMIT 1")) return false; }
    const QStringList checks={
        "SELECT 1 FROM bill_import_keys k WHERE id<=0 OR length(import_key)!=64 OR import_key GLOB '*[^0-9a-f]*' OR length(payload_hash)!=64 OR payload_hash GLOB '*[^0-9a-f]*' OR length(fingerprint)!=64 OR fingerprint GLOB '*[^0-9a-f]*' OR length(file_hash)!=64 OR file_hash GLOB '*[^0-9a-f]*' OR platform NOT IN ('wechat','alipay','kura') OR length(external_id)>256 OR length(source_file)>512 OR length(trim(original_name)) NOT BETWEEN 1 AND 8000 OR (transaction_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM transactions t WHERE t.id=k.transaction_id)) OR imported_ms NOT BETWEEN -62135596800000 AND 253402300799999 LIMIT 1",
        "SELECT 1 FROM transaction_reviews r WHERE id<=0 OR transaction_id<=0 OR tag NOT IN (-1,1) OR length(note)>500 OR updated_ms NOT BETWEEN -62135596800000 AND 253402300799999 OR NOT EXISTS(SELECT 1 FROM transactions t WHERE t.id=r.transaction_id) LIMIT 1",
        "SELECT 1 FROM monthly_stories WHERE id<=0 OR length(month)!=7 OR length(facts_hash)!=64 OR facts_hash GLOB '*[^0-9a-f]*' OR length(facts_json)>40000 OR length(trim(story)) NOT BETWEEN 1 AND 12000 OR length(persona)>1200 OR source NOT IN ('local','ai') OR generated_ms NOT BETWEEN -62135596800000 AND 253402300799999 LIMIT 1",
        "SELECT 1 FROM bill_import_keys GROUP BY import_key HAVING COUNT(*)>1","SELECT 1 FROM transaction_reviews GROUP BY transaction_id HAVING COUNT(*)>1","SELECT 1 FROM monthly_stories GROUP BY month HAVING COUNT(*)>1"};
    for (const auto &sql:checks) if (!reject(sql)) return false;
    QStringList badgeKeys;
    for (const auto &badge:AchievementRepository::definitions()) {
        badgeKeys << "'"+badge.key+"'";
        if (!reject(QStringLiteral("SELECT 1 FROM achievement_marks WHERE badge_key='%1' GROUP BY badge_key HAVING COUNT(*)>%2").arg(badge.key).arg(badge.target))) return false;
    }
    if (!reject("SELECT 1 FROM achievement_marks WHERE id<=0 OR badge_key NOT IN ("+badgeKeys.join(',')+") OR length(trim(fact_key)) NOT BETWEEN 1 AND 256 OR observed_ms NOT BETWEEN 0 AND 253402300799999 LIMIT 1")
        || !reject("SELECT 1 FROM achievement_marks GROUP BY badge_key,fact_key HAVING COUNT(*)>1")) return false;
    if (!query.exec("SELECT month,facts_json FROM monthly_stories")) { error=query.lastError().text(); return false; }
    while (query.next()) { const auto key=query.value(0).toString(); const auto date=QDate::fromString(key+"-01",Qt::ISODate); const auto facts=QJsonDocument::fromJson(query.value(1).toString().toUtf8()); if (!date.isValid() || date.year()<1 || date.year()>9999 || date.toString("yyyy-MM")!=key || !facts.isObject() || facts.object().value("month").toString()!=key) { error=QStringLiteral("月度故事的月份或事实快照无效。"); return false; } }
    if (query.lastError().isValid()) { error=query.lastError().text(); return false; } error.clear(); return true;
}
