#include "WorkflowOverview.h"
#include "WorkflowStorage.h"
#include "ShellWidgets.h"
#include "Transaction.h"
#include <QPushButton>

WorkflowOverview::WorkflowOverview(DatabaseManager &database,QWidget *parent):QFrame(parent),m_database(database),m_prices(database)
{
    setObjectName("workflowOverview"); setProperty("card",true); auto *layout=new QVBoxLayout(this); layout->setContentsMargins(18,16,18,16); layout->setSpacing(8);
    layout->addWidget(ShellWidgets::label(QStringLiteral("留给下一次决定的提醒"),this,"eyebrow"));
    m_priceText=ShellWidgets::label(QString(),this); m_priceText->setObjectName("homePriceReminderLabel"); layout->addWidget(m_priceText);
    auto *row=new QHBoxLayout; m_price=new QPushButton(QStringLiteral("查看价格历史  →"),this); m_price->setObjectName("homePriceButton"); m_dismiss=new QPushButton(QStringLiteral("知道了"),this); m_dismiss->setObjectName("dismissHomePriceAlertButton"); row->addWidget(m_price); row->addStretch(); row->addWidget(m_dismiss); layout->addLayout(row);
    for (auto *button:{m_price,m_dismiss}) button->setProperty("link",true);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("workflowReminderErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    connect(m_price,&QPushButton::clicked,this,&WorkflowOverview::pricesRequested);
    connect(m_dismiss,&QPushButton::clicked,this,[this] { QString error; WorkflowStorage::Transaction transaction(m_database.database(),error); if (!transaction.ok() || !m_prices.dismiss(m_trackId,"target",m_recordId) || !m_prices.dismiss(m_trackId,"drop",m_recordId) || !transaction.commit()) { m_error->setText(error.isEmpty()?m_prices.lastError():error); m_error->show(); return; } refresh(); }); hide();
}
void WorkflowOverview::setDatabaseAvailable(bool available) { m_available=available; if (available) refresh(); }
bool WorkflowOverview::refresh()
{
    if (!m_available) return false; QList<PriceTrack> tracks;
    if (!m_prices.load(tracks)) { m_error->setText(QStringLiteral("提醒更新失败，仍保留上次结果。")); m_error->show(); show(); return false; }
    m_error->hide();
    m_trackId=0; m_recordId=0;
    for (const auto &track:tracks) if (track.targetHit || track.priceDrop) {
        m_trackId=track.id; m_recordId=track.latestRecordId;
        m_priceText->setText(QStringLiteral("%1 · 最近记录 %2%3%4\n来源：%5 · %6。提醒基于你保存的价格。").arg(track.name,Transaction::formatAmount(track.latestCents),track.targetHit?QStringLiteral("，已到目标价"):QString(),track.priceDrop?QStringLiteral("，较上次下降 ")+Transaction::formatAmount(track.previousCents-track.latestCents):QString(),track.source,track.observedAt.toLocalTime().toString("yyyy-MM-dd HH:mm"))); break;
    }
    m_priceText->setVisible(m_trackId>0); m_price->setVisible(m_trackId>0); m_dismiss->setVisible(m_trackId>0); setVisible(m_trackId>0); return true;
}
