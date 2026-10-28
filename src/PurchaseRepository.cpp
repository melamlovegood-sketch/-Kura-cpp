#include "PurchaseRepository.h"
#include "TransactionRepository.h"
#include "ReviewRepository.h"
#include "WorkflowStorage.h"
#include "ProductRepository.h"
#include <QSqlQuery>
#include <QVariant>

bool PurchaseRepository::record(Transaction &expense,qint64 brandId,qint64 sessionId,qint64 productId)
{
    m_error=expense.validationError(); if (!m_error.isEmpty()) return false;
    if (expense.id()!=0 || brandId<0 || sessionId<0) { m_error=QStringLiteral("这次购买已保存或编号无效。"); return false; }
    WorkflowStorage::Transaction transaction(m_database.database(),m_error); if (!transaction.ok()) return false;
    QSqlQuery query(m_database.database());
    if (sessionId) {
        query.prepare("UPDATE execution_sessions SET state='bought',closed_ms=MAX(:now,started_ms) WHERE id=:id AND state='active'"); query.bindValue(":id",sessionId); query.bindValue(":now",QDateTime::currentMSecsSinceEpoch());
        if (!query.exec() || query.numRowsAffected()!=1) { m_error=QStringLiteral("这次决策已结束，请刷新，避免重复购买。"); return false; } query.finish();
    }
    auto saved=expense; TransactionRepository expenses(m_database);
    if (!expenses.add(saved)) { m_error=expenses.lastError(); return false; }
    ReviewRepository reviews(m_database); if (!reviews.schedule(saved.id(),brandId)) { m_error=reviews.lastError(); return false; }
    if (sessionId) {
        query.prepare("UPDATE execution_sessions SET transaction_id=:expense WHERE id=:id AND state='bought'"); query.bindValue(":id",sessionId); query.bindValue(":expense",saved.id());
        if (!query.exec() || query.numRowsAffected()!=1) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    }
    ProductRepository products(m_database);
    if (productId && !products.attach("transaction",saved.id(),productId)) { m_error=products.lastError(); return false; }
    if (sessionId && !products.carry("execution",sessionId,"transaction",saved.id())) { m_error=products.lastError(); return false; }
    if (!transaction.commit()) return false; expense=saved; return true;
}
