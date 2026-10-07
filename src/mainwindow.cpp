#include "mainwindow.h"

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
    // Enable and connect after A's model and dialog have been merged.
    m_addButton->setEnabled(false);
    m_addButton->setToolTip(QStringLiteral("录入功能尚未接入"));
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

    auto *hint = new QLabel(QStringLiteral("暂无消费记录，录入功能尚未接入。"), central);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    setCentralWidget(central);
}
