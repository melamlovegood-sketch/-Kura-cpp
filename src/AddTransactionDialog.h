#pragma once

#include <QDate>
#include <QDialog>
#include <QString>
#include "Transaction.h"

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;

// Validates input, then asks the main window to persist it before closing.
class AddTransactionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddTransactionDialog(QWidget *parent = nullptr);
    explicit AddTransactionDialog(const Transaction &transaction, QWidget *parent = nullptr);

    QString description() const;
    double amount() const;
    QString category() const;
    QDate date() const;
    Transaction transaction() const;
    void showSaveError(const QString &message);
    void completeSave();

signals:
    void saveRequested();

public slots:
    void accept() override;

private:
    qint64 m_transactionId = 0;
    QLineEdit *m_descriptionEdit = nullptr;
    QDoubleSpinBox *m_amountSpinBox = nullptr;
    QComboBox *m_categoryComboBox = nullptr;
    QDateEdit *m_dateEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_saveButton = nullptr;
};
