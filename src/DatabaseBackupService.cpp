#include "ProductRepository.h"
#include "DatabaseBackupService.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "WishlistRepository.h"
#include "MonthlyBudget.h"
#include "BudgetRepository.h"
#include "WishPoolRepository.h"
#include "WorkflowSchema.h"
#include "SupplementSchema.h"
#include "PriceRepository.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>

namespace {
class ReadConnection
{
public:
    ReadConnection() : name(QStringLiteral("backup-%1").arg(QUuid::createUuid().toString())) {}
    ~ReadConnection()
    {
        if (QSqlDatabase::contains(name)) {
            { auto connection = database(); connection.close(); }
            QSqlDatabase::removeDatabase(name);
        }
    }
    bool open(const QString &path, QString &error)
    {
        auto connection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        connection.setDatabaseName(path);
        connection.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000"));
        if (!connection.open()) { error = connection.lastError().text(); return false; }
        return true;
    }
    QSqlDatabase database() const { return QSqlDatabase::database(name, false); }
private:
    QString name;
};

QString resolvedPath(const QString &path)
{
    const QFileInfo file(path);
    if (file.exists()) return file.canonicalFilePath();
    const QFileInfo parent(file.absolutePath());
    return QDir(parent.exists() ? parent.canonicalFilePath() : parent.absoluteFilePath()).filePath(file.fileName());
}

bool copyAtomically(const QString &source, const QString &target, QString &error)
{
    QFile input(source);
    QSaveFile output(target);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("无法保存备份：%1").arg(input.isOpen() ? output.errorString() : input.errorString());
        return false;
    }
    while (!input.atEnd()) {
        const QByteArray block = input.read(1024 * 1024);
        if (input.error() != QFileDevice::NoError || output.write(block) != block.size()) {
            error = QStringLiteral("备份写入失败：%1").arg(input.error() != QFileDevice::NoError ? input.errorString() : output.errorString());
            return false;
        }
    }
    if (!output.commit()) { error = output.errorString(); return false; }
    return true;
}

bool checkSchema(const QSqlDatabase &database, QString &error)
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA integrity_check")) || !query.next()
        || query.value(0).toString() != QStringLiteral("ok")) {
        error = QStringLiteral("文件损坏或不是完整的 SQLite 数据库。");
        return false;
    }
    query.finish();
    if (!query.exec(QStringLiteral("SELECT name,type FROM sqlite_master WHERE name NOT GLOB 'sqlite_*'"))) {
        error = query.lastError().text(); return false;
    }
    const QSet<QString> required = {QStringLiteral("transactions"), QStringLiteral("monthly_budgets"), QStringLiteral("wishlist")};
    QSet<QString> workflowNames; for (const auto &table:WorkflowSchema::tables()) workflowNames.insert(table.name);
    const auto legacyWorkflowNames=workflowNames-QSet<QString>{"execution_wishes"};
    QSet<QString> supplementNames; for (const auto &table:SupplementSchema::tables()) supplementNames.insert(table.name);
    const QSet<QString> legacySupplementNames=supplementNames-QSet<QString>{"achievement_marks","user_categories"};
    QSet<QString> productNames; for (const auto &table:ProductSchema::tables()) productNames.insert(table.name);
    const QSet<QString> allowed = productNames + required + QSet<QString>{QStringLiteral("wish_pools"), QStringLiteral("savings_records")} + workflowNames + supplementNames;
    QSet<QString> found;
    while (query.next()) {
        const QString type = query.value(1).toString();
        if (type == QStringLiteral("index")) continue;
        if (type != QStringLiteral("table") || !allowed.contains(query.value(0).toString())) {
            error = QStringLiteral("备份包含当前版本不支持的表、视图或触发器，不能恢复。");
            return false;
        }
        found.insert(query.value(0).toString());
    }
    if (!(required - found).isEmpty() || found.contains("wish_pools") != found.contains("savings_records")) {
        error = QStringLiteral("请选择包含消费、预算和愿望清单的 Kura 完整备份。");
        return false;
    }
    if (!(found & workflowNames).isEmpty() && (!(legacyWorkflowNames-found).isEmpty() || !found.contains("wish_pools"))) { error=QStringLiteral("备份中的第三步业务表不完整。"); return false; }
    if (!(found & supplementNames).isEmpty() && (!(legacySupplementNames-found).isEmpty() || !(legacyWorkflowNames-found).isEmpty())) { error=QStringLiteral("备份中的导入和月度复盘表不完整。"); return false; }
    const QSet<QString> listNames={"wishlist_price_links","wishlist_order"};
    const auto legacyProducts=productNames-listNames;
    if ((!(found & legacyProducts).isEmpty() && !(legacyProducts-found).isEmpty()) || (!(found & listNames).isEmpty() && !(listNames-found).isEmpty())) { error=QStringLiteral("备份中的商品资料表不完整。"); return false; }
    query.finish();
    QList<QPair<QString, QSet<QString>>> schemas = {
        {QStringLiteral("transactions"), {QStringLiteral("id"),QStringLiteral("description"),QStringLiteral("amount_cents"),QStringLiteral("category"),QStringLiteral("date")}},
        {QStringLiteral("monthly_budgets"), {QStringLiteral("month"),QStringLiteral("limit_cents")}},
        {QStringLiteral("wishlist"), {QStringLiteral("id"),QStringLiteral("name"),QStringLiteral("estimated_cents"),QStringLiteral("category"),QStringLiteral("cooling_days"),QStringLiteral("reason"),QStringLiteral("created_ms"),QStringLiteral("ready_ms"),QStringLiteral("status"),QStringLiteral("closed_ms"),QStringLiteral("transaction_id")}}};
    if (found.contains("wish_pools")) {
        schemas.append({"wish_pools", {"id","wish_id","name","target_cents","created_ms","closed_ms"}});
        schemas.append({"savings_records", {"id","pool_id","amount_cents","description","created_ms","request_token"}});
    }
    for (const auto &table:WorkflowSchema::tables()+SupplementSchema::tables()+ProductSchema::tables()) if (found.contains(table.name)) schemas.append({table.name,QSet<QString>(table.columns.begin(),table.columns.end())});
    for (const auto &schema : schemas) {
        if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(schema.first))) {
            error = query.lastError().text(); return false;
        }
        QSet<QString> columns;
        while (query.next()) {
            const QString column = query.value(1).toString();
            columns.insert(column);
            bool workflowInteger=false; for (const auto &table:WorkflowSchema::tables()+SupplementSchema::tables()+ProductSchema::tables()) if (table.name==schema.first) workflowInteger=table.integers.contains(column);
            const bool integer = workflowInteger || QSet<QString>{QStringLiteral("id"), QStringLiteral("amount_cents"),
                QStringLiteral("limit_cents"), QStringLiteral("estimated_cents"), QStringLiteral("cooling_days"),
                QStringLiteral("created_ms"), QStringLiteral("ready_ms"), QStringLiteral("status"),
                QStringLiteral("closed_ms"), QStringLiteral("transaction_id"), QStringLiteral("priority"), QStringLiteral("wish_id"), QStringLiteral("target_cents"), QStringLiteral("pool_id"), QStringLiteral("basic_limit_cents"), QStringLiteral("discretionary_limit_cents")}.contains(column);
            if (query.value(2).toString().toUpper() != (column == QStringLiteral("source_image") ? QStringLiteral("BLOB") : integer ? QStringLiteral("INTEGER") : QStringLiteral("TEXT"))) {
                error = QStringLiteral("备份字段的类型与当前版本不兼容。"); return false;
            }
            const bool primary = column == QStringLiteral("id") || (schema.first==QStringLiteral("monthly_budgets") && column == QStringLiteral("month"));
            if (query.value(5).toInt() != (primary ? 1 : 0)) {
                error = QStringLiteral("备份主键结构无效。"); return false;
            }
        }
        QSet<QString> optional;
        if (schema.first == QStringLiteral("transactions"))
            optional = {QStringLiteral("occurred_time"), QStringLiteral("note"), QStringLiteral("detail_category"), QStringLiteral("source_image"),QStringLiteral("purpose_category"),QStringLiteral("budget_scope")};
        if (schema.first == QStringLiteral("wishlist")) optional = {QStringLiteral("priority"),QStringLiteral("detail_category"),QStringLiteral("source_image")};
        if (schema.first == QStringLiteral("monthly_budgets")) optional = {QStringLiteral("basic_limit_cents"),QStringLiteral("discretionary_limit_cents")};
        if (!(schema.second - columns).isEmpty()) {
            error = QStringLiteral("备份缺少必要字段，不能恢复。"); return false;
        }
        if (!(columns - schema.second - optional).isEmpty()) {
            error = QStringLiteral("备份有当前版本不支持的字段，不能恢复。"); return false;
        }
        query.finish();
    }
    return true;
}

bool validateData(DatabaseManager &database, BackupInfo &info, QString &error)
{
    QSqlQuery types(database.database());
    const QStringList invalidTypeQueries = {
        QStringLiteral("SELECT 1 FROM transactions WHERE typeof(id)!='integer' OR typeof(amount_cents)!='integer' OR typeof(description)!='text' OR typeof(category)!='text' OR typeof(date)!='text' OR typeof(occurred_time)!='text' OR typeof(note)!='text' OR typeof(detail_category)!='text' OR typeof(source_image)!='blob' OR typeof(purpose_category)!='text' OR typeof(budget_scope)!='text' OR length(source_image)>2097152 LIMIT 1"),
        QStringLiteral("SELECT 1 FROM monthly_budgets WHERE typeof(month)!='text' OR typeof(limit_cents)!='integer' OR (basic_limit_cents IS NOT NULL AND typeof(basic_limit_cents)!='integer') OR (discretionary_limit_cents IS NOT NULL AND typeof(discretionary_limit_cents)!='integer') LIMIT 1"),
        QStringLiteral("SELECT 1 FROM wishlist WHERE typeof(id)!='integer' OR typeof(estimated_cents)!='integer' OR typeof(cooling_days)!='integer' OR typeof(priority)!='integer' OR typeof(created_ms)!='integer' OR typeof(ready_ms)!='integer' OR typeof(status)!='integer' OR typeof(name)!='text' OR typeof(category)!='text' OR typeof(reason)!='text' OR typeof(detail_category)!='text' OR typeof(source_image)!='blob' OR length(source_image)>2097152 OR (closed_ms IS NOT NULL AND typeof(closed_ms)!='integer') OR (transaction_id IS NOT NULL AND typeof(transaction_id)!='integer') LIMIT 1")};
    for (const auto &statement : invalidTypeQueries) {
        if (!types.exec(statement) || types.next()) { error = QStringLiteral("备份中的数据类型无效。"); return false; }
        types.finish();
    }
    QList<Transaction> expenses;
    TransactionRepository transactions(database);
    if (!transactions.findAll(expenses)) { error = transactions.lastError(); return false; }
    QList<WishItem> wishes;
    WishlistRepository wishlist(database);
    if (!wishlist.findAll(wishes)) { error = wishlist.lastError(); return false; }
    for (const auto &wish : wishes) {
        for (const auto &time : {wish.createdAt, wish.readyAt, wish.closedAt}) {
            if (time.isValid() && (time.date().year() < 1 || time.date().year() > 9999)) {
                error = QStringLiteral("备份中的愿望时间超出支持范围。"); return false;
            }
        }
    }
    QSqlQuery sequence(database.database());
    if (!sequence.exec(QStringLiteral("SELECT name,seq,typeof(seq) FROM sqlite_sequence WHERE name IN ('transactions','wishlist','wish_pools','savings_records','shopping_rules','brands','execution_sessions','review_tasks','price_tracks','price_records','bill_import_keys','transaction_reviews','monthly_stories','product_profiles','product_links','product_evidence')"))) {
        error = QStringLiteral("备份缺少自动编号信息。"); return false;
    }
    QSet<QString> sequenceNames;
    while (sequence.next()) {
        const QString name = sequence.value(0).toString();
        if (sequenceNames.contains(name) || sequence.value(2).toString() != QStringLiteral("integer") || sequence.value(1).toLongLong() < 0) {
            error = QStringLiteral("备份的自动编号信息无效。"); return false;
        }
        sequenceNames.insert(name);
    }
    if (sequence.lastError().isValid()) { error = sequence.lastError().text(); return false; }
    QSqlQuery budgets(database.database());
    if (!budgets.exec(QStringLiteral("SELECT month,limit_cents FROM monthly_budgets"))) {
        error = budgets.lastError().text(); return false;
    }
    qsizetype count = 0;
    while (budgets.next()) {
        const QString key = budgets.value(0).toString();
        const QDate month = QDate::fromString(key + QStringLiteral("-01"), QStringLiteral("yyyy-MM-dd"));
        if (!MonthlyBudget(month, budgets.value(1).toLongLong()).validationError().isEmpty()
            || MonthlyBudget::monthKey(month) != key) {
            error = QStringLiteral("备份中的预算月份或金额无效。"); return false;
        }
        ++count;
    }
    if (budgets.lastError().isValid()) { error = budgets.lastError().text(); return false; }
    WishPoolRepository pools(database); WishPoolSnapshot pool;
    if (!pools.load(pool)) { error = pools.lastError(); return false; }
    const QStringList invalidDecisionQueries = {
        QStringLiteral("SELECT 1 FROM wish_pools WHERE id<=0 OR typeof(id)!='integer' OR typeof(wish_id)!='integer' OR wish_id<=0 OR typeof(name)!='text' OR length(trim(name)) NOT BETWEEN 1 AND 100 OR typeof(target_cents)!='integer' OR target_cents NOT BETWEEN 1 AND 9999999999 OR typeof(created_ms)!='integer' OR (closed_ms IS NOT NULL AND (typeof(closed_ms)!='integer' OR closed_ms<created_ms)) LIMIT 1"),
        QStringLiteral("SELECT 1 FROM savings_records s WHERE id<=0 OR typeof(id)!='integer' OR typeof(amount_cents)!='integer' OR amount_cents NOT BETWEEN 1 AND 9999999999 OR typeof(description)!='text' OR length(trim(description)) NOT BETWEEN 1 AND 1000 OR typeof(created_ms)!='integer' OR typeof(request_token)!='text' OR length(request_token) NOT BETWEEN 1 AND 64 OR (pool_id IS NOT NULL AND (typeof(pool_id)!='integer' OR NOT EXISTS(SELECT 1 FROM wish_pools p WHERE p.id=s.pool_id))) LIMIT 1"),
        QStringLiteral("SELECT 1 FROM wish_pools p WHERE closed_ms IS NULL AND NOT EXISTS(SELECT 1 FROM wishlist w WHERE w.id=p.wish_id AND w.status=0) LIMIT 1")};
    for (const auto &sql : invalidDecisionQueries) if (!types.exec(sql) || types.next()) { error=QStringLiteral("备份中的许愿池或忍住记录无效。"); return false; }
    if (!types.exec(QStringLiteral("SELECT request_token FROM savings_records GROUP BY request_token HAVING COUNT(*)>1")) || types.next()) { error=QStringLiteral("忍住记录的操作编号重复。"); return false; }
    QSqlQuery totals(database.database());
    if (!totals.exec(QStringLiteral("SELECT SUM(amount_cents) FROM savings_records GROUP BY pool_id"))) { error=totals.lastError().text(); return false; }
    while (totals.next()) { bool ok; if (totals.value(0).toLongLong(&ok)<0 || !ok) { error=QStringLiteral("忍住金额超过支持范围。"); return false; } }
    if (totals.lastError().isValid()) { error=totals.lastError().text(); return false; }
    BudgetRepository budgetRepository(database);
    if (!budgets.exec(QStringLiteral("SELECT month FROM monthly_budgets"))) { error=budgets.lastError().text(); return false; }
    while (budgets.next()) { MonthlyBudgetStatus status; if (!budgetRepository.loadStatus(QDate::fromString(budgets.value(0).toString()+"-01",Qt::ISODate),status)) { error=budgetRepository.lastError(); return false; } }
    if (!types.exec(QStringLiteral("SELECT (SELECT COUNT(*) FROM wish_pools),(SELECT COUNT(*) FROM savings_records)")) || !types.next()) { error=types.lastError().text(); return false; }
    info = {expenses.size(), count, wishes.size(), types.value(0).toLongLong(), types.value(1).toLongLong()};
    if (!WorkflowSchema::validate(database.database(),error) || !SupplementSchema::validate(database.database(),error) || !ProductSchema::validate(database.database(),error)) return false;
    QList<PriceTrack> tracks; PriceRepository prices(database);
    if (!prices.load(tracks,true)) { error=prices.lastError(); return false; }
    if (!types.exec(QStringLiteral("SELECT (SELECT COUNT(*) FROM shopping_rules),(SELECT COUNT(*) FROM brands),(SELECT COUNT(*) FROM execution_sessions),(SELECT COUNT(*) FROM review_tasks),(SELECT COUNT(*) FROM price_tracks),(SELECT COUNT(*) FROM price_records)")) || !types.next()) { error=types.lastError().text(); return false; }
    info.ruleCount=types.value(0).toLongLong(); info.brandCount=types.value(1).toLongLong(); info.sessionCount=types.value(2).toLongLong(); info.reviewCount=types.value(3).toLongLong(); info.trackCount=types.value(4).toLongLong(); info.priceCount=types.value(5).toLongLong();
    if (!types.exec(QStringLiteral("SELECT (SELECT COUNT(*) FROM bill_import_keys),(SELECT COUNT(*) FROM transaction_reviews),(SELECT COUNT(*) FROM monthly_stories)")) || !types.next()) { error=types.lastError().text(); return false; }
    info.importKeyCount=types.value(0).toLongLong(); info.tagCount=types.value(1).toLongLong(); info.storyCount=types.value(2).toLongLong();
    return true;
}
}

DatabaseBackupService::DatabaseBackupService(DatabaseManager &database) : m_database(database) {}
DatabaseBackupService::~DatabaseBackupService() = default;

bool DatabaseBackupService::isProtectedPath(const QString &path) const
{
    const QString candidate = resolvedPath(path);
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm"), QStringLiteral("-journal")}) {
        const QString active = resolvedPath(m_database.databasePath() + suffix);
#ifdef Q_OS_WIN
        if (candidate.compare(active, Qt::CaseInsensitive) == 0) return true;
#else
        if (candidate == active) return true;
#endif
    }
    return false;
}

bool DatabaseBackupService::snapshot(const QString &source, const QString &destination, QString &error)
{
    if (!QFileInfo(source).isFile()) { error = QStringLiteral("备份文件不存在或不是普通文件。"); return false; }
    ReadConnection sourceConnection;
    if (!sourceConnection.open(source, error)) return false;
    QSqlQuery query(sourceConnection.database());
    if (!query.prepare(QStringLiteral("VACUUM main INTO :destination"))) { error = query.lastError().text(); return false; }
    query.bindValue(QStringLiteral(":destination"), destination);
    if (!query.exec()) { error = QStringLiteral("无法创建数据库快照：%1").arg(query.lastError().text()); return false; }
    return true;
}

bool DatabaseBackupService::backup(const QString &destination, QString &error)
{
    error.clear();
    if (isProtectedPath(destination)) { error = QStringLiteral("不能覆盖正在使用的数据库或日志文件。"); return false; }
    if (!m_database.database().isOpen()) { error = QStringLiteral("数据库不可用。"); return false; }
    QTemporaryDir directory;
    if (!directory.isValid()) { error = QStringLiteral("无法创建备份临时目录。"); return false; }
    const QString copy = directory.filePath(QStringLiteral("snapshot.sqlite"));
    if (!snapshot(m_database.databasePath(), copy, error)) return false;
    return copyAtomically(copy, destination, error);
}

void DatabaseBackupService::discardPrepared()
{
    m_preparedPath.clear();
    m_preparedDirectory.reset();
}

bool DatabaseBackupService::prepareRestore(const QString &source, BackupInfo &info, QString &error)
{
    discardPrepared();
    error.clear();
    if (isProtectedPath(source)) { error = QStringLiteral("请选择另存的备份文件，不能选择当前数据库或日志。"); return false; }
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) { error = QStringLiteral("无法创建恢复临时目录。"); return false; }
    const QString copy = directory->filePath(QStringLiteral("restore.sqlite"));
    if (!snapshot(source, copy, error)) return false;
    {
        ReadConnection read;
        if (!read.open(copy, error) || !checkSchema(read.database(), error)) return false;
    }
    {
        DatabaseManager prepared(copy);
        BackupInfo loaded;
        if (!prepared.initialize()) { error = prepared.lastError(); return false; }
        if (!validateData(prepared, loaded, error)) return false;
        info = loaded;
    }
    m_preparedPath = copy;
    m_preparedDirectory = std::move(directory);
    return true;
}

bool DatabaseBackupService::restorePrepared(QString &safetyBackup, QString &error)
{
    error.clear();
    safetyBackup.clear();
    if (m_preparedPath.isEmpty() || !m_database.database().isOpen()) {
        error = QStringLiteral("请先选择并检查有效的备份文件。"); return false;
    }
    auto connection = m_database.database();
    QSqlQuery query(connection);
    if (!query.prepare(QStringLiteral("ATTACH DATABASE :source AS kura_restore"))) { error = query.lastError().text(); return false; }
    query.bindValue(QStringLiteral(":source"), m_preparedPath);
    if (!query.exec()) { error = query.lastError().text(); return false; }
    query.finish();
    const auto detach = [&] { QSqlQuery cleanup(connection); cleanup.exec(QStringLiteral("DETACH DATABASE kura_restore")); };
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        error = QStringLiteral("数据库正在被占用或不可写：%1").arg(query.lastError().text());
        query.finish(); detach(); return false;
    }
    query.finish();
    // Block other writers while a separate read-only connection snapshots the
    // exact pre-restore state. VACUUM INTO must not run on the write transaction.
    const QString backupDirectory = QDir(QFileInfo(m_database.databasePath()).absolutePath()).filePath(QStringLiteral("backups"));
    const QString before = QDir(backupDirectory).filePath(QStringLiteral("before-restore-%1-%2.sqlite")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")), QUuid::createUuid().toString(QUuid::Id128)));
    if (!QDir().mkpath(backupDirectory) || !backup(before, error)) {
        if (error.isEmpty()) error = QStringLiteral("无法保存恢复前的自动备份。");
        connection.rollback(); detach(); return false;
    }
    safetyBackup = before;
    QStringList statements = {
        QStringLiteral("DELETE FROM main.savings_records"), QStringLiteral("DELETE FROM main.wish_pools"), QStringLiteral("DELETE FROM main.wishlist"), QStringLiteral("DELETE FROM main.monthly_budgets"), QStringLiteral("DELETE FROM main.transactions"),
        QStringLiteral("INSERT INTO main.transactions(id,description,amount_cents,category,date,occurred_time,note,detail_category,source_image,purpose_category,budget_scope) SELECT id,description,amount_cents,category,date,occurred_time,note,detail_category,source_image,purpose_category,budget_scope FROM kura_restore.transactions"),
        QStringLiteral("INSERT INTO main.monthly_budgets(month,limit_cents,basic_limit_cents,discretionary_limit_cents) SELECT month,limit_cents,basic_limit_cents,discretionary_limit_cents FROM kura_restore.monthly_budgets"),
        QStringLiteral("INSERT INTO main.wishlist(id,name,estimated_cents,category,cooling_days,reason,created_ms,ready_ms,status,closed_ms,transaction_id,priority,detail_category,source_image) SELECT id,name,estimated_cents,category,cooling_days,reason,created_ms,ready_ms,status,closed_ms,transaction_id,priority,detail_category,source_image FROM kura_restore.wishlist"),
        QStringLiteral("INSERT INTO main.wish_pools(id,wish_id,name,target_cents,created_ms,closed_ms) SELECT id,wish_id,name,target_cents,created_ms,closed_ms FROM kura_restore.wish_pools"),
        QStringLiteral("INSERT INTO main.savings_records(id,pool_id,amount_cents,description,created_ms,request_token) SELECT id,pool_id,amount_cents,description,created_ms,request_token FROM kura_restore.savings_records"),
        QStringLiteral("DELETE FROM main.sqlite_sequence WHERE name IN ('transactions','wishlist','wish_pools','savings_records','shopping_rules','brands','execution_sessions','review_tasks','price_tracks','price_records','bill_import_keys','transaction_reviews','monthly_stories','product_profiles','product_links','product_evidence')"),
        QStringLiteral("INSERT INTO main.sqlite_sequence(name,seq) SELECT name,seq FROM kura_restore.sqlite_sequence WHERE name IN ('transactions','wishlist','wish_pools','savings_records','shopping_rules','brands','execution_sessions','review_tasks','price_tracks','price_records','bill_import_keys','transaction_reviews','monthly_stories','product_profiles','product_links','product_evidence')")};
    QStringList workflowDeletes,workflowInserts;
    workflowDeletes << "DELETE FROM main.execution_wishes";
    workflowDeletes << "DELETE FROM main.achievement_marks";
    workflowDeletes << "DELETE FROM main.user_categories";
    statements[statements.size()-2].replace("'monthly_stories'","'monthly_stories','achievement_marks','user_categories'");
    statements[statements.size()-1].replace("'monthly_stories'","'monthly_stories','achievement_marks','user_categories'");
    for (const auto &name:{QStringLiteral("wishlist_order"),QStringLiteral("wishlist_price_links"),QStringLiteral("product_evidence"),QStringLiteral("product_links"),QStringLiteral("product_profiles"),QStringLiteral("bill_import_keys"),QStringLiteral("transaction_reviews"),QStringLiteral("monthly_stories"),QStringLiteral("review_tasks"),QStringLiteral("price_records"),QStringLiteral("execution_sessions"),QStringLiteral("brands"),QStringLiteral("shopping_rules"),QStringLiteral("price_tracks")}) workflowDeletes << "DELETE FROM main."+name;
    for (const auto &table:WorkflowSchema::tables()+SupplementSchema::tables()+ProductSchema::tables()) { const auto fields=table.columns.join(','); workflowInserts << "INSERT INTO main."+table.name+"("+fields+") SELECT "+fields+" FROM kura_restore."+table.name; }
    // Insert new business rows before restoring all AUTOINCREMENT sequences.
    statements=workflowDeletes+statements.mid(0,statements.size()-2)+workflowInserts+statements.mid(statements.size()-2);
    for (const auto &statement : statements) {
        if (!query.exec(statement)) {
            error = QStringLiteral("恢复失败，原数据已保留：%1").arg(query.lastError().text());
            query.finish(); connection.rollback(); detach(); return false;
        }
        query.finish();
    }
    if (!connection.commit()) {
        error = QStringLiteral("恢复提交失败，原数据已保留：%1").arg(connection.lastError().text());
        connection.rollback(); detach(); return false;
    }
    detach();
    discardPrepared();
    return true;
}
