#pragma once
#include <QDateTime>
#include <QString>
#include <QSet>
#include <memory>
class DatabaseManager;
class QTemporaryDir;

namespace DemoPersonas {
enum class Persona { Student, Worker };
QString key(Persona persona);
QString title(Persona persona);
QString description(Persona persona);
// Six completed months plus the current month. Student current-month expenses
// and import keys remain empty so the supplied October bill can be imported live.
// Only an entirely empty, initialized database may be populated.
bool populate(DatabaseManager &database, Persona persona, QString &error,
              const QDateTime &now = QDateTime::currentDateTimeUtc());

// Both identities live in the same disposable rehearsal session. Switching
// identities keeps imports and edits until the application exits.
class Session {
public:
    Session();
    ~Session();
    QString open(Persona persona, QString &error, const QDateTime &now = QDateTime::currentDateTimeUtc());
private:
    std::unique_ptr<QTemporaryDir> m_directory;
    QSet<QString> m_ready;
};
}
