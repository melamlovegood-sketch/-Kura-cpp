#include "ExecutionWidget.h"
#include "mainwindow.h"
#include "AppTheme.h"
#include "DemoData.h"
#include "HomeWidget.h"
#include "WishlistWidget.h"
#include "WishPoolWidget.h"
#include "ProductDetailsDialog.h"
#include "AppearanceWidget.h"
#include "AddTransactionDialog.h"
#include "WishDialog.h"
#include "BudgetRepository.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QScrollBar>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>

static void settle()
{
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
static void click(MainWindow &window, const char *name)
{
    auto *button = window.findChild<QAbstractButton *>(QLatin1String(name));
    QVERIFY2(button, name);
    QVERIFY2(button->isVisible(), name);
    QTest::mouseClick(button, Qt::LeftButton);
    settle();
}
static QString label(MainWindow &window, const char *name)
{
    return window.findChild<QLabel *>(QLatin1String(name))->text();
}
class ShellTests : public QObject
{
    Q_OBJECT
private slots:
    void navigationKeepsDraftsAndFilters();
    void homeUsesRealDataAndPurchaseActions();
    void themesPersistAndAllPagesFit();
    void failedHomeRefreshKeepsLastSnapshot();
    void emptyAndUnavailableData();
    void settingsSectionsKeepDrafts();
    void primaryPagesShareCanvasAndReflow();
    void homeReadyActionsAndResume();
    void editorialPagesFitAtLargeScale();
};

void ShellTests::editorialPagesFitAtLargeScale()
{
    QTemporaryDir directory;
    const auto path=directory.filePath("editorial.sqlite");
    { DatabaseManager database(path); QVERIFY(database.initialize()); QString error; QVERIFY(DemoData::populate(database,error)); }
    MainWindow window(path); window.resize(800,600); window.show(); settle();
    const auto preview=qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) QVERIFY(QDir().mkpath(preview));
    const QList<MainWindow::Page> pages={MainWindow::Page::Home,MainWindow::Page::Billing,MainWindow::Page::Wishlist,MainWindow::Page::Execution,MainWindow::Page::Review};
    for (const auto &theme:{"warm","cool","dark"}) {
        for (int scale:{100,115,130}) {
            window.setProperty("fontScale",scale);
            AppTheme::apply(&window,QLatin1String(theme));
            for (int i=0;i<pages.size();++i) {
                window.showPage(pages[i]); settle();
                QCOMPARE(window.size(),QSize(800,600));
                for (auto *scroll:window.findChildren<QScrollArea *>()) if (scroll->isVisible())
                    QCOMPARE(scroll->horizontalScrollBar()->maximum(),0);
                if (i==0) {
                    auto *title=window.findChild<QLabel *>("homePageTitle"); QVERIFY(title);
                    QCOMPARE(title->font().weight(),QFont::Normal);
                    QVERIFY(title->height()>=title->heightForWidth(title->width()));
                    QVERIFY(title->font().families().contains("LXGW WenKai Lite"));
                }
                if (!preview.isEmpty() && scale==130)
                    QVERIFY(window.grab().save(preview+QStringLiteral("/editorial-large-%1-%2.png").arg(QLatin1String(theme)).arg(i)));
            }
        }
    }
}

void ShellTests::primaryPagesShareCanvasAndReflow()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("page-layout.sqlite");
    { DatabaseManager database(path); QVERIFY(database.initialize()); QString error; QVERIFY(DemoData::populate(database,error)); }
    MainWindow window(path);
    window.resize(1440, 1000); window.show(); settle();
    const auto preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) QVERIFY(QDir().mkpath(preview));
    const QList<MainWindow::Page> pages = {MainWindow::Page::Home, MainWindow::Page::Billing,
        MainWindow::Page::Wishlist, MainWindow::Page::Execution, MainWindow::Page::Review};
    const QStringList titles = {"homePageTitle", "billingPageTitle", "planningTitleLabel",
        "executionHeading", "monthlyReviewTitleLabel"};
    const QStringList scrolls = {"homeScrollArea", "billingWorkspaceScrollArea", "planningScrollArea",
        "executionScrollArea", "monthlyReviewScrollArea"};
    QPoint titleOrigin;
    int contentWidth = 0;
    for (int i=0; i<pages.size(); ++i) {
        window.showPage(pages[i]); settle();
        auto *title = window.findChild<QLabel *>(titles[i]); QVERIFY(title);
        auto *scroll = window.findChild<QScrollArea *>(scrolls[i]); QVERIFY(scroll);
        const auto origin = title->mapTo(&window, QPoint());
        if (!i) { titleOrigin = origin; contentWidth = title->parentWidget()->width(); }
        QVERIFY2(qAbs(origin.x()-titleOrigin.x()) <= 3, qPrintable(titles[i]));
        QVERIFY2(qAbs(origin.y()-titleOrigin.y()) <= 3, qPrintable(titles[i]));
        QCOMPARE(title->parentWidget()->width(), contentWidth);
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        if (pages[i] == MainWindow::Page::Review) {
            auto *body = window.findChild<QLabel *>("monthlyStoryText"); QVERIFY(body);
            QVERIFY(body->width() >= 700);
            QVERIFY(body->height() >= body->heightForWidth(body->width()));
        }
        if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+QStringLiteral("/layout-wide-%1.png").arg(i)));
    }
    window.showPage(MainWindow::Page::Home); settle();
    auto *pool = window.findChild<QWidget *>("wishPoolWidget");
    auto *budget = window.findChild<QWidget *>("homeBudgetCard");
    auto *action = window.findChild<QPushButton *>("homeDecisionButton");
    QVERIFY(pool && budget && action);
    QVERIFY(pool->x() > budget->geometry().right());
    QVERIFY(window.findChild<QWidget *>("homeReadySection")->y() > pool->geometry().bottom());
    QVERIFY(action->mapTo(&window,QPoint()).y() < window.findChild<QWidget *>("homeBudgetCard")->mapTo(&window,QPoint()).y());
    window.showPage(MainWindow::Page::Execution); settle();
    auto *steps = window.findChild<QWidget *>("executionSteps");
    auto *rules = window.findChild<QWidget *>("executionRulesCard");
    QVERIFY(steps && rules); QVERIFY(rules->x() > steps->geometry().right());
    window.resize(800,600); settle();
    for (int i=0; i<pages.size(); ++i) {
        window.showPage(pages[i]); settle();
        QCOMPARE(window.size(), QSize(800,600));
        QCOMPARE(window.findChild<QScrollArea *>(scrolls[i])->horizontalScrollBar()->maximum(), 0);
        if (pages[i] == MainWindow::Page::Home) QVERIFY(pool->y() >= budget->geometry().bottom());
        if (pages[i] == MainWindow::Page::Execution) QVERIFY(rules->y() >= steps->geometry().bottom());
        if (!preview.isEmpty()) QVERIFY(window.grab().save(preview+QStringLiteral("/layout-compact-%1.png").arg(i)));
    }
    window.showPage(MainWindow::Page::Review); settle();
    auto *metrics = window.findChild<QWidget *>("monthlyReviewMetrics");
    auto *story = window.findChild<QWidget *>("monthlyStoryCard");
    QVERIFY(metrics && story); QVERIFY(metrics->geometry().bottom() < story->y());
}

void ShellTests::homeReadyActionsAndResume()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("home-actions.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize());
    WishlistRepository wishes(database);
    QList<WishItem> items;
    const auto now = QDateTime::currentDateTimeUtc();
    for (int i = 0; i < 4; ++i) {
        WishItem wish; wish.name = QStringLiteral("待考虑的商品 %1").arg(i);
        wish.estimatedCents = 10000 + i; wish.coolingDays = 1;
        QVERIFY(wishes.add(wish, i == 3 ? now : now.addDays(-5+i)));
        items.append(wish);
    }
    WishPoolRepository pool(database); QVERIFY(pool.setGoal(items[0].id));
    QVERIFY(pool.addSaving(3000, QStringLiteral("一次消费选择"), "home-saving"));
    ExecutionRepository executions(database);
    ExecutionSession session; session.name = QStringLiteral("进行中的键盘分析");
    session.category = "online_shopping"; session.estimatedCents = 25000;
    session.mode = "research";
    QVERIFY(executions.start(session));
    QVERIFY(executions.saveNotes(session.id, QStringLiteral("已经比较过参数"), QString()));
    MainWindow window(path); window.resize(1180, 860); window.show(); settle();
    auto *home = window.findChild<HomeWidget *>();
    QCOMPARE(label(window, "poolTitleLabel"), QStringLiteral("许愿池"));
    QVERIFY(label(window, "poolAmountLabel").contains(QStringLiteral("忍住金额累计 ¥30.00")));
    click(window, "poolChooseGoalButton");
    auto *details = window.findChild<ProductDetailsDialog *>(); QVERIFY(details);
    details->reject(); settle();
    QVERIFY(window.findChild<QWidget *>("homeResumeCard")->isVisible());
    QVERIFY(label(window, "homeResumeNameLabel").contains(session.name));
    QVERIFY(label(window, "homeReadySummaryLabel").startsWith(QStringLiteral("3 件")));
    const auto rows = window.findChildren<QWidget *>("homeReadyRow");
    QCOMPARE(rows.size(), 2);
    auto *first = rows[0]->findChild<QPushButton *>("homeReadyWishButton");
    QCOMPARE(first->property("wishId").toLongLong(), items[0].id);
    QCOMPARE(rows[1]->findChild<QPushButton *>("homeReadyWishButton")->property("wishId").toLongLong(), items[1].id);
    // A shortcut must override a previous archive filter and select the exact product.
    window.resize(800, 600); settle();
    window.findChild<QComboBox *>("wishlistStatusFilter")->setCurrentIndex(4);
    first->click(); settle();
    QCOMPARE(window.currentPage(), MainWindow::Page::Wishlist);
    auto *table = window.findChild<QTableWidget *>("wishlistTable");
    QCOMPARE(table->rowCount(), 3);
    QVERIFY(table->currentRow() >= 0);
    QCOMPARE(table->item(table->currentRow(),0)->data(Qt::UserRole).toLongLong(), items[0].id);
    QVERIFY(window.findChild<QPushButton *>("purchaseWishButton")->isVisible());
    QVERIFY(window.findChild<QScrollArea *>("planningScrollArea")->verticalScrollBar()->value() > 0);
    QVERIFY(!window.findChild<AddTransactionDialog *>());
    window.showPage(MainWindow::Page::Home); settle();
    click(window, "homeContinueButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Execution);
    QCOMPARE(window.findChild<ExecutionWidget *>()->activeSessionId(), session.id);
    ExecutionSession loaded; QVERIFY(executions.loadActive(loaded));
    QCOMPARE(loaded.notes, QStringLiteral("已经比较过参数"));
    // Closing a wish between refresh and click must not select or buy another item.
    window.showPage(MainWindow::Page::Home); settle();
    QVERIFY(wishes.abandon(items[0].id));
    first->click(); settle();
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->currentRow(), -1);
    QVERIFY(window.findChild<QLabel *>("wishlistErrorLabel")->isVisible());
    window.showPage(MainWindow::Page::Home); settle();
    click(window, "homeReadyAllButton");
    QCOMPARE(table->rowCount(), 2);
    window.showPage(MainWindow::Page::Home); settle();
    QVERIFY(wishes.abandon(items[1].id)); QVERIFY(wishes.abandon(items[2].id));
    QVERIFY(executions.finish(session.id, "skipped"));
    QVERIFY(home->refresh()); settle();
    QVERIFY(window.findChild<QWidget *>("homeReadySection")->isHidden());
    QVERIFY(window.findChild<QWidget *>("homeResumeCard")->isHidden());
    TransactionRepository transactions(database); QList<Transaction> expenses;
    QVERIFY(transactions.findAll(expenses)); QVERIFY(expenses.isEmpty());
}

void ShellTests::navigationKeepsDraftsAndFilters()
{
    QTemporaryDir directory;
    MainWindow window(directory.filePath("shell.sqlite"));
    window.show(); settle();
    QCOMPARE(window.currentPage(), MainWindow::Page::Home);
    click(window, "billingNavButton");
    auto *category = window.findChild<QComboBox *>("categoryFilter");
    category->setCurrentText(QStringLiteral("吃喝"));
    window.findChild<QPushButton *>("applyFilterButton")->click();
    auto *budget = window.findChild<QDoubleSpinBox *>("budgetAmountSpinBox");
    budget->setValue(432.10); // Unsaved input survives navigation and theme changes.
    click(window, "wishlistNavButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Wishlist);
    click(window, "executionNavButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Execution);
    click(window, "reviewNavButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Review);
    click(window, "settingsButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Settings);
    click(window, "settingsAppearanceButton");
    click(window, "darkThemeButton");
    click(window, "settingsBackButton");
    click(window, "settingsDataButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Data);
    QVERIFY(window.findChild<QToolButton *>("settingsButton")->isChecked());
    click(window, "dataBackButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Settings);
    click(window, "billingNavButton");
    QCOMPARE(budget->value(), 432.10);
    QCOMPARE(category->currentText(), QStringLiteral("吃喝"));
    QVERIFY(label(window, "appliedFilterLabel").contains(QStringLiteral("吃喝")));
    QCOMPARE(window.findChild<QStackedWidget *>("mainPages")->count(), 7);
    int active = 0;
    for (auto *button : window.findChildren<QToolButton *>())
        if (button->objectName().endsWith("NavButton") && button->isChecked()) ++active;
    QCOMPARE(active, 1);
}

void ShellTests::homeUsesRealDataAndPurchaseActions()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("home.sqlite");
    { DatabaseManager database(path); QVERIFY(database.initialize()); QString error; QVERIFY(DemoData::populate(database, error)); }
    MainWindow window(path); window.show(); settle();
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥19.00"));
    QCOMPARE(window.findChild<QProgressBar *>("homeBudgetProgress")->value(), 790);
    QVERIFY(label(window, "homeReadySummaryLabel").startsWith(QStringLiteral("1 件")));
    QCOMPARE(label(window, "homeReadyNameLabel"), QStringLiteral("蓝牙耳机"));
    QCOMPARE(label(window, "poolTitleLabel"), QStringLiteral("许愿池"));
    QVERIFY(!window.findChild<QWidget *>("workflowOverview"));
    QVERIFY(!window.findChild<QWidget *>("homeWishCard"));
    click(window, "homeBudgetButton");
    window.findChild<QDoubleSpinBox *>("basicBudgetAmountSpinBox")->setValue(0);
    window.findChild<QDoubleSpinBox *>("budgetAmountSpinBox")->setValue(200);
    window.findChild<QPushButton *>("saveBudgetButton")->click();
    click(window, "homeNavButton");
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥19.00"));
    click(window, "executionNavButton");
    window.findChild<QLineEdit *>("executionNameEdit")->setText(QStringLiteral("桌面版入口购买"));
    window.findChild<QDoubleSpinBox *>("executionEstimatedAmountSpinBox")->setValue(10);
    { ParsedProduct p; p.name=window.findChild<QLineEdit *>("executionNameEdit")->text(); p.amountCents=qRound64(window.findChild<QDoubleSpinBox *>("executionEstimatedAmountSpinBox")->value()*100); QVERIFY(window.findChild<ExecutionWidget *>()->prepareDecision(p)); }
    click(window, "startExecutionButton");
    if (window.findChild<QPushButton *>("executionFastDecideButton")->isVisible()) click(window,"executionFastDecideButton");
    click(window, "executionBuyButton");
    auto *dialog = window.findChild<AddTransactionDialog *>(); QVERIFY(dialog && dialog->isVisible());
    dialog->findChild<QLineEdit *>("descriptionEdit")->setText(QStringLiteral("桌面版入口购买"));
    dialog->findChild<QDoubleSpinBox *>("amountSpinBox")->setValue(10);
    dialog->findChild<QPushButton *>("saveButton")->click(); settle();
    QVERIFY(!window.findChild<AddTransactionDialog *>());
    click(window, "homeNavButton");
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥9.00"));
    click(window, "executionNavButton");
    window.findChild<QLineEdit *>("executionNameEdit")->setText(QStringLiteral("再想一想的商品"));
    window.findChild<QDoubleSpinBox *>("executionEstimatedAmountSpinBox")->setValue(20);
    { ParsedProduct p; p.name=window.findChild<QLineEdit *>("executionNameEdit")->text(); p.amountCents=qRound64(window.findChild<QDoubleSpinBox *>("executionEstimatedAmountSpinBox")->value()*100); QVERIFY(window.findChild<ExecutionWidget *>()->prepareDecision(p)); }
    click(window, "startExecutionButton");
    if (window.findChild<QPushButton *>("executionFastDecideButton")->isVisible()) click(window,"executionFastDecideButton");
    click(window, "executionWishButton");
    QCOMPARE(window.currentPage(), MainWindow::Page::Execution);
    auto *wish = window.findChild<WishDialog *>(); QVERIFY(wish && wish->isVisible());
    wish->reject(); settle();
    window.findChild<QComboBox *>("wishlistStatusFilter")->setCurrentIndex(0);
    QCOMPARE(window.findChild<QTableWidget *>("wishlistTable")->rowCount(), 4);
}

void ShellTests::themesPersistAndAllPagesFit()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("appearance.sqlite");
    { DatabaseManager database(path); QVERIFY(database.initialize()); QString error; QVERIFY(DemoData::populate(database, error)); }
    const QString preview = qEnvironmentVariable("KURA_PREVIEW_DIR");
    if (!preview.isEmpty()) QVERIFY(QDir().mkpath(preview));
    {
        MainWindow window(path); window.resize(1180, 860); window.show(); settle();
        const QList<MainWindow::Page> pages = {MainWindow::Page::Home, MainWindow::Page::Billing, MainWindow::Page::Wishlist,
            MainWindow::Page::Execution, MainWindow::Page::Review, MainWindow::Page::Settings, MainWindow::Page::Data};
        const QStringList names = {"home", "billing", "wishlist", "execution", "review", "settings", "data"};
        for (const QString &theme : {QStringLiteral("warm"), QStringLiteral("cool"), QStringLiteral("dark")}) {
            window.showPage(MainWindow::Page::Settings); settle();
            auto *choice = window.findChild<QPushButton *>(theme + "ThemeButton"); choice->click(); settle();
            QCOMPARE(window.property("theme").toString(), theme);
            QVERIFY(choice->isChecked());
            for (int i=0; i<pages.size(); ++i) {
                window.showPage(pages[i]); settle();
                if (pages[i] == MainWindow::Page::Review && theme == QStringLiteral("dark")) {
                    auto *story = window.findChild<QFrame *>(QStringLiteral("monthlyStoryCard"));
                    QVERIFY(story);
                    const auto image = story->grab().toImage();
                    QVERIFY(image.pixelColor(0,0).lightness() < 100);
                }
                QVERIFY(window.findChild<QFrame *>("shellFooter")->isVisible());
                QVERIFY(window.rect().contains(window.findChild<QFrame *>("shellFooter")->geometry()));
                if (!preview.isEmpty()) QVERIFY(window.grab().save(preview + "/shell-" + names[i] + "-" + theme + ".png"));
            }
            window.resize(800, 600); settle();
            QCOMPARE(window.size(), QSize(800,600));
            for (int i=0; i<pages.size(); ++i) {
                window.showPage(pages[i]); settle();
                auto *footer = window.findChild<QFrame *>("shellFooter");
                QVERIFY(footer->isVisible());
                QVERIFY(window.rect().contains(QRect(footer->mapTo(&window, QPoint()), footer->size())));
                if (!preview.isEmpty()) QVERIFY(window.grab().save(preview + "/shell-compact-" + names[i] + "-" + theme + ".png"));
            }
            window.resize(1180,860);
        }
    }
    MainWindow restarted(path);
    QCOMPARE(restarted.property("theme").toString(), QStringLiteral("dark"));
    QCOMPARE(restarted.currentPage(), MainWindow::Page::Home);
    QCOMPARE(label(restarted, "homeBudgetRemainingLabel"), QStringLiteral("¥19.00"));
}

void ShellTests::emptyAndUnavailableData()
{
    MainWindow empty(QStringLiteral(":memory:")); empty.show(); settle();
    QCOMPARE(label(empty, "homeBudgetRemainingLabel"), QStringLiteral("还没有设置预算"));
    QVERIFY(label(empty, "homeBudgetDetailLabel").contains(QStringLiteral("¥0.00")));
    QTemporaryDir directory;
    MainWindow unavailable(directory.path()); unavailable.show(); settle();
    QCOMPARE(label(unavailable, "homeBudgetRemainingLabel"), QStringLiteral("—"));
    QVERIFY(unavailable.findChild<QLabel *>("homeErrorLabel")->isVisible());
    QVERIFY(!unavailable.findChild<QPushButton *>("homeDecisionButton")->isEnabled());
    click(unavailable, "executionNavButton");
    QVERIFY(!unavailable.findChild<QPushButton *>("executionBuyButton")->isEnabled());
}

void ShellTests::failedHomeRefreshKeepsLastSnapshot()
{
    QTemporaryDir directory;
    const auto path = directory.filePath("snapshot.sqlite");
    DatabaseManager database(path); QVERIFY(database.initialize());
    QString error; QVERIFY(DemoData::populate(database, error));
    MainWindow window(path); window.show(); settle();
    auto *home = window.findChild<HomeWidget *>();
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥19.00"));
    BudgetRepository budget(database);
    QVERIFY(budget.save(MonthlyBudget(QDate::currentDate(), 0)));
    QSqlQuery query(database.database());
    QVERIFY(query.exec(QStringLiteral("ALTER TABLE wishlist RENAME TO unavailable_wishes")));
    QVERIFY(!home->refresh());
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥19.00"));
    QVERIFY(window.findChild<QLabel *>("homeErrorLabel")->isVisible());
    QVERIFY(query.exec(QStringLiteral("ALTER TABLE unavailable_wishes RENAME TO wishlist")));
    QVERIFY(home->refresh());
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("-¥237.00"));
    QCOMPARE(window.findChild<QProgressBar *>("homeBudgetProgress")->value(), 1000);
    QVERIFY(window.findChild<QLabel *>("homeErrorLabel")->isHidden());
}
void ShellTests::settingsSectionsKeepDrafts()
{
    QTemporaryDir directory; MainWindow window(directory.filePath("settings.sqlite"));
    window.resize(800,600); window.show(); window.showPage(MainWindow::Page::Settings); settle();
    QVERIFY(window.findChild<QWidget *>("settingsHome")->isVisible());
    QVERIFY(!window.findChild<QLineEdit *>("aiBaseUrlEdit")->isVisible());
    QVERIFY(!window.findChild<QPushButton *>("warmThemeButton")->isVisible());
    const auto capture=[&](const QString &name) {
        settle(); const auto dir=qEnvironmentVariable("KURA_PREVIEW_DIR");
        return dir.isEmpty() || (QDir().mkpath(dir) && window.grab().save(dir+"/settings-refresh-"+name+".png"));
    };
    for (const QString &theme:{QStringLiteral("warm"),QStringLiteral("cool"),QStringLiteral("dark")}) {
        click(window,"settingsAppearanceButton"); click(window,(theme+"ThemeButton").toUtf8().constData());
        QVERIFY(capture(theme+"-appearance")); click(window,"settingsBackButton");
        QVERIFY(capture(theme+"-home"));
    }
    click(window,"settingsAIButton");
    auto *model=window.findChild<QLineEdit *>("aiModelEdit"); model->setText("unsaved-draft-model");
    QVERIFY(!window.findChild<QPushButton *>("cancelAIConnectionTestButton")->isVisible());
    QVERIFY(capture("dark-ai")); click(window,"settingsBackButton"); click(window,"settingsAIButton");
    QCOMPARE(model->text(),QStringLiteral("unsaved-draft-model"));
    click(window,"settingsBackButton"); click(window,"settingsDataButton"); click(window,"dataBackButton");
    QVERIFY(window.findChild<QWidget *>("settingsHome")->isVisible());
}

QTEST_MAIN(ShellTests)
#include "shell_tests.moc"
