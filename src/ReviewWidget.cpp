#include "ModernSelectors.h"
#include "ReviewWidget.h"
#include "ProductDetailsDialog.h"
#include "ShellWidgets.h"
#include "Transaction.h"
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>

ReviewWidget::ReviewWidget(DatabaseManager &database,QWidget *parent):QWidget(parent),m_database(database),m_repository(database)
{
    setObjectName("purchaseReviewWidget"); auto *layout=new QVBoxLayout(this); layout->setContentsMargins(24,24,24,8); layout->setSpacing(12);
    auto *heading=new QHBoxLayout; heading->addWidget(ShellWidgets::label(QStringLiteral("买回来以后，还值得吗？"),this,"heading"),1);
    m_mode=new ModernComboBox(this); m_mode->setObjectName("reviewModeComboBox"); m_mode->addItems({QStringLiteral("待复盘（含未到期）"),QStringLiteral("已完成")}); heading->addWidget(m_mode); layout->addLayout(heading);
    m_summary=ShellWidgets::label(QStringLiteral("明确购买后安排 7 天、30 天复盘；普通补记账单不自动安排。"),this,"muted"); m_summary->setObjectName("purchaseReviewSummaryLabel"); layout->addWidget(m_summary);
    m_table=new QTableWidget(0,5,this); m_table->setObjectName("purchaseReviewTable"); m_table->setHorizontalHeaderLabels({QStringLiteral("商品"),QStringLiteral("周期"),QStringLiteral("到期时间"),QStringLiteral("品牌"),QStringLiteral("状态／评价")}); m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection); m_table->verticalHeader()->hide(); m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_table->setMinimumHeight(150); m_table->setMaximumHeight(220); layout->addWidget(m_table);
    auto *actions=new QHBoxLayout; auto *refresh=new QPushButton(QStringLiteral("刷新"),this); refresh->setObjectName("refreshPurchaseReviewsButton"); refresh->setProperty("link",true); m_review=new QPushButton(QStringLiteral("去复盘"),this); m_review->setObjectName("openPurchaseReviewButton"); m_review->hide(); actions->addWidget(refresh); actions->addStretch(); layout->addLayout(actions);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("purchaseReviewErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    connect(refresh,&QPushButton::clicked,this,[this] { this->refresh(); }); connect(m_mode,&QComboBox::currentIndexChanged,this,[this] { this->refresh(); }); connect(m_table,&QTableWidget::itemSelectionChanged,this,&ReviewWidget::updateAction); connect(m_review,&QPushButton::clicked,this,&ReviewWidget::openReview); updateAction();
    connect(m_table,&QTableWidget::cellClicked,this,[this](int,int) { updateAction(); openReview(); });
    connect(m_table,&QTableWidget::cellActivated,this,[this](int,int) { updateAction(); openReview(); });
    m_table->setToolTip(QStringLiteral("点击到期商品，填写两题复盘。"));
}
void ReviewWidget::setDatabaseAvailable(bool available) { m_available=available; m_mode->setEnabled(available); if (available) refresh(); updateAction(); }
bool ReviewWidget::refresh()
{
    if (!m_available) return false;
    QList<ReviewTask> loaded; qint64 due=0;
    if (!m_repository.list(loaded,m_mode->currentIndex()==1,QDateTime::currentDateTimeUtc(),true) || !m_repository.dueCount(due)) { m_error->setText(m_repository.lastError()); m_error->show(); return false; }
    m_tasks=loaded; m_error->hide(); m_table->setRowCount(0); m_table->setRowCount(loaded.size());
    const auto now=QDateTime::currentDateTimeUtc();
    for (int row=0;row<loaded.size();++row) { const auto &task=loaded[row]; const QString state=task.completedAt.isValid()?(task.worthiness==1?QStringLiteral("值得"):task.worthiness==-1?QStringLiteral("后悔"):QStringLiteral("一般")):(task.dueAt<=now?QStringLiteral("已到期，待复盘"):QStringLiteral("尚未到期")); const QStringList values={task.name,QStringLiteral("%1 天").arg(task.day),task.dueAt.toLocalTime().toString("yyyy-MM-dd HH:mm"),task.brandName.isEmpty()?QStringLiteral("未选择"):task.brandName,state}; for (int column=0;column<values.size();++column) { auto *item=new QTableWidgetItem(values[column]); item->setToolTip(column==4?task.note:values[column]); m_table->setItem(row,column,item); } m_table->item(row,0)->setData(Qt::UserRole,task.id); }
    m_summary->setText(m_mode->currentIndex()==1?QStringLiteral("已完成的购买反馈"):QStringLiteral("%1 项到期 · 点击商品开始复盘").arg(due)); updateAction(); return true;
}
void ReviewWidget::updateAction()
{
    const auto row=m_table->currentRow(); m_review->setEnabled(m_available && row>=0 && row<m_tasks.size() && m_mode->currentIndex()==0 && m_table->item(row,0) && m_table->item(row,0)->isSelected() && m_tasks[row].dueAt<=QDateTime::currentDateTimeUtc());
}
void ReviewWidget::openReview()
{
    if (auto *existing=findChild<QDialog *>("purchaseReviewDialog"); existing && existing->isVisible()) { existing->raise(); return; }
    const auto row=m_table->currentRow(); if (!m_review->isEnabled() || row<0 || row>=m_tasks.size()) return; const auto task=m_tasks[row];
    auto *dialog=new QDialog(this); dialog->setObjectName("purchaseReviewDialog"); dialog->setWindowTitle(QStringLiteral("购买后复盘")); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(500,410);
    auto *layout=new QVBoxLayout(dialog); layout->setContentsMargins(24,24,24,24); layout->setSpacing(14);
    layout->addWidget(ShellWidgets::label(task.name,dialog,"heading")); layout->addWidget(ShellWidgets::label(QStringLiteral("%1 天复盘 · 购买金额 %2").arg(task.day).arg(Transaction::formatAmount(task.amountCents)),dialog,"muted"));
    auto *frequency=new ModernComboBox(dialog); frequency->setObjectName("reviewFrequencyComboBox"); frequency->addItem(QStringLiteral("请选择使用频率"),-1); frequency->addItem(QStringLiteral("几乎不用"),0); frequency->addItem(QStringLiteral("偶尔使用"),1); frequency->addItem(QStringLiteral("每天使用"),2);
    auto *worth=new ModernComboBox(dialog); worth->setObjectName("reviewWorthinessComboBox"); worth->addItem(QStringLiteral("请选择值得度"),99); worth->addItem(QStringLiteral("值得"),1); worth->addItem(QStringLiteral("一般"),0); worth->addItem(QStringLiteral("后悔"),-1);
    auto *note=new QPlainTextEdit(dialog); note->setObjectName("reviewNoteEdit"); note->setMaximumHeight(100); note->setPlaceholderText(QStringLiteral("可选，最多 500 字")); auto *form=new QFormLayout; form->addRow(QStringLiteral("用了多少"),frequency); form->addRow(QStringLiteral("还觉得值吗"),worth); form->addRow(QStringLiteral("想留下的话"),note); layout->addLayout(form);
    auto *error=ShellWidgets::label(QString(),dialog,"error"); error->setObjectName("reviewValidationLabel"); layout->addWidget(error);
    auto *buttons=new QHBoxLayout; auto *cancel=new QPushButton(QStringLiteral("取消"),dialog); auto *save=new QPushButton(QStringLiteral("保存复盘"),dialog); save->setObjectName("savePurchaseReviewButton"); buttons->addWidget(cancel); buttons->addStretch(); buttons->addWidget(save); layout->addLayout(buttons); connect(cancel,&QPushButton::clicked,dialog,&QDialog::reject);
    connect(save,&QPushButton::clicked,dialog,[this,dialog,task,frequency,worth,note,error,save] { if (!save->isEnabled()) return; save->setEnabled(false); if (!m_repository.complete(task.id,frequency->currentData().toInt(),worth->currentData().toInt(),note->toPlainText())) { error->setText(m_repository.lastError()); save->setEnabled(true); return; } dialog->accept(); refresh(); emit reviewsChanged(); }); dialog->open();
}
