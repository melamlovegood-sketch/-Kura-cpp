#pragma once
#include "WorkflowSchema.h"
namespace SupplementSchema {
QList<WorkflowTable> tables();
bool validate(const QSqlDatabase &database,QString &error);
}
