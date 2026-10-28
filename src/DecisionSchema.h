#pragma once
#include <QSqlDatabase>
namespace DecisionSchema { bool upgrade(QSqlDatabase database, QString &error); }
