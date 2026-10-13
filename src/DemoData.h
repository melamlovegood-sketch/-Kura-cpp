#pragma once
#include <QDateTime>
#include <QString>
class DatabaseManager;
namespace DemoData {
bool populate(DatabaseManager &database, QString &error,
              const QDateTime &now = QDateTime::currentDateTimeUtc());
}
