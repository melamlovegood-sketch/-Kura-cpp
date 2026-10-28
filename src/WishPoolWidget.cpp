#include "WishPoolWidget.h"
#include "ProductDetailsDialog.h"
#include "ProductList.h"
#include "ShellWidgets.h"
#include "Transaction.h"
#include <QDialog>
#include <QHeaderView>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QTableWidget>

WishPoolWidget::WishPoolWidget(DatabaseManager &database,QWidget *parent):QFrame(parent),m_database(database),m_repository(database)
{
    setObjectName("wishPoolWidget"); setProperty("cardAlt",true);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(22,20,22,20); layout->setSpacing(12);
    auto *heading=new QHBoxLayout;
    auto *title=ShellWidgets::label(QStringLiteral("许愿池"),this,"eyebrow"); title->setObjectName("poolTitleLabel");
    heading->addWidget(title,1);
    auto *history=new QPushButton(QStringLiteral("忍住记录"),this); m_history=history; history->setObjectName("poolHistoryButton"); history->setProperty("link",true); heading->addWidget(history); layout->addLayout(heading);
    auto *productName=new ProductNameLabel(QStringLiteral("—"),this); productName->setProperty("heading",true); productName->setWordWrap(true); productName->setTextFormat(Qt::PlainText); productName->setCursor(Qt::PointingHandCursor); productName->setToolTip(QStringLiteral("打开目标商品详情")); productName->activated=[this] { if (m_available && m_wishId) ProductDetailsDialog::openFor(m_database,"wish",m_wishId,this); }; m_name=productName; m_name->setObjectName("poolNameLabel"); layout->addWidget(m_name);
    m_amount=ShellWidgets::label(QStringLiteral("—"),this); m_amount->setObjectName("poolAmountLabel"); layout->addWidget(m_amount);
    m_progress=new QProgressBar(this); m_progress->setObjectName("poolProgress"); m_progress->setRange(0,1000); m_progress->setTextVisible(false); layout->addWidget(m_progress);
    m_hint=ShellWidgets::label(QStringLiteral("忍住的心动，也可以慢慢变成一个目标。"),this,"muted"); m_hint->setObjectName("poolHintLabel"); layout->addWidget(m_hint);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("poolErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    auto *actions=new QHBoxLayout;
    auto *goal=new QPushButton(QStringLiteral("去清单选择目标  →"),this); m_goal=goal; goal->setObjectName("poolChooseGoalButton"); goal->setProperty("link",true); actions->addWidget(goal);
    actions->addStretch();
    m_changeGoal=new QPushButton(QStringLiteral("更换目标"),this); m_changeGoal->setObjectName("poolChangeGoalButton"); m_changeGoal->setProperty("link",true); actions->addWidget(m_changeGoal); m_changeGoal->hide(); layout->addLayout(actions);
    setToolTip(QStringLiteral("忍住金额记录消费选择，不代表账户余额或实际收入。"));
    m_animation=new QPropertyAnimation(m_progress,"value",this); m_animation->setDuration(450); m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(goal,&QPushButton::clicked,this,[this] {
        if (!m_available) return;
        if (!m_wishId) { emit wishlistRequested(); return; }
        if (auto *dialog=ProductDetailsDialog::openFor(m_database,"wish",m_wishId,this))
            connect(dialog,&ProductDetailsDialog::profileChanged,this,[this] { refresh(); });
    });
    connect(m_changeGoal,&QPushButton::clicked,this,&WishPoolWidget::wishlistRequested);
    connect(history,&QPushButton::clicked,this,&WishPoolWidget::showHistory);
}
void WishPoolWidget::setDatabaseAvailable(bool available) {
    m_available=available;
    for (auto *button:{m_goal,m_history,m_changeGoal}) button->setEnabled(available);
    if (available) refresh();
}
bool WishPoolWidget::refresh()
{
    if (!m_available) return false;
    WishPoolSnapshot snapshot;
    if (!m_repository.load(snapshot)) { m_error->setText(m_repository.lastError()); m_error->show(); return false; }
    m_error->hide();
    m_wishId=snapshot.wishId;
    m_name->setText(snapshot.id ? snapshot.name : QStringLiteral("还没有许愿目标"));
    m_name->setVisible(snapshot.id>0);
    m_amount->setText(snapshot.id ? QStringLiteral("忍住金额累计 %1  /  目标 %2").arg(Transaction::formatAmount(snapshot.savedCents),Transaction::formatAmount(snapshot.targetCents))
                                : QStringLiteral("忍住金额累计 %1，选好目标后一起计入。").arg(Transaction::formatAmount(snapshot.pendingCents)));
    m_amount->setVisible(snapshot.id>0 || snapshot.pendingCents>0);
    m_progress->setVisible(snapshot.id>0);
    const int value=snapshot.targetCents>0 ? int(qMin(1000.0,double(snapshot.savedCents)/snapshot.targetCents*1000)) : 0;
    m_animation->stop();
    if (!window()->property("reducedMotion").toBool() && m_hasSnapshot && isVisible() && value!=m_progress->value()) { m_animation->setStartValue(m_progress->value()); m_animation->setEndValue(value); m_animation->start(); }
    else m_progress->setValue(value);
    m_hint->setText(snapshot.id ? snapshot.savedCents>=snapshot.targetCents
        ? QStringLiteral("累计金额已达到目标，再确认：现在还想买吗？")
        : QStringLiteral("还差 %1，每一次忍住都算数。").arg(Transaction::formatAmount(snapshot.targetCents-snapshot.savedCents))
        : QStringLiteral("还没有许愿目标 — 在待购清单中选一件商品"));
    m_goal->setText(snapshot.wishId ? QStringLiteral("查看目标  →") : QStringLiteral("去清单选择目标  →"));
    m_changeGoal->setVisible(snapshot.wishId>0);
    m_hasSnapshot=true; return true;
}
void WishPoolWidget::showHistory()
{
    if (!m_available) return;
    QList<SavingRecord> records;
    if (!m_repository.history(records)) { m_error->setText(m_repository.lastError()); m_error->show(); return; }
    auto *dialog=new QDialog(this); dialog->setObjectName("poolHistoryDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("忍住记录")); dialog->resize(720,440);
    auto *layout=new QVBoxLayout(dialog);
    layout->addWidget(ShellWidgets::label(QStringLiteral("每一次忍住，都有记录"),dialog,"heading"));
    auto *table=new QTableWidget(records.size(),4,dialog); table->setObjectName("savingsHistoryTable");
    table->setHorizontalHeaderLabels({QStringLiteral("时间"),QStringLiteral("忍住了什么"),QStringLiteral("金额"),QStringLiteral("许愿目标")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->verticalHeader()->hide(); table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int row=0;row<records.size();++row) {
        const auto &record=records[row];
        const QStringList values={record.createdAt.toString("yyyy-MM-dd HH:mm"),record.description,Transaction::formatAmount(record.amountCents),record.goalName.isEmpty()?QStringLiteral("尚未指定目标"):record.goalName};
        for (int column=0;column<values.size();++column) table->setItem(row,column,new QTableWidgetItem(values[column]));
    }
    layout->addWidget(table,1);
    layout->addWidget(ShellWidgets::label(records.isEmpty()?QStringLiteral("还没有忍住记录。"):QStringLiteral("显示最近 200 条记录。"),dialog,"muted"));
    auto *close=new QPushButton(QStringLiteral("关闭"),dialog); layout->addWidget(close,0,Qt::AlignRight); connect(close,&QPushButton::clicked,dialog,&QDialog::accept); dialog->open();
}
