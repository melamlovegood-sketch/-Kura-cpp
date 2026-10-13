#pragma once
#include "DatabaseBackupService.h"
#include "TransactionFilter.h"
#include <QWidget>
class QPushButton;
class QLabel;
class QComboBox;

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
signals:
    void databaseRestored();
private:
    void chooseFile(int operation);
    void showResult(const QString &message, bool error = false);
    void updateControls();
    bool hasOpenEditor() const;
    DatabaseManager &m_database;
    DatabaseBackupService m_backup;
    TransactionFilter m_filter;
    bool m_available = false;
    bool m_busy = false;
    QPushButton *m_export;
    QPushButton *m_saveBackup;
    QPushButton *m_restore;
    QComboBox *m_exportScope;
    QLabel *m_filterLabel;
    QLabel *m_result;
};
