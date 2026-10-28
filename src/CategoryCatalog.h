#pragma once
#include <QString>
#include <QStringList>
namespace CategoryCatalog {
QStringList keys();
QStringList defaultKeys();
QString productKey(const QString &key);
bool isCustom(const QString &key);
QString customKey(const QString &name);
QString validSql(const QString &column, bool purpose=false);
QString label(const QString &key);
QString legacyCategory(const QString &key);
QString fromLegacy(const QString &category);
bool isBasic(const QString &key);
bool isValid(const QString &key);
// Expense purpose and budget scope are independent of the legacy product catalog.
QStringList purposeKeys();
QString purposeLabel(const QString &key);
bool validPurpose(const QString &key);
QString purposeFromDetail(const QString &key);
QString detailForPurpose(const QString &key);
QString scopeLabel(const QString &scope);
QString basicExpenseSql(const QString &prefix = {});
}
