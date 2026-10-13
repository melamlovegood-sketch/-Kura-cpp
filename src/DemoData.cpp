#include "DemoData.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "BudgetRepository.h"
#include "WishlistRepository.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool DemoData::populate(DatabaseManager &database, QString &error, const QDateTime &now)
{
    error.clear();
    if (!now.isValid() || !database.database().isOpen()) { error = QStringLiteral("演示数据库或时间不可用。"); return false; }
    QSqlQuery count(database.database());
    if (!count.exec(QStringLiteral("SELECT (SELECT COUNT(*) FROM transactions)+(SELECT COUNT(*) FROM monthly_budgets)+(SELECT COUNT(*) FROM wishlist)"))
        || !count.next() || count.value(0).toLongLong() != 0) {
        error = QStringLiteral("演示数据只可写入空数据库。"); return false;
    }
    count.finish();
    const QDate today = now.toLocalTime().date();
    const QDate month(today.year(), today.month(), 1);
    TransactionRepository transactions(database);
    const auto add = [&](const QString &name, qint64 cents, const QString &category, const QDate &date) {
        Transaction record(0, name, cents, category, date, QTime(12, 30), QStringLiteral("虚构演示记录\n可修改或删除，用于课堂验收。"));
        if (transactions.add(record)) return true;
        error = transactions.lastError(); return false;
    };
    const QList<Transaction> current = {
        Transaction(0, QStringLiteral("早餐"),1800,QStringLiteral("餐饮"),today),
        Transaction(0, QStringLiteral("午饭"),3200,QStringLiteral("餐饮"),today),
        Transaction(0, QStringLiteral("地铁"),600,QStringLiteral("交通"),today),
        Transaction(0, QStringLiteral("教材"),6800,QStringLiteral("学习"),today),
        Transaction(0, QStringLiteral("电影"),4500,QStringLiteral("娱乐"),today),
        Transaction(0, QStringLiteral("生活杂项"),900,QStringLiteral("其他"),today)};
    for (const auto &record : current)
        if (!add(record.description(),record.amountCents(),record.category(),today)) return false;
    for (int m = 1; m < today.month(); ++m)
        if (!add(QStringLiteral("历史月度消费"),8900 + 300 * m,QStringLiteral("购物"),QDate(today.year(),m,15))) return false;
    if (!add(QStringLiteral("上一年消费"),9900,QStringLiteral("其他"),QDate(today.year()-1,12,31))) return false;
    BudgetRepository budgets(database);
    if (!budgets.save(MonthlyBudget(month,30000)) || !budgets.save(MonthlyBudget(month.addMonths(-1),20000))) {
        error = budgets.lastError(); return false;
    }
    WishlistRepository wishes(database);
    for (int i = 0; i < 4; ++i) {
        WishItem wish;
        wish.name = QStringList{QStringLiteral("蓝牙耳机"),QStringLiteral("机械键盘"),QStringLiteral("限量摆件"),QStringLiteral("专业书")}[i];
        wish.estimatedCents = QList<qint64>{32990,29900,4900,6800}[i];
        wish.priority = i == 1 ? 1 : i == 2 ? 3 : 2;
        wish.reason = QStringLiteral("虚构演示愿望，可购买、放弃或修改。");
        wish.coolingDays = i == 3 ? 1 : 3;
        wish.category = i == 3 ? QStringLiteral("学习") : QStringLiteral("购物");
        const auto created = i == 1 ? now : now.addDays(i == 0 ? -4 : i == 2 ? -1 : -2);
        if (!wishes.add(wish,created)) { error = wishes.lastError(); return false; }
        if (i == 2 && !wishes.abandon(wish.id,now)) { error = wishes.lastError(); return false; }
        if (i == 3) {
            Transaction purchase(0,wish.name,5900,wish.category,today,QTime(10,15),QStringLiteral("演示购买历史，实际价格与预计价格不同。"));
            if (!wishes.purchase(wish.id,purchase,now)) { error = wishes.lastError(); return false; }
        }
    }
    return true;
}
