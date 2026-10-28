#include "BudgetRepository.h"
#include "DatabaseManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

BudgetRepository::BudgetRepository(DatabaseManager &database) : m_database(database)
{
}

bool BudgetRepository::save(const MonthlyBudget &budget)
{
    m_lastError = budget.validationError();
    if (!m_lastError.isEmpty())
        return false;
    auto connection = m_database.database();
    if (!connection.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，无法保存预算。");
        return false;
    }
    QSqlQuery query(connection);
    if (!query.prepare(QStringLiteral(
            "INSERT INTO monthly_budgets(month, limit_cents) VALUES (:month, :limit) "
            "ON CONFLICT(month) DO UPDATE SET limit_cents=excluded.limit_cents,basic_limit_cents=NULL,discretionary_limit_cents=NULL"))) {
        m_lastError = QStringLiteral("无法准备预算保存操作：%1").arg(query.lastError().text());
        return false;
    }
    query.bindValue(QStringLiteral(":month"), MonthlyBudget::monthKey(budget.month()));
    query.bindValue(QStringLiteral(":limit"), budget.limitCents());
    if (!query.exec()) {
        const int code = query.lastError().nativeErrorCode().toInt() & 0xff;
        if (code == 5 || code == 6)
            m_lastError = QStringLiteral("数据库正在被占用，请稍后重试。");
        else if (code == 8)
            m_lastError = QStringLiteral("数据库只读，请检查文件和目录权限。");
        else
            m_lastError = QStringLiteral("无法保存预算：%1").arg(query.lastError().text());
        return false;
    }
    m_lastError.clear();
    return true;
}

bool BudgetRepository::loadStatus(const QDate &month, MonthlyBudgetStatus &status)
{
    const MonthlyBudget requested(month, 0);
    m_lastError = requested.validationError();
    if (!m_lastError.isEmpty())
        return false;
    auto connection = m_database.database();
    if (!connection.isOpen()) {
        m_lastError = QStringLiteral("数据库未打开，无法加载预算。");
        return false;
    }
    const QDate first = requested.month();
    const QDate last(first.year(), first.month(), first.daysInMonth());
    QSqlQuery query(connection);
    // Both subqueries use the same read snapshot. Inclusive bounds avoid year-end overflow.
    if (!query.prepare(QStringLiteral(
            "SELECT (SELECT limit_cents FROM monthly_budgets WHERE month=:month), "
            "COALESCE((SELECT SUM(amount_cents) FROM transactions "
            "WHERE date BETWEEN :first AND :last), 0), "
            "(SELECT basic_limit_cents FROM monthly_budgets WHERE month=:month), "
            "(SELECT discretionary_limit_cents FROM monthly_budgets WHERE month=:month), "
            "COALESCE((SELECT SUM(amount_cents) FROM transactions WHERE date BETWEEN :first AND :last "
            "AND (detail_category IN ('canteen','transport','daily_supplies','subscription') OR (detail_category='' AND category='交通'))),0)"))) {
        m_lastError = QStringLiteral("无法准备预算查询：%1").arg(query.lastError().text());
        return false;
    }
    query.bindValue(QStringLiteral(":month"), MonthlyBudget::monthKey(first));
    query.bindValue(QStringLiteral(":first"), first.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":last"), last.toString(Qt::ISODate));
    if (!query.exec() || !query.next()) {
        m_lastError = QStringLiteral("无法加载预算与支出：%1").arg(query.lastError().text());
        return false;
    }
    MonthlyBudgetStatus loaded;
    loaded.month = first;
    loaded.hasBudget = !query.value(0).isNull();
    bool spentOk = false, limitOk = true;
    loaded.spentCents = query.value(1).toLongLong(&spentOk);
    if (loaded.hasBudget)
        loaded.limitCents = query.value(0).toLongLong(&limitOk);
    loaded.hasSplit = !query.value(2).isNull() && !query.value(3).isNull();
    bool basicOk = true, discretionaryOk = true, basicSpentOk = true;
    if (loaded.hasSplit) {
        loaded.basicLimitCents = query.value(2).toLongLong(&basicOk);
        loaded.discretionaryLimitCents = query.value(3).toLongLong(&discretionaryOk);
    }
    loaded.basicSpentCents = query.value(4).toLongLong(&basicSpentOk);
    loaded.discretionarySpentCents = loaded.spentCents - loaded.basicSpentCents;
    if (!spentOk || !limitOk || loaded.spentCents < 0
        || !MonthlyBudget(first, loaded.limitCents).validationError().isEmpty()
        || query.value(2).isNull() != query.value(3).isNull()
        || !basicOk || !discretionaryOk || !basicSpentOk || loaded.basicSpentCents < 0 || loaded.discretionarySpentCents < 0
        || (loaded.hasSplit && (loaded.basicLimitCents < 0 || loaded.discretionaryLimitCents < 0
            || loaded.basicLimitCents > MonthlyBudget::MaximumLimitCents || loaded.discretionaryLimitCents > MonthlyBudget::MaximumLimitCents
            || loaded.basicLimitCents + loaded.discretionaryLimitCents != loaded.limitCents))) {
        m_lastError = QStringLiteral("预算或支出数据无效，请检查数据库文件。");
        return false;
    }
    status = loaded;
    m_lastError.clear();
    return true;
}

QString BudgetRepository::lastError() const { return m_lastError; }

bool BudgetRepository::saveSplit(const QDate &month, qint64 basic, qint64 discretionary)
{
    m_lastError = MonthlyBudget(month, 0).validationError();
    if (basic < 0 || discretionary < 0 || basic > MonthlyBudget::MaximumLimitCents
        || discretionary > MonthlyBudget::MaximumLimitCents || basic + discretionary > MonthlyBudget::MaximumLimitCents)
        m_lastError = QStringLiteral("两项预算须为非负金额，合计不超过 99,999,999.99 元。");
    if (!m_lastError.isEmpty()) return false;
    QSqlQuery query(m_database.database());
    query.prepare(QStringLiteral("INSERT INTO monthly_budgets(month,limit_cents,basic_limit_cents,discretionary_limit_cents) VALUES(:month,:total,:basic,:disc) ON CONFLICT(month) DO UPDATE SET limit_cents=excluded.limit_cents,basic_limit_cents=excluded.basic_limit_cents,discretionary_limit_cents=excluded.discretionary_limit_cents"));
    query.bindValue(":month", MonthlyBudget::monthKey(month)); query.bindValue(":total", basic+discretionary);
    query.bindValue(":basic", basic); query.bindValue(":disc", discretionary);
    if (!query.exec()) {
        const int code = query.lastError().nativeErrorCode().toInt() & 0xff;
        m_lastError = code == 5 || code == 6 ? QStringLiteral("数据库正在被占用，请稍后重试。") : QStringLiteral("预算保存失败：%1").arg(query.lastError().text());
        return false;
    }
    return true;
}
