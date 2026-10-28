#pragma once
#include <QWidget>
#include "AIService.h"
#include <QElapsedTimer>
class QPushButton;
class QLineEdit;
class QLabel;
class AppearanceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AppearanceWidget(const QString &settingsPath = {}, QWidget *parent = nullptr);
    void setTheme(const QString &theme);
signals:
    void themeSelected(const QString &theme);
    void dataRequested();
    void shoppingProfileRequested();
private:
    void showSection(int section);
    QWidget *m_home,*m_appearanceSection,*m_aiSection;
    QLabel *m_title;
    QPushButton *m_back;
    QList<QPushButton *> m_choices;
    QString m_settingsPath;
    QLineEdit *m_baseUrl,*m_model,*m_key;
    QLabel *m_aiStatus;
    AIService m_connection;
    QElapsedTimer m_connectionTimer;
    bool m_testing=false;
    QPushButton *m_probeText,*m_probeImage,*m_cancelProbe,*m_saveAI;
    void testConnection(bool image);
    void updateConnectionControls();
};
