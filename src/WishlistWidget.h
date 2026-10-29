#pragma once
#include "WishlistRepository.h"
#include "UnifiedWishlistRepository.h"
#include "PriceRepository.h"
#include <QWidget>
class ProductList;
class QComboBox;
class QPushButton;
class QLabel;
class QAction;
class QToolButton;
class BatchSelectionBar;

class WishlistWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WishlistWidget(DatabaseManager &database, QWidget *parent = nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
    QPushButton *takeAddButton();
    void showPrices(qint64 trackId=0);
    void showReadyWishes(qint64 wishId = 0);
    // Refresh countdowns from the cached snapshot; no disk writes or network work.
    void refreshCountdowns(const QDateTime &now = QDateTime::currentDateTimeUtc());
signals:
    void expenseRecorded(qint64 transactionId);
    void goalRequested(qint64 wishId);
    void pricesChanged();
    void settingsRequested();
private:
    qint64 selectedId() const;
    const WishItem *selectedItem() const;
    void render(const QDateTime &now);
    void updateActions();
    void openWishDialog(const WishItem *item = nullptr);
    void purchaseSelected();
    void confirmWish(qint64 id);
    void confirmWishes(const QList<qint64> &ids);
    void openProductDetails(qint64 id);
    void showError(const QString &error);
    void openPrices();
    void adjustOrder();
    void setSortMode(const QString &mode);
    void sortItems(QList<const WishItem *> &items) const;
    DatabaseManager &m_database;
    WishlistRepository m_repository;
    UnifiedWishlistRepository m_catalog;
    QHash<qint64,PriceTrack> m_prices;
    QSet<qint64> m_legacyArchives;
    QHash<qint64,qint64> m_positions;
    QString m_sortMode=QStringLiteral("priority");
    QAction *m_sortPriority,*m_sortNewest,*m_sortOldest,*m_sortManual,*m_adjustOrder;
    QPushButton *m_priceButton;
    QList<WishItem> m_items;
    bool m_available = false;
    bool m_hasSnapshot = false;
    QDateTime m_now;
    qint64 m_goalWishId = 0;
    ProductList *m_table;
    BatchSelectionBar *m_selection;
    QPushButton *m_deleteSelected;
    QComboBox *m_filter;
    QLabel *m_summary;
    QLabel *m_error;
    QLabel *m_empty;
    QLabel *m_detailsText;
    QWidget *m_addBar;
    QPushButton *m_add;
    QPushButton *m_refresh;
    QPushButton *m_purchase;
};
