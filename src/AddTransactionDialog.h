#pragma once

#include <QDate>
#include <QDialog>
#include <QString>
#include "Transaction.h"
#include "ShoppingProfileRepository.h"

class QComboBox;
class QCheckBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimeEdit;
class QPlainTextEdit;
class QFormLayout;

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
    QTime time() const;
    QString note() const;
    Transaction transaction() const;
    void showSaveError(const QString &message);
    void completeSave();
    void setBrands(const QList<BrandEntry> &brands,qint64 selectedId=0);
    qint64 selectedBrandId() const;

signals:
    void saveRequested();

public slots:
    void accept() override;

private:
    qint64 m_transactionId = 0;
    QByteArray m_sourceImage;
    QLineEdit *m_descriptionEdit = nullptr;
    QDoubleSpinBox *m_amountSpinBox = nullptr;
    QComboBox *m_categoryComboBox = nullptr;
    QDateEdit *m_dateEdit = nullptr;
    QTimeEdit *m_timeEdit = nullptr;
    QCheckBox *m_timeKnownCheckBox = nullptr;
    QPlainTextEdit *m_noteEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_saveButton = nullptr;
    QFormLayout *m_form = nullptr;
    QComboBox *m_brandCombo = nullptr;
    QList<BrandEntry> m_brands;
    void refreshBrands();
};
