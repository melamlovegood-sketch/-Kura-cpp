#pragma once

#include "BudgetRepository.h"
#include <QWidget>

class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class BudgetWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BudgetWidget(DatabaseManager &database, QWidget *parent = nullptr);
    void setDatabaseAvailable(bool available);
    bool refresh();
    void setMonth(const QDate &month);

private:
    bool loadSelectedMonth(bool resetAmount);
    void saveBudget();
    void updateControls();
    void renderStatus();
    BudgetRepository m_repository;
    MonthlyBudgetStatus m_status;
    bool m_databaseAvailable = false;
    bool m_hasLoaded = false;
    QDateEdit *m_monthEdit = nullptr;
    QDoubleSpinBox *m_amountEdit = nullptr;
    QPushButton *m_saveButton = nullptr;
    QLabel *m_limitLabel = nullptr;
    QLabel *m_spentLabel = nullptr;
    QLabel *m_remainingLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
};
