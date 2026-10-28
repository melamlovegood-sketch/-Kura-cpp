#include "ModernSelectors.h"
#include "StatisticsWidget.h"
#include "MonthlyTrendChart.h"
#include "CategorySummary.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QTabWidget>

StatisticsWidget::StatisticsWidget(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("statisticsWidget"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *heading = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("消费复盘"), this);
    title->setProperty("heading", true);
    heading->addWidget(title);
    heading->addStretch();
    heading->addWidget(new QLabel(QStringLiteral("年份"), this));
    m_yearEdit = new QSpinBox(this);
    m_yearEdit->setObjectName(QStringLiteral("statisticsYearSpinBox"));
    m_yearEdit->setRange(1, 9999);
    m_yearEdit->setValue(QDate::currentDate().year());
    m_yearEdit->setEnabled(false);
    heading->addWidget(m_yearEdit);
    layout->addLayout(heading);

    auto *summary = new QHBoxLayout;
    m_totalLabel = new QLabel(QStringLiteral("全年支出：—"), this);
    m_totalLabel->setObjectName(QStringLiteral("yearExpenseLabel"));
    m_averageLabel = new QLabel(QStringLiteral("全年月均：—"), this);
    m_averageLabel->setObjectName(QStringLiteral("yearAverageLabel"));
    m_averageLabel->setToolTip(QStringLiteral("全年总支出除以 12，包含没有消费的月份，四舍五入到分。"));
    m_peakLabel = new QLabel(QStringLiteral("最高月份：—"), this);
    m_peakLabel->setObjectName(QStringLiteral("peakMonthLabel"));
    summary->addWidget(m_totalLabel);
    summary->addWidget(m_averageLabel);
    summary->addWidget(m_peakLabel);
    layout->addLayout(summary);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName(QStringLiteral("statisticsErrorLabel"));
    m_errorLabel->setTextFormat(Qt::PlainText);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setProperty("error", true);
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);
    m_chart = new MonthlyTrendChart(this);
    layout->addWidget(m_chart, 1);
    layout->addWidget(new QLabel(QStringLiteral("悬停查看金额和笔数，双击图表中的月份查看消费记录。"), this));

    auto *details = new QTabWidget(this);
    details->setObjectName(QStringLiteral("statisticsDetailsTabs"));
    auto *monthlyPage = new QWidget(details);
    auto *monthlyLayout = new QVBoxLayout(monthlyPage);
    auto *tableHeading = new QHBoxLayout;
    tableHeading->addWidget(new QLabel(QStringLiteral("月度明细"), this));
    tableHeading->addStretch();
    m_viewMonthButton = new QPushButton(QStringLiteral("查看所选月份记录"), this);
    m_viewMonthButton->setObjectName(QStringLiteral("viewStatisticsMonthButton"));
    m_viewMonthButton->setEnabled(false);
    tableHeading->addWidget(m_viewMonthButton);
    monthlyLayout->addLayout(tableHeading);
    m_monthTable = new QTableWidget(0, 3, this);
    m_monthTable->setObjectName(QStringLiteral("monthlyStatisticsTable"));
    m_monthTable->setHorizontalHeaderLabels({QStringLiteral("月份"), QStringLiteral("消费笔数"), QStringLiteral("支出金额")});
    m_monthTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_monthTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_monthTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_monthTable->setAlternatingRowColors(true);
    m_monthTable->verticalHeader()->hide();
    m_monthTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    monthlyLayout->addWidget(m_monthTable, 1);
    details->addTab(monthlyPage,QStringLiteral("月度明细"));
    auto *categoryPage = new QWidget(details);
    auto *categoryLayout = new QVBoxLayout(categoryPage);
    auto *categoryHeading = new QHBoxLayout;
    categoryHeading->addWidget(new QLabel(QStringLiteral("统计月份"),categoryPage));
    m_categoryMonth = new ModernComboBox(categoryPage);
    m_categoryMonth->setObjectName(QStringLiteral("categoryReviewMonthComboBox"));
    for (int month=1;month<=12;++month) m_categoryMonth->addItem(QStringLiteral("%1月").arg(month));
    m_categoryMonth->setCurrentIndex(QDate::currentDate().month()-1);
    m_categoryMonth->setEnabled(false);
    categoryHeading->addWidget(m_categoryMonth);
    categoryHeading->addStretch();
    m_categoryTotal = new QLabel(QStringLiteral("分类合计：—"),categoryPage);
    m_categoryTotal->setObjectName(QStringLiteral("categoryReviewTotalLabel"));
    categoryHeading->addWidget(m_categoryTotal);
    categoryLayout->addLayout(categoryHeading);
    m_categoryTable = new QTableWidget(0,6,categoryPage);
    m_categoryTable->setObjectName(QStringLiteral("categoryReviewTable"));
    m_categoryTable->setHorizontalHeaderLabels({QStringLiteral("分类"),QStringLiteral("金额"),QStringLiteral("笔数"),QStringLiteral("占比"),QStringLiteral("上月金额"),QStringLiteral("变化")});
    m_categoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_categoryTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_categoryTable->setAlternatingRowColors(true);
    m_categoryTable->verticalHeader()->hide();
    m_categoryTable->verticalHeader()->setDefaultSectionSize(34);
    m_categoryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_categoryTable->setMinimumHeight(240);
    categoryLayout->addWidget(m_categoryTable);
    auto *categoryHint = new QLabel(QStringLiteral("变化 = 所选月 − 上月；正数为增加，负数为减少。占比四舍五入后合计可能略有差异。"),categoryPage);
    categoryHint->setWordWrap(true); categoryLayout->addWidget(categoryHint);
    details->addTab(categoryPage,QStringLiteral("分类复盘"));
    layout->addWidget(details,1);
    connect(m_categoryMonth,&QComboBox::currentIndexChanged,this,[this] { if (m_hasSnapshot) renderCategories(); });

    connect(m_yearEdit, &QSpinBox::valueChanged, this, [this](int year) {
        if (!m_hasSnapshot)
            return;
        YearlyExpenseStatistics statistics;
        QString error;
        if (ExpenseStatistics::calculateYear(m_records, year, statistics, error))
            render(statistics);
        else
            showLoadError(error);
    });
    connect(m_monthTable, &QTableWidget::itemSelectionChanged, this, &StatisticsWidget::updateSelection);
    connect(m_monthTable, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { activateSelectedMonth(); });
    connect(m_viewMonthButton, &QPushButton::clicked, this, &StatisticsWidget::activateSelectedMonth);
    connect(m_chart, &MonthlyTrendChart::monthActivated, this, &StatisticsWidget::monthActivated);
}

bool StatisticsWidget::setTransactions(const QList<Transaction> &records)
{
    YearlyExpenseStatistics statistics;
    QString error;
    if (!ExpenseStatistics::calculateYear(records, m_yearEdit->value(), statistics, error)) {
        showLoadError(error);
        return false;
    }
    m_records = records;
    m_loadError.clear();
    m_hasSnapshot = true;
    m_yearEdit->setEnabled(true);
    m_categoryMonth->setEnabled(true);
    render(statistics);
    return true;
}

void StatisticsWidget::render(const YearlyExpenseStatistics &statistics)
{
    int selectedMonth = QDate::currentDate().month();
    if (m_monthTable->currentRow() >= 0)
        selectedMonth = m_monthTable->currentRow() + 1;
    m_totalLabel->setText(QStringLiteral("全年支出：%1 · %2 笔")
                         .arg(Transaction::formatAmount(statistics.totalCents)).arg(statistics.recordCount));
    m_averageLabel->setText(QStringLiteral("全年月均：%1")
                           .arg(Transaction::formatAmount(statistics.averageMonthCents())));
    const int peak = statistics.peakMonthIndex();
    m_peakLabel->setText(peak < 0 ? QStringLiteral("最高月份：—")
        : QStringLiteral("最高月份：%1 · %2")
            .arg(statistics.months[peak].month.toString(QStringLiteral("yyyy-MM")))
            .arg(Transaction::formatAmount(statistics.months[peak].totalCents)));
    m_chart->setStatistics(statistics);
    renderCategories();
    m_monthTable->setRowCount(0);
    for (const auto &month : statistics.months) {
        const int row = m_monthTable->rowCount();
        m_monthTable->insertRow(row);
        auto *date = new QTableWidgetItem(month.month.toString(QStringLiteral("yyyy-MM")));
        date->setData(Qt::UserRole, month.month);
        m_monthTable->setItem(row, 0, date);
        m_monthTable->setItem(row, 1, new QTableWidgetItem(QString::number(month.recordCount)));
        auto *amount = new QTableWidgetItem(Transaction::formatAmount(month.totalCents));
        amount->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_monthTable->setItem(row, 2, amount);
    }
    m_monthTable->selectRow(selectedMonth - 1);
    m_monthTable->scrollToItem(m_monthTable->item(selectedMonth - 1, 0));
    if (m_loadError.isEmpty())
        m_errorLabel->hide();
    else
        showLoadError(m_loadError);
    updateSelection();
}

void StatisticsWidget::setMonth(const QDate &month)
{
    if (!month.isValid()) return;
    m_yearEdit->setValue(month.year());
    m_categoryMonth->setCurrentIndex(month.month()-1);
}

void StatisticsWidget::renderCategories()
{
    const QDate month(m_yearEdit->value(),m_categoryMonth->currentIndex()+1,1);
    const auto categories = CategorySummary::calculatePurposes(m_records,month);
    qint64 total=0, count=0;
    m_categoryTable->setRowCount(categories.size());
    for (int row=0;row<categories.size();++row) {
        const auto &entry = categories[row];
        total += entry.amountCents; count += entry.transactionCount;
        const QString change = (entry.changeCents>0 ? QStringLiteral("+") : QString()) + Transaction::formatAmount(entry.changeCents);
        const QStringList cells = {entry.category,Transaction::formatAmount(entry.amountCents),QString::number(entry.transactionCount),entry.formattedPercentage(),Transaction::formatAmount(entry.previousMonthAmountCents),change};
        for (int column=0;column<cells.size();++column) m_categoryTable->setItem(row,column,new QTableWidgetItem(cells[column]));
    }
    m_categoryTotal->setText(QStringLiteral("%1 · %2 · %3 笔").arg(month.toString(QStringLiteral("yyyy-MM")),Transaction::formatAmount(total)).arg(count));
}

void StatisticsWidget::updateSelection()
{
    const int row = m_monthTable->currentRow();
    m_viewMonthButton->setEnabled(m_hasSnapshot && row >= 0 && m_monthTable->item(row, 0)
                                 && m_monthTable->item(row, 0)->isSelected());
}

void StatisticsWidget::activateSelectedMonth()
{
    if (m_viewMonthButton->isEnabled())
        emit monthActivated(m_monthTable->item(m_monthTable->currentRow(), 0)->data(Qt::UserRole).toDate());
}

void StatisticsWidget::showLoadError(const QString &message)
{
    m_loadError = message;
    m_errorLabel->setText(m_hasSnapshot
        ? QStringLiteral("统计更新失败，仍显示上次读取的数据：%1").arg(message)
        : QStringLiteral("统计加载失败：%1").arg(message));
    m_errorLabel->show();
}
