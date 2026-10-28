#pragma once
#include "DatabaseManager.h"
#include "WorkflowSchema.h"
#include <QJsonObject>
#include <QJsonArray>

struct ProductContext {
    QString kind,name,category;
    qint64 entityId=0,amountCents=0;
    QByteArray image;
    QString purchaseReason;
};
struct ProductProfile {
    qint64 id=0,revision=0;
    QString name,category,notes,conversation;
    QJsonObject fields;
};
struct ProductEvidence { qint64 id=0,createdMs=0; QByteArray image; QJsonObject extracted; };
namespace ProductSchema {
QList<WorkflowTable> tables();
bool upgrade(const QSqlDatabase &database,QString &error);
bool validate(const QSqlDatabase &database,QString &error);
}
class ProductRepository {
public:
    explicit ProductRepository(DatabaseManager &database):m_database(database) {}
    static QList<QPair<QString,QString>> fieldLabels();
    static QString purchaseReason(const ProductProfile &profile,const ProductContext &context);
    static bool validFields(const QJsonObject &fields);
    static bool validConversation(const QString &conversation);
    bool context(const QString &kind,qint64 entityId,ProductContext &context);
    bool ensure(const ProductContext &context,ProductProfile &profile);
    bool load(qint64 id,ProductProfile &profile);
    bool list(QList<ProductProfile> &profiles);
    bool attach(const QString &kind,qint64 entityId,qint64 productId);
    bool carry(const QString &fromKind,qint64 fromId,const QString &toKind,qint64 toId);
    bool save(ProductProfile &profile,const QByteArray &image={},const QJsonObject &extracted={});
    bool evidence(qint64 productId,QList<ProductEvidence> &images);
    bool saveConversation(qint64 id,const QString &conversation,const QString &previous);
    QString aiContext(qint64 productId,const ProductContext &context);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database;
    QString m_error;
};
