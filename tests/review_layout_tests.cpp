#include "mainwindow.h"
#include "MonthlyReviewWidget.h"
#include "MonthlyReviewRepository.h"
#include "TransactionRepository.h"
#include "WishPoolRepository.h"
#include "ReviewRepository.h"
#include "CategoryCatalog.h"
#include "AppTheme.h"
#include <QAction>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QDialog>
#include <QJsonDocument>
#include <QtTest>

namespace {
void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
}
QLabel *label(QWidget &root,const char *name) { return root.findChild<QLabel *>(QLatin1String(name)); }
QPushButton *button(QWidget &root,const char *name) { return root.findChild<QPushButton *>(QLatin1String(name)); }
bool screenshot(QWidget &widget,const QString &name)
{
    settle();
    const QString directory=qEnvironmentVariable("KURA_PREVIEW_DIR");
    return directory.isEmpty() || (QDir().mkpath(directory) && widget.grab().save(directory+"/review-refresh-"+name+".png"));
}
bool populate(DatabaseManager &database)
{
    const auto month=MonthlyBudget::firstDay(QDate::currentDate());
    TransactionRepository transactions(database);
    MonthlyReviewRepository monthly(database);
    struct Item { QString name,category,note; qint64 cents; int tag; };
    const QList<Item> items={
        {QStringLiteral("一本想读很久的书"),"daily",QStringLiteral("读完后，还想再翻一次。"),6800,1},
        {QStringLiteral("每天都穿的跑鞋"),"online_shopping",QStringLiteral("合脚，也让我更愿意出门。"),46900,1},
        {QStringLiteral("凑单买的收纳盒"),"online_shopping",QStringLiteral("没有想好放在哪里，买回来就闲置了。"),3900,-1},
        {QStringLiteral("这个月的餐饮"),"canteen",{},110000,0},
        {QStringLiteral("交通出行"),"transport",{},22000,0},
        {QStringLiteral("日常用品"),"daily",{},59000,0}
    };
    for (const auto &item:items) {
        Transaction record(0,item.name,item.cents,CategoryCatalog::legacyCategory(item.category),month,QTime(12,0),{},item.category);
        if (!transactions.add(record) || (item.tag && !monthly.setTag(record.id(),item.tag,item.note))) return false;
    }
    WishPoolRepository pool(database);
    for (int i=0;i<6;++i) if (!pool.addSaving(i==5?23900:10000,QStringLiteral("一次忍住的购买"),QStringLiteral("review-fixture-%1").arg(i))) return false;
    MonthlyReviewData data;
    if (!monthly.load(month,data)) return false;
    return monthly.saveStory(month,data.factsHash,
        QStringLiteral("把钱花在真正喜欢的事上。\n这个月记下了 ¥2,486 的支出，忍住了 6 次冲动。\n你把书和跑鞋标为「值得」——常常用到的东西，慢慢成了生活的一部分。"),data.localPersona,"local");
}
}

class ReviewLayoutTests:public QObject {
    Q_OBJECT
private slots:
    void emptyStateAndMonthBoundaries();
    void realRecordsThemesAndSecondaryNavigation();
    void failedRefreshPreservesStoryAndMonth();
    void personalityCardAndStaleHistory();
    void recordsAlwaysHavePersonality();
    void duePurchaseReviewsAppearAboveStory();
};

void ReviewLayoutTests::emptyStateAndMonthBoundaries()
{
    MainWindow window(":memory:"); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Review); settle();
    auto *sections=window.findChild<QTabWidget *>("reviewSectionsTabs");
    QVERIFY(sections->tabBar()->isHidden());
    QCOMPARE(label(window,"monthlyReviewTitleLabel")->text(),QStringLiteral("复盘"));
    QCOMPARE(label(window,"monthlySpentLabel")->text(),QStringLiteral("¥0.00"));
    QCOMPARE(label(window,"monthlyStoryTitleLabel")->text(),QStringLiteral("这个月，还没有记录。"));
    QVERIFY(button(window,"generateMonthlyStoryButton")->isHidden());
    QVERIFY(!window.findChild<QAction *>("saveLocalMonthlyStoryAction")->isEnabled());
    QVERIFY(!button(window,"manageMonthlyTagsButton")->isEnabled());
    QCOMPARE(window.findChild<QWidget *>("monthlyWorthBoard")->findChildren<QWidget *>("monthlyReviewExpenseRow").size(),0);
    QTest::mouseClick(button(window,"monthlyRegretTabButton"),Qt::LeftButton); settle();
    QVERIFY(window.findChild<QWidget *>("monthlyRegretBoard")->isVisible());
    QVERIFY(button(window,"monthlyRegretTabButton")->isChecked());
    QVERIFY(window.findChild<QWidget *>("monthlyRegretBoard")->findChild<QLabel *>("monthlyReviewEmptyBoardLabel")->isVisible());
    QVERIFY(screenshot(window,"empty"));
    auto *month=window.findChild<QDateEdit *>("monthlyReviewMonthEdit");
    month->setDate(QDate(1,1,1)); settle(); QVERIFY(!button(window,"previousReviewMonthButton")->isEnabled());
    month->setDate(QDate(9999,12,1)); settle(); QVERIFY(!button(window,"nextReviewMonthButton")->isEnabled());
}

void ReviewLayoutTests::realRecordsThemesAndSecondaryNavigation()
{
    QTemporaryDir directory; const auto path=directory.filePath("review.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize()); QVERIFY(populate(database));
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Review); settle();
    QCOMPARE(label(window,"monthlySpentLabel")->text(),QStringLiteral("¥2486.00"));
    QCOMPARE(label(window,"monthlyResistCountLabel")->text(),QStringLiteral("6"));
    QCOMPARE(label(window,"monthlyResistedAmountLabel")->text(),QStringLiteral("¥739.00"));
    QCOMPARE(label(window,"monthlyStoryTitleLabel")->text(),QStringLiteral("把钱花在真正喜欢的事上。"));
    QCOMPARE(window.findChild<QWidget *>("monthlyWorthBoard")->findChildren<QWidget *>("monthlyReviewExpenseRow").size(),2);
    QTest::mouseClick(button(window,"monthlyRegretTabButton"),Qt::LeftButton); settle();
    QCOMPARE(window.findChild<QWidget *>("monthlyRegretBoard")->findChild<QLabel *>("monthlyReviewExpenseNameLabel")->text(),QStringLiteral("凑单买的收纳盒"));
    QVERIFY(button(window,"monthlyRegretTabButton")->isChecked());
    QTest::mouseClick(button(window,"monthlyWorthTabButton"),Qt::LeftButton); settle();
    const QList<QString> themes={"warm","cool","dark"};
    auto *scroll=window.findChild<QScrollArea *>("monthlyReviewScrollArea");
    for (const auto &theme:themes) {
        window.showPage(MainWindow::Page::Settings); settle(); button(window,(theme+"ThemeButton").toUtf8().constData())->click();
        window.showPage(MainWindow::Page::Review); window.resize(1180,860); QTest::qWait(50); settle();
        QVERIFY2(scroll->horizontalScrollBar()->maximum()==0,qPrintable(theme));
        auto *statistics=button(window,"monthlyStatisticsButton");
        QVERIFY(scroll->viewport()->rect().contains(QRect(statistics->mapTo(scroll->viewport(),QPoint()),statistics->size())));
        QVERIFY(screenshot(window,theme));
        window.resize(800,600); settle();
        QVERIFY(scroll->horizontalScrollBar()->maximum()==0);
        auto *footer=window.findChild<QFrame *>("shellFooter");
        QVERIFY(window.rect().contains(QRect(footer->mapTo(&window,QPoint()),footer->size())));
        QVERIFY(screenshot(window,"compact-"+theme));
    }
    window.resize(1180,860); settle();
    auto *month=window.findChild<QDateEdit *>("monthlyReviewMonthEdit");
    const QDate selected(2024,10,1); month->setDate(selected); settle();
    QTest::mouseClick(button(window,"monthlyStatisticsButton"),Qt::LeftButton); settle();
    QCOMPARE(window.findChild<QTabWidget *>("reviewSectionsTabs")->currentIndex(),2);
    QCOMPARE(window.findChild<QSpinBox *>("statisticsYearSpinBox")->value(),2024);
    QCOMPARE(window.findChild<QComboBox *>("categoryReviewMonthComboBox")->currentIndex(),9);
    QTest::mouseClick(button(window,"reviewStatisticsBackButton"),Qt::LeftButton); settle();
    QCOMPARE(month->date(),selected);
    QVERIFY(window.findChild<MonthlyReviewWidget *>()->isVisible());
    QTest::mouseClick(button(window,"monthlyPurchaseReviewButton"),Qt::LeftButton); settle();
    QCOMPARE(window.findChild<QTabWidget *>("reviewSectionsTabs")->currentIndex(),1);
    QTest::mouseClick(button(window,"purchaseReviewBackButton"),Qt::LeftButton); settle();
    QCOMPARE(window.findChild<QTabWidget *>("reviewSectionsTabs")->currentIndex(),0);
    QCOMPARE(month->date(),selected);
}

void ReviewLayoutTests::failedRefreshPreservesStoryAndMonth()
{
    QTemporaryDir directory; const auto path=directory.filePath("failure.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize()); QVERIFY(populate(database));
    MainWindow window(path); window.show(); window.showPage(MainWindow::Page::Review); settle();
    const auto text=label(window,"monthlyStoryText")->text();
    auto *month=window.findChild<QDateEdit *>("monthlyReviewMonthEdit"); const auto before=month->date();
    QSqlQuery query(database.database()); QVERIFY(query.exec("ALTER TABLE transaction_reviews RENAME TO unavailable_reviews"));
    month->setDate(before.addMonths(-1)); settle();
    QCOMPARE(month->date(),before);
    QCOMPARE(label(window,"monthlyStoryText")->text(),text);
    QCOMPARE(label(window,"monthlySpentLabel")->text(),QStringLiteral("¥2486.00"));
    QVERIFY(label(window,"monthlyReviewErrorLabel")->isVisible());
    QVERIFY(query.exec("ALTER TABLE unavailable_reviews RENAME TO transaction_reviews"));
    QVERIFY(window.findChild<MonthlyReviewWidget *>()->refresh());
    QVERIFY(label(window,"monthlyReviewErrorLabel")->isHidden());
}

void ReviewLayoutTests::personalityCardAndStaleHistory()
{
    QTemporaryDir directory; const auto path=directory.filePath("personality.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize()); QVERIFY(populate(database));
    MonthlyReviewRepository monthly(database); MonthlyReviewData data;
    QVERIFY(monthly.load(QDate::currentDate(),data)); auto content=MonthlyStoryTemplate::local(data);
    content.interlude=QStringLiteral("收纳盒还在等一个合适的位置。");
    for (const auto &e:data.expenses) if (e.tag==-1) { content.evidenceIds.append(QString::number(e.id)); break; }
    QVERIFY2(monthly.saveStory(data.month,data.factsHash,MonthlyStoryTemplate::encode(content),MonthlyStoryTemplate::personaCaption(content),"local"),qPrintable(monthly.lastError()));
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Review); settle();
    QVERIFY(label(window,"monthlyStoryPersonaLabel")->text().contains(QStringLiteral("松鼠型")));
    QCOMPARE(label(window,"monthlyStoryTitleLabel")->text(),content.title);
    QVERIFY(label(window,"monthlyStoryNextStepLabel")->isHidden());
    QVERIFY(label(window,"monthlyStoryInterludeLabel")->isHidden());
    QVERIFY(button(window,"generateMonthlyStoryButton")->isHidden());
    QVERIFY(window.findChild<QAction *>("regenerateMonthlyStoryAction")->isVisible());
    QTest::mouseClick(button(window,"monthlyStoryExpandButton"),Qt::LeftButton); settle();
    QVERIFY(label(window,"monthlyStoryNextStepLabel")->isVisible());
    QVERIFY(label(window,"monthlyStoryInterludeLabel")->isVisible());
    QVERIFY(screenshot(window,"expanded"));
    QVERIFY(label(window,"monthlyStoryText")->text().contains(content.story.toHtmlEscaped().replace('\n',"<br>")));
    QTest::mouseClick(button(window,"monthlyStoryExpandButton"),Qt::LeftButton); settle();
    QVERIFY(label(window,"monthlyStoryNextStepLabel")->isHidden());
    QVERIFY(!label(window,"monthlyStoryText")->text().contains("persona_id"));
    for (const QString &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) {
        window.showPage(MainWindow::Page::Settings); settle();
        button(window,(theme+"ThemeButton").toUtf8().constData())->click(); window.showPage(MainWindow::Page::Review); QTest::qWait(40);
        QVERIFY(window.findChild<QScrollArea *>("monthlyReviewScrollArea")->horizontalScrollBar()->maximum()==0);
        QVERIFY(screenshot(window,"persona-"+theme));
    }
    window.resize(800,600); QTest::qWait(40); QVERIFY(screenshot(window,"persona-compact"));
    QCOMPARE(window.findChild<QScrollArea *>("monthlyReviewScrollArea")->horizontalScrollBar()->maximum(),0);
    QSqlQuery query(database.database()); QVERIFY(query.exec("UPDATE transactions SET amount_cents=amount_cents+100 WHERE id=(SELECT MIN(id) FROM transactions)"));
    QVERIFY(window.findChild<MonthlyReviewWidget *>()->refresh());
    QVERIFY(label(window,"monthlyStorySourceLabel")->text().contains(QStringLiteral("数据已变化")));
    QVERIFY(!label(window,"monthlyStoryText")->text().contains(content.story));
    QVERIFY(label(window,"monthlyStoryNextStepLabel")->isHidden());
    QVERIFY(label(window,"monthlyStoryPersonaLabel")->text().contains(QStringLiteral("松鼠型")));
    button(window,"monthlyPreviousStoryButton")->click(); settle();
    auto *previous=window.findChild<QDialog *>("previousMonthlyStoryDialog"); QVERIFY(previous && previous->isVisible());
    QVERIFY(previous->findChild<QTextBrowser *>("previousMonthlyStoryText")->toPlainText().contains(content.story));
    previous->accept(); settle(); window.resize(1180,860); QTest::qWait(40); QVERIFY(screenshot(window,"persona-stale"));
    QVERIFY(monthly.load(data.month,data));
    const auto legacy=QStringLiteral("旧版长故事，保留原始内容。\n").repeated(80);
    QVERIFY(monthly.saveStory(data.month,data.factsHash,legacy,"legacy","local"));
    QVERIFY(window.findChild<MonthlyReviewWidget *>()->refresh());
    QVERIFY(label(window,"monthlyStoryText")->text().size()<400);
    QVERIFY(button(window,"monthlyStoryExpandButton")->isVisible());
    button(window,"monthlyStoryExpandButton")->click(); settle();
    QVERIFY(label(window,"monthlyStoryText")->text().contains(legacy.section('\n',1).trimmed().toHtmlEscaped().replace('\n',"<br>")));
    button(window,"monthlyStoryExpandButton")->click(); settle();
    QVERIFY(label(window,"monthlyStoryText")->text().size()<400);
}

void ReviewLayoutTests::recordsAlwaysHavePersonality()
{
    QTemporaryDir directory; const auto path=directory.filePath("always-persona.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize()); TransactionRepository transactions(database);
    const auto month=MonthlyBudget::firstDay(QDate::currentDate());
    Transaction item(0,QStringLiteral("午饭"),1500,QStringLiteral("餐饮"),month,QTime(12,0),{},"canteen"); QVERIFY(transactions.add(item));
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Review); QTest::qWait(40);
    QVERIFY(label(window,"monthlyStoryPersonaLabel")->text().contains(QStringLiteral("烟火小猫型")));
    QVERIFY(!label(window,"monthlyStoryPersonaLabel")->text().contains(QStringLiteral("待解锁")));
    window.findChild<QAction *>("saveLocalMonthlyStoryAction")->trigger(); settle();
    MonthlyReviewRepository monthly(database); MonthlyReviewData data; MonthlyStory saved;
    QVERIFY(monthly.load(month,data)); QVERIFY(monthly.loadStory(data,saved));
    ReviewStoryContent card; QString error; QVERIFY(MonthlyStoryTemplate::decode(saved.story,card,error));
    QCOMPARE(card.personaId,QStringLiteral("custom")); QCOMPARE(card.customName,QStringLiteral("烟火小猫型"));
    QVERIFY(screenshot(window,"exclusive-persona"));
    auto old=QJsonDocument::fromJson(saved.story.toUtf8()).object(); old["schema_version"]=1; old["persona_id"]="pending"; old["subtitle"]=QString(); old.remove("persona_name");
    QSqlQuery query(database.database()); query.prepare("UPDATE monthly_stories SET story=:story"); query.bindValue(":story",QString::fromUtf8(QJsonDocument(old).toJson(QJsonDocument::Compact))); QVERIFY(query.exec());
    QVERIFY(window.findChild<MonthlyReviewWidget *>()->refresh());
    QVERIFY(label(window,"monthlyStoryPersonaLabel")->text().contains(QStringLiteral("烟火小猫型")));
    QVERIFY(label(window,"monthlyStorySourceLabel")->text().contains(QStringLiteral("本地人格预览")));
    WishPoolRepository savings(database); QVERIFY(savings.addSaving(1500,QStringLiteral("一次忍住"),"always-persona-first")); QVERIFY(savings.addSaving(3000,QStringLiteral("再一次忍住"),"always-persona-second"));
    QVERIFY(window.findChild<MonthlyReviewWidget *>()->refresh());
    QVERIFY(label(window,"monthlyStoryPersonaLabel")->text().contains(QStringLiteral("松鼠型")));
    window.findChild<QAction *>("saveLocalMonthlyStoryAction")->trigger(); settle();
    QVERIFY(monthly.load(month,data)); QVERIFY(monthly.loadStory(data,saved)); QVERIFY(MonthlyStoryTemplate::decode(saved.story,card,error));
    QCOMPARE(card.personaId,QStringLiteral("squirrel"));
}

void ReviewLayoutTests::duePurchaseReviewsAppearAboveStory()
{
    QTemporaryDir directory; const auto path=directory.filePath("due.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize());
    TransactionRepository transactions(database);
    Transaction item(0,QStringLiteral("待回顾的跑鞋"),8000,QStringLiteral("购物"),QDate::currentDate().addDays(-8));
    QVERIFY(transactions.add(item)); ReviewRepository reviews(database);
    QVERIFY(reviews.schedule(item.id()));
    MainWindow window(path); window.resize(1180,860); window.show(); window.showPage(MainWindow::Page::Review); settle();
    auto *due=button(window,"monthlyDueReviewsButton"); QVERIFY(due->isVisible());
    QVERIFY(due->text().contains(QStringLiteral("1 项")));
    auto *story=window.findChild<QFrame *>("monthlyStoryCard"); QVERIFY(due->geometry().bottom()<story->geometry().top());
    QTest::mouseClick(due,Qt::LeftButton); settle();
    QCOMPARE(window.findChild<QTabWidget *>("reviewSectionsTabs")->currentIndex(),1);
    auto *table=window.findChild<QTableWidget *>("purchaseReviewTable"); table->selectRow(0); settle();
    QTest::mouseClick(button(window,"openPurchaseReviewButton"),Qt::LeftButton); settle();
    auto *dialog=window.findChild<QDialog *>("purchaseReviewDialog"); QVERIFY(dialog);
    dialog->findChild<QComboBox *>("reviewFrequencyComboBox")->setCurrentIndex(2);
    dialog->findChild<QComboBox *>("reviewWorthinessComboBox")->setCurrentIndex(1);
    dialog->findChild<QPushButton *>("savePurchaseReviewButton")->click(); settle();
    QTest::mouseClick(button(window,"purchaseReviewBackButton"),Qt::LeftButton); settle();
    QVERIFY(due->isHidden());
    qint64 count=-1; QVERIFY(reviews.dueCount(count)); QCOMPARE(count,0);
}

QTEST_MAIN(ReviewLayoutTests)
#include "review_layout_tests.moc"
