#pragma once
#include "AIService.h"
#include "PriceRepository.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QDoubleSpinBox;
class QImage;
class QLabel;
class QLineEdit;
class QPushButton;

class PriceRecordDialog:public QDialog {
    Q_OBJECT
public:
    explicit PriceRecordDialog(DatabaseManager &database,const PriceTrack *track=nullptr,QWidget *parent=nullptr,qint64 wishId=0);
    bool setScreenshot(const QString &path);
    void reject() override;
signals:
    void priceSaved(qint64 trackId);
    void settingsRequested();
private:
    bool useImage(const QImage &image);
    void setBusy(bool busy);
    void analyze();
    void save();
    void showError(const QString &message);
    DatabaseManager &m_database;
    PriceRepository m_repository;
    AIService m_service;
    QString m_settingsPath,m_token,m_trackName;
    qint64 m_trackId=0,m_productProfileId=0,m_wishId=0;
    QByteArray m_image;
    bool m_saved=false,m_aiDraft=false,m_aiFailed=false;
    QWidget *m_form,*m_options;
    QLineEdit *m_name,*m_source;
    QComboBox *m_category;
    QDoubleSpinBox *m_amount;
    QDateTimeEdit *m_time;
    QLabel *m_preview,*m_status,*m_aiNote,*m_error;
    QCheckBox *m_confirmMatch;
    QPushButton *m_choose,*m_paste,*m_remove,*m_analyze,*m_manual,*m_save,*m_settings;
};
