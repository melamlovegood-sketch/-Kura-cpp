#pragma once
#include "AchievementRepository.h"
#include <QDialog>
#include <QPointer>
#include <QSet>
#include <QTimer>

class QFrame;

// One card for all achievements from the same saved action. Never auto-dismisses.
class AchievementCelebration final : public QDialog {
    Q_OBJECT
public:
    explicit AchievementCelebration(const QList<Achievement> &badges, QWidget *parent);
signals:
    void collectionRequested();
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void showEvent(QShowEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
private:
    void fit();
    QFrame *m_card;
};

// Observes committed local changes, including saves made in nested editors.
// The first successful snapshot is a silent baseline for existing accounts.
class AchievementNotifier final : public QObject {
    Q_OBJECT
public:
    explicit AchievementNotifier(DatabaseManager &database, QWidget *owner);
    bool resetBaseline();
    void checkNow();
signals:
    void collectionRequested();
private:
    bool readRevision(qint64 &changes, qint64 &version);
    void presentPending();
    DatabaseManager &m_database;
    QWidget *m_owner;
    QTimer m_timer;
    QSet<QString> m_known;
    QList<Achievement> m_pending;
    QPointer<AchievementCelebration> m_dialog;
    qint64 m_changes=-1, m_version=-1;
    QDate m_day;
    bool m_initialized=false;
};
