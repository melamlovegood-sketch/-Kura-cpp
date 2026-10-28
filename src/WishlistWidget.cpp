#include "ModernSelectors.h"
#include "WishlistWidget.h"
#include "WishPriceDialog.h"
#include <QActionGroup>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QSettings>
#include <algorithm>
#include <limits>
#include "ProductDetailsDialog.h"
#include "CategoryCatalog.h"
#include "WishDialog.h"
#include "AddTransactionDialog.h"
#include "ProductList.h"
#include "ShellWidgets.h"
#include <QAbstractButton>
#include <QComboBox>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QMenu>
#include <QToolButton>
#include <QWidgetAction>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

WishlistWidget::WishlistWidget(DatabaseManager &database, QWidget *parent)
    : QWidget(parent), m_database(database), m_repository(database), m_catalog(database), m_now(QDateTime::currentDateTimeUtc())
{
    setObjectName(QStringLiteral("wishlistWidget"));
    if (database.databasePath()!=":memory:") {
        QSettings settings(database.databasePath()+".ui.ini",QSettings::IniFormat);
        const auto mode=settings.value("wishlist/sort_mode","newest").toString();
        if (mode=="oldest" || mode=="manual") m_sortMode=mode;
    }
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    m_addBar = new QWidget(this);
    auto *heading = new QHBoxLayout(m_addBar);
    heading->setContentsMargins(0, 0, 0, 0);
    heading->addStretch();
    m_add = new QPushButton(QStringLiteral("＋ 添加商品"), m_addBar);
    m_add->setObjectName(QStringLiteral("addWishButton"));
    m_add->setProperty("planningAdd", true);
    m_refresh = new QPushButton(QStringLiteral("刷新"), this);
    m_refresh->setObjectName(QStringLiteral("refreshWishlistButton"));
    heading->addWidget(m_add);
    layout->addWidget(m_addBar);
    m_summary = new QLabel(QStringLiteral("愿望数据：—"), this);
    m_summary->setObjectName(QStringLiteral("wishlistSummaryLabel"));
    m_summary->setWordWrap(true);
    m_summary->setProperty("muted", true);
    auto *summary = new QHBoxLayout;
    summary->addWidget(m_summary, 1);
    auto *more = new QToolButton(this);
    more->setObjectName(QStringLiteral("wishlistMoreButton"));
    more->setText(QStringLiteral("···"));
    more->setAccessibleName(QStringLiteral("筛选与刷新清单"));
    more->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(more);
    menu->addSection(QStringLiteral("显示状态"));
    m_filter = new ModernComboBox(this);
    m_filter->setObjectName(QStringLiteral("wishlistStatusFilter"));
    m_filter->addItems({QStringLiteral("全部"), QStringLiteral("冷静期中"), QStringLiteral("待决定"),
                       QStringLiteral("已购买"), QStringLiteral("已放弃／归档"), QStringLiteral("待购中")});
    m_filter->setCurrentIndex(5);
    auto *filterAction = new QWidgetAction(menu);
    filterAction->setDefaultWidget(m_filter);
    menu->addAction(filterAction);
    menu->addSection(QStringLiteral("排序"));
    auto *sortGroup=new QActionGroup(menu);
    m_sortNewest=menu->addAction(QStringLiteral("添加时间：新到旧")); m_sortNewest->setObjectName("wishlistSortNewestAction");
    m_sortOldest=menu->addAction(QStringLiteral("添加时间：旧到新")); m_sortOldest->setObjectName("wishlistSortOldestAction");
    m_sortManual=menu->addAction(QStringLiteral("手动顺序")); m_sortManual->setObjectName("wishlistSortManualAction");
    for (auto *action:{m_sortNewest,m_sortOldest,m_sortManual}) { action->setCheckable(true); sortGroup->addAction(action); }
    m_sortNewest->setChecked(m_sortMode=="newest"); m_sortOldest->setChecked(m_sortMode=="oldest"); m_sortManual->setChecked(m_sortMode=="manual");
    connect(m_sortNewest,&QAction::triggered,this,[this] { setSortMode("newest"); });
    connect(m_sortOldest,&QAction::triggered,this,[this] { setSortMode("oldest"); });
    connect(m_sortManual,&QAction::triggered,this,[this] { if (m_positions.isEmpty()) adjustOrder(); else setSortMode("manual"); });
    m_adjustOrder=menu->addAction(QStringLiteral("调整顺序…")); m_adjustOrder->setObjectName("wishlistAdjustOrderAction"); connect(m_adjustOrder,&QAction::triggered,this,&WishlistWidget::adjustOrder);
    menu->addSeparator();
    auto *refreshAction = menu->addAction(QStringLiteral("刷新"));
    connect(refreshAction, &QAction::triggered, m_refresh, &QPushButton::click);
    m_refresh->hide();
    more->setMenu(menu);
    summary->addWidget(more);
    layout->addLayout(summary);
    connect(m_filter, &QComboBox::activated, menu, &QMenu::close);
    m_purchase = new QPushButton(QStringLiteral("决定是否购买"), this);
    m_purchase->setObjectName(QStringLiteral("purchaseWishButton"));
    m_abandon = new QPushButton(QStringLiteral("这次不买"), this);
    m_abandon->setObjectName(QStringLiteral("abandonWishButton"));
    m_abandon->setProperty("link", true);
    auto *details = new QWidget(this);
    details->setObjectName(QStringLiteral("wishProductDetails"));
    auto *detailLayout = new QVBoxLayout(details);
    detailLayout->setContentsMargins(25, 0, 25, 24);
    detailLayout->setSpacing(18);
    m_detailsText = ShellWidgets::label(QString(), details, "muted");
    m_detailsText->setObjectName(QStringLiteral("wishProductDetailsLabel"));
    auto *info = new QHBoxLayout;
    info->addWidget(m_detailsText, 1);
    m_productMore = new QToolButton(details);
    m_productMore->setObjectName(QStringLiteral("wishProductMoreButton"));
    m_productMore->setText(QStringLiteral("···"));
    m_productMore->setAccessibleName(QStringLiteral("待购商品选项"));
    m_productMore->setPopupMode(QToolButton::InstantPopup);
    auto *productMenu = new QMenu(m_productMore);
    productMenu->setObjectName(QStringLiteral("wishProductMenu"));
    m_edit = productMenu->addAction(QStringLiteral("修改商品"));
    m_edit->setObjectName(QStringLiteral("editWishAction"));
    m_pin = productMenu->addAction(QStringLiteral("设为许愿目标"));
    m_pin->setObjectName(QStringLiteral("setWishPoolGoalAction"));
    m_abandonAction = productMenu->addAction(QStringLiteral("放弃购买"));
    m_abandonAction->setObjectName(QStringLiteral("abandonWishAction"));
    productMenu->addSeparator();
    m_delete = productMenu->addAction(QStringLiteral("删除商品"));
    m_delete->setObjectName(QStringLiteral("deleteWishAction"));
    m_productMore->setMenu(productMenu);
    info->addStretch(); info->addWidget(m_productMore, 0, Qt::AlignTop);
    detailLayout->addLayout(info);
    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    actions->addWidget(m_purchase);
    m_abandon->hide();
    m_priceButton=new QPushButton(QStringLiteral("价格记录 →"),details); m_priceButton->setObjectName("wishPriceButton"); m_priceButton->setProperty("link",true); actions->addWidget(m_priceButton);
    connect(m_priceButton,&QPushButton::clicked,this,&WishlistWidget::openPrices);
    actions->addStretch();
    m_purchase->setProperty("primary", true);
    detailLayout->addLayout(actions);
    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("wishlistErrorLabel"));
    m_error->setTextFormat(Qt::PlainText);
    m_error->setWordWrap(true);
    m_error->setProperty("error", true);
    m_error->hide();
    layout->addWidget(m_error);
    m_table = new ProductList(7, this);
    m_table->setObjectName(QStringLiteral("wishlistTable"));
    auto *productInfo=new QAction(QStringLiteral("商品资料"),productMenu); productMenu->insertAction(m_edit,productInfo); productInfo->setObjectName("wishProductInfoAction");
    connect(productInfo,&QAction::triggered,this,[this] { if (const auto *item=selectedItem()) ProductDetailsDialog::openFor(m_database,"wish",item->id,this); });
    m_table->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("预计价格"), QStringLiteral("分类"),
        QStringLiteral("状态与剩余时间"), QStringLiteral("冷静期截止"), QStringLiteral("购买理由"), QStringLiteral("优先级")});
    m_table->setDetailWidget(details);
    layout->addWidget(m_table);
    m_empty = new QLabel(QStringLiteral("还没有待购商品，点击右上角添加。"), this);
    m_empty->setObjectName(QStringLiteral("wishlistEmptyLabel"));
    m_empty->setWordWrap(true);
    m_empty->setProperty("planningEmpty", true);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setMinimumHeight(90);
    layout->addWidget(m_empty);
    layout->addStretch();
    connect(m_add, &QPushButton::clicked, this, [this] { openWishDialog(); });
    connect(m_pin,&QAction::triggered,this,[this] { if (const auto *item=selectedItem()) emit goalRequested(item->id); });
    connect(m_refresh, &QPushButton::clicked, this, [this] { refresh(); });
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { refreshCountdowns(); });
    connect(m_edit, &QAction::triggered, this, [this] { openWishDialog(selectedItem()); });
    connect(m_purchase, &QPushButton::clicked, this, &WishlistWidget::purchaseSelected);
    connect(m_abandon, &QPushButton::clicked, this, [this] { confirmSelected(false); });
    connect(m_abandonAction, &QAction::triggered, this, [this] { confirmSelected(false); });
    connect(m_delete, &QAction::triggered, this, [this] { confirmSelected(true); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &WishlistWidget::updateActions);
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] { refreshCountdowns(); });
    timer->start();
    updateActions();
}

QPushButton *WishlistWidget::takeAddButton()
{
    m_addBar->hide();
    return m_add;
}

void WishlistWidget::setDatabaseAvailable(bool available)
{
    m_available = available;
    if (available)
        refresh();
    else {
        showError(QStringLiteral("数据库不可用，请检查错误后重新启动程序。"));
        m_empty->setText(QStringLiteral("愿望暂时无法加载。"));
    }
    updateActions();
}

bool WishlistWidget::refresh()
{
    if (!m_available)
        return false;
    QList<WishItem> loaded; QList<PriceTrack> prices; PriceRepository priceRepository(m_database);
    QHash<qint64,qint64> links,positions; QSet<qint64> legacyArchives;
    if (!m_catalog.synchronize() || !m_catalog.state(links,positions,&legacyArchives)) { showError(m_catalog.lastError()); return false; }
    if (!priceRepository.load(prices,true)) { showError(priceRepository.lastError()); return false; }
    if (!m_repository.findAll(loaded)) {
        showError(QStringLiteral("加载失败%1：%2")
            .arg(m_hasSnapshot ? QStringLiteral("，仍显示上次数据") : QString(), m_repository.lastError()));
        return false;
    }
    m_items = loaded; m_positions=positions; m_legacyArchives=legacyArchives; m_prices.clear();
    for (const auto &price:prices) for (auto it=links.cbegin();it!=links.cend();++it) if (it.value()==price.id) m_prices.insert(it.key(),price);
    m_hasSnapshot = true;
    m_error->hide();
    refreshCountdowns();
    return true;
}

void WishlistWidget::refreshCountdowns(const QDateTime &now)
{
    if (!m_hasSnapshot)
        return;
    m_now = now;
    render(now);
}

void WishlistWidget::render(const QDateTime &now)
{
    const qint64 selected = selectedId();
    int waiting = 0, ready = 0, purchased = 0, abandoned = 0;
    qint64 abandonedCents = 0;
    QList<const WishItem *> visible;
    for (const auto &item : m_items) {
        const int state = item.status == WishStatus::Purchased ? 3
            : item.status == WishStatus::Abandoned ? 4 : item.canPurchase(now) ? 2 : 1;
        if (state == 1) ++waiting;
        if (state == 2) ++ready;
        if (state == 3) ++purchased;
        if (state == 4) { ++abandoned; abandonedCents += item.estimatedCents; }
        if (m_filter->currentIndex() == 5 ? state >= 3
            : m_filter->currentIndex() != 0 && m_filter->currentIndex() != state)
            continue;
        visible.append(&item);
    }
    sortItems(visible);
    bool rebuild = m_table->rowCount() != visible.size();
    for (int row = 0; !rebuild && row < visible.size(); ++row)
        rebuild = m_table->item(row, 0)->data(Qt::UserRole).toLongLong() != visible[row]->id;
    if (rebuild) {
        m_table->resetRows(visible.size());
    }
    int selectedRow = -1;
    for (int row = 0; row < visible.size(); ++row) {
        const auto &item = *visible[row];
        const QStringList values = {item.name, Transaction::formatAmount(item.estimatedCents), CategoryCatalog::label(item.detailCategory.isEmpty()?CategoryCatalog::fromLegacy(item.category):item.detailCategory),
            item.statusText(now), item.readyAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")), item.reason, item.priorityText()};
        for (int column = 0; column < values.size(); ++column) {
            auto *cell = m_table->item(row, column);
            if (!cell) {
                cell = new QTableWidgetItem;
                m_table->setItem(row, column, cell);
            }
            if (cell->text() != values[column]) {
                cell->setText(values[column]);
                cell->setToolTip(values[column]);
            }
            if (column == 0) {
                cell->setData(Qt::UserRole, item.id);
                cell->setToolTip(QStringLiteral("%1\n创建：%2\n冷静期：%3 天%4")
                    .arg(item.name, item.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")))
                    .arg(item.coolingDays).arg(item.transactionId > 0
                        ? QStringLiteral("\n已关联消费编号：%1").arg(item.transactionId) : QString()));
            }
        }
        QString status = m_legacyArchives.contains(item.id)?QStringLiteral("已归档"):item.canPurchase(now)?QStringLiteral("待决定"):item.statusText(now);
        if (item.status == WishStatus::Waiting && !item.canPurchase(now)) {
            const qint64 minutes = (qMax(qint64(0), now.msecsTo(item.readyAt)) + 59999) / 60000;
            const qint64 hours = (minutes + 59) / 60;
            status = hours >= 24
                ? QStringLiteral("冷静期 · 还剩 %1 天 %2 小时").arg(hours / 24).arg(hours % 24)
                : minutes >= 60 ? QStringLiteral("冷静期 · 还剩 %1 小时").arg(hours)
                : QStringLiteral("冷静期 · 不足 1 小时");
        }
        const auto price=m_prices.value(item.id);
        m_table->setSummary(row,item.name,status,Transaction::formatAmount(price.id?price.latestCents:item.estimatedCents),price.id?QStringLiteral("最近价格"):QStringLiteral("预计价格"));
        m_table->setCoolingState(row,item.status==WishStatus::Waiting && !item.canPurchase(now),item.status!=WishStatus::Waiting);
        if (item.id == selected) selectedRow = row;
    }
    if (rebuild && selectedRow >= 0) m_table->selectRow(selectedRow);
    m_summary->setText(m_filter->currentIndex() == 5
        ? QStringLiteral("%1 件待购").arg(waiting + ready)
        : QStringLiteral("%1 件 · %2").arg(visible.size()).arg(m_filter->currentText()));
    m_summary->setToolTip(QStringLiteral("冷静期中 %1 · 可购买 %2 · 已购买 %3 · 已放弃 %4（预计金额 %5）")
        .arg(waiting).arg(ready).arg(purchased).arg(abandoned).arg(Transaction::formatAmount(abandonedCents)));
    m_empty->setText(m_items.isEmpty() ? QStringLiteral("还没有待购商品，点击右上角添加。")
        : QStringLiteral("当前没有商品，可以在“···”中查看其他状态。"));
    m_empty->setVisible(m_table->rowCount() == 0);
    m_table->setVisible(m_table->rowCount() > 0);
    updateActions();
    m_table->updateExpansion();
}

qint64 WishlistWidget::selectedId() const
{
    const int row = m_table->currentRow();
    const auto *cell = row >= 0 ? m_table->item(row, 0) : nullptr;
    return cell && cell->isSelected() ? cell->data(Qt::UserRole).toLongLong() : 0;
}

const WishItem *WishlistWidget::selectedItem() const
{
    const qint64 id = selectedId();
    for (const auto &item : m_items)
        if (item.id == id) return &item;
    return nullptr;
}

void WishlistWidget::updateActions()
{
    const auto *item = selectedItem();
    const bool open = m_available && item && item->status == WishStatus::Waiting;
    m_add->setEnabled(m_available);
    m_adjustOrder->setEnabled(m_available && std::count_if(m_items.cbegin(),m_items.cend(),[](const WishItem &w) { return w.status==WishStatus::Waiting; })>1);
    m_refresh->setEnabled(m_available);
    m_filter->setEnabled(m_available && m_hasSnapshot);
    m_edit->setEnabled(open);
    m_pin->setEnabled(open);
    m_delete->setEnabled(m_available && item);
    m_productMore->setEnabled(m_available && item);
    m_abandon->setEnabled(open);
    const bool ready = open && item->canPurchase(m_now);
    m_purchase->setEnabled(ready);
    m_purchase->setVisible(open);
    m_purchase->setText(ready?QStringLiteral("决定是否购买"):QStringLiteral("冷静期内，暂不可购买"));
    m_abandon->hide();
    m_priceButton->setEnabled(m_available && item);
    m_edit->setVisible(item && item->status == WishStatus::Waiting);
    m_pin->setVisible(item && item->status == WishStatus::Waiting);
    m_abandonAction->setEnabled(open);
    m_abandonAction->setVisible(item && item->status == WishStatus::Waiting);
    if (item) {
        m_detailsText->setText(item->reason.isEmpty()?QString():QStringLiteral("购买理由：")+item->reason);
        m_detailsText->setVisible(!item->reason.isEmpty());
        m_detailsText->setToolTip(QStringLiteral("%1 · 优先级：%2\n冷静期截止：%3")
            .arg(CategoryCatalog::label(item->detailCategory.isEmpty() ? CategoryCatalog::fromLegacy(item->category) : item->detailCategory),
                 item->priorityText(), item->readyAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    } else m_detailsText->clear();
}

void WishlistWidget::openWishDialog(const WishItem *existing)
{
    auto *dialog = existing ? new WishDialog(*existing, this) : new WishDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    const bool editing = existing != nullptr;
    connect(dialog, &WishDialog::saveRequested, this, [this, dialog, editing] {
        auto item = dialog->item();
        const bool saved = editing ? m_repository.update(item) : m_repository.add(item);
        if (!saved) {
            dialog->showSaveError(m_repository.lastError());
            return;
        }
        dialog->completeSave();
        if (!refresh())
            showError(QStringLiteral("愿望已保存，但刷新失败：%1。请点击刷新，避免重复新增。").arg(m_repository.lastError()));
    });
    dialog->open();
}

void WishlistWidget::purchaseSelected()
{
    const auto *item = selectedItem();
    if (!item || !item->canPurchase(QDateTime::currentDateTimeUtc()))
        return;
    const qint64 id = item->id;
    const auto price=m_prices.value(id);
    const Transaction draft(0, item->name, price.id?price.latestCents:item->estimatedCents, item->category, QDate::currentDate(),{},item->reason,item->detailCategory,item->sourceImage);
    auto *dialog = new AddTransactionDialog(draft, this);
    ShoppingProfileRepository profile(m_database); QList<BrandEntry> brands;
    if (!profile.loadBrands(brands)) { dialog->deleteLater(); showError(profile.lastError()); return; }
    dialog->setBrands(brands);
    dialog->setObjectName(QStringLiteral("wishPurchaseDialog"));
    dialog->setWindowTitle(QStringLiteral("购买并记账（请确认实际金额）"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &AddTransactionDialog::saveRequested, this, [this, dialog, id] {
        auto expense = dialog->transaction();
        if (!m_repository.purchase(id, expense,QDateTime::currentDateTimeUtc(),dialog->selectedBrandId())) {
            dialog->showSaveError(m_repository.lastError());
            return;
        }
        dialog->completeSave();
        if (!refresh())
            showError(QStringLiteral("购买和记账已完成，但愿望刷新失败：%1。请点击刷新。").arg(m_repository.lastError()));
        emit expenseRecorded(expense.id());
    });
    dialog->open();
}

void WishlistWidget::confirmSelected(bool removing)
{
    const auto *item = selectedItem();
    if (!item) return;
    const qint64 id = item->id;
    const QString action = removing ? QStringLiteral("删除愿望") : QStringLiteral("放弃购买");
    const QString explanation = removing
        ? QStringLiteral("仅删除清单条目，已记录的消费仍保留。")
        : QStringLiteral("会保留放弃记录，不会计入消费或预算。");
    auto *confirmation = new QMessageBox(QMessageBox::Question, action,
        QStringLiteral("确定对“%1”%2吗？\n%3").arg(item->name, action, explanation),
        QMessageBox::Yes | QMessageBox::No, this);
    confirmation->setObjectName(QStringLiteral("wishConfirmationDialog"));
    confirmation->setTextFormat(Qt::PlainText);
    confirmation->setAttribute(Qt::WA_DeleteOnClose);
    confirmation->button(QMessageBox::Yes)->setText(action);
    confirmation->button(QMessageBox::No)->setText(QStringLiteral("取消"));
    confirmation->setDefaultButton(QMessageBox::No);
    confirmation->setEscapeButton(QMessageBox::No);
    connect(confirmation, &QMessageBox::finished, this, [this, confirmation, id, removing](int) {
        if (confirmation->standardButton(confirmation->clickedButton()) != QMessageBox::Yes)
            return;
        if (!(removing ? m_repository.remove(id) : m_repository.abandon(id))) {
            showError(m_repository.lastError());
            return;
        }
        if (!refresh())
            showError(QStringLiteral("操作已完成，但刷新失败：%1。请点击刷新。").arg(m_repository.lastError()));
    });
    confirmation->open();
}

void WishlistWidget::showError(const QString &error)
{
    m_error->setText(error);
    m_error->show();
}

void WishlistWidget::sortItems(QList<const WishItem *> &items) const
{
    std::stable_sort(items.begin(),items.end(),[this](const WishItem *a,const WishItem *b) {
        if (m_sortMode=="manual") {
            const auto ap=m_positions.value(a->id,std::numeric_limits<qint64>::max()),bp=m_positions.value(b->id,std::numeric_limits<qint64>::max());
            if (ap!=bp) return ap<bp;
        }
        if (a->createdAt!=b->createdAt) return m_sortMode=="oldest"?a->createdAt<b->createdAt:a->createdAt>b->createdAt;
        return m_sortMode=="oldest"?a->id<b->id:a->id>b->id;
    });
}

void WishlistWidget::setSortMode(const QString &mode)
{
    m_sortMode=mode; m_sortNewest->setChecked(mode=="newest"); m_sortOldest->setChecked(mode=="oldest"); m_sortManual->setChecked(mode=="manual");
    if (m_database.databasePath()!=":memory:") { QSettings settings(m_database.databasePath()+".ui.ini",QSettings::IniFormat); settings.setValue("wishlist/sort_mode",mode); }
    refreshCountdowns();
}

void WishlistWidget::adjustOrder()
{
    QList<const WishItem *> pending;
    for (const auto &item:m_items) if (item.status==WishStatus::Waiting) pending.append(&item);
    sortItems(pending); if (pending.size()<2) return;
    auto *dialog=new QDialog(this); dialog->setObjectName("wishlistOrderDialog"); dialog->setWindowTitle(QStringLiteral("调整商品顺序")); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(520,520);
    auto *layout=new QVBoxLayout(dialog); layout->addWidget(ShellWidgets::label(QStringLiteral("拖动商品调整顺序，也可以选中后上下移动。"),dialog,"muted"));
    auto *list=new QListWidget(dialog); list->setObjectName("wishlistOrderList"); list->setDragDropMode(QAbstractItemView::InternalMove); list->setDefaultDropAction(Qt::MoveAction); list->setDragDropOverwriteMode(false); list->setMinimumHeight(230);
    for (const auto *wish:pending) { auto *row=new QListWidgetItem(wish->name,list); row->setData(Qt::UserRole,wish->id); row->setSizeHint(QSize(0,44)); }
    list->setCurrentRow(0); layout->addWidget(list,1); auto *moves=new QHBoxLayout;
    auto *up=new QPushButton(QStringLiteral("上移"),dialog); up->setObjectName("wishlistMoveUpButton"); auto *down=new QPushButton(QStringLiteral("下移"),dialog); down->setObjectName("wishlistMoveDownButton"); moves->addWidget(up); moves->addWidget(down); moves->addStretch(); layout->addLayout(moves);
    const auto move=[list](int delta) { const int row=list->currentRow(),next=row+delta; if (row<0 || next<0 || next>=list->count()) return; auto *item=list->takeItem(row); list->insertItem(next,item); list->setCurrentRow(next); };
    const auto update=[list,up,down] { const int row=list->currentRow(); up->setEnabled(row>0); down->setEnabled(row>=0 && row<list->count()-1); };
    connect(up,&QPushButton::clicked,dialog,[move,update] { move(-1); update(); }); connect(down,&QPushButton::clicked,dialog,[move,update] { move(1); update(); }); connect(list,&QListWidget::currentRowChanged,dialog,update); update();
    auto *error=ShellWidgets::label(QString(),dialog,"error"); error->setObjectName("wishlistOrderErrorLabel"); layout->addWidget(error);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,dialog); buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存顺序")); buttons->button(QDialogButtonBox::Save)->setObjectName("saveWishlistOrderButton"); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,list,error] {
        QList<qint64> ids; for (int row=0;row<list->count();++row) ids.append(list->item(row)->data(Qt::UserRole).toLongLong());
        if (!m_catalog.saveOrder(ids)) { error->setText(m_catalog.lastError()); return; }
        if (!refresh()) { error->setText(QStringLiteral("顺序已保存，清单读取失败，请返回后刷新。")); return; }
        setSortMode("manual"); dialog->accept();
    });
    // Opening the editor does not change the selected sort mode until Save.
    m_sortNewest->setChecked(m_sortMode=="newest"); m_sortOldest->setChecked(m_sortMode=="oldest"); m_sortManual->setChecked(m_sortMode=="manual"); dialog->open();
}

void WishlistWidget::openPrices()
{
    const auto *item=selectedItem(); if (!m_available || !item) return;
    auto *dialog=new WishPriceDialog(m_database,item->id,this); dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog,&WishPriceDialog::pricesChanged,this,[this] { refresh(); emit pricesChanged(); });
    connect(dialog,&WishPriceDialog::settingsRequested,this,&WishlistWidget::settingsRequested); dialog->open();
}

void WishlistWidget::showPrices(qint64 trackId)
{
    if (!refresh()) return;
    qint64 wishId=0;
    for (const auto &wish:m_items) {
        const auto track=m_prices.value(wish.id);
        if (track.id && ((trackId && track.id==trackId) || (!trackId && (track.targetHit || track.priceDrop)))) { wishId=wish.id; break; }
    }
    if (!wishId) return;
    m_filter->setCurrentIndex(0);
    for (int row=0;row<m_table->rowCount();++row) if (m_table->item(row,0)->data(Qt::UserRole).toLongLong()==wishId) { m_table->selectRow(row); openPrices(); break; }
}
