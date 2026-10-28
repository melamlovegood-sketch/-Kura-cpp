#include "MonthlyReviewRepository.h"
#include "CategoryCatalog.h"
#include "TransactionRepository.h"
#include "BudgetRepository.h"
#include "WishPoolRepository.h"
#include "ShoppingProfileRepository.h"
#include "DatabaseBackupService.h"
#include "SupplementSchema.h"
#include "AIService.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>

static bool expense(DatabaseManager &database,const QString &name,qint64 cents,const QString &detail,const QDate &date,qint64 &id) { Transaction record(0,name,cents,CategoryCatalog::legacyCategory(detail),date,QTime(12,0),{},detail); TransactionRepository repository(database); if (!repository.add(record)) return false; id=record.id(); return true; }
class MonthlyReviewTests:public QObject { Q_OBJECT
private slots:
    void aggregationTagsAndMonthBoundary();
    void localStoryStalenessAndNoBrandFeedback();
    void emptyBoundaryAndFailurePreserveSnapshot();
    void storiesAndTagsBackupWithOldElevenTables();
    void structuredStoryValidation();
    void personalityCandidatesAndSamples();
    void structuredPersistenceAndNotesInvalidate();
};
void MonthlyReviewTests::aggregationTagsAndMonthBoundary()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("month.sqlite")); QVERIFY(database.initialize()); const auto today=QDate::currentDate(); const QDate month(today.year(),today.month(),1); qint64 food=0,shopping=0,old=0;
    QVERIFY(expense(database,QStringLiteral("食堂午饭"),1000,"canteen",month,food)); QVERIFY(expense(database,QStringLiteral("键盘"),2000,"online_shopping",month.addDays(1),shopping)); QVERIFY(expense(database,QStringLiteral("上月地铁"),500,"transport",month.addDays(-1),old));
    BudgetRepository budgets(database); QVERIFY(budgets.saveSplit(month,5000,6000)); MonthlyReviewRepository monthly(database); QVERIFY(monthly.setTag(food,-1,QStringLiteral("不如预期"))); QVERIFY(monthly.setTag(shopping,1,QStringLiteral("日常在用")));
    WishPoolRepository pool(database); QVERIFY(pool.addSaving(500,QStringLiteral("上月边界"),"before-month")); QVERIFY(pool.addSaving(1000,QStringLiteral("本月忍住"),"in-month"));
    QSqlQuery query(database.database()); const auto start=QDateTime(month,QTime(0,0),QTimeZone::systemTimeZone()).toUTC().toMSecsSinceEpoch(); query.prepare("UPDATE savings_records SET created_ms=:time WHERE request_token=:token"); query.bindValue(":time",start-1); query.bindValue(":token","before-month"); QVERIFY(query.exec()); query.finish(); query.prepare("UPDATE savings_records SET created_ms=:time WHERE request_token=:token"); query.bindValue(":time",start); query.bindValue(":token","in-month"); QVERIFY(query.exec());
    MonthlyReviewData data; QVERIFY2(monthly.load(month,data),qPrintable(monthly.lastError())); QCOMPARE(data.totalCents,3000); QCOMPARE(data.basicCents,1000); QCOMPARE(data.discretionaryCents,2000); QCOMPARE(data.previousCents,500); QCOMPARE(data.count,2); QCOMPARE(data.savingsCount,1); QCOMPARE(data.savingsCents,1000); QCOMPARE(data.worthCount,1); QCOMPARE(data.regretCount,1); QCOMPARE(data.biggestSaving,QStringLiteral("本月忍住")); QVERIFY(data.localPersona.contains(QStringLiteral("未分类"))); QVERIFY(data.localStory.contains(QStringLiteral("不代表实际收入"))); QCOMPARE(data.expenses.first().id,shopping); QVERIFY(QJsonDocument::fromJson(data.factsJson.toUtf8()).isObject());
    for (const auto &category:data.categories) if (category.key=="travel") { QCOMPARE(category.cents,0); QCOMPARE(category.previousCents,500); }
}
void MonthlyReviewTests::localStoryStalenessAndNoBrandFeedback()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("story.sqlite")); QVERIFY(database.initialize()); const auto month=MonthlyBudget::firstDay(QDate::currentDate()); qint64 id=0; QVERIFY(expense(database,QStringLiteral("键盘"),2000,"online_shopping",month,id));
    ShoppingProfileRepository profile(database); BrandEntry brand; brand.name="Fixture Brand"; brand.category="online_shopping"; brand.weight=8; QVERIFY(profile.saveBrand(brand)); MonthlyReviewRepository monthly(database); MonthlyReviewData data; QVERIFY(monthly.load(month,data)); QVERIFY(monthly.saveStory(month,data.factsHash,data.localStory,data.localPersona,"local")); MonthlyStory saved; QVERIFY(monthly.loadStory(data,saved)); QVERIFY(saved.id>0); QVERIFY(!saved.stale); const auto firstStory=saved.story;
    QVERIFY(monthly.setTag(id,-1,QStringLiteral("买早了"))); MonthlyReviewData changed; QVERIFY(monthly.load(month,changed)); QVERIFY(changed.factsHash!=data.factsHash); QVERIFY(monthly.loadStory(changed,saved)); QVERIFY(saved.stale); QVERIFY(!monthly.saveStory(month,data.factsHash,"stale story","stale persona","ai")); QVERIFY(monthly.loadStory(changed,saved)); QCOMPARE(saved.story,firstStory);
    QVERIFY(monthly.saveStory(month,changed.factsHash,changed.localStory,changed.localPersona,"local")); QList<BrandEntry> brands; QVERIFY(profile.loadBrands(brands)); QCOMPARE(brands[0].weight,8);
    TransactionRepository expenses(database); QVERIFY(expenses.remove(id)); QVERIFY(monthly.load(month,changed)); QCOMPARE(changed.regretCount,0); QCOMPARE(changed.count,0); QVERIFY(monthly.loadStory(changed,saved)); QVERIFY(saved.stale); QString error; QVERIFY2(SupplementSchema::validate(database.database(),error),qPrintable(error));
}
void MonthlyReviewTests::emptyBoundaryAndFailurePreserveSnapshot()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("empty.sqlite")); QVERIFY(database.initialize()); MonthlyReviewRepository monthly(database); MonthlyReviewData data;
    QVERIFY(monthly.load(QDate(1,1,1),data)); QCOMPARE(data.count,0); QCOMPARE(data.previousCents,0); QVERIFY(!data.hasData()); QVERIFY(!data.localStory.isEmpty()); QVERIFY(monthly.load(QDate(9999,12,1),data)); QCOMPARE(data.totalCents,0);
    QSqlQuery query(database.database()); QVERIFY(query.exec("ALTER TABLE transaction_reviews RENAME TO unavailable_reviews")); const auto before=data.month; QVERIFY(!monthly.load(QDate::currentDate(),data)); QCOMPARE(data.month,before); QCOMPARE(data.totalCents,0); QVERIFY(query.exec("ALTER TABLE unavailable_reviews RENAME TO transaction_reviews")); QVERIFY(monthly.load(QDate::currentDate(),data));
}
void MonthlyReviewTests::storiesAndTagsBackupWithOldElevenTables()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("current.sqlite")); QVERIFY(database.initialize()); qint64 id=0; const auto month=MonthlyBudget::firstDay(QDate::currentDate()); QVERIFY(expense(database,QStringLiteral("午饭"),1000,"daily",month,id)); MonthlyReviewRepository monthly(database); QVERIFY(monthly.setTag(id,1,QStringLiteral("值得"))); MonthlyReviewData data; QVERIFY(monthly.load(month,data)); QVERIFY(monthly.saveStory(month,data.factsHash,data.localStory,data.localPersona,"local"));
    DatabaseBackupService backup(database); const auto saved=directory.filePath("all.sqlite"); QString error,safety; BackupInfo info; QVERIFY(backup.backup(saved,error)); QVERIFY2(backup.prepareRestore(saved,info,error),qPrintable(error)); QCOMPARE(info.tagCount,1); QCOMPARE(info.storyCount,1); QVERIFY(monthly.setTag(id,0,{})); QVERIFY(backup.restorePrepared(safety,error)); QVERIFY(monthly.load(month,data)); QCOMPARE(data.worthCount,1); MonthlyStory story; QVERIFY(monthly.loadStory(data,story)); QVERIFY(!story.stale);
    const auto old=directory.filePath("eleven.sqlite"); QVERIFY(backup.backup(old,error)); { DatabaseManager legacy(old); QVERIFY(legacy.initialize()); QSqlQuery query(legacy.database()); for (const auto &name:{"user_categories","achievement_marks","bill_import_keys","transaction_reviews","monthly_stories"}) QVERIFY(query.exec("DROP TABLE "+QLatin1String(name))); }
    QFile file(old); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes=file.readAll(); file.close(); QVERIFY2(backup.prepareRestore(old,info,error),qPrintable(error)); QCOMPARE(info.storyCount,0); QVERIFY(backup.restorePrepared(safety,error)); QVERIFY(monthly.load(month,data)); QCOMPARE(data.worthCount,0); QVERIFY(monthly.loadStory(data,story)); QCOMPARE(story.id,0); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),bytes);
}
void MonthlyReviewTests::structuredStoryValidation()
{
    const auto envelope=[](const QString &content) { return QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",content}}}}}}}).toJson(); }; MonthlyAnalysis analysis; QString error;
    ReviewStoryContent c{"custom",QStringLiteral("记录自己的小主题"),QStringLiteral("每一块拼图，都有自己的位置。"),QStringLiteral("这页小本子留下了消费的足迹，拼成自己的生活主题。每一笔记录，都能成为这一页故事里的小片段。回头看看其中的喜欢与取舍，再为下一次心动留一个参照。"),{},QStringLiteral("给一笔消费补一句自己的感受。"),{"1"}};
    c.customName=QStringLiteral("生活拼图型");
    const auto good=MonthlyStoryTemplate::encode(c);
    QVERIFY(AIService::decodeMonthly(envelope("```json\n"+good+"\n```"),analysis,error)); QCOMPARE(analysis.content.personaId,QStringLiteral("custom"));
    QCOMPARE(analysis.persona,QStringLiteral("生活拼图型 · 记录自己的小主题"));
    ReviewStoryContent roundTrip; QVERIFY(MonthlyStoryTemplate::decode(analysis.story,roundTrip,error)); QCOMPARE(roundTrip.story,c.story);
    for (const auto &text:{QStringLiteral("{}"),QStringLiteral("{\"story\":12,\"persona\":\"画像\"}"),QStringLiteral("{\"story\":\"故事\"}"),QStringLiteral("不是 JSON")}) QVERIFY(!AIService::decodeMonthly(envelope(text),analysis,error));
    for (int invalid=0;invalid<8;++invalid) {
        auto object=QJsonDocument::fromJson(good.toUtf8()).object();
        if (invalid==0) object["persona_id"]="invented_animal";
        if (invalid==1) object["story"]=QString(241,QChar(0x6545));
        if (invalid==2) object["next_step"]=QString();
        if (invalid==3) object["story"]=QStringLiteral("这是一个含有 savings_count 的技术字段的故事，不应该把这样的报告直接展示给用户。这里补足协议要求的最小字数。");
        if (invalid==4) object["evidence_ids"]=QJsonArray{1};
        if (invalid==5) object["persona_id"]="pending";
        if (invalid==6) object["persona_name"]=QStringLiteral("角色待解锁型");
        if (invalid==7) object["persona_name"]=QString();
        QVERIFY(!AIService::decodeMonthly(envelope(QString::fromUtf8(QJsonDocument(object).toJson())),analysis,error));
    }
    QJsonObject facts{{"persona_options",QJsonArray{"custom"}},{"selected_expenses",QJsonArray{QJsonObject{{"id","1"}}}}};
    QVERIFY(MonthlyStoryTemplate::matchesFacts(c,facts,error)); c.evidenceIds={"999"}; QVERIFY(!MonthlyStoryTemplate::matchesFacts(c,facts,error));
    c.evidenceIds.clear(); c.personaId="owl"; QVERIFY(!MonthlyStoryTemplate::matchesFacts(c,facts,error));
    auto legacy=QJsonDocument::fromJson(good.toUtf8()).object(); legacy["schema_version"]=1; legacy["persona_id"]="pending"; legacy["subtitle"]=QString(); legacy.remove("persona_name");
    const auto historical=QString::fromUtf8(QJsonDocument(legacy).toJson());
    QVERIFY(MonthlyStoryTemplate::decode(historical,roundTrip,error));
    QVERIFY(!AIService::decodeMonthly(envelope(historical),analysis,error));
}
void MonthlyReviewTests::personalityCandidatesAndSamples()
{
    const auto make=[](const QStringList &categories,const QStringList &names,bool marked=false) {
        MonthlyReviewData data; data.month=MonthlyBudget::firstDay(QDate::currentDate()); data.count=categories.size();
        for (const auto &key:CategoryCatalog::keys()) data.categories.append({key,0,0,0});
        for (int i=0;i<categories.size();++i) {
            MonthlyReviewExpense e; e.id=i+1; e.cents=1000; e.category=categories[i]; e.name=names.value(i,QStringLiteral("一笔消费")); e.date=data.month;
            if (marked) { e.tag=1; ++data.worthCount; e.note=QStringLiteral("每天都在用"); }
            data.expenses.append(e); data.totalCents+=e.cents;
            for (auto &category:data.categories) if (category.key==e.category) { category.cents+=e.cents; ++category.count; }
        }
        return data;
    };
    const QList<MonthlyReviewData> examples={
        make({"online_shopping","online_shopping"},{QStringLiteral("键盘"),QStringLiteral("鼠标")},true),
        make({"study","study"},{QStringLiteral("教材"),QStringLiteral("课程")},true),
        make({"entertainment","entertainment"},{QStringLiteral("电影"),QStringLiteral("音乐会")},true),
        make({"canteen","canteen","daily_supplies","daily","transport"},{}),
        make({"canteen","transport","entertainment","canteen","transport","entertainment"},{})
    };
    const QStringList expected={"beaver","owl","otter","cat","migratory_bird"};
    QString error;
    for (int i=0;i<examples.size();++i) {
        QJsonObject facts; MonthlyStoryTemplate::addContext(facts,examples[i]);
        QVERIFY2(facts["persona_options"].toArray().contains(expected[i]),qPrintable(expected[i]));
        const auto local=MonthlyStoryTemplate::local(examples[i]); ReviewStoryContent roundTrip;
        QVERIFY2(MonthlyStoryTemplate::decode(MonthlyStoryTemplate::encode(local),roundTrip,error),qPrintable(error));
        QVERIFY2(MonthlyStoryTemplate::matchesFacts(local,facts,error),qPrintable(error));
    }
    auto sparse=make({"study"},{QStringLiteral("教材")}); QJsonObject facts; MonthlyStoryTemplate::addContext(facts,sparse);
    QCOMPARE(facts["persona_options"].toArray(),QJsonArray{"custom"});
    const auto custom=MonthlyStoryTemplate::local(sparse); QVERIFY(!custom.customName.isEmpty());
    ReviewStoryContent decoded; QVERIFY(MonthlyStoryTemplate::decode(MonthlyStoryTemplate::encode(custom),decoded,error));
    QVERIFY(MonthlyStoryTemplate::matchesFacts(custom,facts,error));
    sparse.savingsCount=2; sparse.savingsCents=4500; facts={}; MonthlyStoryTemplate::addContext(facts,sparse);
    QCOMPARE(MonthlyStoryTemplate::local(sparse).personaId,QStringLiteral("squirrel"));
    QVERIFY(facts["persona_options"].toArray().contains("squirrel"));
    QCOMPARE(facts["persona_options"].toArray(),QJsonArray{"squirrel"});
    auto unsupported=custom; unsupported.evidenceIds.clear(); QVERIFY(!MonthlyStoryTemplate::matchesFacts(unsupported,facts,error));
    MonthlyReviewData empty; QJsonObject noFacts; MonthlyStoryTemplate::addContext(noFacts,empty); QVERIFY(noFacts["persona_options"].toArray().isEmpty());
    for (int mode=0;mode<2;++mode) {
        MonthlyReviewData onlyChoice; if (mode==0) { onlyChoice.savingsCount=1; onlyChoice.savingsCents=1500; } else onlyChoice.abandonedCount=1;
        QJsonObject choiceFacts; MonthlyStoryTemplate::addContext(choiceFacts,onlyChoice);
        const auto choice=MonthlyStoryTemplate::local(onlyChoice);
        QCOMPARE(choice.personaId,QStringLiteral("custom")); QVERIFY(choice.customName.endsWith(QStringLiteral("型")));
        QVERIFY2(MonthlyStoryTemplate::decode(MonthlyStoryTemplate::encode(choice),decoded,error),qPrintable(error));
        QVERIFY(MonthlyStoryTemplate::matchesFacts(choice,choiceFacts,error));
    }
    auto many=make(QStringList(40,"online_shopping"),{}); many.expenses[0].purchaseNote=QString(500,QChar(0x4E66)); facts={}; MonthlyStoryTemplate::addContext(facts,many);
    QVERIFY(facts["selected_expenses"].toArray().size()<=12);
    QCOMPARE(facts["selected_expenses"].toArray()[0].toObject()["purchase_note"].toString().size(),160);
    const auto system=MonthlyStoryTemplate::prompt();
    for (const auto &id:{"squirrel","beaver","cat","otter","owl","migratory_bird","custom"}) QVERIFY(system.contains(QLatin1String(id)));
}
void MonthlyReviewTests::structuredPersistenceAndNotesInvalidate()
{
    QTemporaryDir directory; DatabaseManager database(directory.filePath("cards.sqlite")); QVERIFY(database.initialize());
    const auto month=MonthlyBudget::firstDay(QDate::currentDate()); qint64 id=0; QVERIFY(expense(database,QStringLiteral("教材"),2000,"study",month,id));
    MonthlyReviewRepository monthly(database); MonthlyReviewData data; QVERIFY(monthly.load(month,data));
    const auto card=MonthlyStoryTemplate::local(data); const auto encoded=MonthlyStoryTemplate::encode(card);
    QVERIFY2(monthly.saveStory(month,data.factsHash,encoded,MonthlyStoryTemplate::personaCaption(card),"local"),qPrintable(monthly.lastError()));
    MonthlyStory loaded; QVERIFY(monthly.loadStory(data,loaded)); QCOMPARE(loaded.story,encoded);
    DatabaseBackupService backup(database); QString error,safety; BackupInfo info;
    const auto backupPath=directory.filePath("structured-backup.sqlite"); QVERIFY(backup.backup(backupPath,error));
    QSqlQuery query(database.database()); QVERIFY(query.exec("UPDATE transactions SET note='note changed'"));
    MonthlyReviewData changed; QVERIFY(monthly.load(month,changed)); QVERIFY(changed.factsHash!=data.factsHash);
    QVERIFY(monthly.loadStory(changed,loaded)); QVERIFY(loaded.stale); QCOMPARE(loaded.story,encoded);
    auto wrong=card; wrong.personaId="owl"; wrong.subtitle=QStringLiteral("给未来充电");
    QVERIFY(!monthly.saveStory(month,changed.factsHash,MonthlyStoryTemplate::encode(wrong),"owl","ai"));
    QVERIFY(monthly.loadStory(changed,loaded)); QCOMPARE(loaded.story,encoded);
    QVERIFY2(backup.prepareRestore(backupPath,info,error),qPrintable(error)); QVERIFY(backup.restorePrepared(safety,error));
    QVERIFY(monthly.load(month,data)); QVERIFY(monthly.loadStory(data,loaded)); QVERIFY(!loaded.stale); QCOMPARE(loaded.story,encoded);
}
QTEST_GUILESS_MAIN(MonthlyReviewTests)
#include "monthly_review_tests.moc"
