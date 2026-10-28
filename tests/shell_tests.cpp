#include "ExecutionWidget.h"
#include "mainwindow.h"
#include "DemoData.h"
#include "HomeWidget.h"
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
};

void ShellTests::navigationKeepsDraftsAndFilters()
{
    QTemporaryDir directory;
    MainWindow window(directory.filePath("shell.sqlite"));
    window.show(); settle();
    QCOMPARE(window.currentPage(), MainWindow::Page::Home);
    click(window, "billingNavButton");
    auto *category = window.findChild<QComboBox *>("categoryFilter");
    category->setCurrentText(QStringLiteral("餐饮"));
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
    QCOMPARE(category->currentText(), QStringLiteral("餐饮"));
    QVERIFY(label(window, "appliedFilterLabel").contains(QStringLiteral("餐饮")));
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
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥63.00"));
    QCOMPARE(window.findChild<QProgressBar *>("homeBudgetProgress")->value(), 790);
    QVERIFY(label(window, "homeWishCountLabel").startsWith(QStringLiteral("2 件")));
    QVERIFY(label(window, "homeWishDetailLabel").contains(QStringLiteral("蓝牙耳机")));
    click(window, "homeBudgetButton");
    window.findChild<QDoubleSpinBox *>("basicBudgetAmountSpinBox")->setValue(0);
    window.findChild<QDoubleSpinBox *>("budgetAmountSpinBox")->setValue(200);
    window.findChild<QPushButton *>("saveBudgetButton")->click();
    click(window, "homeNavButton");
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("-¥37.00"));
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
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("-¥47.00"));
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
                    const auto chart = window.findChild<QWidget *>(QStringLiteral("monthlyTrendChart"))->grab().toImage();
                    QVERIFY(chart.pixelColor(0,0).lightness() < 100);
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
    QCOMPARE(label(restarted, "homeBudgetRemainingLabel"), QStringLiteral("¥63.00"));
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
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥63.00"));
    BudgetRepository budget(database);
    QVERIFY(budget.save(MonthlyBudget(QDate::currentDate(), 0)));
    QSqlQuery query(database.database());
    QVERIFY(query.exec(QStringLiteral("ALTER TABLE wishlist RENAME TO unavailable_wishes")));
    QVERIFY(!home->refresh());
    QCOMPARE(label(window, "homeBudgetRemainingLabel"), QStringLiteral("¥63.00"));
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
