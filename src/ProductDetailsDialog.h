#pragma once
#include "AIService.h"
#include "ProductRepository.h"
#include <QDialog>
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QImage;
class QScrollArea;
class ProductDetailsDialog:public QDialog {
    Q_OBJECT
public:
    explicit ProductDetailsDialog(DatabaseManager &database,const ProductContext &context,qint64 profileId=0,QWidget *parent=nullptr);
    static ProductDetailsDialog *openFor(DatabaseManager &database,const QString &kind,qint64 entityId,QWidget *parent);
    qint64 profileId() const { return m_profile.id; }
    bool setScreenshot(const QString &path);
    void reject() override;
signals:
    void profileChanged(qint64 profileId);
private:
    void render();
    void edit();
    void cancelEdit();
    void save();
    bool useImage(const QImage &image);
    void recognize();
    void ask();
    void associate();
    void share();
    void setBusy(bool busy);
    void error(const QString &message);
    ProductRepository m_repository;
    ProductContext m_context;
    ProductProfile m_profile;
    AIService m_service;
    QString m_settingsPath,m_question;
    QByteArray m_pendingImage;
    QJsonObject m_pendingExtracted;
    bool m_recognizing=false;
    QMap<QString,QLineEdit *> m_fields;
    QLabel *m_summary,*m_status,*m_error,*m_preview;
    QWidget *m_editor,*m_gallery,*m_captureTools,*m_chatPanel;
    QVBoxLayout *m_galleryLayout;
    QPlainTextEdit *m_notes,*m_reason,*m_alternatives,*m_transcript,*m_input;
    QPushButton *m_edit,*m_upload,*m_paste,*m_recognize,*m_save,*m_cancelEdit,*m_ask,*m_cancel;
    QToolButton *m_more,*m_images,*m_chatToggle;
    QPushButton *m_share;
    QScrollArea *m_scroll;
};
