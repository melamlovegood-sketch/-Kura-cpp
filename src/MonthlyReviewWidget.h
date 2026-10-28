#pragma once
#include "MonthlyReviewRepository.h"
#include "AIService.h"
#include <QWidget>
class QDateEdit;
class QLabel;
class QPushButton;
class QAction;
class QToolButton;
class MonthlyReviewWidget:public QWidget {
    Q_OBJECT
public:
    explicit MonthlyReviewWidget(DatabaseManager &database,const QString &settingsPath,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
    void cancelAnalysis();
signals:
    void tagsChanged();
    void purchaseReviewRequested();
    void statisticsRequested(const QDate &month);
private:
    void render();
    void updateControls();
    void generate(bool ai);
    void manageTags();
    void renderBoards();
    void showPreviousStory();
    void renderStoryBody();
    DatabaseManager &m_database;
    MonthlyReviewRepository m_repository;
    AIService m_ai;
    QString m_settingsPath,m_requestedHash;
    QDate m_requestedMonth;
    MonthlyReviewData m_data;
    MonthlyStory m_story;
    QString m_fullStory,m_storyInterlude,m_storyNextStep;
    bool m_available=false,m_hasSnapshot=false,m_busy=false;
    QDateEdit *m_month;
    QLabel *m_title,*m_spent,*m_resists,*m_saved,*m_source,*m_storyTitle,*m_storyText,*m_error,*m_personaBadge,*m_interlude,*m_nextTask;
    QWidget *m_worth,*m_regret;
    QAction *m_local,*m_regenerate;
    QToolButton *m_more;
    QPushButton *m_prev,*m_next,*m_generate,*m_cancel,*m_tags,*m_worthTab,*m_regretTab,*m_previousStory;
    QPushButton *m_expandStory,*m_dueReviews;
};
