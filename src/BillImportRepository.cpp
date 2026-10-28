#include "BillImportRepository.h"
#include "CategoryCatalog.h"
#include "TransactionRepository.h"
#include "WorkflowStorage.h"
#include <QHash>
#include <QSet>
#include <QSqlQuery>
#include <QVariant>
#include <QRegularExpression>

namespace {
double similarity(const QString &a,const QString &b) { const auto x=BillParser::normalized(a),y=BillParser::normalized(b); if (x==y) return 1; if (x.isEmpty() || y.isEmpty()) return 0; QHash<QString,int> pairs; for (int i=0;i<x.size()-1;++i) ++pairs[x.mid(i,2)]; int common=0; for (int i=0;i<y.size()-1;++i) { const auto key=y.mid(i,2); if (pairs.value(key)>0) { ++common; --pairs[key]; } } const int size=x.size()+y.size()-2; return size>0?2.0*common/size:0; }
struct Existing { QString name; QTime time; };
}
bool BillImportRepository::prepare(BillParseResult &preview)
{
    m_error.clear(); if (!m_database.database().isOpen()) { m_error=QStringLiteral("数据库未打开。"); return false; }
    QSqlQuery query(m_database.database()); QHash<QString,QString> keys; QSet<QString> fingerprints;
    if (!query.exec("SELECT import_key,payload_hash,fingerprint FROM bill_import_keys")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) { keys.insert(query.value(0).toString(),query.value(1).toString()); fingerprints.insert(query.value(2).toString()); } if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
    QHash<QString,QList<Existing>> expenses;
    if (!query.exec("SELECT description,amount_cents,date,occurred_time FROM transactions")) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    while (query.next()) { const auto bucket=query.value(2).toString()+"|"+query.value(1).toString(); if (expenses[bucket].size()<500) expenses[bucket].append({query.value(0).toString(),QTime::fromString(query.value(3).toString(),"HH:mm:ss")}); }
    if (query.lastError().isValid()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
    auto rows=preview.rows; QHash<QString,int> seen; QSet<QString> batchFingerprints;
    for (int i=0;i<rows.size();++i) { auto &row=rows[i];
        if (keys.contains(row.key)) { row.state=keys[row.key]==row.payloadHash?BillDraft::Duplicate:BillDraft::Conflict; row.selected=false; row.reason=row.state==BillDraft::Duplicate?QStringLiteral("已导入过此记录；删除原账单后也不会自动重建"):QStringLiteral("同一交易号的日期、金额或商品发生变化，请核对源文件"); continue; }
        if (seen.contains(row.key)) { auto &first=rows[seen[row.key]]; if (first.payloadHash!=row.payloadHash) { first.state=row.state=BillDraft::Conflict; first.selected=row.selected=false; first.reason=row.reason=QStringLiteral("文件内同一交易号出现不同明细，请先核对源文件"); } else { row.state=BillDraft::Duplicate; row.selected=false; row.reason=QStringLiteral("文件内重复记录"); } continue; } seen.insert(row.key,i);
        bool possible=fingerprints.contains(row.fingerprint) || batchFingerprints.contains(row.fingerprint);
        const auto bucket=row.date.toString(Qt::ISODate)+"|"+QString::number(row.cents);
        for (const auto &expense:expenses.value(bucket)) if ((!row.time.isValid() || !expense.time.isValid() || row.time==expense.time) && similarity(row.originalName,expense.name)>0.9) { possible=true; break; }
        if (possible) { row.state=BillDraft::PossibleDuplicate; row.selected=false; row.reason=QStringLiteral("与现有账单或本文件其他行疑似重复，默认不选；确认是另一笔时可保留"); }
        batchFingerprints.insert(row.fingerprint);
    }
    preview.rows=rows; return true;
}
bool BillImportRepository::commit(const BillParseResult &preview,BillImportOutcome &outcome)
{
    m_error.clear(); BillImportOutcome result;
    const auto validHash=[](const QString &value) { return QRegularExpression("^[0-9a-f]{64}$").match(value).hasMatch(); };
    if (!validHash(preview.fileHash) || preview.fileName.size()>512) { m_error=QStringLiteral("导入预览缺少有效文件信息，请重新读取。"); return false; }
    for (const auto &row:preview.rows) if (row.selected) {
        if (row.state==BillDraft::Duplicate || row.state==BillDraft::Conflict || !row.confirmed || !validHash(row.key) || !validHash(row.payloadHash) || !validHash(row.fingerprint) || row.originalName.isEmpty() || row.originalName.size()>8000 || row.externalId.size()>256 || (row.platform!="wechat" && row.platform!="alipay" && row.platform!="kura")) { m_error=QStringLiteral("请先确认所选记录的分类，排除重复或冲突记录。"); return false; }
        const auto error=Transaction(0,row.name,row.cents,CategoryCatalog::legacyCategory(row.category),row.date,row.time,row.note,row.category).validationError(); if (!error.isEmpty()) { m_error=QStringLiteral("第 %1 行：%2").arg(row.sourceRow).arg(error); return false; }
    }
    WorkflowStorage::Transaction batch(m_database.database(),m_error); if (!batch.ok()) return false;
    QSqlQuery query(m_database.database()); TransactionRepository expenses(m_database);
    for (const auto &row:preview.rows) if (row.selected) {
        query.prepare("SELECT payload_hash FROM bill_import_keys WHERE import_key=:key"); query.bindValue(":key",row.key);
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; }
        if (query.next()) { if (query.value(0).toString()!=row.payloadHash) { m_error=QStringLiteral("预览后出现同交易号冲突，整批未导入，请重新读取。"); return false; } ++result.alreadyImported; query.finish(); continue; } query.finish();
        query.prepare("INSERT INTO bill_import_keys(import_key,payload_hash,fingerprint,platform,external_id,file_hash,source_file,original_name,imported_ms) VALUES(:key,:payload,:fp,:platform,:external,:file,:source,:original,:now)");
        query.bindValue(":key",row.key); query.bindValue(":payload",row.payloadHash); query.bindValue(":fp",row.fingerprint); query.bindValue(":platform",row.platform); query.bindValue(":external",row.externalId.isNull()?QStringLiteral(""):row.externalId); query.bindValue(":file",preview.fileHash); query.bindValue(":source",preview.fileName.isNull()?QStringLiteral(""):preview.fileName); query.bindValue(":original",row.originalName); query.bindValue(":now",QDateTime::currentMSecsSinceEpoch());
        if (!query.exec()) { m_error=WorkflowStorage::error(query.lastError()); return false; } const auto keyId=query.lastInsertId().toLongLong(); query.finish();
        Transaction expense(0,row.name,row.cents,CategoryCatalog::legacyCategory(row.category),row.date,row.time,row.note,row.category);
        if (!expenses.add(expense)) { m_error=expenses.lastError(); return false; }
        query.prepare("UPDATE bill_import_keys SET transaction_id=:expense WHERE id=:id"); query.bindValue(":expense",expense.id()); query.bindValue(":id",keyId); if (!query.exec() || query.numRowsAffected()!=1) { m_error=WorkflowStorage::error(query.lastError()); return false; } query.finish();
        ++result.inserted; result.ids << expense.id();
    }
    if (!batch.commit()) return false; outcome=result; return true;
}
