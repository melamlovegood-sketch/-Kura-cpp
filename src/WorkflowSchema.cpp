#include "WorkflowSchema.h"
#include "ExecutionRepository.h"
#include "WorkflowStorage.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

QList<WorkflowTable> WorkflowSchema::tables()
{
    return {
        {"shopping_rules", "CREATE TABLE IF NOT EXISTS shopping_rules(id INTEGER PRIMARY KEY AUTOINCREMENT,content TEXT NOT NULL CHECK(length(trim(content)) BETWEEN 1 AND 500),sort_order INTEGER NOT NULL)", {"id","content","sort_order"},{"id","sort_order"},{}},
        {"brands", "CREATE TABLE IF NOT EXISTS brands(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL CHECK(length(trim(name)) BETWEEN 1 AND 80),name_key TEXT NOT NULL,detail_category TEXT NOT NULL,weight INTEGER NOT NULL DEFAULT 0 CHECK(weight BETWEEN -100 AND 100),note TEXT NOT NULL DEFAULT '' CHECK(length(note)<=500),active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),UNIQUE(detail_category,name_key))", {"id","name","name_key","detail_category","weight","note","active"},{"id","weight","active"},{}},
        {"execution_sessions", "CREATE TABLE IF NOT EXISTS execution_sessions(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,detail_category TEXT NOT NULL,estimated_cents INTEGER NOT NULL,mode TEXT NOT NULL,duration_seconds INTEGER NOT NULL,started_ms INTEGER NOT NULL,deadline_ms INTEGER NOT NULL,state TEXT NOT NULL DEFAULT 'active',closed_ms INTEGER,transaction_id INTEGER,notes TEXT NOT NULL DEFAULT '',advice TEXT NOT NULL DEFAULT '')", {"id","name","detail_category","estimated_cents","mode","duration_seconds","started_ms","deadline_ms","state","closed_ms","transaction_id","notes","advice"},{"id","estimated_cents","duration_seconds","started_ms","deadline_ms","closed_ms","transaction_id"},{"closed_ms","transaction_id"}},
        {"review_tasks", "CREATE TABLE IF NOT EXISTS review_tasks(id INTEGER PRIMARY KEY AUTOINCREMENT,transaction_id INTEGER NOT NULL,name TEXT NOT NULL,amount_cents INTEGER NOT NULL,detail_category TEXT NOT NULL,brand_id INTEGER,brand_name TEXT NOT NULL DEFAULT '',review_day INTEGER NOT NULL CHECK(review_day IN (7,30)),purchased_ms INTEGER NOT NULL,due_ms INTEGER NOT NULL,frequency INTEGER,worthiness INTEGER,note TEXT NOT NULL DEFAULT '',completed_ms INTEGER,UNIQUE(transaction_id,review_day))", {"id","transaction_id","name","amount_cents","detail_category","brand_id","brand_name","review_day","purchased_ms","due_ms","frequency","worthiness","note","completed_ms"},{"id","transaction_id","amount_cents","brand_id","review_day","purchased_ms","due_ms","frequency","worthiness","completed_ms"},{"brand_id","frequency","worthiness","completed_ms"}},
        {"price_tracks", "CREATE TABLE IF NOT EXISTS price_tracks(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,name_key TEXT NOT NULL,detail_category TEXT NOT NULL,target_cents INTEGER,source_url TEXT NOT NULL DEFAULT '',active INTEGER NOT NULL DEFAULT 1,created_ms INTEGER NOT NULL,dismissed_target_record_id INTEGER,dismissed_drop_record_id INTEGER)", {"id","name","name_key","detail_category","target_cents","source_url","active","created_ms","dismissed_target_record_id","dismissed_drop_record_id"},{"id","target_cents","active","created_ms","dismissed_target_record_id","dismissed_drop_record_id"},{"target_cents","dismissed_target_record_id","dismissed_drop_record_id"}},
        {"price_records", "CREATE TABLE IF NOT EXISTS price_records(id INTEGER PRIMARY KEY AUTOINCREMENT,track_id INTEGER NOT NULL,amount_cents INTEGER NOT NULL,source TEXT NOT NULL,observed_ms INTEGER NOT NULL,request_token TEXT NOT NULL UNIQUE)", {"id","track_id","amount_cents","source","observed_ms","request_token"},{"id","track_id","amount_cents","observed_ms"},{}}
    };
}

bool WorkflowSchema::validate(const QSqlDatabase &database,QString &error)
{
    QSqlQuery query(database);
    const auto rejectRows=[&](const QString &sql) {
        if (!query.exec(sql) || query.next() || query.lastError().isValid()) { error=QStringLiteral("备份中的执行、品牌、复盘或价格数据无效。"); query.finish(); return false; }
        query.finish(); return true;
    };
    for (const auto &table:tables()) {
        QStringList invalid;
        for (const auto &column:table.columns) {
            const auto check="typeof("+column+")!='"+(table.integers.contains(column)?QStringLiteral("integer"):QStringLiteral("text"))+"'";
            invalid << (table.nullable.contains(column)?"("+column+" IS NOT NULL AND "+check+")":check);
        }
        if (!rejectRows("SELECT 1 FROM "+table.name+" WHERE "+invalid.join(" OR ")+" LIMIT 1")) return false;
    }
    const QString category="('canteen','transport','daily_supplies','subscription','daily','online_shopping','entertainment','other','study')";
    const QStringList checks={
        "SELECT 1 FROM shopping_rules WHERE id<=0 OR length(trim(content)) NOT BETWEEN 1 AND 500 OR sort_order<0 LIMIT 1",
        "SELECT 1 FROM brands WHERE id<=0 OR length(trim(name)) NOT BETWEEN 1 AND 80 OR length(note)>500 OR weight NOT BETWEEN -100 AND 100 OR active NOT IN (0,1) OR detail_category NOT IN "+category+" LIMIT 1",
        "SELECT 1 FROM execution_sessions WHERE id<=0 OR length(trim(name)) NOT BETWEEN 1 AND 100 OR detail_category NOT IN "+category+" OR estimated_cents NOT BETWEEN 0 AND 9999999999 OR mode NOT IN ('fast','research') OR duration_seconds NOT BETWEEN 60 AND 7200 OR deadline_ms!=started_ms+duration_seconds*1000 OR started_ms NOT BETWEEN -62135596800000 AND 253402300799999 OR deadline_ms NOT BETWEEN -62135596800000 AND 253402300799999 OR state NOT IN ('active','bought','skipped','undecided') OR length(notes)>4000 OR length(advice)>"+QString::number(ExecutionSession::MaximumAdviceLength)+" OR (state='active' AND (closed_ms IS NOT NULL OR transaction_id IS NOT NULL)) OR (state!='active' AND (closed_ms IS NULL OR closed_ms<started_ms)) OR (state='bought' AND (transaction_id IS NULL OR transaction_id<=0)) OR (state!='bought' AND transaction_id IS NOT NULL) LIMIT 1",
        "SELECT 1 FROM review_tasks r WHERE id<=0 OR transaction_id<=0 OR length(trim(name)) NOT BETWEEN 1 AND 100 OR amount_cents NOT BETWEEN 1 AND 9999999999 OR detail_category NOT IN "+category+" OR length(brand_name)>80 OR length(note)>500 OR review_day NOT IN (7,30) OR purchased_ms NOT BETWEEN -62135596800000 AND 253402300799999 OR due_ms NOT BETWEEN -62135596800000 AND 253402300799999 OR due_ms!=purchased_ms+review_day*86400000 OR (brand_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM brands b WHERE b.id=r.brand_id)) OR (completed_ms IS NULL AND (frequency IS NOT NULL OR worthiness IS NOT NULL OR NOT EXISTS(SELECT 1 FROM transactions t WHERE t.id=r.transaction_id))) OR (completed_ms IS NOT NULL AND (frequency IS NULL OR frequency NOT IN (0,1,2) OR worthiness IS NULL OR worthiness NOT IN (-1,0,1) OR completed_ms<due_ms OR completed_ms>253402300799999)) LIMIT 1",
        "SELECT 1 FROM price_tracks WHERE id<=0 OR length(trim(name)) NOT BETWEEN 1 AND 100 OR detail_category NOT IN "+category+" OR (target_cents IS NOT NULL AND target_cents NOT BETWEEN 1 AND 9999999999) OR active NOT IN (0,1) OR length(source_url)>2000 OR created_ms NOT BETWEEN -62135596800000 AND 253402300799999 LIMIT 1",
        "SELECT 1 FROM price_records p WHERE id<=0 OR amount_cents NOT BETWEEN 1 AND 9999999999 OR length(trim(source)) NOT BETWEEN 1 AND 80 OR observed_ms NOT BETWEEN -62135596800000 AND 253402300799999 OR length(request_token) NOT BETWEEN 1 AND 64 OR NOT EXISTS(SELECT 1 FROM price_tracks t WHERE t.id=p.track_id) LIMIT 1",
        "SELECT 1 FROM price_tracks t WHERE NOT EXISTS(SELECT 1 FROM price_records p WHERE p.track_id=t.id) OR (dismissed_target_record_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM price_records p WHERE p.track_id=t.id AND p.id=t.dismissed_target_record_id)) OR (dismissed_drop_record_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM price_records p WHERE p.track_id=t.id AND p.id=t.dismissed_drop_record_id)) LIMIT 1",
        "SELECT 1 FROM review_tasks GROUP BY transaction_id,review_day HAVING COUNT(*)>1",
        "SELECT 1 FROM brands GROUP BY detail_category,name_key HAVING COUNT(*)>1",
        "SELECT 1 FROM price_records GROUP BY request_token HAVING COUNT(*)>1"
    };
    for (const auto &sql:checks) if (!rejectRows(sql)) return false;
    for (const auto &table:{QStringLiteral("brands"),QStringLiteral("price_tracks")}) {
        if (!query.exec("SELECT name,name_key FROM "+table)) { error=query.lastError().text(); return false; }
        while (query.next()) if (WorkflowStorage::nameKey(query.value(0).toString())!=query.value(1).toString()) { error=QStringLiteral("品牌或价格条目的名称索引无效。"); return false; }
        if (query.lastError().isValid()) { error=query.lastError().text(); return false; }
    }
    error.clear(); return true;
}
