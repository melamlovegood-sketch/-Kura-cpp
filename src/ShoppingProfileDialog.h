#pragma once
#include "ShoppingProfileRepository.h"
#include <QDialog>
class QPlainTextEdit;
class QTableWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QLabel;
class QPushButton;
class ShoppingProfileDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShoppingProfileDialog(DatabaseManager &database,QWidget *parent=nullptr);
signals:
    void profileChanged();
private:
    bool refreshBrands(qint64 selectedId=0);
    void loadSelection();
    void resetBrand();
    void openBrandEditor(bool existing);
    ShoppingProfileRepository m_repository;
    QList<BrandEntry> m_brands;
    qint64 m_selected=0;
    QPlainTextEdit *m_rules;
    QTableWidget *m_table;
    QPushButton *m_editBrand,*m_archiveBrand;
    QLabel *m_status;
};
