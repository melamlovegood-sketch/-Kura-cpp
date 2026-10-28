#pragma once
#include "AchievementRepository.h"
#include <QFrame>
class QLabel;
class QGridLayout;
class AchievementBadge;

class AchievementWall:public QFrame {
    Q_OBJECT
public:
    explicit AchievementWall(DatabaseManager &database,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    void reflow();
    void select(int index);
    AchievementRepository m_repository;
    QList<Achievement> m_badges;
    QList<AchievementBadge *> m_buttons;
    QLabel *m_count,*m_detailTitle,*m_condition,*m_status,*m_error;
    QGridLayout *m_grid;
    bool m_available=false;
    int m_columns=0,m_selected=0;
};
