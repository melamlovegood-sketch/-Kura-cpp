#pragma once
#include "WishlistRepository.h"
#include <QWidget>
class QTableWidget;
class QComboBox;
class QPushButton;
class QLabel;

class WishlistWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WishlistWidget(DatabaseManager &database, QWidget *parent = nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
    // Refresh countdowns from the cached snapshot; no disk writes or network work.
    void refreshCountdowns(const QDateTime &now = QDateTime::currentDateTimeUtc());
signals:
    void expenseRecorded(qint64 transactionId);
private:
    qint64 selectedId() const;
    const WishItem *selectedItem() const;
    void render(const QDateTime &now);
    void updateActions();
    void openWishDialog(const WishItem *item = nullptr);
    void purchaseSelected();
    void confirmSelected(bool removing);
    void showError(const QString &error);
    WishlistRepository m_repository;
    QList<WishItem> m_items;
    bool m_available = false;
    bool m_hasSnapshot = false;
    QDateTime m_now;
    QTableWidget *m_table;
    QComboBox *m_filter;
    QLabel *m_summary;
    QLabel *m_error;
    QLabel *m_empty;
    QPushButton *m_add;
    QPushButton *m_refresh;
    QPushButton *m_edit;
    QPushButton *m_delete;
    QPushButton *m_purchase;
    QPushButton *m_abandon;
};
