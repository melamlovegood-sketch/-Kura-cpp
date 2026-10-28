#pragma once
#include "PricePerspective.h"
#include <QWidget>
class QDoubleSpinBox;
class QSpinBox;
class QLabel;
class QPushButton;
class PricePerspectiveWidget:public QWidget {
    Q_OBJECT
public:
    explicit PricePerspectiveWidget(const QString &settingsPath,QWidget *parent=nullptr,bool compact=false);
    bool save();
    void reload();
signals:
    void saved();
private:
    PricePerspective values() const;
    void updatePreview();
    QString m_settingsPath;
    PricePerspective::Mode m_mode=PricePerspective::Off;
    QWidget *m_student,*m_worker,*m_preview;
    QDoubleSpinBox *m_food,*m_salary,*m_days,*m_hours,*m_sample;
    QSpinBox *m_meals;
    QLabel *m_reference,*m_example,*m_basis,*m_status;
};
