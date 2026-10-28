#include "CategoryCatalog.h"
#include <QChar>
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
QStringList CategoryCatalog::defaultKeys() { return {"food","fun","clothing","travel","other"}; }
QStringList CategoryCatalog::keys() { auto result=defaultKeys(); for (const auto &e : entries) if(!result.contains(QLatin1String(e.key))) result << QLatin1String(e.key); return result; }
QString CategoryCatalog::label(const QString &key) { if (validPurpose(key)) return purposeLabel(key); for (const auto &e : entries) if (key == QLatin1String(e.key)) return purposeLabel(productKey(key)); return {}; }
QString CategoryCatalog::legacyCategory(const QString &key) {
    for (const auto &e : entries) if (key == QLatin1String(e.key)) return QString::fromUtf8(e.legacy);
    if(key=="food") return QStringLiteral("餐饮");
    if(key=="fun") return QStringLiteral("娱乐");
    if(key=="clothing") return QStringLiteral("购物");
    if(key=="travel") return QStringLiteral("交通");
    return isCustom(key)?QStringLiteral("其他"):QString();
}
bool CategoryCatalog::isBasic(const QString &key) { for (const auto &e : entries) if (key == QLatin1String(e.key)) return e.basic; return false; }
bool CategoryCatalog::isValid(const QString &key) { return keys().contains(key) || isCustom(key); }
QString CategoryCatalog::fromLegacy(const QString &category)
{
    for (const QString &key : {QStringLiteral("daily"), QStringLiteral("transport"), QStringLiteral("online_shopping"),
                              QStringLiteral("entertainment"), QStringLiteral("study"), QStringLiteral("other")})
        if (legacyCategory(key) == category) return key;
    for(const auto &entry:entries)if(category==QString::fromUtf8(entry.label))return QLatin1String(entry.key);
    return {};
}

QStringList CategoryCatalog::purposeKeys() { return {"food","fun","clothing","travel","other","unclassified"}; }
QString CategoryCatalog::purposeLabel(const QString &key) {
    if(isCustom(key)) return key.mid(7);
    const QStringList labels={QStringLiteral("吃喝"),QStringLiteral("玩乐"),QStringLiteral("服饰鞋包"),QStringLiteral("交通"),QStringLiteral("其他"),QStringLiteral("未分类")};
    const int index=purposeKeys().indexOf(key); return index<0?QString():labels[index];
}
bool CategoryCatalog::validPurpose(const QString &key) { return purposeKeys().contains(key) || isCustom(key); }
QString CategoryCatalog::purposeFromDetail(const QString &key) {
    if(validPurpose(key)) return key;
    if (key=="canteen" || key=="daily") return "food";
    if (key=="transport") return "travel";
    if (key=="entertainment") return "fun";
    // A shopping channel does not establish whether a purchase was clothing.
    if (key=="online_shopping") return "unclassified";
    return "other";
}
QString CategoryCatalog::detailForPurpose(const QString &key) {
    if(defaultKeys().contains(key) || isCustom(key)) return key;
    return "other";
}
QString CategoryCatalog::productKey(const QString &key) {
    const auto purpose=purposeFromDetail(key);
    return defaultKeys().contains(purpose) || isCustom(purpose)?purpose:QStringLiteral("other");
}
QString CategoryCatalog::customKey(const QString &name) { return QStringLiteral("custom:")+name.trimmed().normalized(QString::NormalizationForm_C); }
bool CategoryCatalog::isCustom(const QString &key) {
    if(!key.startsWith("custom:")) return false;
    const auto name=key.mid(7);
    if(name.isEmpty() || name.size()>20 || name!=name.trimmed().normalized(QString::NormalizationForm_C)) return false;
    for(const QChar c:name) if(c.isNull() || c.category()==QChar::Other_Control || c.category()==QChar::Other_Format) return false;
    return true;
}
QString CategoryCatalog::validSql(const QString &column,bool purpose) {
    QStringList values; for(const auto &key:purpose?purposeKeys():keys()) values << "'"+key+"'";
    return "("+column+" IN ("+values.join(',')+") OR (substr("+column+",1,7)='custom:' AND length("+column+") BETWEEN 8 AND 27 AND trim(substr("+column+",8))=substr("+column+",8)))";
}
QString CategoryCatalog::scopeLabel(const QString &scope) { return scope=="basic"?QStringLiteral("必要生活"):QStringLiteral("自由消费"); }
QString CategoryCatalog::basicExpenseSql(const QString &p) {
    return "("+p+"budget_scope='basic' OR ("+p+"budget_scope='' AND ("+p+"detail_category IN ('canteen','transport','daily_supplies','subscription') OR ("+p+"detail_category='' AND "+p+"category='交通'))))";
}
