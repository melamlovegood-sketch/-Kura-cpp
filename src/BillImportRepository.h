#pragma once
#include "DatabaseManager.h"
#include "BillParser.h"
struct BillImportOutcome { int inserted=0,alreadyImported=0; QList<qint64> ids; };
class BillImportRepository {
public:
    explicit BillImportRepository(DatabaseManager &database):m_database(database) {}
    bool prepare(BillParseResult &preview);
    bool commit(const BillParseResult &preview,BillImportOutcome &outcome);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database; QString m_error;
};
