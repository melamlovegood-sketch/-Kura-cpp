#pragma once
#include "AIService.h"
#include "WishItem.h"
#include <QWidget>
class DatabaseManager;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class OnboardingPages;
class KuraMascot;
class OnboardingTour;
class PricePerspectiveWidget;

class OnboardingDialog final : public QWidget {
    Q_OBJECT
public:
    explicit OnboardingDialog(DatabaseManager &database,const QString &settingsPath,QWidget *parent,bool resume=false);
    void beginTour();
    int step() const { return m_step; }
signals:
    void pageRequested(int page);
    void preferencesSaved();
    void importRequested();
    void tourStarted();
    void finished(bool completed);
protected:
    bool eventFilter(QObject *,QEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool focusNextPrevChild(bool next) override;
private:
    void buildForms();
    void showStep(int step);
    void next();
    bool saveStep();
    bool saveWish();
    void parseWishText();
    AISettings aiValues(bool effective=false) const;
    void testAI();
    void cancelTest();
    void finish(bool completed);
    void rememberStep();
    DatabaseManager &m_database;
    QString m_settingsPath;
    QDate m_month=QDate::currentDate();
    int m_step=0;
    QWidget *m_form=nullptr,*m_content=nullptr;
    QScrollArea *m_scroll=nullptr;
    OnboardingPages *m_forms=nullptr;
    QLabel *m_progress=nullptr,*m_title=nullptr,*m_subtitle=nullptr,*m_error=nullptr;
    KuraMascot *m_mascot=nullptr;
    QPushButton *m_previous=nullptr,*m_skip=nullptr,*m_next=nullptr;
    QComboBox *m_provider=nullptr;
    QLineEdit *m_url=nullptr,*m_model=nullptr,*m_key=nullptr,*m_wishName=nullptr;
    QPushButton *m_trial=nullptr,*m_test=nullptr;
    QDoubleSpinBox *m_basic=nullptr,*m_discretionary=nullptr,*m_price=nullptr;
    QButtonGroup *m_priority=nullptr;
    QCheckBox *m_goal=nullptr;
    QPlainTextEdit *m_rules=nullptr;
    PricePerspectiveWidget *m_perspective=nullptr;
    OnboardingTour *m_tour=nullptr;
    WishItem m_savedWish;
    QString m_savedWishKey;
    AIService m_connection;
    bool m_testing=false,m_finishing=false;
};
