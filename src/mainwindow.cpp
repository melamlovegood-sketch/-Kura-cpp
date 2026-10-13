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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
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
    m_emptyHint->setWordWrap(true);
    layout->addWidget(m_emptyHint);
    setCentralWidget(central);

    connect(m_addButton, &QPushButton::clicked, this, [this] {
        auto *dialog = new AddTransactionDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QDialog::accepted, this, [this, dialog] {
            // Display form values directly until the Transaction model is integrated.
            const int row = m_transactionTable->rowCount();
            m_transactionTable->insertRow(row);
            m_transactionTable->setItem(row, 0, new QTableWidgetItem(
                dialog->date().toString(QStringLiteral("yyyy-MM-dd"))));
            m_transactionTable->setItem(row, 1, new QTableWidgetItem(dialog->description()));
            m_transactionTable->setItem(row, 2, new QTableWidgetItem(dialog->category()));
            auto *amountItem = new QTableWidgetItem(
                QStringLiteral("¥%1").arg(dialog->amount(), 0, 'f', 2));
            amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_transactionTable->setItem(row, 3, amountItem);
            m_transactionTable->selectRow(row);
            m_transactionTable->scrollToItem(amountItem);
            m_emptyHint->hide();
        });
        dialog->open();
    });
}
