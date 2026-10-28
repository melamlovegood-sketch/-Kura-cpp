#pragma once
#include "DatabaseManager.h"
#include <QStringList>
struct BrandEntry { qint64 id=0; QString name,category,note; int weight=0; bool active=true; int worthCount=0,regretCount=0; };
class ShoppingProfileRepository {
public:
    explicit ShoppingProfileRepository(DatabaseManager &database):m_database(database) {}
    bool loadRules(QStringList &rules);
    bool saveRules(const QStringList &rules);
    bool loadBrands(QList<BrandEntry> &brands,const QString &category={});
    bool saveBrand(BrandEntry &brand);
    bool archiveBrand(qint64 id);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database; QString m_error;
};
