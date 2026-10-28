#pragma once
#include <QWidget>
#include <QString>
class QComboBox;
class QPushButton;
class QVBoxLayout;

// Both entry modes edit the same two independent values.
class ExpenseClassification : public QWidget {
    Q_OBJECT
public:
    explicit ExpenseClassification(const QString &settingsPath={}, QWidget *parent=nullptr);
    QComboBox *purposeCombo() const { return m_purpose; }
    QString purpose() const;
    QString scope() const;
    void setValues(const QString &purpose, const QString &scope);
    void useBudgetMode(bool budget) { if (budget!=m_budgetMode) setBudgetMode(budget,false); }
    bool budgetMode() const { return m_budgetMode; }
signals:
    void changed();
    void entryModeChanged(bool budget);
private:
    void setBudgetMode(bool budget, bool persist);
    void setScope(bool basic);
    QString m_settingsPath;
    bool m_budgetMode=false;
    QComboBox *m_purpose;
    QPushButton *m_byPurpose,*m_byBudget,*m_basic,*m_free,*m_more;
    QWidget *m_purposePanel,*m_scopePanel;
    QVBoxLayout *m_fields;
};
