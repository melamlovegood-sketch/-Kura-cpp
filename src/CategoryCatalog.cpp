#include "CategoryCatalog.h"
namespace {
struct Entry { const char *key; const char *label; const char *legacy; bool basic; };
const Entry entries[] = {
    {"canteen", "食堂", "餐饮", true}, {"transport", "交通", "交通", true},
    {"daily_supplies", "日用物资", "购物", true}, {"subscription", "订阅", "其他", true},
    {"daily", "日常", "餐饮", false}, {"online_shopping", "网购", "购物", false},
    {"entertainment", "娱乐", "娱乐", false}, {"other", "其他", "其他", false},
    {"study", "学习（旧分类）", "学习", false}
};
}
QStringList CategoryCatalog::keys() { QStringList result; for (const auto &e : entries) result << QLatin1String(e.key); return result; }
QString CategoryCatalog::label(const QString &key) { if (validPurpose(key)) return purposeLabel(key); for (const auto &e : entries) if (key == QLatin1String(e.key)) return QString::fromUtf8(e.label); return {}; }
QString CategoryCatalog::legacyCategory(const QString &key) { for (const auto &e : entries) if (key == QLatin1String(e.key)) return QString::fromUtf8(e.legacy); return {}; }
bool CategoryCatalog::isBasic(const QString &key) { for (const auto &e : entries) if (key == QLatin1String(e.key)) return e.basic; return false; }
bool CategoryCatalog::isValid(const QString &key) { return keys().contains(key); }
QString CategoryCatalog::fromLegacy(const QString &category)
{
    for (const QString &key : {QStringLiteral("daily"), QStringLiteral("transport"), QStringLiteral("online_shopping"),
                              QStringLiteral("entertainment"), QStringLiteral("study"), QStringLiteral("other")})
        if (legacyCategory(key) == category) return key;
    return {};
}

QStringList CategoryCatalog::purposeKeys() { return {"food","fun","clothing","travel","other","unclassified"}; }
QString CategoryCatalog::purposeLabel(const QString &key) {
    const QStringList labels={QStringLiteral("吃喝"),QStringLiteral("玩乐"),QStringLiteral("服饰鞋包"),QStringLiteral("出行"),QStringLiteral("其他"),QStringLiteral("未分类")};
    const int index=purposeKeys().indexOf(key); return index<0?QString():labels[index];
}
bool CategoryCatalog::validPurpose(const QString &key) { return purposeKeys().contains(key); }
QString CategoryCatalog::purposeFromDetail(const QString &key) {
    if (key=="canteen" || key=="daily") return "food";
    if (key=="transport") return "travel";
    if (key=="entertainment") return "fun";
    // A shopping channel does not establish whether a purchase was clothing.
    if (key=="online_shopping") return "unclassified";
    return "other";
}
QString CategoryCatalog::detailForPurpose(const QString &key) {
    if (key=="food") return "daily";
    if (key=="travel") return "transport";
    if (key=="fun") return "entertainment";
    if (key=="clothing") return "online_shopping";
    return "other";
}
QString CategoryCatalog::scopeLabel(const QString &scope) { return scope=="basic"?QStringLiteral("必要生活"):QStringLiteral("自由消费"); }
QString CategoryCatalog::basicExpenseSql(const QString &p) {
    return "("+p+"budget_scope='basic' OR ("+p+"budget_scope='' AND ("+p+"detail_category IN ('canteen','transport','daily_supplies','subscription') OR ("+p+"detail_category='' AND "+p+"category='交通'))))";
}
