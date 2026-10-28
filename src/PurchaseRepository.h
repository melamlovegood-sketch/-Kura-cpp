#pragma once
#include "DatabaseManager.h"
#include "Transaction.h"
class PurchaseRepository {
public:
    explicit PurchaseRepository(DatabaseManager &database):m_database(database) {}
    bool record(Transaction &expense,qint64 brandId=0,qint64 sessionId=0,qint64 productId=0);
    QString lastError() const { return m_error; }
private:
    DatabaseManager &m_database; QString m_error;
};
