#pragma once
#include "AIService.h"
#include "DatabaseManager.h"
#include <QWidget>
class QPlainTextEdit;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QFrame;
class QStackedWidget;
class ModernComboBox;
class DatePicker;
class ExpenseComposer:public QWidget {
    Q_OBJECT
public:
    explicit ExpenseComposer(DatabaseManager &database,const QString &settingsPath,QWidget *parent=nullptr);
    void setDatabaseAvailable(bool available);
    void suggestDate(const QDate &date);
    void resetAfterRestore();
    bool setScreenshot(const QString &path);
    bool hasDraft() const;
    void setShowExamples(bool show);
signals:
    void expenseSaved(qint64 id,const QDate &date);
    void settingsRequested();
private:
    void setMode(bool manual);
    void updateControls();
    void analyze();
    void save(bool ai);
    void clearDraft();
    DatabaseManager &m_database;
    QString m_settingsPath;
    AIService m_ai;
    QByteArray m_image;
    bool m_available=false,m_busy=false,m_hasAiDraft=false,m_saving=false;
    bool m_showExamples=false;
    QWidget *m_examples;
    QPushButton *m_aiMode,*m_manualMode,*m_parse,*m_upload,*m_cancel,*m_aiSave,*m_manualSave;
    QStackedWidget *m_modes;
    QPlainTextEdit *m_text,*m_aiNote,*m_manualNote;
    QLineEdit *m_name,*m_aiName;
    QDoubleSpinBox *m_amount,*m_aiAmount;
    ModernComboBox *m_category,*m_aiCategory;
    DatePicker *m_date;
    QFrame *m_confirmation;
    QLabel *m_error,*m_file;
};
