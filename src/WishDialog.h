#pragma once
#include "WishItem.h"
#include <QDialog>
class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class QButtonGroup;

class WishDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WishDialog(QWidget *parent = nullptr);
    explicit WishDialog(const WishItem &item, QWidget *parent = nullptr);
    WishItem item() const;
    void showSaveError(const QString &message);
    void completeSave();
    void accept() override;
signals:
    void saveRequested();
private:
    WishItem m_original;
    bool m_customCooling=false;
    QLineEdit *m_name;
    QDoubleSpinBox *m_amount;
    QComboBox *m_category;
    QSpinBox *m_days;
    QButtonGroup *m_priority;
    QLineEdit *m_reason;
    QLabel *m_error;
    QPushButton *m_save;
};
