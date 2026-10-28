#pragma once
#include "ExecutionRepository.h"
#include "ShoppingProfileRepository.h"
#include "AIService.h"
#include "Transaction.h"
#include <QWidget>
class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QSpinBox;
class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QLabel;
class QToolButton;
class QBoxLayout;
class QFrame;
class QResizeEvent;
class DecisionChatWidget;
class ExecutionWidget:public QWidget {
    Q_OBJECT
public:
    explicit ExecutionWidget(DatabaseManager &database,const QString &settingsPath,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    bool refreshReferences();
    bool reloadState();
    bool prepareDecision(const ParsedProduct &product,const QByteArray &image={},qint64 brandId=0,qint64 productId=0);
    qint64 activeSessionId() const { return m_session.id; }
signals:
    void inputRequested(const QString &initialText);
    void wishlistRequested();
    void purchaseRequested(const Transaction &draft,qint64 sessionId,qint64 brandId);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    bool buildBrief();
    bool saveNotes();
    void updateControls();
    void updateTimer();
    void start();
    void finish(const QString &decision);
    void ask(const QString &question,bool brief,bool regenerate=false);
    void invalidateAdvice();
    bool acceptProduct();
    void goToStep(int step);
    void showStep(int step);
    void updateSummary();
    void deferToWishlist();
    void editRules(bool adding);
    void switchMode(const QString &mode);
    void openProductDetails();
    qint64 m_draftProductId=0;
    QByteArray m_draftImage;
    QPushButton *m_productDetails,*m_activeProductDetails;
    DatabaseManager &m_database;
    ExecutionRepository m_repository;
    ShoppingProfileRepository m_profile;
    AIService m_advisor;
    QString m_settingsPath,m_context,m_completionText,m_budgetSummary;
    ExecutionSession m_session;
    bool m_available=false,m_loading=false,m_aiBusy=false,m_quickReady=false;
    int m_phase=0;
    QLineEdit *m_name,*m_reviewName;
    QLineEdit *m_purpose;
    QDoubleSpinBox *m_amount;
    QComboBox *m_category,*m_mode,*m_brands;
    QSpinBox *m_minutes;
    QPlainTextEdit *m_brief,*m_notes;
    DecisionChatWidget *m_chat;
    QList<QCheckBox *> m_checks;
    QWidget *m_research;
    QPushButton *m_start,*m_buy,*m_wish,*m_input,*m_skip,*m_undecided,*m_saveNotes;
    QLabel *m_timer,*m_error;
    QWidget *m_steps;
    QList<QWidget *> m_stepPages;
    QPushButton *m_next,*m_edit,*m_fastChoice,*m_researchChoice;
    QPushButton *m_back,*m_addRule,*m_editRules,*m_activeFast,*m_activeResearch,*m_fastDecide;
    QToolButton *m_notesToggle,*m_rulesToggle;
    QWidget *m_notesBody,*m_duration,*m_references,*m_activeModes,*m_fastPanel,*m_decisionActions;
    QLabel *m_productTitle,*m_productPrice,*m_productPriceCaption,*m_productPurpose,*m_modeProduct,*m_budgetTitle,*m_budgetKind,*m_remaining,*m_after,*m_remainingCaption,*m_checkCount,*m_rules,*m_decisionHint;
    QBoxLayout *m_productLayout,*m_choicesLayout;
    QFrame *m_budgetPanel,*m_rulesCard;
};
