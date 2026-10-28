#include "BillTestFixtures.h"
#include "BillParser.h"
#include "BillImportRepository.h"
#include "DatabaseBackupService.h"
#include "TransactionRepository.h"
#include "SupplementSchema.h"
#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static qint64 count(DatabaseManager &database,const QString &table) { QSqlQuery query(database.database()); if (!query.exec("SELECT COUNT(*) FROM "+table) || !query.next()) return -1; return query.value(0).toLongLong(); }
class ImportTests:public QObject { Q_OBJECT
private slots:
    void moneyAndCsvPrecision();
    void platformsFiltersAndEncoding();
    void actualXlsxStoredDeflatedDatesAndIds();
    void previewAtomicRetryAndDeletedTombstone();
    void conflictsAndNoIdAmbiguity();
    void keysSurviveBackupRestore();
    void shippedExamplesAcrossCsvAndXlsx();
};
void ImportTests::moneyAndCsvPrecision()
{
    qint64 value=0; QVERIFY(BillParser::money(QStringLiteral("￥1,234.56 元"),value)); QCOMPARE(value,123456); QVERIFY(BillParser::money("1.234e2",value)); QCOMPARE(value,12340); QVERIFY(BillParser::money("0.01",value)); QCOMPARE(value,1); QVERIFY(BillParser::money("99999999.99",value)); QCOMPARE(value,9999999999LL);
    for (const auto &bad:{"0","-1","1.234","NaN","abc12","1e99","100000000"}) QVERIFY(!BillParser::money(bad,value));
    const QList<QStringList> original={{QStringLiteral("一行,两列"),QStringLiteral("引号\"与\n换行"),""},{"a","b","c"}}; QList<QStringList> rows; QString error; QVERIFY(BillParser::parseCsv(BillTestFixtures::csv(original),rows,error)); QCOMPARE(rows,original); QVERIFY(!BillParser::parseCsv("\"unterminated",rows,error));
}
void ImportTests::platformsFiltersAndEncoding()
{
    QTemporaryDir directory; auto rows=BillTestFixtures::wechat(); auto refund=rows[2]; refund[6]=QStringLiteral("部分退款"); auto income=rows[2]; income[4]=QStringLiteral("收入"); auto transfer=rows[2]; transfer[1]=QStringLiteral("转账"); auto invalid=rows[2]; invalid[0]="2026-02-31 12:00:00"; rows << refund << income << transfer << invalid;
    const auto path=directory.filePath(QStringLiteral("微信.csv")); QVERIFY(BillTestFixtures::write(path,BillTestFixtures::csv(rows))); BillParseResult parsed; QString error; QVERIFY2(BillParser::parseFile(path,parsed,error),qPrintable(error)); QCOMPARE(parsed.rows.size(),1); QCOMPARE(parsed.filtered,3); QCOMPARE(parsed.invalid,1); QCOMPARE(parsed.rows[0].cents,650); QCOMPARE(parsed.rows[0].time,QTime(12,30,45)); QCOMPARE(parsed.rows[0].category,QStringLiteral("transport"));
    QVERIFY(BillParser::parseFile(path,parsed,error,{},true)); QCOMPARE(parsed.rows.size(),2); QVERIFY(!BillParser::parseFile(path,parsed,error,"alipay"));
    QList<QStringList> alipay={{QStringLiteral("支付宝交易记录明细查询")},{QStringLiteral("交易时间"),QStringLiteral("交易分类"),QStringLiteral("交易对方"),QStringLiteral("商品说明"),QStringLiteral("收/支"),QStringLiteral("金额"),QStringLiteral("交易状态"),QStringLiteral("交易订单号")},{"2026/10/2 09:08:07",QStringLiteral("餐饮美食"),QStringLiteral("咖啡店"),QStringLiteral("拿铁"),QStringLiteral("支出"),"12.34",QStringLiteral("交易成功"),"ALI-123456789012345678901234"}};
    auto bytes=BillTestFixtures::csv(alipay);
#ifdef Q_OS_WIN
    const auto text=QString::fromUtf8(bytes); const int size=WideCharToMultiByte(936,0,reinterpret_cast<const wchar_t *>(text.utf16()),text.size(),nullptr,0,nullptr,nullptr); bytes.resize(size); QVERIFY(WideCharToMultiByte(936,0,reinterpret_cast<const wchar_t *>(text.utf16()),text.size(),bytes.data(),size,nullptr,nullptr)>0);
#endif
    const auto aliPath=directory.filePath("alipay.csv"); QVERIFY(BillTestFixtures::write(aliPath,bytes)); QVERIFY2(BillParser::parseFile(aliPath,parsed,error),qPrintable(error)); QCOMPARE(parsed.platform,QStringLiteral("alipay")); QCOMPARE(parsed.rows[0].name,QStringLiteral("拿铁")); QCOMPARE(parsed.rows[0].externalId,QStringLiteral("ALI-123456789012345678901234")); QCOMPARE(parsed.rows[0].cents,1234);
}
void ImportTests::actualXlsxStoredDeflatedDatesAndIds()
{
    QTemporaryDir directory; const auto rows=BillTestFixtures::wechat("000012345678901234567890123456"); const QDateTime when(QDate(2026,10,3),QTime(12,30,45)); BillParseResult parsed; QString error;
    for (bool compressed:{false,true}) for (bool shared:{false,true}) for (bool epoch:{false,true}) {
        const auto path=directory.filePath("bill.xlsx"); QVERIFY(BillTestFixtures::write(path,BillTestFixtures::xlsx(rows,compressed,shared,epoch,when))); QVERIFY2(BillParser::parseFile(path,parsed,error),qPrintable(error)); QCOMPARE(parsed.rows.size(),1); QCOMPARE(parsed.rows[0].date,when.date()); QCOMPARE(parsed.rows[0].time,when.time()); QCOMPARE(parsed.rows[0].externalId,QStringLiteral("000012345678901234567890123456")); QCOMPARE(parsed.rows[0].sourceRow,17);
    }
    auto corrupt=BillTestFixtures::xlsx(rows,false,false); const auto at=corrupt.indexOf("<workbook "); QVERIFY(at>=0); corrupt[at]='X'; const auto bad=directory.filePath("bad.xlsx"); QVERIFY(BillTestFixtures::write(bad,corrupt)); QVERIFY(!BillParser::parseFile(bad,parsed,error)); QVERIFY(error.contains(QStringLiteral("校验")));
}
void ImportTests::previewAtomicRetryAndDeletedTombstone()
{
    QTemporaryDir directory; const auto path=directory.filePath("two.csv"); auto rows=BillTestFixtures::wechat("one",QStringLiteral("地铁票")); auto second=rows[2]; second[3]=QStringLiteral("咖啡"); second[7]="two"; second[5]="12.34"; rows << second; QVERIFY(BillTestFixtures::write(path,BillTestFixtures::csv(rows)));
    DatabaseManager database(directory.filePath("current.sqlite")); QVERIFY(database.initialize()); BillParseResult preview; QString error; QVERIFY(BillParser::parseFile(path,preview,error)); BillImportRepository imports(database); QVERIFY(imports.prepare(preview)); QCOMPARE(count(database,"transactions"),0);
    QSqlQuery query(database.database()); QVERIFY(query.exec("CREATE TRIGGER fail_second BEFORE INSERT ON transactions WHEN NEW.description LIKE '%咖啡%' BEGIN SELECT RAISE(FAIL,'fixture'); END")); BillImportOutcome result; result.inserted=99; QVERIFY(!imports.commit(preview,result)); QCOMPARE(result.inserted,99); QCOMPARE(count(database,"transactions"),0); QCOMPARE(count(database,"bill_import_keys"),0); QVERIFY(query.exec("DROP TRIGGER fail_second"));
    QVERIFY2(imports.commit(preview,result),qPrintable(imports.lastError())); QCOMPARE(result.inserted,2); QCOMPARE(count(database,"review_tasks"),0); QVERIFY(imports.commit(preview,result)); QCOMPARE(result.inserted,0); QCOMPARE(result.alreadyImported,2); QCOMPARE(count(database,"transactions"),2);
    TransactionRepository expenses(database); QList<Transaction> loaded; QVERIFY(expenses.findAll(loaded)); QVERIFY(expenses.remove(loaded[0].id())); QCOMPARE(count(database,"transactions"),1); QCOMPARE(count(database,"bill_import_keys"),2);
    QVERIFY(BillParser::parseFile(path,preview,error)); QVERIFY(imports.prepare(preview)); for (const auto &row:preview.rows) { QCOMPARE(row.state,BillDraft::Duplicate); QVERIFY(!row.selected); } QVERIFY2(SupplementSchema::validate(database.database(),error),qPrintable(error));
}
void ImportTests::conflictsAndNoIdAmbiguity()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("dedupe.sqlite")); QVERIFY(database.initialize()); BillImportRepository imports(database); const auto path=directory.filePath("one.csv"); QString error; BillParseResult preview; BillImportOutcome result; QVERIFY(BillTestFixtures::write(path,BillTestFixtures::csv(BillTestFixtures::wechat("same")))); QVERIFY(BillParser::parseFile(path,preview,error)); QVERIFY(imports.prepare(preview)); QVERIFY(imports.commit(preview,result));
    QVERIFY(BillTestFixtures::write(path,BillTestFixtures::csv(BillTestFixtures::wechat("same",QStringLiteral("地铁票"),"7.00")))); QVERIFY(BillParser::parseFile(path,preview,error)); QVERIFY(imports.prepare(preview)); QCOMPARE(preview.rows[0].state,BillDraft::Conflict); QVERIFY(!preview.rows[0].selected);
    auto rows=BillTestFixtures::wechat("",QStringLiteral("奶茶"),"10.00"); rows[2][2]=QStringLiteral("饮品店"); rows[2][0]="2026-10-04"; rows << rows[2]; const auto noIds=directory.filePath("no-id.csv"); QVERIFY(BillTestFixtures::write(noIds,BillTestFixtures::csv(rows))); QVERIFY(BillParser::parseFile(noIds,preview,error)); QVERIFY(imports.prepare(preview)); QCOMPARE(preview.rows.size(),2); QCOMPARE(preview.rows[1].state,BillDraft::PossibleDuplicate); QVERIFY(!preview.rows[1].selected); preview.rows[1].selected=true; QVERIFY(imports.commit(preview,result)); QCOMPARE(result.inserted,2);
    rows.prepend({QStringLiteral("另一次导出")}); const auto overlap=directory.filePath("overlap.csv"); QVERIFY(BillTestFixtures::write(overlap,BillTestFixtures::csv(rows))); QVERIFY(BillParser::parseFile(overlap,preview,error)); QVERIFY(imports.prepare(preview)); for (const auto &row:preview.rows) { QCOMPARE(row.state,BillDraft::PossibleDuplicate); QVERIFY(!row.selected); }
}
void ImportTests::keysSurviveBackupRestore()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("restore.sqlite")); QVERIFY(database.initialize()); const auto path=directory.filePath("bill.csv"); QVERIFY(BillTestFixtures::write(path,BillTestFixtures::csv(BillTestFixtures::wechat()))); QString error,safety; BillParseResult preview; QVERIFY(BillParser::parseFile(path,preview,error)); BillImportRepository imports(database); QVERIFY(imports.prepare(preview)); BillImportOutcome result; QVERIFY(imports.commit(preview,result));
    DatabaseBackupService backup(database); const auto copy=directory.filePath("copy.sqlite"); QVERIFY(backup.backup(copy,error)); BackupInfo info; QVERIFY2(backup.prepareRestore(copy,info,error),qPrintable(error)); QCOMPARE(info.importKeyCount,1); QVERIFY(backup.restorePrepared(safety,error)); QVERIFY(imports.commit(preview,result)); QCOMPARE(result.inserted,0); QCOMPARE(result.alreadyImported,1);
}
void ImportTests::shippedExamplesAcrossCsvAndXlsx()
{
    const auto examples=qEnvironmentVariable("KURA_EXAMPLES_DIR"); if (examples.isEmpty()) QSKIP("Examples directory not supplied for this isolated run.");
    QTemporaryDir directory; DatabaseManager database(directory.filePath("examples.sqlite")); QVERIFY(database.initialize()); BillImportRepository imports(database); BillParseResult preview; QString error;
    QVERIFY2(BillParser::parseFile(examples+"/import-wechat.csv",preview,error),qPrintable(error)); QCOMPARE(preview.rows.size(),3); for (auto &row:preview.rows) row.confirmed=true; QVERIFY(imports.prepare(preview)); BillImportOutcome result; QVERIFY(imports.commit(preview,result)); QCOMPARE(result.inserted,3);
    QVERIFY2(BillParser::parseFile(examples+"/import-wechat.xlsx",preview,error),qPrintable(error)); QCOMPARE(preview.rows.size(),3); QVERIFY(imports.prepare(preview)); for (const auto &row:preview.rows) QCOMPARE(row.state,BillDraft::Duplicate);
    QVERIFY2(BillParser::parseFile(examples+"/import-alipay.csv",preview,error),qPrintable(error)); QCOMPARE(preview.platform,QStringLiteral("alipay")); QCOMPARE(preview.rows.size(),3);
}
QTEST_GUILESS_MAIN(ImportTests)
#include "import_tests.moc"
