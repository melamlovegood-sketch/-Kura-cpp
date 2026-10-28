#include "PriceWidget.h"
#include "ShellWidgets.h"
#include "CategoryCatalog.h"
#include "Transaction.h"
#include "ProductList.h"
#include "PriceRecordDialog.h"
#include "ProductDetailsDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QGridLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QWidgetAction>
#include <QTableWidget>
#include <QUrl>
#include <QUuid>

PriceTrendChart::PriceTrendChart(QWidget *parent):QWidget(parent) { setObjectName("priceTrendChart"); setMinimumHeight(180); }
void PriceTrendChart::setRecords(const QList<PriceRecord> &records) { m_records=records; update(); }
void PriceTrendChart::paintEvent(QPaintEvent *)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing); const auto colors=window()->palette(); const auto ink=colors.windowText().color(); p.fillRect(rect(),colors.window()); p.setPen(ink);
    if (m_records.isEmpty()) { p.drawText(rect(),Qt::AlignCenter,QStringLiteral("选择商品，查看真实记录的价格走势。")); return; }
    qint64 low=m_records[0].amountCents,high=low; for (const auto &record:m_records) { low=qMin(low,record.amountCents); high=qMax(high,record.amountCents); }
    const qint64 range=qMax(qint64(100),high-low); const auto labelWidth=fontMetrics().horizontalAdvance(Transaction::formatAmount(high))+14;
    const QRectF plot=QRectF(rect()).adjusted(labelWidth,18,-20,-42); if (plot.width()<1 || plot.height()<1) return;
    QColor grid=ink; grid.setAlpha(40); p.setPen(grid);
    for (int i=0;i<3;++i) { const double y=plot.top()+plot.height()*i/2; p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y)); }
    p.setPen(ink); p.drawText(QRectF(0,plot.top()-8,labelWidth-8,20),Qt::AlignRight,Transaction::formatAmount(high)); p.drawText(QRectF(0,plot.bottom()-8,labelWidth-8,20),Qt::AlignRight,Transaction::formatAmount(low));
    const auto first=m_records.first().observedAt.toMSecsSinceEpoch(),last=m_records.last().observedAt.toMSecsSinceEpoch();
    QPainterPath line; QList<QPointF> points;
    for (int i=0;i<m_records.size();++i) { const auto &record=m_records[i]; const double x=m_records.size()==1?plot.center().x():last>first?plot.left()+plot.width()*double(record.observedAt.toMSecsSinceEpoch()-first)/double(last-first):plot.left()+plot.width()*i/(m_records.size()-1); const double y=high==low?plot.center().y():plot.bottom()-plot.height()*double(record.amountCents-low)/range; const QPointF point(x,y); points << point; if (i==0) line.moveTo(point); else line.lineTo(point); }
    p.setPen(QPen(ink,2)); if (m_records.size()>1) p.drawPath(line); p.setBrush(ink); for (const auto &point:points) p.drawEllipse(point,3,3);
    p.drawText(QRectF(plot.left(),plot.bottom()+10,plot.width()/2,24),Qt::AlignLeft,m_records.first().observedAt.toLocalTime().toString("yyyy-MM-dd")); p.drawText(QRectF(plot.center().x(),plot.bottom()+10,plot.width()/2,24),Qt::AlignRight,m_records.size()==1?QStringLiteral("只有一次记录"):m_records.last().observedAt.toLocalTime().toString("yyyy-MM-dd"));
}

PriceWidget::PriceWidget(DatabaseManager &database,QWidget *parent):QWidget(parent),m_database(database),m_repository(database)
{
    setObjectName("priceWidget");
    auto *layout=new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(12);
    m_addBar=new QWidget(this);
    auto *heading=new QHBoxLayout(m_addBar);
    heading->setContentsMargins(0,0,0,0);
    heading->addStretch();
    m_new=new QPushButton(QStringLiteral("＋ 添加商品"),m_addBar);
    m_new->setObjectName("newPriceTrackButton");
    m_new->setProperty("planningAdd",true);
    heading->addWidget(m_new);
    layout->addWidget(m_addBar);
    auto *summary=new QHBoxLayout;
    m_summary=ShellWidgets::label(QStringLiteral("价格记录：—"),this,"muted");
    m_summary->setObjectName("priceCountLabel");
    summary->addWidget(m_summary,1);
    auto *more=new QToolButton(this);
    more->setObjectName("priceMoreButton");
    more->setText(QStringLiteral("···"));
    more->setAccessibleName(QStringLiteral("价格清单选项"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu=new QMenu(more);
    m_archived=new QCheckBox(QStringLiteral("包含已结束追踪"),this);
    m_archived->setObjectName("showArchivedPricesCheckBox");
    auto *archivedAction=new QWidgetAction(menu);
    archivedAction->setDefaultWidget(m_archived);
    menu->addAction(archivedAction);
    connect(menu->addAction(QStringLiteral("刷新")),&QAction::triggered,this,[this] { refresh(); });
    connect(m_archived,&QCheckBox::clicked,menu,&QMenu::close);
    more->setMenu(menu);
    summary->addWidget(more);
    layout->addLayout(summary);
    m_table=new ProductList(6,this);
    m_table->setObjectName("priceTracksTable");
    m_table->setProductDetailsHandler([this](int row) { if (m_available && row>=0 && row<m_tracks.size()) ProductDetailsDialog::openFor(m_database,"price",m_tracks[row].id,this); });
    m_table->setHorizontalHeaderLabels({QStringLiteral("商品"),QStringLiteral("最近记录"),QStringLiteral("记录时间"),QStringLiteral("最低记录"),QStringLiteral("目标价"),QStringLiteral("次数")});
    layout->addWidget(m_table);
    m_empty=ShellWidgets::label(QStringLiteral("还没有在蹲的商品，点击右上角添加。"),this);
    m_empty->setObjectName("priceEmptyLabel");
    m_empty->setProperty("planningEmpty",true);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setMinimumHeight(90);
    layout->addWidget(m_empty);
    auto *details=new QWidget(this);
    details->setObjectName("priceProductDetails");
    auto *detailLayout=new QVBoxLayout(details);
    detailLayout->setContentsMargins(25,0,25,24);
    detailLayout->setSpacing(16);
    m_detail=ShellWidgets::label(QString(),details,"muted");
    m_detail->setObjectName("priceDetailLabel");
    auto *info=new QHBoxLayout; info->addWidget(m_detail,1);
    auto *productMore=new QToolButton(details); productMore->setObjectName("priceProductMoreButton"); productMore->setText(QStringLiteral("···")); productMore->setAccessibleName(QStringLiteral("商品蹲价选项")); productMore->setPopupMode(QToolButton::InstantPopup);
    auto *productMenu=new QMenu(productMore); productMenu->setObjectName("priceProductMenu"); productMore->setMenu(productMenu); info->addWidget(productMore); detailLayout->addLayout(info);
    auto *actions=new QHBoxLayout;
    actions->setSpacing(8);
    m_record=new QPushButton(QStringLiteral("记录价格"),details); m_record->setObjectName("appendPriceButton");
    m_record->setProperty("primary",true);
    m_configure=productMenu->addAction(QStringLiteral("编辑目标价与商品链接")); m_configure->setObjectName("configurePriceAction");
    m_link=productMenu->addAction(QStringLiteral("打开商品页 ↗")); m_link->setObjectName("openPriceLinkAction");
    productMenu->addSeparator();
    m_end=productMenu->addAction(QStringLiteral("结束蹲价")); m_end->setObjectName("archivePriceAction");
    m_history=new QPushButton(QStringLiteral("查看价格走势 →"),details); m_history->setObjectName("priceHistoryButton"); m_history->setProperty("link",true);
    actions->addWidget(m_record); actions->addStretch(); actions->addWidget(m_history);
    detailLayout->addLayout(actions);
    m_table->setDetailWidget(details);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("priceErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    layout->addStretch();
    connect(m_table,&QTableWidget::itemSelectionChanged,this,&PriceWidget::updateSelection); connect(m_new,&QPushButton::clicked,this,[this] { openRecord(false); }); connect(m_record,&QPushButton::clicked,this,[this] { openRecord(true); }); connect(m_configure,&QAction::triggered,this,&PriceWidget::configure); connect(m_history,&QPushButton::clicked,this,&PriceWidget::showHistory); connect(m_archived,&QCheckBox::toggled,this,[this] { refresh(); });
    connect(m_link,&QAction::triggered,this,[this] { if (const auto *track=selected(); track && PriceRepository::validLink(track->sourceUrl) && !track->sourceUrl.isEmpty()) QDesktopServices::openUrl(QUrl(track->sourceUrl)); });
    connect(m_end,&QAction::triggered,this,[this] {
        const auto *track=selected(); if (!track || !track->active) return; const auto id=track->id;
        auto *confirmation=new QMessageBox(QMessageBox::Question,QStringLiteral("结束追踪"),QStringLiteral("结束后不再提醒，价格历史会保留。确定吗？"),QMessageBox::Yes|QMessageBox::No,this); confirmation->setObjectName("archivePriceConfirmation"); confirmation->setDefaultButton(QMessageBox::No); confirmation->setAttribute(Qt::WA_DeleteOnClose);
        connect(confirmation,&QMessageBox::finished,this,[this,confirmation,id](int) { if (confirmation->standardButton(confirmation->clickedButton())!=QMessageBox::Yes) return; if (!m_repository.archive(id)) { m_error->setText(m_repository.lastError()); m_error->show(); return; } refresh(); emit pricesChanged(); }); confirmation->open();
    }); updateSelection();
}
QPushButton *PriceWidget::takeAddButton()
{
    m_addBar->hide();
    return m_new;
}
void PriceWidget::setDatabaseAvailable(bool available) { m_available=available; m_archived->setEnabled(available); if (available) refresh(); updateSelection(); }
const PriceTrack *PriceWidget::selected() const
{
    const int row=m_table->currentRow(); if (row<0 || !m_table->item(row,0) || !m_table->item(row,0)->isSelected()) return nullptr; const auto id=m_table->item(row,0)->data(Qt::UserRole).toLongLong(); for (const auto &track:m_tracks) if (track.id==id) return &track; return nullptr;
}
bool PriceWidget::refresh(qint64 selectedId)
{
    if (!m_available) return false;
    if (!selectedId) if (const auto *track=selected()) selectedId=track->id;
    QList<PriceTrack> loaded; if (!m_repository.load(loaded,m_archived->isChecked())) { m_error->setText(m_repository.lastError()); m_error->show(); return false; }
    m_tracks=loaded; m_error->hide(); m_table->resetRows(loaded.size());
    int selectedRow=-1,activeCount=0;
    for (int row=0;row<loaded.size();++row) {
        const auto &track=loaded[row];
        const QStringList values={track.name+(track.active?QString():QStringLiteral("（已结束）")),Transaction::formatAmount(track.latestCents),track.observedAt.toLocalTime().toString("yyyy-MM-dd HH:mm"),Transaction::formatAmount(track.lowestCents),track.targetCents?Transaction::formatAmount(track.targetCents):QStringLiteral("未设置"),QString::number(track.recordCount)};
        for (int column=0;column<values.size();++column) m_table->setItem(row,column,new QTableWidgetItem(values[column]));
        m_table->item(row,0)->setData(Qt::UserRole,track.id);
        QString status=track.targetCents?QStringLiteral("目标价 %1").arg(Transaction::formatAmount(track.targetCents)):QStringLiteral("尚未设置目标价");
        if (!track.active) status+=QStringLiteral(" · 已结束");
        else if (track.targetHit) status+=QStringLiteral(" · 已到目标价");
        m_table->setSummary(row,track.name,status,Transaction::formatAmount(track.latestCents),QStringLiteral("最近记录"));
        if (track.active) ++activeCount;
        if (track.id==selectedId) selectedRow=row;
    }
    if (selectedRow>=0) m_table->selectRow(selectedRow);
    m_summary->setText(m_archived->isChecked()?QStringLiteral("%1 件商品 · %2 件在蹲").arg(loaded.size()).arg(activeCount):QStringLiteral("%1 件在蹲").arg(activeCount));
    m_empty->setVisible(loaded.isEmpty());
    m_table->setVisible(!loaded.isEmpty());
    updateSelection();
    m_table->updateExpansion();
    return true;
}
void PriceWidget::updateSelection()
{
    const auto *track=selected(); const bool active=m_available && track && track->active;
    m_new->setEnabled(m_available); m_record->setEnabled(active); m_record->setVisible(track && track->active);
    m_configure->setEnabled(active); m_configure->setVisible(track && track->active);
    m_end->setEnabled(active); m_end->setVisible(track && track->active);
    m_history->setEnabled(m_available && track);
    m_link->setEnabled(m_available && track && PriceRepository::validLink(track->sourceUrl) && !track->sourceUrl.isEmpty());
    m_link->setVisible(track && !track->sourceUrl.isEmpty());
    findChild<QToolButton *>("priceProductMoreButton")->setEnabled(m_available && track);
    findChild<QToolButton *>("priceProductMoreButton")->setVisible(track && (track->active || !track->sourceUrl.isEmpty()));
    if (!track) { m_detail->clear(); return; }
    m_detail->setText(QStringLiteral("当前价 %1 · 目标价 %2")
        .arg(Transaction::formatAmount(track->latestCents), track->targetCents?Transaction::formatAmount(track->targetCents):QStringLiteral("未设置")));
    m_detail->setToolTip(QStringLiteral("%1更新 · 来源：%2").arg(track->observedAt.toLocalTime().toString("yyyy-MM-dd HH:mm"),track->source));
}
void PriceWidget::openRecord(bool existing)
{
    if (!m_available) return;
    const auto *track=existing?selected():nullptr;
    if (existing && (!track || !track->active)) return;
    auto *dialog=new PriceRecordDialog(m_database,track,this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog,&PriceRecordDialog::priceSaved,this,[this](qint64 id) { refresh(id); emit pricesChanged(); });
    connect(dialog,&PriceRecordDialog::settingsRequested,this,&PriceWidget::settingsRequested);
    dialog->open();
}
void PriceWidget::configure()
{
    const auto *track=selected(); if (!track || !track->active) return; const auto id=track->id;
    auto *dialog=new QDialog(this); dialog->setObjectName("priceSettingsDialog"); dialog->setWindowTitle(QStringLiteral("目标价与商品链接")); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(540,260); auto *layout=new QVBoxLayout(dialog); auto *form=new QFormLayout;
    auto *target=new QDoubleSpinBox(dialog); target->setObjectName("priceTargetSpinBox"); target->setRange(0,99999999.99); target->setDecimals(2); target->setValue(double(track->targetCents)/100); target->setSpecialValueText(QStringLiteral("不设置目标价"));
    auto *url=new QLineEdit(track->sourceUrl,dialog); url->setObjectName("priceSourceUrlEdit"); url->setMaxLength(2000); form->addRow(QStringLiteral("目标价（元）"),target); form->addRow(QStringLiteral("商品链接"),url); layout->addLayout(form); auto *error=ShellWidgets::label(QString(),dialog,"error"); layout->addWidget(error); auto *save=new QPushButton(QStringLiteral("保存目标价"),dialog); save->setObjectName("savePriceTargetButton"); layout->addWidget(save,0,Qt::AlignRight);
    connect(save,&QPushButton::clicked,dialog,[this,dialog,id,target,url,error] { if (!target->hasAcceptableInput()) { error->setText(QStringLiteral("请填写有效目标价。")); return; } target->interpretText(); if (!m_repository.configure(id,qRound64(target->value()*100),url->text())) { error->setText(m_repository.lastError()); return; } dialog->accept(); refresh(id); emit pricesChanged(); }); dialog->open();
}
void PriceWidget::showHistory()
{
    const auto *track=selected(); if (!m_available || !track) return;
    QList<PriceRecord> records;
    if (!m_repository.history(track->id,records)) { m_error->setText(m_repository.lastError()); m_error->show(); return; }
    auto *dialog=new QDialog(this);
    dialog->setObjectName("priceHistoryDialog"); dialog->setWindowTitle(track->name+QStringLiteral(" · 价格走势"));
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(680,580);
    auto *layout=new QVBoxLayout(dialog); layout->setSpacing(16);
    auto *summary=ShellWidgets::label(QStringLiteral("当前价 %1 · 最低 %2 · %3 次记录")
        .arg(Transaction::formatAmount(track->latestCents),Transaction::formatAmount(track->lowestCents)).arg(records.size()),dialog,"muted");
    layout->addWidget(summary);
    auto *chart=new PriceTrendChart(dialog);
    chart->setRecords(records.mid(qMax(qsizetype(0),records.size()-200))); layout->addWidget(chart);
    layout->addWidget(ShellWidgets::label(records.size()>200?QStringLiteral("走势展示最近 200 次记录，以下为全部历史明细"):QStringLiteral("历史明细"),dialog,"muted"));
    auto *table=new QTableWidget(records.size(),3,dialog); table->setObjectName("priceHistoryTable");
    table->setHorizontalHeaderLabels({QStringLiteral("记录时间"),QStringLiteral("金额"),QStringLiteral("平台／来源")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); table->verticalHeader()->hide();
    table->setMinimumHeight(150);
    for (int row=0;row<records.size();++row) {
        const QStringList values={records[row].observedAt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss"),Transaction::formatAmount(records[row].amountCents),records[row].source};
        for (int column=0;column<values.size();++column) table->setItem(row,column,new QTableWidgetItem(values[column]));
    }
    layout->addWidget(table);
    auto *close=new QPushButton(QStringLiteral("返回清单"),dialog); close->setObjectName("closePriceHistoryButton");
    layout->addWidget(close,0,Qt::AlignRight); connect(close,&QPushButton::clicked,dialog,&QDialog::accept);
    dialog->open();
}
