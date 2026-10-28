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
QString CategoryCatalog::label(const QString &key) { for (const auto &e : entries) if (key == QLatin1String(e.key)) return QString::fromUtf8(e.label); return {}; }
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
