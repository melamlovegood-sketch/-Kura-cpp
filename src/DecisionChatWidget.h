#pragma once
#include <QFrame>
#include <QJsonArray>
#include <QList>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QBoxLayout;
class QResizeEvent;
class QEvent;

class DecisionChatWidget : public QFrame {
    Q_OBJECT
public:
    explicit DecisionChatWidget(QWidget *parent=nullptr);
    void setAvailable(bool available);
    void beginRequest(const QString &question,bool brief,bool regenerate=false);
    void completeRequest(const QString &answer);
    void failRequest(const QString &message);
    void clearConversation(const QString &notice={});
    void restoreConversation(const QString &saved);
    QString serialize() const;
    QString plainText() const;
    QJsonArray requestHistory(bool regenerate=false) const;
    bool isBusy() const { return m_busy; }
    bool isExpanded() const;
    void setExpanded(bool expanded);
signals:
    void requestSubmitted(const QString &question,bool brief,bool regenerate);
    void cancelRequested();
protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    struct Message { QString role,text,question; bool brief=false; };
    void renderMessages();
    void updateControls();
    void submitDraft();
    void retry();
    void reveal(QWidget *target,bool follow);
    void fitContent();
    QList<Message> m_messages;
    QToolButton *m_toggle;
    QWidget *m_body,*m_transcript,*m_empty,*m_suggestions;
    QVBoxLayout *m_messageLayout;
    QBoxLayout *m_suggestionLayout;
    QPlainTextEdit *m_input;
    QPushButton *m_send,*m_analyze,*m_budget,*m_alternative,*m_retry;
    QLabel *m_status;
    bool m_busy=false,m_available=false,m_failed=false,m_brief=false,m_reflowScheduled=false;
    int m_regenerateIndex=-1;
    QString m_question,m_lastError;
};
