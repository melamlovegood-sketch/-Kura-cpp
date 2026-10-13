#include "mainwindow.h"
#include "AddTransactionDialog.h"

#include <QAbstractItemView>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(QString(), parent)
{
}

MainWindow::MainWindow(const QString &databasePath, QWidget *parent)
    : QMainWindow(parent), m_database(databasePath), m_repository(m_database)
{
    setWindowTitle(QStringLiteral("Kura C++"));
    resize(800, 520);
    setMinimumSize(560, 360);

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
    heading->addWidget(m_addButton);
    layout->addLayout(heading);

    m_errorLabel = new QLabel(central);
    m_errorLabel->setObjectName(QStringLiteral("databaseErrorLabel"));
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #b42318;"));
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);

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
    auto *header = m_transactionTable->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_transactionTable, 1);

    m_emptyHint = new QLabel(QStringLiteral("暂无消费记录，点击“记一笔”添加。"), central);
    m_emptyHint->setObjectName(QStringLiteral("emptyHint"));
    m_emptyHint->setWordWrap(true);
    layout->addWidget(m_emptyHint);
    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("消费记录会自动保存到本机。"));
    statusBar()->setToolTip(m_database.databasePath());

    connect(m_addButton, &QPushButton::clicked, this, [this] {
        auto *dialog = new AddTransactionDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &AddTransactionDialog::saveRequested, this, [this, dialog] {
            auto transaction = dialog->transaction();
            if (!m_repository.add(transaction)) {
                dialog->showSaveError(QStringLiteral("保存失败：%1").arg(m_repository.lastError()));
                return;
            }
            // The insert has committed. Close the form even if the subsequent reload fails,
            // so retrying a refresh cannot accidentally insert the same expense twice.
            dialog->completeSave();
            if (reloadTransactions(transaction.id()))
                statusBar()->showMessage(QStringLiteral("已保存消费记录。"), 5000);
            else
                showError(QStringLiteral("记录已保存，但列表刷新失败：%1。请重新启动程序加载。")
                          .arg(m_repository.lastError()));
        });
        dialog->open();
    });

    if (!m_database.initialize()) {
        showError(m_database.lastError());
        m_addButton->setEnabled(false);
        m_emptyHint->setText(QStringLiteral("数据库不可用，请检查上述错误后重新启动程序。"));
        statusBar()->showMessage(QStringLiteral("数据保存暂不可用。"));
    } else if (!reloadTransactions()) {
        m_addButton->setEnabled(false);
        m_emptyHint->setText(QStringLiteral("暂时无法加载消费记录，请检查上述错误。"));
        statusBar()->showMessage(QStringLiteral("消费记录加载失败。"));
    }
}

bool MainWindow::reloadTransactions(qint64 selectedId)
{
    QList<Transaction> loaded;
    if (!m_repository.findAll(loaded)) {
        showError(m_repository.lastError());
        return false;
    }
    m_transactions = loaded;
    m_transactionTable->setRowCount(0);
    int selectedRow = -1;
    for (const auto &transaction : m_transactions) {
        const int row = m_transactionTable->rowCount();
        m_transactionTable->insertRow(row);
        auto *dateItem = new QTableWidgetItem(transaction.date().toString(Qt::ISODate));
        // B can use this stable id for editing/deletion; a row number is not an id.
        dateItem->setData(Qt::UserRole, transaction.id());
        m_transactionTable->setItem(row, 0, dateItem);
        m_transactionTable->setItem(row, 1, new QTableWidgetItem(transaction.description()));
        m_transactionTable->setItem(row, 2, new QTableWidgetItem(transaction.category()));
        auto *amountItem = new QTableWidgetItem(QStringLiteral("¥%1.%2")
            .arg(transaction.amountCents() / 100)
            .arg(transaction.amountCents() % 100, 2, 10, QLatin1Char('0')));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_transactionTable->setItem(row, 3, amountItem);
        if (transaction.id() == selectedId)
            selectedRow = row;
    }
    if (selectedRow >= 0) {
        m_transactionTable->selectRow(selectedRow);
        m_transactionTable->scrollToItem(m_transactionTable->item(selectedRow, 0));
    }
    m_emptyHint->setVisible(m_transactions.isEmpty());
    m_errorLabel->hide();
    return true;
}

void MainWindow::showError(const QString &message)
{
    m_errorLabel->setText(message);
    m_errorLabel->show();
}
