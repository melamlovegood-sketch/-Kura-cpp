#include "mainwindow.h"
#include "AddTransactionDialog.h"
#include "ExpenseSummary.h"
#include "BudgetWidget.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
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
    resize(920, 740);
    setMinimumSize(800, 600);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *heading = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("消费记录"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
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

    m_budgetWidget = new BudgetWidget(m_database, central);
    layout->addWidget(m_budgetWidget);

    auto *totals = new QHBoxLayout;
    m_totalLabel = new QLabel(QStringLiteral("全部支出：—"), central);
    m_totalLabel->setObjectName(QStringLiteral("totalExpenseLabel"));
    m_monthLabel = new QLabel(QStringLiteral("本月支出：—"), central);
    m_monthLabel->setObjectName(QStringLiteral("monthExpenseLabel"));
    m_todayLabel = new QLabel(QStringLiteral("今日支出：—"), central);
    m_todayLabel->setObjectName(QStringLiteral("todayExpenseLabel"));
    totals->addWidget(m_totalLabel);
    totals->addWidget(m_monthLabel);
    totals->addWidget(m_todayLabel);
    layout->addLayout(totals);

    auto *filters = new QHBoxLayout;
    filters->addWidget(new QLabel(QStringLiteral("分类"), central));
    m_categoryFilter = new QComboBox(central);
    m_categoryFilter->setObjectName(QStringLiteral("categoryFilter"));
    m_categoryFilter->addItem(QStringLiteral("全部分类"), QString());
    for (const auto &category : Transaction::categories())
        m_categoryFilter->addItem(category, category);
    filters->addWidget(m_categoryFilter);
    m_useDateRange = new QCheckBox(QStringLiteral("按日期"), central);
    m_useDateRange->setObjectName(QStringLiteral("useDateRangeCheckBox"));
    filters->addWidget(m_useDateRange);
    const QDate today = QDate::currentDate();
    m_startDate = new QDateEdit(QDate(today.year(), today.month(), 1), central);
    m_startDate->setObjectName(QStringLiteral("startDateFilter"));
    m_startDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_startDate->setCalendarPopup(true);
    m_startDate->setEnabled(false);
    filters->addWidget(m_startDate);
    filters->addWidget(new QLabel(QStringLiteral("至"), central));
    m_endDate = new QDateEdit(today, central);
    m_endDate->setObjectName(QStringLiteral("endDateFilter"));
    m_endDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_endDate->setCalendarPopup(true);
    m_endDate->setEnabled(false);
    filters->addWidget(m_endDate);
    filters->addStretch();
    m_applyFilterButton = new QPushButton(QStringLiteral("筛选"), central);
    m_applyFilterButton->setObjectName(QStringLiteral("applyFilterButton"));
    m_resetFilterButton = new QPushButton(QStringLiteral("重置"), central);
    m_resetFilterButton->setObjectName(QStringLiteral("resetFilterButton"));
    filters->addWidget(m_applyFilterButton);
    filters->addWidget(m_resetFilterButton);
    layout->addLayout(filters);

    m_errorLabel = new QLabel(central);
    m_errorLabel->setObjectName(QStringLiteral("databaseErrorLabel"));
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #b42318;"));
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

    m_transactionTable = new QTableWidget(0, 4, central);
    m_transactionTable->setObjectName(QStringLiteral("transactionTable"));
    m_transactionTable->setHorizontalHeaderLabels({
        QStringLiteral("日期"), QStringLiteral("名称"),
        QStringLiteral("分类"), QStringLiteral("金额")
    });
    m_transactionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_transactionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_transactionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_transactionTable->setAlternatingRowColors(true);
    m_transactionTable->verticalHeader()->hide();
    m_transactionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_transactionTable, 1);

    m_emptyHint = new QLabel(QStringLiteral("暂无消费记录，点击“记一笔”添加。"), central);
    m_emptyHint->setObjectName(QStringLiteral("emptyHint"));
    m_emptyHint->setWordWrap(true);
    layout->addWidget(m_emptyHint);
    setCentralWidget(central);
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

    m_databaseReady = m_database.initialize();
    if (!m_databaseReady) {
        showError(m_database.lastError());
        m_emptyHint->setText(QStringLiteral("数据库不可用，请检查上述错误后重新启动程序。"));
        statusBar()->showMessage(QStringLiteral("数据保存暂不可用。"));
    } else if (!reloadTransactions()) {
        m_databaseReady = false;
        m_emptyHint->setText(QStringLiteral("暂时无法加载消费记录，请检查上述错误。"));
        statusBar()->showMessage(QStringLiteral("消费记录加载失败。"));
    }
    updateActionState();
    m_budgetWidget->setDatabaseAvailable(m_databaseReady);
}

void MainWindow::openTransactionDialog(const Transaction *existing)
{
    auto *dialog = existing ? new AddTransactionDialog(*existing, this) : new AddTransactionDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const bool editing = existing != nullptr;
    connect(dialog, &AddTransactionDialog::saveRequested, this, [this, dialog, editing] {
        auto transaction = dialog->transaction();
        const bool saved = editing ? m_repository.update(transaction) : m_repository.add(transaction);
        if (!saved) {
            dialog->showSaveError(QStringLiteral("保存失败：%1").arg(m_repository.lastError()));
            return;
        }
        dialog->completeSave();
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

bool MainWindow::reloadTransactions(qint64 selectedId)
{
    if (selectedId == 0)
        selectedId = selectedTransactionId();
    QList<Transaction> loaded;
    if (!m_repository.findAll(loaded)) {
        showError(m_repository.lastError());
        return false;
    }
    const auto summary = ExpenseSummary::calculate(loaded, m_filter);
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
        auto *dateItem = new QTableWidgetItem(transaction.date().toString(Qt::ISODate));
        dateItem->setData(Qt::UserRole, transaction.id());
        m_transactionTable->setItem(row, 0, dateItem);
        m_transactionTable->setItem(row, 1, new QTableWidgetItem(transaction.description()));
        m_transactionTable->setItem(row, 2, new QTableWidgetItem(transaction.category()));
        auto *amountItem = new QTableWidgetItem(Transaction::formatAmount(transaction.amountCents()));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_transactionTable->setItem(row, 3, amountItem);
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
    m_emptyHint->setText(loaded.isEmpty() ? QStringLiteral("暂无消费记录，点击“记一笔”添加。")
        : QStringLiteral("没有符合条件的消费记录，可以调整筛选条件或点击“重置”。"));
    m_emptyHint->setVisible(m_transactions.isEmpty());
    m_errorLabel->hide();
    m_budgetWidget->refresh();
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
}
