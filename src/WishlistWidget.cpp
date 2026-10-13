#include "WishlistWidget.h"
#include "WishDialog.h"
#include "AddTransactionDialog.h"
#include <QAbstractButton>
#include <QComboBox>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

WishlistWidget::WishlistWidget(DatabaseManager &database, QWidget *parent)
    : QWidget(parent), m_repository(database), m_now(QDateTime::currentDateTimeUtc())
{
    setObjectName(QStringLiteral("wishlistWidget"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto *heading = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("愿望清单与冷静期"), this);
    auto font = title->font();
    font.setPointSize(18);
    font.setBold(true);
    title->setFont(font);
    heading->addWidget(title);
    heading->addStretch();
    m_add = new QPushButton(QStringLiteral("加入愿望"), this);
    m_add->setObjectName(QStringLiteral("addWishButton"));
    m_refresh = new QPushButton(QStringLiteral("刷新"), this);
    m_refresh->setObjectName(QStringLiteral("refreshWishlistButton"));
    heading->addWidget(m_add);
    heading->addWidget(m_refresh);
    layout->addLayout(heading);
    auto *hint = new QLabel(QStringLiteral("先放进清单，等一等再决定。购买时填写实际金额并记账；放弃不会计入支出。"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_summary = new QLabel(QStringLiteral("愿望数据：—"), this);
    m_summary->setObjectName(QStringLiteral("wishlistSummaryLabel"));
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);
    auto *actions = new QHBoxLayout;
    actions->addWidget(new QLabel(QStringLiteral("状态"), this));
    m_filter = new QComboBox(this);
    m_filter->setObjectName(QStringLiteral("wishlistStatusFilter"));
    m_filter->addItems({QStringLiteral("全部"), QStringLiteral("冷静期中"), QStringLiteral("可购买"),
                       QStringLiteral("已购买"), QStringLiteral("已放弃")});
    actions->addWidget(m_filter);
    actions->addStretch();
    m_edit = new QPushButton(QStringLiteral("修改"), this);
    m_edit->setObjectName(QStringLiteral("editWishButton"));
    m_delete = new QPushButton(QStringLiteral("删除"), this);
    m_delete->setObjectName(QStringLiteral("deleteWishButton"));
    m_purchase = new QPushButton(QStringLiteral("购买并记账"), this);
    m_purchase->setObjectName(QStringLiteral("purchaseWishButton"));
    m_abandon = new QPushButton(QStringLiteral("放弃购买"), this);
    m_abandon->setObjectName(QStringLiteral("abandonWishButton"));
    for (auto *button : {m_edit, m_delete, m_purchase, m_abandon})
        actions->addWidget(button);
    layout->addLayout(actions);
    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("wishlistErrorLabel"));
    m_error->setTextFormat(Qt::PlainText);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("color: #b42318;"));
    m_error->hide();
    layout->addWidget(m_error);
    m_table = new QTableWidget(0, 6, this);
    m_table->setObjectName(QStringLiteral("wishlistTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("预计价格"), QStringLiteral("分类"),
        QStringLiteral("状态与剩余时间"), QStringLiteral("冷静期截止"), QStringLiteral("购买理由")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);
    m_empty = new QLabel(QStringLiteral("暂无愿望，点击“加入愿望”记录想买的物品。"), this);
    m_empty->setObjectName(QStringLiteral("wishlistEmptyLabel"));
    m_empty->setWordWrap(true);
    layout->addWidget(m_empty);
    connect(m_add, &QPushButton::clicked, this, [this] { openWishDialog(); });
    connect(m_refresh, &QPushButton::clicked, this, [this] { refresh(); });
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { refreshCountdowns(); });
    connect(m_edit, &QPushButton::clicked, this, [this] { openWishDialog(selectedItem()); });
    connect(m_purchase, &QPushButton::clicked, this, &WishlistWidget::purchaseSelected);
    connect(m_abandon, &QPushButton::clicked, this, [this] { confirmSelected(false); });
    connect(m_delete, &QPushButton::clicked, this, [this] { confirmSelected(true); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &WishlistWidget::updateActions);
    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, [this] { refreshCountdowns(); });
    timer->start();
    updateActions();
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
    QList<WishItem> loaded;
    if (!m_repository.findAll(loaded)) {
        showError(QStringLiteral("加载失败%1：%2")
            .arg(m_hasSnapshot ? QStringLiteral("，仍显示上次数据") : QString(), m_repository.lastError()));
        return false;
    }
    m_items = loaded;
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
        if (m_filter->currentIndex() != 0 && m_filter->currentIndex() != state)
            continue;
        visible.append(&item);
    }
    bool rebuild = m_table->rowCount() != visible.size();
    for (int row = 0; !rebuild && row < visible.size(); ++row)
        rebuild = m_table->item(row, 0)->data(Qt::UserRole).toLongLong() != visible[row]->id;
    if (rebuild) {
        m_table->setRowCount(0);
        m_table->setRowCount(visible.size());
    }
    int selectedRow = -1;
    for (int row = 0; row < visible.size(); ++row) {
        const auto &item = *visible[row];
        const QStringList values = {item.name, Transaction::formatAmount(item.estimatedCents), item.category,
            item.statusText(now), item.readyAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")), item.reason};
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
        if (item.id == selected) selectedRow = row;
    }
    if (rebuild && selectedRow >= 0) m_table->selectRow(selectedRow);
    m_summary->setText(QStringLiteral("冷静期中 %1 · 可购买 %2 · 已购买 %3 · 已放弃 %4（预计金额 %5）")
        .arg(waiting).arg(ready).arg(purchased).arg(abandoned).arg(Transaction::formatAmount(abandonedCents)));
    m_empty->setText(m_items.isEmpty() ? QStringLiteral("暂无愿望，点击“加入愿望”记录想买的物品。")
        : QStringLiteral("没有符合当前状态的愿望，可以切换到“全部”。"));
    m_empty->setVisible(m_table->rowCount() == 0);
    updateActions();
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
    m_refresh->setEnabled(m_available);
    m_filter->setEnabled(m_available && m_hasSnapshot);
    m_edit->setEnabled(open);
    m_delete->setEnabled(m_available && item);
    m_abandon->setEnabled(open);
    m_purchase->setEnabled(open && item->canPurchase(m_now));
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
    const Transaction draft(0, item->name, item->estimatedCents, item->category, QDate::currentDate());
    auto *dialog = new AddTransactionDialog(draft, this);
    dialog->setObjectName(QStringLiteral("wishPurchaseDialog"));
    dialog->setWindowTitle(QStringLiteral("购买并记账（请确认实际金额）"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &AddTransactionDialog::saveRequested, this, [this, dialog, id] {
        auto expense = dialog->transaction();
        if (!m_repository.purchase(id, expense)) {
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
