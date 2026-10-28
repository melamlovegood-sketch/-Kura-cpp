#include "WishPriceDialog.h"
#include "UnifiedWishlistRepository.h"
#include "WishlistRepository.h"
#include "PriceRecordDialog.h"
#include "PriceWidget.h"
#include "CategoryCatalog.h"
#include "ShellWidgets.h"
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

WishPriceDialog::WishPriceDialog(DatabaseManager &database,qint64 wishId,QWidget *parent)
    :QDialog(parent),m_database(database),m_wishId(wishId)
{
    setObjectName("wishPriceDialog"); setWindowTitle(QStringLiteral("价格记录")); resize(650,600);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(24,20,24,20); layout->setSpacing(16);
    m_summary=ShellWidgets::label(QString(),this,"muted"); m_summary->setObjectName("wishPriceSummaryLabel"); layout->addWidget(m_summary);
    auto *actions=new QHBoxLayout;
    m_record=new QPushButton(QStringLiteral("＋ 记录价格"),this); m_record->setObjectName("recordWishPriceButton"); m_record->setProperty("primary",true); actions->addWidget(m_record); actions->addStretch();
    m_target=new QPushButton(QStringLiteral("目标价与链接"),this); m_target->setObjectName("wishPriceTargetButton"); m_target->setProperty("link",true); actions->addWidget(m_target);
    m_link=new QPushButton(QStringLiteral("打开商品页"),this); m_link->setObjectName("wishPriceLinkButton"); m_link->setProperty("link",true); actions->addWidget(m_link); layout->addLayout(actions);
    m_history=new QWidget(this); auto *history=new QVBoxLayout(m_history); history->setContentsMargins(0,0,0,0); history->setSpacing(12);
    m_chart=new PriceTrendChart(m_history); history->addWidget(m_chart);
    m_table=new QTableWidget(0,3,m_history); m_table->setObjectName("wishPriceHistoryTable"); m_table->setHorizontalHeaderLabels({QStringLiteral("记录时间"),QStringLiteral("价格"),QStringLiteral("来源与条件")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->verticalHeader()->hide(); m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); m_table->setMinimumHeight(150); history->addWidget(m_table); layout->addWidget(m_history,1);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("wishPriceErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    auto *close=new QPushButton(QStringLiteral("返回清单"),this); close->setObjectName("closeWishPriceButton"); layout->addWidget(close,0,Qt::AlignRight);
    connect(close,&QPushButton::clicked,this,&QDialog::accept); connect(m_record,&QPushButton::clicked,this,&WishPriceDialog::record); connect(m_target,&QPushButton::clicked,this,&WishPriceDialog::configure);
    connect(m_link,&QPushButton::clicked,this,[this] { if (PriceRepository::validLink(m_track.sourceUrl) && !m_track.sourceUrl.isEmpty()) QDesktopServices::openUrl(QUrl(m_track.sourceUrl)); });
    refresh();
}

bool WishPriceDialog::refresh()
{
    QList<WishItem> wishes; WishlistRepository repository(m_database);
    QHash<qint64,qint64> links,positions; UnifiedWishlistRepository catalog(m_database);
    QList<PriceTrack> tracks; PriceRepository prices(m_database); QList<PriceRecord> records;
    const auto fail=[this](const QString &message) { m_error->setText(message); m_error->show(); return false; };
    if (!repository.findAll(wishes)) return fail(repository.lastError());
    bool found=false; for (const auto &wish:wishes) if (wish.id==m_wishId) { m_wish=wish; found=true; break; }
    if (!found) { m_record->setEnabled(false); m_target->setEnabled(false); return fail(QStringLiteral("商品已不存在，请返回清单。")); }
    if (!catalog.state(links,positions)) return fail(catalog.lastError());
    if (!prices.load(tracks,true)) return fail(prices.lastError());
    m_track=PriceTrack(); for (const auto &track:tracks) if (track.id==links.value(m_wishId)) { m_track=track; break; }
    if (m_track.id && !prices.history(m_track.id,records)) return fail(prices.lastError());
    setWindowTitle(m_wish.name+QStringLiteral(" · 价格记录"));
    m_summary->setText(records.isEmpty()?QStringLiteral("还没有价格记录。看到合适的价格时，随手记下来。"):
        QStringLiteral("最近 %1 · 最低 %2 · %3 次记录%4").arg(Transaction::formatAmount(m_track.latestCents),Transaction::formatAmount(m_track.lowestCents)).arg(records.size()).arg(m_track.targetCents?QStringLiteral("\n目标价 ")+Transaction::formatAmount(m_track.targetCents):QString()));
    m_history->setVisible(!records.isEmpty()); m_record->setEnabled(m_wish.status==WishStatus::Waiting);
    m_target->setVisible(m_track.id>0 && m_wish.status==WishStatus::Waiting); m_link->setVisible(!m_track.sourceUrl.isEmpty());
    m_chart->setRecords(records.mid(qMax(qsizetype(0),records.size()-200))); m_table->setRowCount(records.size());
    for (int row=0;row<records.size();++row) {
        const QStringList values={records[row].observedAt.toLocalTime().toString("yyyy-MM-dd HH:mm"),Transaction::formatAmount(records[row].amountCents),records[row].source};
        for (int col=0;col<3;++col) m_table->setItem(row,col,new QTableWidgetItem(values[col]));
    }
    m_error->hide(); return true;
}

void WishPriceDialog::record()
{
    PriceTrack context=m_track; context.name=m_wish.name; context.category=m_wish.detailCategory.isEmpty()?CategoryCatalog::fromLegacy(m_wish.category):m_wish.detailCategory;
    auto *dialog=new PriceRecordDialog(m_database,&context,this,m_wishId); dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog,&PriceRecordDialog::priceSaved,this,[this] { refresh(); emit pricesChanged(); });
    connect(dialog,&PriceRecordDialog::settingsRequested,this,[this] { reject(); emit settingsRequested(); }); dialog->open();
}

void WishPriceDialog::configure()
{
    if (!m_track.id) return;
    auto *dialog=new QDialog(this); dialog->setWindowTitle(QStringLiteral("目标价与商品链接")); dialog->setObjectName("wishPriceTargetDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(500,260);
    auto *layout=new QVBoxLayout(dialog); auto *form=new QFormLayout;
    auto *target=new QDoubleSpinBox(dialog); target->setObjectName("wishTargetAmountSpinBox"); target->setRange(0,99999999.99); target->setDecimals(2); target->setPrefix(QStringLiteral("¥ ")); target->setSpecialValueText(QStringLiteral("不设置")); target->setValue(double(m_track.targetCents)/100);
    auto *url=new QLineEdit(m_track.sourceUrl,dialog); url->setObjectName("wishPriceUrlEdit"); url->setMaxLength(2000); form->addRow(QStringLiteral("目标价"),target); form->addRow(QStringLiteral("商品链接"),url); layout->addLayout(form);
    auto *error=ShellWidgets::label(QString(),dialog,"error"); layout->addWidget(error); auto *buttons=new QHBoxLayout;
    auto *cancel=new QPushButton(QStringLiteral("取消"),dialog); auto *save=new QPushButton(QStringLiteral("保存"),dialog); save->setObjectName("saveWishPriceTargetButton"); save->setProperty("primary",true); buttons->addWidget(cancel); buttons->addStretch(); buttons->addWidget(save); layout->addLayout(buttons);
    connect(cancel,&QPushButton::clicked,dialog,&QDialog::reject);
    connect(save,&QPushButton::clicked,dialog,[this,dialog,target,url,error] {
        if (!target->hasAcceptableInput()) { error->setText(QStringLiteral("请填写有效目标价。")); return; } target->interpretText();
        PriceRepository repository(m_database);
        if (!repository.configure(m_track.id,qRound64(target->value()*100),url->text())) { error->setText(repository.lastError()); return; }
        dialog->accept(); refresh(); emit pricesChanged();
    }); dialog->open();
}
