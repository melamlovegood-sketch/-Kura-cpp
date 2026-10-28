#include "DemoData.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "BudgetRepository.h"
#include "WishlistRepository.h"
#include "WishPoolRepository.h"
#include "CategoryCatalog.h"
#include "ShoppingProfileRepository.h"
#include "ReviewRepository.h"
#include "PriceRepository.h"
#include "MonthlyReviewRepository.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool DemoData::populate(DatabaseManager &database, QString &error, const QDateTime &now)
{
    error.clear();
    if (!now.isValid() || !database.database().isOpen()) { error = QStringLiteral("演示数据库或时间不可用。"); return false; }
    QSqlQuery count(database.database());
    if (!count.exec(QStringLiteral("SELECT (SELECT COUNT(*) FROM transactions)+(SELECT COUNT(*) FROM monthly_budgets)+(SELECT COUNT(*) FROM wishlist)+(SELECT COUNT(*) FROM wish_pools)+(SELECT COUNT(*) FROM savings_records)+(SELECT COUNT(*) FROM shopping_rules)+(SELECT COUNT(*) FROM brands)+(SELECT COUNT(*) FROM execution_sessions)+(SELECT COUNT(*) FROM review_tasks)+(SELECT COUNT(*) FROM price_tracks)+(SELECT COUNT(*) FROM price_records)+(SELECT COUNT(*) FROM bill_import_keys)+(SELECT COUNT(*) FROM transaction_reviews)+(SELECT COUNT(*) FROM monthly_stories)"))
        || !count.next() || count.value(0).toLongLong() != 0) {
        error = QStringLiteral("演示数据只可写入空数据库。"); return false;
    }
    count.finish();
    const QDate today = now.toLocalTime().date();
    const QDate month(today.year(), today.month(), 1);
    TransactionRepository transactions(database);
    const auto add = [&](const QString &name, qint64 cents, const QString &category, const QDate &date) {
        Transaction record(0, name, cents, category, date, QTime(12, 30), QStringLiteral("虚构演示记录\n可修改或删除，用于课堂验收。"),name==QStringLiteral("早餐")||name==QStringLiteral("午饭")?QStringLiteral("canteen"):CategoryCatalog::fromLegacy(category));
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
    if (!budgets.saveSplit(month,10000,20000) || !budgets.save(MonthlyBudget(month.addMonths(-1),20000))) {
        error = budgets.lastError(); return false;
    }
    WishlistRepository wishes(database);
    qint64 goalId=0;
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
        if (i==1) goalId=wish.id;
        if (i == 2 && !wishes.abandon(wish.id,now)) { error = wishes.lastError(); return false; }
        if (i == 3) {
            Transaction purchase(0,wish.name,5900,wish.category,today,QTime(10,15),QStringLiteral("演示购买历史，实际价格与预计价格不同。"));
            if (!wishes.purchase(wish.id,purchase,now)) { error = wishes.lastError(); return false; }
        }
    }
    WishPoolRepository pool(database);
    if (!pool.setGoal(goalId) || !pool.addSaving(4500,QStringLiteral("忍住了：一次冲动外卖"),QStringLiteral("demo-saving"))) { error=pool.lastError(); return false; }
    ShoppingProfileRepository profile(database);
    if (!profile.saveRules({QStringLiteral("先确认实际用途，再比较价格。"),QStringLiteral("高金额购买先比较参数和退换条件。" )})) { error=profile.lastError(); return false; }
    BrandEntry digital; digital.name=QStringLiteral("演示数码品牌"); digital.category="online_shopping"; digital.note=QStringLiteral("虚构品牌，仅用于演示。"); digital.weight=3;
    BrandEntry daily; daily.name=QStringLiteral("演示日常品牌"); daily.category="other"; daily.note=QStringLiteral("虚构品牌，仅用于演示。"); daily.weight=2;
    if (!profile.saveBrand(digital) || !profile.saveBrand(daily)) { error=profile.lastError(); return false; }
    QList<Transaction> all; if (!transactions.findAll(all)) { error=transactions.lastError(); return false; }
    ReviewRepository reviews(database);
    for (const auto &record:all) if (record.description()==QStringLiteral("上一年消费")) { if (!reviews.schedule(record.id(),daily.id)) { error=reviews.lastError(); return false; } break; }
    PriceRepository prices(database); qint64 trackId=0;
    if (!prices.intake(QStringLiteral("机械键盘"),"online_shopping",32900,QStringLiteral("虚构演示价格"),"demo-price-one",trackId,now.addDays(-7))
        || !prices.record(trackId,27900,QStringLiteral("虚构演示券后价"),"demo-price-two",now)
        || !prices.configure(trackId,27900,{})) { error=prices.lastError(); return false; }
    MonthlyReviewRepository monthly(database);
    for (const auto &record:all) if (record.date().year()==today.year() && record.date().month()==today.month()) {
        if (record.description()==QStringLiteral("教材") && !monthly.setTag(record.id(),1,QStringLiteral("虚构示例：用于本学期课程"))) { error=monthly.lastError(); return false; }
        if (record.description()==QStringLiteral("电影") && !monthly.setTag(record.id(),-1,QStringLiteral("虚构示例：体验不如预期"))) { error=monthly.lastError(); return false; }
    }
    MonthlyReviewData review; if (!monthly.load(month,review) || !monthly.saveStory(month,review.factsHash,review.localStory,review.localPersona,"local")) { error=monthly.lastError(); return false; }
    return true;
}
