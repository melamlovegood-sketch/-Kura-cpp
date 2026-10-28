#include "ModernSelectors.h"
#include "mainwindow.h"
#include "AddTransactionDialog.h"
#include "ExpenseSummary.h"
#include "CategorySummary.h"
#include "BudgetWidget.h"
#include "BillingWorkspace.h"
#include "BillImportDialog.h"
#include <QDialog>
#include "StatisticsWidget.h"
#include "WishlistWidget.h"
#include "DataManagementWidget.h"
#include "AppTheme.h"
#include "HomeWidget.h"
#include "AppearanceWidget.h"
#include "ShellWidgets.h"
#include "AISettings.h"
#include "DecisionDialog.h"
#include "WishPoolRepository.h"
#include "CategoryCatalog.h"
#include "ExecutionWidget.h"
#include "ReviewWidget.h"
#include "MonthlyReviewWidget.h"
#include "PriceWidget.h"
#include "ProductDetailsDialog.h"
#include "ShoppingProfileDialog.h"
#include "PurchaseRepository.h"
#include <QTabWidget>
#include <QTabBar>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QScrollArea>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QTableWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(QString(), parent)
{
}

MainWindow::MainWindow(const QString &databasePath, QWidget *parent)
    : QMainWindow(parent), m_database(databasePath), m_repository(m_database)
{
    setWindowTitle(QStringLiteral("Kura C++"));
    m_preferencesPath = m_database.databasePath() == QStringLiteral(":memory:")
        ? QString() : m_database.databasePath() + QStringLiteral(".ui.ini");
    m_theme = QStringLiteral("warm");
    if (!m_preferencesPath.isEmpty()) {
        QSettings preferences(m_preferencesPath, QSettings::IniFormat);
        m_theme = AppTheme::normalized(preferences.value(QStringLiteral("appearance/theme")).toString());
    }
    AppTheme::apply(this, m_theme);
    const QSize screen = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry().size() : QSize(1240, 960);
    resize(qBound(800, screen.width() - 60, 1180), qBound(600, screen.height() - 90, 860));
    setMinimumSize(800, 600);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("mainPages"));
    auto *central = new QWidget(m_pages);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *heading = new QHBoxLayout;
    auto *backToBilling = new QPushButton(QStringLiteral("← 返回记账"), central);
    backToBilling->setObjectName("billingRecordsBackButton");
    heading->addWidget(backToBilling);
    auto *title = new QLabel(QStringLiteral("全部记录"), central);
    title->setProperty("heading", true);
    heading->addWidget(title);
    heading->addStretch();

    m_addButton = new QPushButton(QStringLiteral("记一笔"), central);
    m_addButton->setObjectName(QStringLiteral("addTransactionButton"));
    m_addButton->setMinimumHeight(36);
    m_editButton = new QPushButton(QStringLiteral("修改"), central);
    m_editButton->setObjectName(QStringLiteral("editTransactionButton"));
    m_deleteButton = new QPushButton(QStringLiteral("删除"), central);
    m_deleteButton->setObjectName(QStringLiteral("deleteTransactionButton"));
    heading->addWidget(m_addButton);
    heading->addWidget(m_editButton);
    heading->addWidget(m_deleteButton);
    layout->addLayout(heading);

    m_budgetDialog = new QDialog(this);
    m_budgetDialog->setObjectName("billingBudgetDialog");
    m_budgetDialog->setWindowTitle(QStringLiteral("调整月度预算"));
    auto *budgetLayout = new QVBoxLayout(m_budgetDialog);
    budgetLayout->setContentsMargins(24,24,24,24);
    m_budgetWidget = new BudgetWidget(m_database, m_budgetDialog);
    budgetLayout->addWidget(m_budgetWidget);
    auto *budgetClose = new QPushButton(QStringLiteral("完成"), m_budgetDialog);
    budgetLayout->addWidget(budgetClose, 0, Qt::AlignRight);
    connect(budgetClose, &QPushButton::clicked, m_budgetDialog, &QDialog::accept);

    auto *totals = new QHBoxLayout;
    m_totalLabel = new QLabel(QStringLiteral("全部支出：—"), central);
    m_totalLabel->setObjectName(QStringLiteral("totalExpenseLabel"));
    m_monthLabel = new QLabel(QStringLiteral("本月支出：—"), central);
    m_monthLabel->setObjectName(QStringLiteral("monthExpenseLabel"));
    m_todayLabel = new QLabel(QStringLiteral("今日支出：—"), central);
    m_todayLabel->setObjectName(QStringLiteral("todayExpenseLabel"));
    // Retain aggregate snapshots for other consumers; the records page shows only its filtered result.
    m_totalLabel->hide(); m_monthLabel->hide(); m_todayLabel->hide();
    delete totals;

    auto *filters = new QHBoxLayout;
    filters->addWidget(new QLabel(QStringLiteral("汇总分类"), central));
    m_categoryFilter = new ModernComboBox(central);
    m_categoryFilter->setObjectName(QStringLiteral("categoryFilter"));
    m_categoryFilter->addItem(QStringLiteral("全部分类"), QString());
    for (const auto &category : Transaction::categories())
        m_categoryFilter->addItem(category, category);
    filters->addWidget(m_categoryFilter);
    m_useDateRange = new QCheckBox(QStringLiteral("按日期"), central);
    m_useDateRange->setObjectName(QStringLiteral("useDateRangeCheckBox"));
    filters->addWidget(m_useDateRange);
    const QDate today = QDate::currentDate();
    m_startDate = new DatePicker(QDate(today.year(), today.month(), 1), central);
    m_startDate->setObjectName(QStringLiteral("startDateFilter"));
    m_startDate->setDateRange(QDate(1, 1, 1), QDate(9999, 12, 31));
    m_startDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_startDate->setCalendarPopup(true);
    m_startDate->setEnabled(false);
    filters->addStretch();
    layout->addLayout(filters);
    auto *dateFilters = new QHBoxLayout;
    dateFilters->addWidget(new QLabel(QStringLiteral("日期范围"), central));
    dateFilters->addWidget(m_startDate);
    dateFilters->addWidget(new QLabel(QStringLiteral("至"), central));
    m_endDate = new DatePicker(today, central);
    m_endDate->setObjectName(QStringLiteral("endDateFilter"));
    m_endDate->setDateRange(QDate(1, 1, 1), QDate(9999, 12, 31));
    m_endDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_endDate->setCalendarPopup(true);
    m_endDate->setEnabled(false);
    dateFilters->addWidget(m_endDate);
    dateFilters->addStretch();
    m_applyFilterButton = new QPushButton(QStringLiteral("筛选"), central);
    m_applyFilterButton->setObjectName(QStringLiteral("applyFilterButton"));
    m_resetFilterButton = new QPushButton(QStringLiteral("重置"), central);
    m_resetFilterButton->setObjectName(QStringLiteral("resetFilterButton"));
    dateFilters->addWidget(m_applyFilterButton);
    dateFilters->addWidget(m_resetFilterButton);
    layout->addLayout(dateFilters);

    m_errorLabel = new QLabel(central);
    m_errorLabel->setObjectName(QStringLiteral("databaseErrorLabel"));
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setProperty("error", true);
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);

    auto *range = new QHBoxLayout;
    m_filterDescriptionLabel = new QLabel(m_filter.description(), central);
    m_filterDescriptionLabel->setObjectName(QStringLiteral("appliedFilterLabel"));
    m_filterDescriptionLabel->setWordWrap(true);
    m_filteredSummaryLabel = new QLabel(QStringLiteral("当前结果：—"), central);
    m_filteredSummaryLabel->setObjectName(QStringLiteral("filteredExpenseLabel"));
    range->addWidget(m_filterDescriptionLabel, 1);
    range->addWidget(m_filteredSummaryLabel);
    layout->addLayout(range);

    m_transactionTable = new QTableWidget(0, 5, central);
    m_transactionTable->setObjectName(QStringLiteral("transactionTable"));
    m_transactionTable->setHorizontalHeaderLabels({
        QStringLiteral("消费时间"), QStringLiteral("名称"),
        QStringLiteral("分类"), QStringLiteral("金额"), QStringLiteral("备注")
    });
    m_transactionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_transactionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transactionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_transactionTable->setAlternatingRowColors(true);
    m_transactionTable->verticalHeader()->hide();
    m_transactionTable->verticalHeader()->setDefaultSectionSize(36);
    m_transactionTable->setMinimumHeight(200);
    m_transactionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_transactionTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    auto *records = new QHBoxLayout;
    m_recordsLayout = records;
    records->addWidget(m_transactionTable, 3);
    auto *categoryBox = new QGroupBox(QStringLiteral("本月分类支出"), central);
    m_categoryBox = categoryBox;
    auto *categoryLayout = new QVBoxLayout(categoryBox);
    m_categorySummaryTable = new QTableWidget(Transaction::categories().size(), 2, categoryBox);
    m_categorySummaryTable->setObjectName(QStringLiteral("categorySummaryTable"));
    m_categorySummaryTable->setAccessibleName(QStringLiteral("本月分类支出"));
    m_categorySummaryTable->setHorizontalHeaderLabels({QStringLiteral("分类"), QStringLiteral("金额")});
    m_categorySummaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_categorySummaryTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_categorySummaryTable->setAlternatingRowColors(true);
    m_categorySummaryTable->verticalHeader()->hide();
    m_categorySummaryTable->verticalHeader()->setDefaultSectionSize(34);
    m_categorySummaryTable->setMinimumHeight(245);
    m_categorySummaryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_categorySummaryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_categorySummaryTable->setMinimumWidth(200);
    const auto categories = Transaction::categories();
    for (int row = 0; row < categories.size(); ++row) {
        m_categorySummaryTable->setItem(row, 0, new QTableWidgetItem(categories[row]));
        auto *amount = new QTableWidgetItem(QStringLiteral("—"));
        amount->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_categorySummaryTable->setItem(row, 1, amount);
    }
    categoryLayout->addWidget(m_categorySummaryTable);
    auto *categoryHint = new QLabel(QStringLiteral("按消费日期统计，不受列表筛选影响。"), categoryBox);
    categoryHint->setWordWrap(true);
    categoryLayout->addWidget(categoryHint);
    categoryBox->hide();
    layout->addLayout(records, 1);

    m_emptyHint = new QLabel(QStringLiteral("暂无消费记录，点击“记一笔”添加。"), central);
    m_emptyHint->setObjectName(QStringLiteral("emptyHint"));
    m_emptyHint->setWordWrap(true);
    layout->addWidget(m_emptyHint);
    auto *recordScroll = ShellWidgets::centeredPage(central, 1160, m_pages);
    recordScroll->setObjectName(QStringLiteral("recordsScrollArea"));
    m_billingPages = new QStackedWidget(m_pages);
    m_billingPages->setObjectName("billingPageStack");
    m_billingWorkspace = new BillingWorkspace(m_database,m_preferencesPath,m_billingPages);
    auto *billingScroll = ShellWidgets::centeredPage(m_billingWorkspace,1160,m_billingPages,Qt::AlignTop);
    billingScroll->setObjectName("billingWorkspaceScrollArea");
    m_billingPages->addWidget(billingScroll);
    m_billingPages->addWidget(recordScroll);
    m_pages->addWidget(m_billingPages);
    connect(backToBilling,&QPushButton::clicked,this,[this] { m_billingPages->setCurrentIndex(0); });
    connect(m_billingWorkspace,&BillingWorkspace::recordsRequested,this,&MainWindow::showRecords);
    connect(m_billingWorkspace,&BillingWorkspace::budgetRequested,this,[this](const QDate &month) {
        m_budgetWidget->setMonth(month); m_budgetWidget->refresh();
        m_budgetDialog->resize(680,m_budgetDialog->sizeHint().height()); m_budgetDialog->open();
    });
    connect(m_budgetDialog,&QDialog::finished,this,[this] { m_billingWorkspace->refresh(); m_homeWidget->refresh(); });
    connect(m_budgetWidget,&BudgetWidget::budgetSaved,this,[this] { m_billingWorkspace->refresh(); m_homeWidget->refresh(); });
    connect(m_billingWorkspace,&BillingWorkspace::expenseSaved,this,[this](qint64 id) {
        if (reloadTransactions(id)) statusBar()->showMessage(QStringLiteral("已记账，预算和图表已更新。"),5000);
    });
    connect(m_billingWorkspace,&BillingWorkspace::settingsRequested,this,[this] { showPage(Page::Settings); });
    connect(m_billingWorkspace,&BillingWorkspace::editRequested,this,[this](qint64 id) {
        if (selectRecord(id)) editSelectedTransaction();
    });
    connect(m_billingWorkspace,&BillingWorkspace::deleteRequested,this,[this](qint64 id) {
        if (selectRecord(id)) deleteSelectedTransaction();
    });
    connect(m_billingWorkspace,&BillingWorkspace::importRequested,this,[this] {
        auto *dialog=new BillImportDialog(m_database,m_preferencesPath,this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog,&BillImportDialog::imported,this,[this](int count) {
            if (reloadTransactions()) statusBar()->showMessage(QStringLiteral("已导入 %1 笔消费，预算和图表已更新。").arg(count),8000);
        });
        dialog->open();
    });
    m_statisticsWidget = new StatisticsWidget(m_pages);
    m_reviewTabs=new QTabWidget(m_pages); m_reviewTabs->setObjectName("reviewSectionsTabs");
    m_reviewWidget=new ReviewWidget(m_database,m_reviewTabs);
    m_monthlyReviewWidget=new MonthlyReviewWidget(m_database,m_preferencesPath,m_reviewTabs);
    auto *monthlyPage=ShellWidgets::centeredPage(m_monthlyReviewWidget,894,m_reviewTabs,Qt::AlignTop);
    monthlyPage->setObjectName("monthlyReviewScrollArea");
    m_reviewTabs->addTab(monthlyPage,QStringLiteral("月度复盘"));
    const auto reviewDetail=[this](QWidget *content,const char *backName) {
        auto *page=new QWidget;
        auto *column=new QVBoxLayout(page); column->setContentsMargins(0,12,0,0); column->setSpacing(0);
        auto *back=new QPushButton(QStringLiteral("← 返回月度复盘"),page);
        back->setObjectName(QLatin1String(backName)); back->setProperty("reviewLink",true);
        column->addWidget(back,0,Qt::AlignLeft); column->addWidget(content);
        connect(back,&QPushButton::clicked,this,[this] { m_reviewTabs->setCurrentIndex(0); });
        return ShellWidgets::centeredPage(page,1120,m_reviewTabs,Qt::AlignTop);
    };
    m_reviewTabs->addTab(reviewDetail(m_reviewWidget,"purchaseReviewBackButton"),QStringLiteral("购买后复盘"));
    m_reviewTabs->addTab(reviewDetail(m_statisticsWidget,"reviewStatisticsBackButton"),QStringLiteral("年度与分类统计"));
    m_reviewTabs->tabBar()->hide(); m_pages->addWidget(m_reviewTabs);
    connect(m_monthlyReviewWidget,&MonthlyReviewWidget::purchaseReviewRequested,this,[this] { m_reviewTabs->setCurrentIndex(1); });
    connect(m_monthlyReviewWidget,&MonthlyReviewWidget::statisticsRequested,this,[this](const QDate &month) {
        m_statisticsWidget->setMonth(month); m_reviewTabs->setCurrentIndex(2);
    });
    m_wishlistWidget = new WishlistWidget(m_database, m_pages);
    auto *planningPage = new QWidget(m_pages);
    planningPage->setObjectName(QStringLiteral("planningPage"));
    auto *planningLayout = new QVBoxLayout(planningPage);
    planningLayout->setContentsMargins(32, 40, 32, 70);
    planningLayout->setSpacing(0);
    auto *planningHeading = new QHBoxLayout;
    auto *planningTitle = ShellWidgets::label(QStringLiteral("清单"), planningPage);
    planningTitle->setObjectName(QStringLiteral("planningTitleLabel"));
    planningHeading->addWidget(planningTitle, 1);
    planningHeading->addWidget(m_wishlistWidget->takeAddButton());
    planningLayout->addLayout(planningHeading); planningLayout->addSpacing(24);
    planningLayout->addWidget(m_wishlistWidget);
    planningLayout->addStretch();
    auto *planningScroll = ShellWidgets::centeredPage(planningPage, 804, m_pages, Qt::AlignTop);
    planningScroll->setObjectName(QStringLiteral("planningScrollArea"));
    m_pages->addWidget(planningScroll);
    m_dataWidget = new DataManagementWidget(m_database, m_pages);
    auto *dataPage = new QWidget;
    auto *dataLayout = new QVBoxLayout(dataPage);
    auto *backToSettings = new QPushButton(QStringLiteral("← 返回设置"), dataPage);
    backToSettings->setObjectName(QStringLiteral("dataBackButton"));
    dataLayout->addWidget(backToSettings, 0, Qt::AlignLeft);
    dataLayout->addWidget(m_dataWidget);
    dataLayout->addStretch();
    m_pages->addWidget(ShellWidgets::centeredPage(dataPage, 800, m_pages));
    connect(backToSettings, &QPushButton::clicked, this, [this] { showPage(Page::Settings); });
    m_homeWidget = new HomeWidget(m_database, m_pages);
    m_pages->addWidget(ShellWidgets::centeredPage(m_homeWidget, 640, m_pages));
    m_pages->addWidget(ShellWidgets::centeredPage(buildExecutionPage(), 640, m_pages,Qt::AlignTop));
    m_appearanceWidget = new AppearanceWidget(m_preferencesPath,m_pages);
    m_pages->addWidget(ShellWidgets::centeredPage(m_appearanceWidget, 640, m_pages,Qt::AlignTop));
    connect(m_homeWidget, &HomeWidget::budgetRequested, this, [this] {
        showPage(Page::Billing);
        m_billingWorkspace->setMonth(QDate::currentDate());
        m_billingWorkspace->budgetRequested(QDate::currentDate());
    });
    connect(m_homeWidget, &HomeWidget::wishlistRequested, this, [this] { showPage(Page::Wishlist); });
    connect(m_wishlistWidget,&WishlistWidget::goalRequested,this,&MainWindow::choosePoolGoal);
    connect(m_homeWidget,&HomeWidget::reviewRequested,this,[this] { m_reviewTabs->setCurrentIndex(1); showPage(Page::Review); });
    connect(m_homeWidget,&HomeWidget::pricesRequested,this,[this] { showPage(Page::Wishlist); m_wishlistWidget->showPrices(); });
    connect(m_homeWidget,&HomeWidget::attentionChanged,this,[this](qint64 count) { m_dueReviews=count; updateNavigation(); });
    connect(m_wishlistWidget,&WishlistWidget::pricesChanged,this,[this] { m_homeWidget->refresh(); });
    connect(m_wishlistWidget,&WishlistWidget::settingsRequested,this,[this] { showPage(Page::Settings); });
    connect(m_reviewWidget,&ReviewWidget::reviewsChanged,this,[this] { m_homeWidget->refresh(); m_monthlyReviewWidget->refresh(); m_executionWidget->refreshReferences(); });
    connect(m_appearanceWidget, &AppearanceWidget::themeSelected, this, [this](const QString &theme) { applyTheme(theme, true); });
    connect(m_appearanceWidget, &AppearanceWidget::dataRequested, this, [this] { showPage(Page::Data); });
    connect(m_appearanceWidget,&AppearanceWidget::shoppingProfileRequested,this,[this] {
        auto *dialog=new ShoppingProfileDialog(m_database,this); dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog,&ShoppingProfileDialog::profileChanged,this,[this] { m_executionWidget->refreshReferences(); }); dialog->open();
    });
    buildShell();
    applyTheme(m_theme, false);
    statusBar()->showMessage(QStringLiteral("消费记录会自动保存到本机。"));
    statusBar()->setToolTip(m_database.databasePath());

    connect(m_addButton, &QPushButton::clicked, this, [this] { openTransactionDialog(); });
    connect(m_editButton, &QPushButton::clicked, this, &MainWindow::editSelectedTransaction);
    connect(m_deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTransaction);
    connect(m_transactionTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateActionState);
    connect(m_transactionTable, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { editSelectedTransaction(); });
    connect(m_applyFilterButton, &QPushButton::clicked, this, &MainWindow::applyFilters);
    connect(m_resetFilterButton, &QPushButton::clicked, this, &MainWindow::resetFilters);
    connect(m_useDateRange, &QCheckBox::toggled, this, &MainWindow::updateActionState);
    connect(m_statisticsWidget, &StatisticsWidget::monthActivated, this, [this](const QDate &month) {
        m_categoryFilter->setCurrentIndex(0);
        m_useDateRange->setChecked(true);
        m_startDate->setDate(month);
        m_endDate->setDate(QDate(month.year(), month.month(), month.daysInMonth()));
        applyFilters();
        m_budgetWidget->setMonth(month);
        showPage(Page::Billing);
        m_billingWorkspace->setMonth(month);
        m_billingPages->setCurrentIndex(1);
    });

    connect(m_wishlistWidget, &WishlistWidget::expenseRecorded, this, [this](qint64 id) {
        if (reloadTransactions(id))
            statusBar()->showMessage(QStringLiteral("购买已记账，预算和消费统计已更新。"), 8000);
        else
            statusBar()->showMessage(QStringLiteral("购买已记账，但消费列表和统计刷新失败。请检查错误并重新启动程序。"));
    });
    connect(m_dataWidget,&DataManagementWidget::recordsImported,this,[this](int count) { if (reloadTransactions()) statusBar()->showMessage(QStringLiteral("已导入 %1 笔历史消费，预算和复盘已更新。").arg(count),10000); else statusBar()->showMessage(QStringLiteral("导入已保存，但页面刷新失败，请检查数据库。"),10000); });
    connect(m_dataWidget, &DataManagementWidget::databaseRestored, this, [this] {
        for (auto *dialog:findChildren<ProductDetailsDialog *>()) dialog->reject();
        m_monthlyReviewWidget->cancelAnalysis();
        m_filter = {};
        m_categoryFilter->setCurrentIndex(0);
        m_useDateRange->setChecked(false);
        m_budgetWidget->setDatabaseAvailable(false);
        m_budgetWidget->setDatabaseAvailable(true);
        m_billingWorkspace->resetAfterRestore();
        m_wishlistWidget->refresh(); m_executionWidget->reloadState(); m_reviewWidget->refresh();
        if (reloadTransactions())
            statusBar()->showMessage(QStringLiteral("备份已恢复，所有页面已刷新。"), 8000);
        else
            statusBar()->showMessage(QStringLiteral("数据已恢复，但消费页面刷新失败，请重新启动程序。"));
    });
    m_databaseReady = m_database.initialize();
    if (!m_databaseReady) {
        showError(m_database.lastError());
        m_statisticsWidget->showLoadError(m_database.lastError());
        m_emptyHint->setText(QStringLiteral("数据库不可用，请检查上述错误后重新启动程序。"));
        statusBar()->showMessage(QStringLiteral("数据保存暂不可用。"));
    } else if (!reloadTransactions()) {
        m_databaseReady = false;
        m_emptyHint->setText(QStringLiteral("暂时无法加载消费记录，请检查上述错误。"));
        statusBar()->showMessage(QStringLiteral("消费记录加载失败。"));
    }
    updateActionState();
    m_budgetWidget->setDatabaseAvailable(m_databaseReady);
    m_billingWorkspace->setDatabaseAvailable(m_databaseReady);
    m_wishlistWidget->setDatabaseAvailable(m_databaseReady);
    m_dataWidget->setDatabaseAvailable(m_databaseReady);
    m_homeWidget->setDatabaseAvailable(m_databaseReady);
    m_executionWidget->setDatabaseAvailable(m_databaseReady);
    m_reviewWidget->setDatabaseAvailable(m_databaseReady);
    m_monthlyReviewWidget->setDatabaseAvailable(m_databaseReady);
    m_homeDecision->setEnabled(m_databaseReady);

    showPage(Page::Home);
}

void MainWindow::buildShell()
{
    auto *shell = new QWidget(this);
    shell->setProperty("page", true);
    auto *layout = new QVBoxLayout(shell);
    layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *header = new QFrame(shell);
    header->setObjectName(QStringLiteral("shellHeader")); header->setFixedHeight(52);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 16, 0); headerLayout->setSpacing(10);
    auto *balance = new QWidget(header); balance->setFixedWidth(32); headerLayout->addWidget(balance);
    headerLayout->addStretch();
    m_brandMark = new QLabel(header); m_brandMark->setFixedSize(26,28);
    headerLayout->addWidget(m_brandMark);
    auto *brand = new QLabel(QStringLiteral("KURA"), header);
    brand->setObjectName(QStringLiteral("brandWordmark"));
    brand->setAccessibleName(QStringLiteral("Kura 蔵"));
    headerLayout->addWidget(brand); headerLayout->addStretch();
    m_settingsButton = new QToolButton(header);
    m_settingsButton->setObjectName(QStringLiteral("settingsButton"));
    m_settingsButton->setAccessibleName(QStringLiteral("设置"));
    m_settingsButton->setToolTip(QStringLiteral("设置"));
    m_settingsButton->setCheckable(true); m_settingsButton->setFixedSize(32, 32);
    m_settingsButton->setIconSize(QSize(20,20)); m_settingsButton->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(m_settingsButton);
    connect(m_settingsButton, &QToolButton::clicked, this, [this] { showPage(Page::Settings); });
    layout->addWidget(header); layout->addWidget(m_pages, 1);
    m_homeActionBar = new QWidget(shell);
    m_homeActionBar->setObjectName(QStringLiteral("homeActionBar"));
    auto *homeActionLayout = new QHBoxLayout(m_homeActionBar);
    homeActionLayout->setContentsMargins(24, 12, 24, 12);
    homeActionLayout->addStretch();
    m_homeDecision = new QPushButton(QStringLiteral("我现在想买……"), m_homeActionBar);
    m_homeDecision->setObjectName(QStringLiteral("homeDecisionButton"));
    m_homeDecision->setFixedHeight(46); m_homeDecision->setMaximumWidth(592);
    m_homeDecision->setCursor(Qt::PointingHandCursor);
    homeActionLayout->addWidget(m_homeDecision, 1); homeActionLayout->addStretch();
    connect(m_homeDecision, &QPushButton::clicked, this, &MainWindow::openDecisionDialog);
    layout->addWidget(m_homeActionBar);
    auto *footer = new QFrame(shell);
    footer->setObjectName(QStringLiteral("shellFooter")); footer->setFixedHeight(68);
    auto *navigation = new QHBoxLayout(footer);
    navigation->setContentsMargins(12, 8, 12, 8); navigation->setSpacing(24); navigation->addStretch();
    const QList<Page> pages = {Page::Home, Page::Billing, Page::Wishlist, Page::Execution, Page::Review};
    const QStringList names = {"home", "billing", "wishlist", "execution", "review"};
    const QStringList labels = {QStringLiteral("主页"), QStringLiteral("账单"), QStringLiteral("清单"), QStringLiteral("执行"), QStringLiteral("复盘")};
    for (int i = 0; i < pages.size(); ++i) {
        auto *button = new QToolButton(footer);
        button->setObjectName(names[i] + QStringLiteral("NavButton"));
        button->setProperty("iconName", names[i]); button->setProperty("pageIndex", int(pages[i]));
        button->setText(labels[i]); button->setAccessibleName(labels[i]); button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); button->setIconSize(QSize(20,20));
        button->setMinimumWidth(52); button->setCursor(Qt::PointingHandCursor);
        button->setShortcut(QKeySequence(QStringLiteral("Alt+%1").arg(i+1)));
        navigation->addWidget(button); m_navigation.append(button);
        connect(button, &QToolButton::clicked, this, [this, page = pages[i]] { showPage(page); });
    }
    navigation->addStretch(); layout->addWidget(footer);
    setCentralWidget(shell);
    connect(m_pages, &QStackedWidget::currentChanged, this, [this] { updateNavigation(); });
}

QWidget *MainWindow::buildExecutionPage()
{
    m_executionWidget=new ExecutionWidget(m_database,m_preferencesPath,m_pages);
    connect(m_executionWidget,&ExecutionWidget::inputRequested,this,[this](const QString &initialText) {
        openDecisionDialog();
        if (auto *dialog=findChild<DecisionDialog *>()) dialog->setInitialText(initialText);
    });
    connect(m_executionWidget,&ExecutionWidget::wishlistRequested,this,[this] { m_wishlistWidget->refresh(); m_homeWidget->refresh(); showPage(Page::Wishlist); });
    connect(m_executionWidget,&ExecutionWidget::purchaseRequested,this,[this](const Transaction &draft,qint64 sessionId,qint64 brandId) { openTransactionDialog(&draft,true,sessionId,brandId); });
    return m_executionWidget;
}

void MainWindow::showPage(Page page)
{
    m_pages->setCurrentIndex(int(page));
    if (page == Page::Billing) { m_billingPages->setCurrentIndex(0); m_billingWorkspace->refresh(); }
    if (page == Page::Home) m_homeWidget->refresh();
    if (page == Page::Review) { m_reviewWidget->refresh(); m_monthlyReviewWidget->refresh(); }
    if (page == Page::Execution) m_executionWidget->refreshReferences();
    if (page == Page::Wishlist) m_wishlistWidget->refresh();
    updateNavigation();
}

MainWindow::Page MainWindow::currentPage() const { return static_cast<Page>(m_pages->currentIndex()); }

void MainWindow::openDecisionDialog()
{
    if (!m_databaseReady) return;
    if (auto *existing=findChild<DecisionDialog *>()) { existing->raise(); existing->activateWindow(); return; }
    auto *dialog=new DecisionDialog(m_database,AISettingsStore::load(m_preferencesPath),this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setResearchAvailable(m_executionWidget->activeSessionId()==0);
    connect(dialog,&DecisionDialog::researchRequested,this,[this,dialog](const ParsedProduct &product,const QByteArray &image,qint64 brandId,qint64 productId) {
        if (!m_executionWidget->prepareDecision(product,image,brandId,productId)) {
            dialog->showHandoffError(QStringLiteral("暂时无法开始研究。请先处理执行页进行中的决策，或稍后重试。")); return;
        }
        dialog->accept(); showPage(Page::Execution);
    });
    connect(dialog,&DecisionDialog::settingsRequested,this,[this] { showPage(Page::Settings); });
    connect(dialog,&DecisionDialog::committed,this,[this](const QString &choice,qint64 id) {
        m_wishlistWidget->refresh();
        const bool reloaded=reloadTransactions(choice==QStringLiteral("buy")?id:0);
        const QString result=choice==QStringLiteral("hold") ? QStringLiteral("已保存忍住金额，有目标时计入许愿池，没有目标时先暂存。")
            : choice==QStringLiteral("wishlist") ? QStringLiteral("已加入清单，冷静期从现在开始。") : choice==QStringLiteral("track") ? QStringLiteral("价格已记录，没有生成消费。") : QStringLiteral("购买已记账，并已安排 7 天和 30 天复盘。");
        statusBar()->showMessage(reloaded?result:result+QStringLiteral("页面更新失败，请检查数据后刷新。"),10000);
        if (choice==QStringLiteral("track")) { showPage(Page::Wishlist); m_wishlistWidget->showPrices(id); }
        else { showPage(choice==QStringLiteral("wishlist")?Page::Wishlist:Page::Home); }
    });
    dialog->open();
}

void MainWindow::choosePoolGoal(qint64 wishId)
{
    const auto apply=[this,wishId](bool replace) {
        WishPoolRepository repository(m_database);
        if (!repository.setGoal(wishId,replace)) {
            auto *message=new QMessageBox(QMessageBox::Warning,QStringLiteral("无法设置许愿目标"),repository.lastError(),QMessageBox::Ok,this);
            message->setAttribute(Qt::WA_DeleteOnClose); message->open(); return;
        }
        m_homeWidget->refresh(); showPage(Page::Home); statusBar()->showMessage(QStringLiteral("许愿目标已设置，暂存的忍住金额已一起计入。"),8000);
    };
    WishPoolSnapshot pool; WishPoolRepository repository(m_database);
    if (!repository.load(pool)) { statusBar()->showMessage(repository.lastError(),8000); return; }
    if (!pool.id || pool.wishId==wishId) { apply(false); return; }
    auto *confirmation=new QMessageBox(QMessageBox::Question,QStringLiteral("切换许愿目标"),
        QStringLiteral("当前目标是“%1”，已攒 %2。切换后原目标与忍住记录会保留为历史，不转入新目标。确定切换吗？").arg(pool.name,Transaction::formatAmount(pool.savedCents)),QMessageBox::Yes|QMessageBox::No,this);
    confirmation->setObjectName("replaceWishPoolDialog"); confirmation->setTextFormat(Qt::PlainText); confirmation->setDefaultButton(QMessageBox::No);
    confirmation->setAttribute(Qt::WA_DeleteOnClose);
    connect(confirmation,&QMessageBox::finished,this,[confirmation,apply](int) { if (confirmation->standardButton(confirmation->clickedButton())==QMessageBox::Yes) apply(true); }); confirmation->open();
}

void MainWindow::applyTheme(const QString &theme, bool save)
{
    m_theme = AppTheme::normalized(theme);
    AppTheme::apply(this, m_theme);
    m_appearanceWidget->setTheme(m_theme);
    updateNavigation();
    if (save && !m_preferencesPath.isEmpty()) {
        QSettings preferences(m_preferencesPath, QSettings::IniFormat);
        preferences.setValue(QStringLiteral("appearance/theme"), m_theme); preferences.sync();
        if (preferences.status() != QSettings::NoError)
            statusBar()->showMessage(QStringLiteral("外观已应用，但未能保存到本机。"), 8000);
    }
}

void MainWindow::updateNavigation()
{
    m_homeActionBar->setVisible(currentPage() == Page::Home);
    for (auto *button : m_navigation) {
        const bool active = button->property("pageIndex").toInt() == m_pages->currentIndex();
        button->setChecked(active);
        if (button->property("iconName").toString()==QStringLiteral("review")) button->setText(m_dueReviews>0?QStringLiteral("复盘 · %1").arg(m_dueReviews):QStringLiteral("复盘"));
        button->setIcon(AppTheme::icon(button->property("iconName").toString(), m_theme, active));
    }
    const bool settings = currentPage() == Page::Settings || currentPage() == Page::Data;
    m_brandMark->setPixmap(AppTheme::icon(QStringLiteral("brand"), m_theme, true).pixmap(26,28));
    m_settingsButton->setChecked(settings);
    m_settingsButton->setIcon(AppTheme::icon(QStringLiteral("settings"), m_theme, settings));
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_recordsLayout) {
        const bool compact = width() < 1050;
        m_recordsLayout->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_categoryBox->setMaximumHeight(compact ? 365 : QWIDGETSIZE_MAX);
    }
}

void MainWindow::openTransactionDialog(const Transaction *existing,bool purchase,qint64 sessionId,qint64 brandId)
{
    auto *dialog = existing ? new AddTransactionDialog(*existing, this) : new AddTransactionDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const bool editing = existing && existing->id()>0;
    if (editing) {
        auto *details=new QPushButton(QStringLiteral("商品详情与 AI 讨论 →"),dialog); details->setObjectName("transactionProductDetailsButton");
        dialog->layout()->addWidget(details);
        connect(details,&QPushButton::clicked,dialog,[this,dialog,id=existing->id()] { ProductDetailsDialog::openFor(m_database,"transaction",id,dialog); });
    }
    if (purchase) {
        ShoppingProfileRepository profile(m_database); QList<BrandEntry> brands;
        if (!profile.loadBrands(brands)) { dialog->deleteLater(); statusBar()->showMessage(profile.lastError(),8000); return; }
        dialog->setBrands(brands,brandId); dialog->setWindowTitle(QStringLiteral("确认购买 · 安排 7／30 天复盘"));
    }
    connect(dialog, &AddTransactionDialog::saveRequested, this, [this, dialog, editing,purchase,sessionId] {
        auto transaction = dialog->transaction();
        PurchaseRepository purchases(m_database);
        const bool saved = editing ? m_repository.update(transaction) : purchase?purchases.record(transaction,dialog->selectedBrandId(),sessionId):m_repository.add(transaction);
        if (!saved) {
            dialog->showSaveError(QStringLiteral("保存失败：%1").arg(purchase?purchases.lastError():m_repository.lastError()));
            return;
        }
        dialog->completeSave();
        if (purchase && sessionId) m_executionWidget->reloadState();
        if (!reloadTransactions(transaction.id())) {
            showError(QStringLiteral("记录已保存，但列表刷新失败：%1。请重新启动程序加载。")
                      .arg(m_repository.lastError()));
            return;
        }
        const QString action = editing ? QStringLiteral("已修改消费记录。") : QStringLiteral("已保存消费记录。");
        statusBar()->showMessage(selectedTransactionId() == transaction.id()
            ? action : action + QStringLiteral("该记录不符合当前筛选条件。"), 8000);
    });
    dialog->open();
}

void MainWindow::editSelectedTransaction()
{
    if (const auto *transaction = selectedTransaction())
        openTransactionDialog(transaction);
}

void MainWindow::deleteSelectedTransaction()
{
    const auto *transaction = selectedTransaction();
    if (!transaction)
        return;
    const qint64 id = transaction->id();
    auto *confirmation = new QMessageBox(QMessageBox::Question, QStringLiteral("删除消费记录"),
        QStringLiteral("确定删除“%1”（%2）吗？")
            .arg(transaction->description(), Transaction::formatAmount(transaction->amountCents())),
        QMessageBox::Yes | QMessageBox::No, this);
    confirmation->setObjectName(QStringLiteral("deleteConfirmationDialog"));
    confirmation->setTextFormat(Qt::PlainText);
    confirmation->setAttribute(Qt::WA_DeleteOnClose);
    confirmation->button(QMessageBox::Yes)->setText(QStringLiteral("删除"));
    confirmation->button(QMessageBox::No)->setText(QStringLiteral("取消"));
    confirmation->setDefaultButton(QMessageBox::No);
    confirmation->setEscapeButton(QMessageBox::No);
    connect(confirmation, &QMessageBox::finished, this, [this, confirmation, id](int) {
        if (confirmation->standardButton(confirmation->clickedButton()) != QMessageBox::Yes)
            return;
        if (!m_repository.remove(id)) {
            showError(QStringLiteral("删除失败：%1").arg(m_repository.lastError()));
            return;
        }
        if (reloadTransactions())
            statusBar()->showMessage(QStringLiteral("已删除消费记录。"), 5000);
        else
            showError(QStringLiteral("记录已删除，但列表刷新失败：%1。请重新启动程序加载。")
                      .arg(m_repository.lastError()));
    });
    confirmation->open();
}

void MainWindow::applyFilters()
{
    TransactionFilter candidate;
    candidate.category = m_categoryFilter->currentData().toString();
    candidate.useDateRange = m_useDateRange->isChecked();
    m_startDate->interpretText();
    m_endDate->interpretText();
    candidate.startDate = m_startDate->date();
    candidate.endDate = m_endDate->date();
    const QString error = candidate.validationError();
    if (!error.isEmpty()) {
        showError(error);
        return;
    }
    const auto previous = m_filter;
    m_filter = candidate;
    if (!reloadTransactions())
        m_filter = previous; // The visible range and totals still describe the old snapshot.
}

void MainWindow::resetFilters()
{
    m_categoryFilter->setCurrentIndex(0);
    m_useDateRange->setChecked(false);
    const auto today = QDate::currentDate();
    m_startDate->setDate(QDate(today.year(), today.month(), 1));
    m_endDate->setDate(today);
    applyFilters();
}

void MainWindow::showRecords(const QDate &first, const QDate &last)
{
    m_categoryFilter->setCurrentIndex(0);
    m_useDateRange->setChecked(true);
    m_startDate->setDate(first); m_endDate->setDate(last);
    applyFilters();
    m_billingPages->setCurrentIndex(1);
}

bool MainWindow::selectRecord(qint64 id)
{
    // Detail actions must still find records excluded by the full-list filter.
    bool present=false;
    for (const auto &record:m_transactions) if (record.id()==id) { present=true; break; }
    if (!present) { resetFilters(); }
    for (int row=0;row<m_transactionTable->rowCount();++row) {
        if (m_transactionTable->item(row,0)->data(Qt::UserRole).toLongLong()==id) {
            m_transactionTable->selectRow(row); return true;
        }
    }
    showError(QStringLiteral("这笔记录已不存在，请刷新后重试。"));
    return false;
}

bool MainWindow::reloadTransactions(qint64 selectedId)
{
    if (selectedId == 0)
        selectedId = selectedTransactionId();
    QList<Transaction> loaded;
    if (!m_repository.findAll(loaded)) {
        showError(m_repository.lastError());
        m_statisticsWidget->showLoadError(m_repository.lastError());
        m_billingWorkspace->showLoadError(m_repository.lastError());
        return false;
    }
    const auto today = QDate::currentDate();
    const auto summary = ExpenseSummary::calculate(loaded, m_filter, today);
    const auto categories = CategorySummary::calculate(loaded, today);
    for (int row = 0; row < categories.size(); ++row) {
        auto *amount = m_categorySummaryTable->item(row, 1);
        amount->setText(Transaction::formatAmount(categories[row].amountCents));
        amount->setData(Qt::UserRole, categories[row].amountCents);
    }
    m_statisticsWidget->setTransactions(loaded);
    m_transactions.clear();
    for (const auto &transaction : loaded) {
        if (m_filter.matches(transaction))
            m_transactions.append(transaction);
    }
    m_transactionTable->setRowCount(0);
    int selectedRow = -1;
    for (const auto &transaction : m_transactions) {
        const int row = m_transactionTable->rowCount();
        m_transactionTable->insertRow(row);
        const QString dateText = transaction.date().toString(Qt::ISODate)
            + (transaction.time().isValid() ? QStringLiteral(" ") + transaction.time().toString(QStringLiteral("HH:mm:ss"))
                                           : QStringLiteral("（时间未记录）"));
        auto *dateItem = new QTableWidgetItem(dateText);
        dateItem->setData(Qt::UserRole, transaction.id());
        m_transactionTable->setItem(row, 0, dateItem);
        m_transactionTable->setItem(row, 1, new QTableWidgetItem(transaction.description()));
        auto *categoryItem=new QTableWidgetItem(CategoryCatalog::label(transaction.detailCategory()));
        categoryItem->setToolTip(QStringLiteral("%1 · 汇总分类：%2").arg(CategoryCatalog::isBasic(transaction.detailCategory())?QStringLiteral("基础生活"):QStringLiteral("可支配"),transaction.category()));
        m_transactionTable->setItem(row,2,categoryItem);
        auto *amountItem = new QTableWidgetItem(Transaction::formatAmount(transaction.amountCents()));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_transactionTable->setItem(row, 3, amountItem);
        QString notePreview = transaction.note();
        notePreview.replace(QLatin1Char('\n'), QLatin1Char(' '));
        notePreview.replace(QLatin1Char('\r'), QLatin1Char(' '));
        if (notePreview.size() > 60)
            notePreview = notePreview.left(60) + QStringLiteral("…");
        auto *noteItem = new QTableWidgetItem(notePreview);
        noteItem->setToolTip(transaction.note());
        m_transactionTable->setItem(row, 4, noteItem);
        if (transaction.id() == selectedId)
            selectedRow = row;
    }
    if (selectedRow >= 0) {
        m_transactionTable->selectRow(selectedRow);
        m_transactionTable->scrollToItem(m_transactionTable->item(selectedRow, 0));
    }
    m_totalLabel->setText(QStringLiteral("全部支出：%1").arg(Transaction::formatAmount(summary.totalCents)));
    m_monthLabel->setText(QStringLiteral("本月支出：%1").arg(Transaction::formatAmount(summary.monthCents)));
    m_todayLabel->setText(QStringLiteral("今日支出：%1").arg(Transaction::formatAmount(summary.todayCents)));
    m_filteredSummaryLabel->setText(QStringLiteral("当前结果：%1 笔 · %2")
        .arg(summary.filteredCount).arg(Transaction::formatAmount(summary.filteredCents)));
    m_filterDescriptionLabel->setText(m_filter.description());
    m_dataWidget->setAppliedFilter(m_filter);
    m_emptyHint->setText(loaded.isEmpty() ? QStringLiteral("暂无消费记录，点击“记一笔”添加。")
        : QStringLiteral("没有符合条件的消费记录，可以调整筛选条件或点击“重置”。"));
    m_emptyHint->setVisible(m_transactions.isEmpty());
    m_errorLabel->hide();
    m_budgetWidget->refresh();
    m_billingWorkspace->refresh();
    m_homeWidget->refresh();
    m_reviewWidget->refresh(); m_monthlyReviewWidget->refresh(); m_executionWidget->refreshReferences();
    updateActionState();
    return true;
}

qint64 MainWindow::selectedTransactionId() const
{
    const int row = m_transactionTable->currentRow();
    if (row < 0 || !m_transactionTable->item(row, 0)
        || !m_transactionTable->item(row, 0)->isSelected())
        return 0;
    return m_transactionTable->item(row, 0)->data(Qt::UserRole).toLongLong();
}

const Transaction *MainWindow::selectedTransaction() const
{
    const qint64 id = selectedTransactionId();
    for (const auto &transaction : m_transactions) {
        if (transaction.id() == id)
            return &transaction;
    }
    return nullptr;
}

void MainWindow::updateActionState()
{
    const bool selected = m_databaseReady && selectedTransaction() != nullptr;
    m_editButton->setEnabled(selected);
    m_deleteButton->setEnabled(selected);
    m_addButton->setEnabled(m_databaseReady);
    m_applyFilterButton->setEnabled(m_databaseReady);
    m_resetFilterButton->setEnabled(m_databaseReady);
    m_categoryFilter->setEnabled(m_databaseReady);
    m_useDateRange->setEnabled(m_databaseReady);
    m_startDate->setEnabled(m_databaseReady && m_useDateRange->isChecked());
    m_endDate->setEnabled(m_databaseReady && m_useDateRange->isChecked());
}

void MainWindow::showError(const QString &message)
{
    m_errorLabel->setText(message);
    m_errorLabel->show();
    if (m_billingWorkspace) m_billingWorkspace->showLoadError(message);
}
