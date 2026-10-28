#pragma once
#include "BillImportRepository.h"
#include "AIService.h"
#include <QDialog>
class QTableWidget;
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
class QThread;
class QStackedWidget;
class QFrame;
class QLineEdit;
class QTimer;
class QDragEnterEvent;
class QDropEvent;
class BillImportDialog:public QDialog {
    Q_OBJECT
public:
    explicit BillImportDialog(DatabaseManager &database,const QString &settingsPath,QWidget *parent=nullptr);
    ~BillImportDialog() override;
    bool loadFile(const QString &path);
    void reject() override;
signals:
    void previewReady();
    void previewFailed(const QString &message);
    void imported(int inserted);
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    void render();
    void updateState();
    void applyFilters();
    void chooseFile();
    void applyCategory();
    void showSkipped();
    bool matchesView(const BillDraft &row) const;
    void commit();
    void classify();
    bool inRange(const BillDraft &row) const;
    BillImportRepository m_repository;
    AIService m_ai;
    QString m_settingsPath,m_path;
    BillParseResult m_preview;
    QThread *m_worker=nullptr;
    bool m_loading=false,m_needsReload=true,m_done=false,m_aiRunning=false,m_closed=false;
    QSet<int> m_aiSuggested;
    int m_filter=0;
    QTableWidget *m_table;
    QStackedWidget *m_pages;
    QFrame *m_options,*m_bulkBar;
    QTimer *m_reloadTimer;
    QLineEdit *m_search;
    QList<QLabel *> m_steps;
    QLabel *m_empty,*m_fileName,*m_footerSummary,*m_footerHint,*m_doneTitle,*m_doneHint,*m_loadingHint;
    QLabel *m_summary,*m_error;
    QComboBox *m_platform,*m_bulkCategory,*m_bulkScope;
    QCheckBox *m_transfers,*m_recent,*m_selectAll;
    QPushButton *m_choose,*m_reload,*m_bulk,*m_import,*m_classify,*m_cancelWork,*m_close,*m_again,*m_skipped,*m_bulkToggle;
    QPushButton *m_newTab,*m_reviewTab,*m_excludedTab;
};
