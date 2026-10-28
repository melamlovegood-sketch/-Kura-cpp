#include "DecisionSchema.h"
#include "CategoryRepository.h"
#include "WorkflowSchema.h"
#include "SupplementSchema.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSet>
#include <QVariant>

bool DecisionSchema::upgrade(QSqlDatabase database, QString &error)
{
    if (!database.transaction()) { error = database.lastError().text(); return false; }
    QSqlQuery query(database);
    const auto run = [&](const QString &sql) {
        if (query.exec(sql)) { query.finish(); return true; }
        error = QStringLiteral("无法升级消费决策数据：%1").arg(query.lastError().text());
        query.finish(); database.rollback(); return false;
    };
    for (const QString &table : {QStringLiteral("transactions"), QStringLiteral("wishlist"), QStringLiteral("monthly_budgets")}) {
        if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) { error = query.lastError().text(); database.rollback(); return false; }
        QSet<QString> columns;
        while (query.next()) columns.insert(query.value(1).toString());
        if (query.lastError().isValid()) { error = query.lastError().text(); query.finish(); database.rollback(); return false; }
        query.finish();
        const QList<QPair<QString, QString>> fields = table == QStringLiteral("monthly_budgets")
            ? QList<QPair<QString, QString>>{{"basic_limit_cents", "INTEGER"}, {"discretionary_limit_cents", "INTEGER"}}
            : QList<QPair<QString, QString>>{{"detail_category", "TEXT NOT NULL DEFAULT ''"}, {"source_image", "BLOB NOT NULL DEFAULT X''"}};
        for (const auto &field : fields)
            if (!columns.contains(field.first) && !run(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, field.first, field.second))) return false;
    }
    if (!query.exec(QStringLiteral("PRAGMA table_info(transactions)"))) { error=query.lastError().text(); database.rollback(); return false; }
    QSet<QString> expenseColumns; while (query.next()) expenseColumns.insert(query.value(1).toString()); query.finish();
    if (!expenseColumns.contains("purpose_category") && !run(QStringLiteral("ALTER TABLE transactions ADD COLUMN purpose_category TEXT NOT NULL DEFAULT '' CHECK(purpose_category IN ('','food','fun','clothing','travel','other','unclassified'))"))) return false;
    if (!expenseColumns.contains("budget_scope") && !run(QStringLiteral("ALTER TABLE transactions ADD COLUMN budget_scope TEXT NOT NULL DEFAULT '' CHECK(budget_scope IN ('','basic','discretionary'))"))) return false;
    if (!run(QStringLiteral("CREATE TABLE IF NOT EXISTS wish_pools (id INTEGER PRIMARY KEY AUTOINCREMENT, wish_id INTEGER NOT NULL, name TEXT NOT NULL CHECK(length(name) BETWEEN 1 AND 100), target_cents INTEGER NOT NULL CHECK(typeof(target_cents)='integer' AND target_cents BETWEEN 1 AND 9999999999), created_ms INTEGER NOT NULL, closed_ms INTEGER)"))) return false;
    if (!run(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS one_active_wish_pool ON wish_pools((1)) WHERE closed_ms IS NULL"))) return false;
    if (!run(QStringLiteral("CREATE TABLE IF NOT EXISTS savings_records (id INTEGER PRIMARY KEY AUTOINCREMENT, pool_id INTEGER, amount_cents INTEGER NOT NULL CHECK(typeof(amount_cents)='integer' AND amount_cents BETWEEN 1 AND 9999999999), description TEXT NOT NULL CHECK(length(description) BETWEEN 1 AND 1000), created_ms INTEGER NOT NULL, request_token TEXT NOT NULL UNIQUE CHECK(length(request_token) BETWEEN 1 AND 64))"))) return false;
    for (const auto &table:WorkflowSchema::tables()) if (!run(table.createSql)) return false;
    for (const auto &table:SupplementSchema::tables()) if (!run(table.createSql)) return false;
    if (!run(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS one_active_execution ON execution_sessions((1)) WHERE state='active'"))) return false;
    if (!run(QStringLiteral("DROP INDEX IF EXISTS unique_active_price_name"))) return false;
    if (!run(QStringLiteral("CREATE INDEX IF NOT EXISTS price_name_lookup ON price_tracks(detail_category,name_key) WHERE active=1"))) return false;
    if (!CategoryRepository::upgrade(database,error)) {database.rollback();return false;}
    if (!database.commit()) { error = database.lastError().text(); database.rollback(); return false; }
    return true;
}
