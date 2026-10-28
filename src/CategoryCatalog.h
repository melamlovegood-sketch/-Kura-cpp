#pragma once
#include <QString>
#include <QStringList>
namespace CategoryCatalog {
QStringList keys();
QString label(const QString &key);
QString legacyCategory(const QString &key);
QString fromLegacy(const QString &category);
bool isBasic(const QString &key);
bool isValid(const QString &key);
}
