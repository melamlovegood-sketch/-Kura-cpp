#pragma once
#include "WishPoolRepository.h"
#include <QFrame>
class QLabel;
class QProgressBar;
class QPropertyAnimation;
class QPushButton;
class WishPoolWidget : public QFrame {
    Q_OBJECT
public:
    explicit WishPoolWidget(DatabaseManager &database,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
signals:
    void wishlistRequested();
private:
    void showHistory();
    DatabaseManager &m_database;
    WishPoolRepository m_repository;
    qint64 m_wishId=0;
    bool m_available=false, m_hasSnapshot=false;
    QLabel *m_name,*m_amount,*m_hint,*m_error;
    QProgressBar *m_progress;
    QPropertyAnimation *m_animation;
    QPushButton *m_goal, *m_history, *m_changeGoal;
};
