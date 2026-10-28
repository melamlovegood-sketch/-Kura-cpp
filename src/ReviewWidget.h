#pragma once
#include "ReviewRepository.h"
#include <QWidget>
class QLabel;
class QTableWidget;
class QPushButton;
class QComboBox;
class ReviewWidget:public QWidget {
    Q_OBJECT
public:
    explicit ReviewWidget(DatabaseManager &database,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
signals:
    void reviewsChanged();
private:
    void openReview();
    void updateAction();
    DatabaseManager &m_database;
    ReviewRepository m_repository;
    bool m_available=false;
    QList<ReviewTask> m_tasks;
    QTableWidget *m_table;
    QComboBox *m_mode;
    QPushButton *m_review;
    QLabel *m_summary,*m_error;
};
