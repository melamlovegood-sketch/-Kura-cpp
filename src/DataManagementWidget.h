#pragma once
#include "DatabaseBackupService.h"
#include "TransactionFilter.h"
#include "AutoBackupService.h"
#include <QWidget>
#include <QList>
class QPushButton;
class QLabel;
class QComboBox;
class QStackedWidget;
class QCheckBox;
class QSpinBox;

class DataManagementWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DataManagementWidget(DatabaseManager &database, QWidget *parent = nullptr);
    void setDatabaseAvailable(bool available);
    void setAppliedFilter(const TransactionFilter &filter);
    bool exportTo(const QString &path, bool allRecords);
    bool backupTo(const QString &path);
    bool requestRestore(const QString &path);
    void openImportDialog();
    QString backupSummary() const;
signals:
    void databaseRestored();
    void recordsImported(int count);
    void backupStateChanged();
private:
    void chooseFile(int operation);
    void showOperation(int operation);
    void showResult(const QString &message, bool error = false);
    void updateControls();
    bool hasOpenEditor() const;
    void refreshBackupStatus();
    DatabaseManager &m_database;
    DatabaseBackupService m_backup;
    AutoBackupService m_autoBackup;
    TransactionFilter m_filter;
    bool m_available = false;
    bool m_busy = false;
    QPushButton *m_export;
    QPushButton *m_saveBackup;
    QPushButton *m_restore;
    QPushButton *m_import;
    QComboBox *m_exportScope;
    QLabel *m_filterLabel;
    QLabel *m_result;
    QStackedWidget *m_sections;
    QPushButton *m_back;
    QList<QPushButton *> m_entries;
    QLabel *m_title;
    QCheckBox *m_autoEnabled;
    QSpinBox *m_backupKeep;
    QLabel *m_backupStatus;
    QPushButton *m_saveBackupPreferences;
};
