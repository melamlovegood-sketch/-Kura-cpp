#include "MonthlyReviewRepository.h"
#include "MonthlyStoryTemplate.h"
#include "BudgetRepository.h"
#include "CategoryCatalog.h"
#include "Transaction.h"
#include "WorkflowStorage.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QTimeZone>
#include <QVariant>
#include <limits>

namespace {
bool add(qint64 &sum,qint64 value) { if (value<0 || sum>std::numeric_limits<qint64>::max()-value) return false; sum+=value; return true; }
QString amount(qint64 cents) { return Transaction::formatAmount(cents); }
QJsonObject expenseJson(const MonthlyReviewExpense &row) { return {{"id",QString::number(row.id)},{"name",row.name},{"amount",amount(row.cents)},{"category",CategoryCatalog::purposeLabel(row.purposeCategory)},{"date",row.date.toString(Qt::ISODate)},{"note",row.note}}; }
}
bool MonthlyReviewRepository::load(const QDate &requested,MonthlyReviewData &data)
{
    m_error=MonthlyBudget(requested,0).validationError(); if (!m_error.isEmpty()) return false;
    WorkflowStorage::Transaction snapshot(m_database.database(),m_error); if (!snapshot.ok()) return false;
    MonthlyReviewData loaded; loaded.month=MonthlyBudget::firstDay(requested); for (const auto &key:CategoryCatalog::purposeKeys()) loaded.categories.append({key,0,0,0});
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto hashFields=[&](const QStringList &fields) { for (const auto &field:fields) { const auto bytes=field.toUtf8(); hash.addData(QByteArray::number(bytes.size())+":"+bytes); } };
    const auto collect=[&](const QDate &first,bool current) {
        if (!first.isValid()) { hashFields({"no-previous-month"}); return true; }
        const QDate last(first.year(),first.month(),first.daysInMonth()); QSqlQuery query(m_database.database());
        query.prepare("SELECT t.id,t.description,t.amount_cents,t.category,t.detail_category,t.date,t.occurred_time,COALESCE(r.tag,0),COALESCE(r.note,''),COALESCE(t.note,''),t.purpose_category,t.budget_scope FROM transactions t LEFT JOIN transaction_reviews r ON r.transaction_id=t.id WHERE t.date BETWEEN :first AND :last ORDER BY t.amount_cents DESC,t.id"); query.bindValue(":first",first.toString(Qt::ISODate)); query.bindValue(":last",last.toString(Qt::ISODate));
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
        while (query.next()) {
            MonthlyReviewExpense expense; expense.id=query.value(0).toLongLong(); expense.name=query.value(1).toString(); expense.cents=query.value(2).toLongLong(); expense.category=query.value(4).toString().isEmpty()?CategoryCatalog::fromLegacy(query.value(3).toString()):query.value(4).toString(); expense.date=QDate::fromString(query.value(5).toString(),Qt::ISODate); const auto time=query.value(6).toString(); expense.time=QTime::fromString(time,"HH:mm:ss"); expense.tag=query.value(7).toInt(); expense.note=query.value(8).toString();
            if (expense.id<=0 || expense.name.trimmed().isEmpty() || expense.name.size()>100 || expense.cents<1 || expense.cents>Transaction::MaximumAmountCents || !CategoryCatalog::isValid(expense.category) || !expense.date.isValid() || (!time.isEmpty() && !expense.time.isValid()) || (expense.tag!=-1 && expense.tag!=0 && expense.tag!=1)) { m_error=QStringLiteral("月度消费数据无效。"); return false; }
            expense.purchaseNote=query.value(9).toString();
            Transaction record(expense.id,expense.name,expense.cents,query.value(3).toString(),expense.date,expense.time,expense.purchaseNote,query.value(4).toString(),{},query.value(10).toString(),query.value(11).toString());
            if (!record.validationError().isEmpty()) { m_error=record.validationError(); return false; }
            expense.purposeCategory=record.purposeCategory(); expense.budgetScope=record.budgetScope();
            hashFields({record.purposeCategory(),record.budgetScope()});
            hashFields({current?"current":"previous",QString::number(expense.id),expense.name,QString::number(expense.cents),expense.category,expense.date.toString(Qt::ISODate),time,QString::number(expense.tag),expense.note,expense.purchaseNote});
            if (current) { if (!add(loaded.totalCents,expense.cents) || !add(record.isBasic()?loaded.basicCents:loaded.discretionaryCents,expense.cents)) { m_error=QStringLiteral("月度金额超过支持范围。"); return false; } ++loaded.count; if (expense.tag==1) ++loaded.worthCount; if (expense.tag==-1) ++loaded.regretCount; loaded.expenses << expense; }
            else if (!add(loaded.previousCents,expense.cents)) { m_error=QStringLiteral("上月金额超过支持范围。"); return false; }
            for (auto &category:loaded.categories) if (category.key==record.purposeCategory()) { if (!add(current?category.cents:category.previousCents,expense.cents)) { m_error=QStringLiteral("分类金额超过支持范围。"); return false; } if (current) ++category.count; break; }
        }
        if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } return true;
    };
    if (!collect(loaded.month,true) || !collect(loaded.month.addMonths(-1),false)) return false;
    const auto firstTime=QDateTime(loaded.month,QTime(0,0),QTimeZone::systemTimeZone()).toUTC(); const auto lastTime=QDateTime(QDate(loaded.month.year(),loaded.month.month(),loaded.month.daysInMonth()),QTime(23,59,59,999),QTimeZone::systemTimeZone()).toUTC();
    QSqlQuery query(m_database.database()); query.prepare("SELECT id,amount_cents,description,created_ms FROM savings_records WHERE created_ms BETWEEN :first AND :last ORDER BY id"); query.bindValue(":first",firstTime.toMSecsSinceEpoch()); query.bindValue(":last",lastTime.toMSecsSinceEpoch());
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) { const qint64 cents=query.value(1).toLongLong(); if (!add(loaded.savingsCents,cents)) { m_error=QStringLiteral("忍住金额超过支持范围。"); return false; } ++loaded.savingsCount; if (cents>loaded.biggestSavingCents) { loaded.biggestSavingCents=cents; loaded.biggestSaving=query.value(2).toString(); } hashFields({"saving",query.value(0).toString(),QString::number(cents),query.value(2).toString(),query.value(3).toString()}); }
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    query.prepare("SELECT w.id,w.name,w.closed_ms FROM wishlist w WHERE w.status=2 AND w.closed_ms BETWEEN :first AND :last AND NOT EXISTS(SELECT 1 FROM wishlist_price_links l WHERE l.wish_id=w.id AND l.legacy_archived=1) ORDER BY w.id"); query.bindValue(":first",firstTime.toMSecsSinceEpoch()); query.bindValue(":last",lastTime.toMSecsSinceEpoch());
    if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } while (query.next()) { ++loaded.abandonedCount; hashFields({"abandoned",query.value(0).toString(),query.value(1).toString(),query.value(2).toString()}); } if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    BudgetRepository budgets(m_database); if (!budgets.loadStatus(loaded.month,loaded.budget)) { m_error=budgets.lastError(); return false; }
    if (loaded.budget.spentCents!=loaded.totalCents || loaded.budget.basicSpentCents!=loaded.basicCents || loaded.budget.discretionarySpentCents!=loaded.discretionaryCents) { m_error=QStringLiteral("月度预算与分类统计不一致，请检查数据。"); return false; }
    QJsonArray categories,worth,regret;
    const MonthlyCategory *top=nullptr;
    for (const auto &category:loaded.categories) { categories.append(QJsonObject{{"category",CategoryCatalog::label(category.key)},{"amount",amount(category.cents)},{"previous_amount",amount(category.previousCents)},{"count",QString::number(category.count)}}); if (category.cents>0 && (!top || category.cents>top->cents)) top=&category; }
    for (const auto &expense:loaded.expenses) { if (expense.tag==1 && worth.size()<5) worth.append(expenseJson(expense)); if (expense.tag==-1 && regret.size()<5) regret.append(expenseJson(expense)); }
    QJsonObject facts{{"month",loaded.month.toString("yyyy-MM")},{"expense_count",QString::number(loaded.count)},{"recorded_expenses",amount(loaded.totalCents)},{"basic_expenses",amount(loaded.basicCents)},{"discretionary_expenses",amount(loaded.discretionaryCents)},{"previous_month_expenses",amount(loaded.previousCents)},{"savings_count",QString::number(loaded.savingsCount)},{"resisted_amount",amount(loaded.savingsCents)},{"abandoned_count",QString::number(loaded.abandonedCount)},{"worth_count",QString::number(loaded.worthCount)},{"regret_count",QString::number(loaded.regretCount)},{"categories",categories},{"worth",worth},{"regret",regret},{"budget_set",loaded.budget.hasBudget},{"split_budget",loaded.budget.hasSplit},{"budget_total",loaded.budget.hasBudget?amount(loaded.budget.limitCents):QStringLiteral("未设置")},{"budget_remaining",loaded.budget.hasBudget?amount(loaded.budget.remainingCents()):QStringLiteral("未知")},{"basic_limit",loaded.budget.hasSplit?amount(loaded.budget.basicLimitCents):QStringLiteral("未分配")},{"discretionary_limit",loaded.budget.hasSplit?amount(loaded.budget.discretionaryLimitCents):QStringLiteral("未分配")}};
    if (!loaded.expenses.isEmpty()) facts.insert("largest_expense",expenseJson(loaded.expenses.first())); if (loaded.biggestSavingCents) facts.insert("largest_resist",QJsonObject{{"name",loaded.biggestSaving},{"amount",amount(loaded.biggestSavingCents)}});
    MonthlyStoryTemplate::addContext(facts,loaded);
    loaded.factsJson=QString::fromUtf8(QJsonDocument(facts).toJson(QJsonDocument::Compact)); hashFields({loaded.factsJson}); loaded.factsHash=QString::fromLatin1(hash.result().toHex());
    loaded.localPersona=top?QStringLiteral("本月记录中，%1支出最多，占 %2%。这是当月分布，不是固定人格标签。").arg(CategoryCatalog::label(top->key),QString::number(double(top->cents)/loaded.totalCents*100,'f',1)):QStringLiteral("本月暂无支出记录，不据此判断消费偏好。");
    if (!loaded.hasData()) loaded.localStory=QStringLiteral("这个月还没有保存消费、忍住或放弃购买记录。添加数据后，可以再回来回顾。");
    else { loaded.localStory=QStringLiteral("%1，已保存 %2 笔支出，共 %3。必要生活 %4，自由消费 %5。").arg(loaded.month.toString(QStringLiteral("yyyy 年 M 月"))).arg(loaded.count).arg(amount(loaded.totalCents),amount(loaded.basicCents),amount(loaded.discretionaryCents));
        loaded.localStory+=loaded.previousCents>0?QStringLiteral("与上月相比%1 %2。").arg(loaded.totalCents>=loaded.previousCents?QStringLiteral("增加"):QStringLiteral("减少"),amount(qAbs(loaded.totalCents-loaded.previousCents))):QStringLiteral("上月没有支出记录，不计算变化比率。");
        loaded.localStory+=QStringLiteral("本月记录忍住 %1 次、%2，另有 %3 次放弃购买；忍住金额不代表实际收入。").arg(loaded.savingsCount).arg(amount(loaded.savingsCents)).arg(loaded.abandonedCount);
        loaded.localStory+=QStringLiteral("你标记了 %1 笔值得、%2 笔后悔。").arg(loaded.worthCount).arg(loaded.regretCount);
    }
    if (!snapshot.commit()) return false; data=loaded; m_error.clear(); return true;
}
bool MonthlyReviewRepository::loadStory(const MonthlyReviewData &data,MonthlyStory &story)
{
    m_error.clear(); if (!m_database.database().isOpen()) { m_error=QStringLiteral("数据库未打开。"); return false; }
    QSqlQuery query(m_database.database()); query.prepare("SELECT id,facts_hash,facts_json,story,persona,source,generated_ms FROM monthly_stories WHERE month=:month"); query.bindValue(":month",data.month.toString("yyyy-MM")); if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    MonthlyStory loaded; loaded.month=data.month;
    if (query.next()) { loaded.id=query.value(0).toLongLong(); loaded.factsHash=query.value(1).toString(); loaded.factsJson=query.value(2).toString(); loaded.story=query.value(3).toString(); loaded.persona=query.value(4).toString(); loaded.source=query.value(5).toString(); loaded.generatedAt=QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong(),Qt::UTC); loaded.stale=loaded.factsHash!=data.factsHash; }
    story=loaded; return true;
}
bool MonthlyReviewRepository::saveStory(const QDate &month,const QString &expectedHash,const QString &story,const QString &persona,const QString &source)
{
    m_error.clear(); if (story.trimmed().isEmpty() || story.size()>12000 || persona.size()>1200 || (source!="local" && source!="ai")) { m_error=QStringLiteral("故事或画像内容无效／过长。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    MonthlyReviewData current; if (!load(month,current)) return false;
    if (current.factsHash!=expectedHash) { m_error=QStringLiteral("生成期间数据已变化，本次结果未保存，请刷新后重新生成。"); return false; }
    if (story.trimmed().startsWith('{')) {
        ReviewStoryContent content;
        if (!MonthlyStoryTemplate::decode(story,content,m_error) || !MonthlyStoryTemplate::matchesFacts(content,QJsonDocument::fromJson(current.factsJson.toUtf8()).object(),m_error)) return false;
    }
    QSqlQuery query(m_database.database()); query.prepare("INSERT INTO monthly_stories(month,facts_hash,facts_json,story,persona,source,generated_ms) VALUES(:month,:hash,:facts,:story,:persona,:source,:now) ON CONFLICT(month) DO UPDATE SET facts_hash=excluded.facts_hash,facts_json=excluded.facts_json,story=excluded.story,persona=excluded.persona,source=excluded.source,generated_ms=excluded.generated_ms");
    query.bindValue(":month",current.month.toString("yyyy-MM")); query.bindValue(":hash",current.factsHash); query.bindValue(":facts",current.factsJson); query.bindValue(":story",story.trimmed()); query.bindValue(":persona",persona.isNull()?QStringLiteral(""):persona); query.bindValue(":source",source); query.bindValue(":now",QDateTime::currentMSecsSinceEpoch()); if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish(); return transaction.commit();
}
bool MonthlyReviewRepository::setTag(qint64 id,int tag,const QString &note)
{
    m_error.clear(); if (id<=0 || (tag!=-1 && tag!=0 && tag!=1) || note.size()>500) { m_error=QStringLiteral("请选择有效标记，备注最多 500 字。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false; QSqlQuery query(m_database.database()); query.prepare("SELECT id FROM transactions WHERE id=:id"); query.bindValue(":id",id); if (!query.exec() || !query.next()) { m_error=QStringLiteral("消费已不存在，请刷新后重试。"); return false; } query.finish();
    query.prepare(tag==0?"DELETE FROM transaction_reviews WHERE transaction_id=:id":"INSERT INTO transaction_reviews(transaction_id,tag,note,updated_ms) VALUES(:id,:tag,:note,:now) ON CONFLICT(transaction_id) DO UPDATE SET tag=excluded.tag,note=excluded.note,updated_ms=excluded.updated_ms"); query.bindValue(":id",id); if (tag) { query.bindValue(":tag",tag); query.bindValue(":note",note.trimmed().isNull()?QStringLiteral(""):note.trimmed()); query.bindValue(":now",QDateTime::currentMSecsSinceEpoch()); } if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish(); return transaction.commit();
}
