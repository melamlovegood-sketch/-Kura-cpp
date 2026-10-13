#pragma once

#include <QDate>
#include <QDialog>
#include <QString>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

// Provides form values without depending on the Transaction model.
class AddTransactionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddTransactionDialog(QWidget *parent = nullptr);

    QString description() const;
    double amount() const;
    QString category() const;
    QDate date() const;

public slots:
    void accept() override;

private:
    QLineEdit *m_descriptionEdit = nullptr;
    QDoubleSpinBox *m_amountSpinBox = nullptr;
    QComboBox *m_categoryComboBox = nullptr;
    QDateEdit *m_dateEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
};
