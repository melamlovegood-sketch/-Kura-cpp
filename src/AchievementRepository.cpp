#include "AchievementRepository.h"
#include "WorkflowStorage.h"
#include <QDate>
#include <QMap>
#include <QSet>
#include <QTimeZone>
#include <QVariant>

QList<Achievement> AchievementRepository::definitions()
{
    return {
        {"first",QStringLiteral("第一颗栗子"),QStringLiteral("整理小窝"),QStringLiteral("保存第一笔有效支出。"),QStringLiteral("小小一颗，是认真生活的开始。")},
        {"seven",QStringLiteral("小窝有账"),QStringLiteral("整理小窝"),QStringLiteral("累计 7 个不同日期有支出记录，不要求连续。"),QStringLiteral("慢慢整理，小窝就有了秩序。"),7},
        {"thirty",QStringLiteral("收藏成习"),QStringLiteral("整理小窝"),QStringLiteral("累计 30 个不同日期有支出记录，不要求连续。"),QStringLiteral("日子一颗颗，藏成了自己的习惯。"),30},
        {"stash",QStringLiteral("先藏再想"),QStringLiteral("给冲动降温"),QStringLiteral("第一次将心动商品加入待购清单。"),QStringLiteral("喜欢可以先藏着，答案晚点再说。")},
        {"wait",QStringLiteral("慢慢挑栗"),QStringLiteral("给冲动降温"),QStringLiteral("一件商品的冷静期结束后，再确认购买或放弃。"),QStringLiteral("等一等，才听得清自己想要什么。")},
        {"letgo",QStringLiteral("轻装回窝"),QStringLiteral("给冲动降温"),QStringLiteral("对 5 件不同商品确认不买或放弃；同一商品重复操作不累加，历史归档不计入。"),QStringLiteral("放下一些，才有位置装真正喜欢的。"),5},
        {"worth",QStringLiteral("这颗真香"),QStringLiteral("认真尝栗子"),QStringLiteral("首次将一笔消费标为“值得”，并写下理由。"),QStringLiteral("有些栗子，就是值得好好享用。")},
        {"regret",QStringLiteral("酸栗也有收获"),QStringLiteral("认真尝栗子"),QStringLiteral("首次将一笔消费标为“后悔”，并写下原因。"),QStringLiteral("尝过酸涩，也更懂自己的口味。")},
        {"review",QStringLiteral("越嚼越明白"),QStringLiteral("认真尝栗子"),QStringLiteral("为 10 笔不同消费标记值得或后悔，并留下复盘文字；修改同一笔不重复计数。"),QStringLiteral("每次回头看看，都更懂自己一点。"),10},
        {"wish",QStringLiteral("树洞里的心愿"),QStringLiteral("为喜欢留位置"),QStringLiteral("第一次将清单中的商品设为许愿目标。"),QStringLiteral("把最想要的，安放在心里的树洞。")},
        {"budget",QStringLiteral("给明天留一颗"),QStringLiteral("为喜欢留位置"),QStringLiteral("保存首份月度预算，自由消费额度大于零。"),QStringLiteral("今天好好生活，也为明天留些余地。")},
        {"principle",QStringLiteral("我的选栗法"),QStringLiteral("为喜欢留位置"),QStringLiteral("第一次保存非空的个人购物原则。"),QStringLiteral("什么值得带回窝，由你自己决定。")}
    };
}

bool AchievementRepository::load(QList<Achievement> &achievements)
{
    m_error.clear();
    auto db=m_database.database();
    if (!db.isOpen()) { m_error=QStringLiteral("数据库未打开，暂时无法读取收藏。和消费记录一样，成就只保存在本机。"); return false; }
    WorkflowStorage::Transaction transaction(db,m_error); if (!transaction.ok()) return false;
    auto loaded=definitions();
    QSqlQuery query(db);
    QMap<QString,QSet<QString>> evidence;
    QMap<QString,qint64> observed;
    if (!query.exec("SELECT badge_key,fact_key,observed_ms FROM achievement_marks ORDER BY id")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) {
        const auto key=query.value(0).toString();
        evidence[key].insert(query.value(1).toString());
        observed[key]=qMax(observed.value(key),query.value(2).toLongLong());
    }
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    const QString notArchived="NOT EXISTS(SELECT 1 FROM wishlist_price_links l WHERE l.wish_id=w.id AND l.legacy_archived=1)";
    const QString days="SELECT DISTINCT date AS fact_key FROM transactions WHERE amount_cents>0 AND date<=:today";
    const QString reviewed="FROM transaction_reviews r JOIN transactions t ON t.id=r.transaction_id WHERE r.tag IN (-1,1) AND length(trim(r.note))>0";
    // Normalize product names across decision sessions and wishlist entries. A product's
    // re-added list entry or repeated decision must not create another rejected item.
    const QString rejected="SELECT lower(trim(name)) AS fact_key FROM execution_sessions WHERE state='skipped' "
        "UNION SELECT lower(trim(w.name)) FROM wishlist w WHERE w.status=2 AND "+notArchived+
        // The homepage's direct 'not buying' path stores a saving with an optional
        // reason after this separator; the reason must not create another product.
        " UNION SELECT lower(trim(CASE WHEN instr(description,' · ')>0 THEN substr(description,1,instr(description,' · ')-1) ELSE description END)) FROM savings_records";
    const QMap<QString,QString> sources={
        {"first","SELECT 'once' AS fact_key FROM transactions WHERE amount_cents>0 LIMIT 1"},
        {"seven",days},{"thirty",days},
        {"stash","SELECT 'once' AS fact_key FROM wishlist w WHERE "+notArchived+" LIMIT 1"},
        {"wait","SELECT 'once' AS fact_key FROM wishlist w WHERE w.status IN (1,2) AND w.ready_ms>w.created_ms AND w.closed_ms>=w.ready_ms AND "+notArchived+" LIMIT 1"},
        {"letgo",rejected},
        {"worth","SELECT 'once' AS fact_key "+reviewed+" AND r.tag=1 LIMIT 1"},
        {"regret","SELECT 'once' AS fact_key "+reviewed+" AND r.tag=-1 LIMIT 1"},
        {"review","SELECT CAST(r.transaction_id AS TEXT) AS fact_key "+reviewed},
        {"wish","SELECT 'once' AS fact_key FROM wish_pools LIMIT 1"},
        {"budget","SELECT 'once' AS fact_key FROM monthly_budgets WHERE COALESCE(discretionary_limit_cents,limit_cents)>0 LIMIT 1"},
        {"principle","SELECT 'once' AS fact_key FROM shopping_rules WHERE length(trim(content))>0 LIMIT 1"}
    };
    const qint64 now=QDateTime::currentMSecsSinceEpoch();
    for (auto &badge:loaded) {
        auto &facts=evidence[badge.key];
        if (facts.size()<badge.target) {
            // Only return previously unseen evidence and never store beyond the goal.
            query.prepare("SELECT fact_key FROM ("+sources.value(badge.key)+") s WHERE length(fact_key) BETWEEN 1 AND 256 AND NOT EXISTS(SELECT 1 FROM achievement_marks m WHERE m.badge_key=:badge AND m.fact_key=s.fact_key) ORDER BY fact_key LIMIT :remaining");
            query.bindValue(":badge",badge.key); query.bindValue(":remaining",badge.target-facts.size());
            if (badge.key=="seven" || badge.key=="thirty") query.bindValue(":today",QDate::currentDate().toString(Qt::ISODate));
            if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
            QStringList fresh;
            while (query.next()) fresh << query.value(0).toString();
            if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
            for (const auto &fact:fresh) {
                query.prepare("INSERT INTO achievement_marks(badge_key,fact_key,observed_ms) VALUES(:badge,:fact,:now)");
                query.bindValue(":badge",badge.key); query.bindValue(":fact",fact); query.bindValue(":now",now);
                if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
                facts.insert(fact); observed[badge.key]=now;
            }
        }
        badge.progress=qMin(badge.target,int(facts.size()));
        if (badge.unlocked()) badge.unlockedAt=QDateTime::fromMSecsSinceEpoch(observed.value(badge.key),QTimeZone::UTC);
    }
    if (!transaction.commit()) return false;
    achievements=loaded; return true;
}
