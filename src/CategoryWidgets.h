#pragma once
#include <QWidget>
#include <QString>
class QComboBox;
class QLineEdit;
class QLabel;
class QVBoxLayout;
class DatabaseManager;

namespace CategoryWidgets {
void bind(QComboBox *combo,bool purpose=false,const QString &settingsPath={});
void select(QComboBox *combo,const QString &key,bool purpose=false);
void refreshAll(QWidget *root,const QString &oldKey={},const QString &newKey={});
}
class CategorySettingsWidget final:public QWidget {
    Q_OBJECT
public:
    explicit CategorySettingsWidget(DatabaseManager *database,QWidget *parent=nullptr);
    void refresh();
signals:
    void categoriesChanged(const QString &oldKey,const QString &newKey);
protected:
    void showEvent(QShowEvent *) override;
private:
    DatabaseManager *m_database;
    QLineEdit *m_name;
    QLabel *m_error;
    QVBoxLayout *m_rows;
};
