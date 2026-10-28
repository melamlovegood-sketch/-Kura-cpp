#pragma once
#include "AIService.h"
#include "DatabaseManager.h"
#include <QDialog>
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QSpinBox;
class QImage;
class QScrollArea;
class QButtonGroup;
class DecisionDialog : public QDialog {
    Q_OBJECT
public:
    explicit DecisionDialog(DatabaseManager &database, const AISettings &settings, QWidget *parent=nullptr);
    bool setScreenshot(const QString &path);
    void reject() override;
    void setResearchAvailable(bool available);
    void setInitialText(const QString &text);
    void showHandoffError(const QString &message);
signals:
    void committed(const QString &outcome, qint64 id);
    void settingsRequested();
    void researchRequested(const ParsedProduct &product,const QByteArray &image,qint64 productId);
private:
    bool useImage(const QImage &image);
    void setBusy(bool busy);
    void analyze();
    void showDraft(const ParsedProduct &product);
    void saveChoice(const QString &choice);
    void invalidateDraft();
    void showStage(int stage);
    void selectChoice(const QString &choice);
    bool validateDraft();
    void requestExecution();
    DatabaseManager &m_database;
    AISettings m_settings;
    AIService m_service;
    QByteArray m_image;
    QString m_token;
    qint64 m_productProfileId=0;
    bool m_saved=false,m_hasDraft=false,m_researchAvailable=true;
    int m_stage=0;
    QString m_choice;
    QString m_purposeCategory,m_budgetScope;
    QPlainTextEdit *m_text, *m_note;
    QLabel *m_error, *m_imagePreview, *m_status;
    QWidget *m_draft,*m_inputPage,*m_choicePage,*m_extras,*m_wishlistOptions;
    QLabel *m_stepLabel,*m_summary,*m_choiceHint;
    QPushButton *m_continue,*m_back,*m_commit,*m_research;
    QPushButton *m_parse, *m_imageButton, *m_removeImage, *m_manual, *m_settingsButton;
    QPushButton *m_hold, *m_wish, *m_buy, *m_track;
    QLineEdit *m_name;
    QDoubleSpinBox *m_amount;
    QComboBox *m_category;
    QSpinBox *m_days;
    QButtonGroup *m_priority;
    QScrollArea *m_scroll;
};
