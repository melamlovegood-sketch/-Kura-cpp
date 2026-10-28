#pragma once
#include <QWidget>
#include "AIService.h"
#include <QElapsedTimer>
class QPushButton;
class QLineEdit;
class QLabel;
class QComboBox;
class QCheckBox;
class QShowEvent;
class AppearanceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppearanceWidget(const QString &settingsPath = {}, QWidget *parent = nullptr);
    void setTheme(const QString &theme);
    void setBackupSummary(const QString &summary);
    void refreshSummary();
    void reloadOnboardingSettings();
    void showHome();
signals:
    void themeSelected(const QString &theme);
    void dataRequested();
    void onboardingRequested();
    void pricePerspectiveChanged();
    void displayOptionsChanged(int fontScale, bool reducedMotion);
protected:
    void showEvent(QShowEvent *event) override;
private:
    void showSection(int section);
    QWidget *m_home,*m_appearanceSection,*m_aiSection,*m_priceSection,*m_aboutSection,*m_feedbackSection;
    QLabel *m_title;
    QPushButton *m_back;
    QList<QPushButton *> m_choices;
    QString m_settingsPath;
    QLineEdit *m_baseUrl,*m_model,*m_key;
    QLabel *m_aiStatus;
    QLabel *m_trialStatus;
    QPushButton *m_useTrial,*m_usePersonal;
    AIService m_connection;
    QElapsedTimer m_connectionTimer;
    bool m_testing=false;
    QPushButton *m_probeText,*m_probeImage,*m_cancelProbe,*m_saveAI;
    QLabel *m_appearanceSummary,*m_priceSummary,*m_serviceSummary,*m_dataSummary,*m_lastTest;
    QPushButton *m_unsavedAI;
    QComboBox *m_fontScale;
    QCheckBox *m_reducedMotion;
    AISettings m_savedAI;
    QString m_selectedTheme="warm";
    bool m_imageProbe=false;
    AISettings currentAI() const;
    AISettings effectiveAI() const;
    void selectTrial(bool enabled);
    bool aiDirty() const;
    QString configHash(const AISettings &settings) const;
    void recordTest(bool success);
    void updateTestSummary();
    void testConnection(bool image);
    void updateConnectionControls();
};
