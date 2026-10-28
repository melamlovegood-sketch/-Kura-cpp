#pragma once
#include <QSqlDatabase>
#include <QStringList>
#include <QSet>
struct WorkflowTable { QString name,createSql; QStringList columns; QSet<QString> integers,nullable; };
namespace WorkflowSchema {
QList<WorkflowTable> tables();
bool validate(const QSqlDatabase &database,QString &error);
}
