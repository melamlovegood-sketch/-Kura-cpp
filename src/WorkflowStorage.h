#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDateTime>

namespace WorkflowStorage {
inline QString error(const QSqlError &value) {
    const int code=value.nativeErrorCode().toInt() & 255;
    return code==5 || code==6 ? QStringLiteral("数据库正在被占用，请稍后重试。") : QStringLiteral("数据操作失败：%1").arg(value.text());
}
inline bool validTime(const QDateTime &time) { return time.isValid() && time.date().year()>=1 && time.date().year()<=9999; }
inline QString nameKey(const QString &name) { return name.simplified().toCaseFolded(); }
class Transaction {
public:
    Transaction(const QSqlDatabase &database,QString &message):db(database),out(message),name("kura_"+QUuid::createUuid().toString(QUuid::Id128)) {
        if (!db.isOpen()) { out=QStringLiteral("数据库未打开。"); return; }
        QSqlQuery query(db); active=query.exec("SAVEPOINT "+name); if (!active) out=error(query.lastError());
    }
    ~Transaction() { if (active) { QSqlQuery query(db); query.exec("ROLLBACK TO "+name); query.exec("RELEASE "+name); } }
    bool ok() const { return active; }
    bool commit() { QSqlQuery query(db); if (!active || !query.exec("RELEASE "+name)) { out=error(query.lastError()); return false; } active=false; return true; }
private:
    QSqlDatabase db; QString &out,name; bool active=false;
};
}
