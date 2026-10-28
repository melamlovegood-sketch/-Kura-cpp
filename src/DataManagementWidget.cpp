#include "ModernSelectors.h"
#include "AppTheme.h"
#include "DataManagementWidget.h"
#include "DecisionDialog.h"
#include "BillImportDialog.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "CsvExporter.h"
#include <QAbstractButton>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

DataManagementWidget::DataManagementWidget(DatabaseManager &database, QWidget *parent)
    : QWidget(parent), m_database(database), m_backup(database)
{
    setObjectName(QStringLiteral("dataManagementWidget"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(20);
    auto *heading=new QHBoxLayout;
    m_back=new QPushButton(QStringLiteral("← 返回"),this); m_back->setObjectName("dataOperationBackButton"); m_back->setProperty("link",true); m_back->hide(); heading->addWidget(m_back);
    m_title=new QLabel(QStringLiteral("数据管理"),this); m_title->setProperty("heading",true); heading->addWidget(m_title,1); layout->addLayout(heading);
    m_sections=new QStackedWidget(this); m_sections->setObjectName("dataOperationPages"); layout->addWidget(m_sections);
    auto *home=new QWidget; auto *homeLayout=new QVBoxLayout(home); homeLayout->setContentsMargins(0,0,0,0); homeLayout->setSpacing(12);
    const auto entry=[&](const QString &text,const char *name) { auto *button=new QPushButton(text,home); button->setObjectName(QLatin1String(name)); button->setMinimumHeight(52); homeLayout->addWidget(button); m_entries.append(button); return button; };
    m_import=entry(QStringLiteral("导入账单  →"),"importBillsButton");
    auto *exportEntry=entry(QStringLiteral("导出记录  →"),"openExportOptionsButton");
    auto *backupEntry=entry(QStringLiteral("备份数据  →"),"openBackupOptionsButton");
    auto *restoreEntry=entry(QStringLiteral("恢复备份  →"),"openRestoreOptionsButton");
    homeLayout->addStretch(); m_sections->addWidget(home);
    const auto page=[&](const QString &hint) { auto *body=new QWidget; auto *column=new QVBoxLayout(body); column->setContentsMargins(0,0,0,0); column->setSpacing(14); auto *label=new QLabel(hint,body); label->setWordWrap(true); label->setProperty("muted",true); column->addWidget(label); m_sections->addWidget(body); return column; };
    auto *exports=page(QStringLiteral("导出为 CSV，可用 Excel 打开。包含消费时间、名称、分类、金额与备注。"));
    m_exportScope=new ModernComboBox(this); m_exportScope->setObjectName("exportScopeComboBox"); m_exportScope->addItems({QStringLiteral("全部消费记录"),QStringLiteral("当前已应用的筛选结果")}); exports->addWidget(m_exportScope);
    m_filterLabel=new QLabel(this); m_filterLabel->setObjectName("exportFilterLabel"); m_filterLabel->setWordWrap(true); exports->addWidget(m_filterLabel);
    m_export=new QPushButton(QStringLiteral("选择位置并导出"),this); m_export->setObjectName("exportCsvButton"); m_export->setProperty("primary",true); exports->addWidget(m_export,0,Qt::AlignRight); exports->addStretch();
    auto *backups=page(QStringLiteral("保存完整的 SQLite 备份，包含账单、截图、预算、清单、购买复盘、价格历史和商品资料。"));
    m_saveBackup=new QPushButton(QStringLiteral("选择位置并备份"),this); m_saveBackup->setObjectName("saveBackupButton"); m_saveBackup->setProperty("primary",true); backups->addWidget(m_saveBackup,0,Qt::AlignRight); backups->addStretch();
    auto *restores=page(QStringLiteral("从完整备份恢复会替换当前所有业务数据。选择文件后先检查备份内容，再由你确认；恢复前会自动备份当前数据。"));
    m_restore=new QPushButton(QStringLiteral("选择备份文件"),this); m_restore->setObjectName("restoreBackupButton"); restores->addWidget(m_restore,0,Qt::AlignRight); restores->addStretch();
    connect(m_import,&QPushButton::clicked,this,&DataManagementWidget::openImportDialog);
    connect(exportEntry,&QPushButton::clicked,this,[this] { showOperation(1); });
    connect(backupEntry,&QPushButton::clicked,this,[this] { showOperation(2); });
    connect(restoreEntry,&QPushButton::clicked,this,[this] { showOperation(3); });
    connect(m_back,&QPushButton::clicked,this,[this] { showOperation(0); });
    m_result = new QLabel(this);
    m_result->setObjectName(QStringLiteral("dataManagementResultLabel"));
    m_result->setTextFormat(Qt::PlainText);
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_result);
    layout->addStretch();
    connect(m_export, &QPushButton::clicked, this, [this] { chooseFile(0); });
    connect(m_saveBackup, &QPushButton::clicked, this, [this] { chooseFile(1); });
    connect(m_restore, &QPushButton::clicked, this, [this] { chooseFile(2); });
    setAppliedFilter({}); updateControls();
}

void DataManagementWidget::showOperation(int operation)
{
    m_sections->setCurrentIndex(operation); m_back->setVisible(operation!=0);
    m_title->setText(QStringList{QStringLiteral("数据管理"),QStringLiteral("导出消费记录"),QStringLiteral("备份数据"),QStringLiteral("恢复备份")}.value(operation));
}
void DataManagementWidget::setDatabaseAvailable(bool available)
{
    m_available = available;
    if (!available) showResult(QStringLiteral("数据库不可用，暂时无法导出、备份或恢复。"), true);
    updateControls();
}
void DataManagementWidget::setAppliedFilter(const TransactionFilter &filter)
{
    m_filter = filter;
    m_filterLabel->setText(QStringLiteral("当前筛选：%1（以记账页点击“筛选”后应用的条件为准）").arg(filter.description()));
}
void DataManagementWidget::updateControls()
{
    for (auto *button : {m_export, m_saveBackup, m_restore,m_import}) button->setEnabled(m_available && !m_busy);
    for (auto *button:m_entries) button->setEnabled(m_available && !m_busy);
    m_exportScope->setEnabled(m_available && !m_busy);
}
void DataManagementWidget::showResult(const QString &message, bool error)
{
    AppTheme::setTone(m_result, error ? QStringLiteral("danger") : QString());
    m_result->setText(message);
}
void DataManagementWidget::openImportDialog()
{
    if (!m_available || m_busy) return; if (auto *existing=window()->findChild<BillImportDialog *>(); existing && existing->isVisible()) { existing->raise(); return; }
    const auto settings=m_database.databasePath()==QStringLiteral(":memory:")?QString():m_database.databasePath()+QStringLiteral(".ui.ini"); auto *dialog=new BillImportDialog(m_database,settings,this); dialog->setAttribute(Qt::WA_DeleteOnClose); connect(dialog,&BillImportDialog::imported,this,&DataManagementWidget::recordsImported); dialog->open();
}
bool DataManagementWidget::hasOpenEditor() const
{
    for (const auto *dialog : window()->findChildren<QDialog *>())
        if (dialog->isVisible() && dialog->objectName() != QStringLiteral("dataFileDialog")) return true;
    return false;
}

void DataManagementWidget::chooseFile(int operation)
{
    if (!m_available || m_busy) return;
    if (operation == 2 && hasOpenEditor()) {
        showResult(QStringLiteral("请先保存或取消打开的表单，再恢复备份。"), true); return;
    }
    auto *dialog = new QFileDialog(this);
    dialog->setObjectName(QStringLiteral("dataFileDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setAcceptMode(operation == 2 ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
    dialog->setFileMode(operation == 2 ? QFileDialog::ExistingFile : QFileDialog::AnyFile);
    dialog->setNameFilter(operation == 0 ? QStringLiteral("CSV 文件 (*.csv)") : QStringLiteral("SQLite 完整备份 (*.sqlite *.db)"));
    dialog->setDefaultSuffix(operation == 0 ? QStringLiteral("csv") : QStringLiteral("sqlite"));
    dialog->setWindowTitle(operation == 0 ? QStringLiteral("导出消费记录") : operation == 1 ? QStringLiteral("保存完整备份") : QStringLiteral("选择完整备份"));
    if (operation != 2)
        dialog->selectFile(QStringLiteral("kura-%1-%2.%3").arg(operation == 0 ? QStringLiteral("expenses") : QStringLiteral("backup"),
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")), operation == 0 ? QStringLiteral("csv") : QStringLiteral("sqlite")));
    const bool allRecords = m_exportScope->currentIndex() == 0;
    connect(dialog, &QFileDialog::fileSelected, this, [this, operation, allRecords](const QString &path) {
        if (operation == 0) exportTo(path, allRecords);
        else if (operation == 1) backupTo(path);
        else requestRestore(path);
    });
    dialog->open();
}

bool DataManagementWidget::exportTo(const QString &path, bool allRecords)
{
    if (!m_available || m_busy) return false;
    if (m_backup.isProtectedPath(path)) { showResult(QStringLiteral("不能覆盖当前数据库或日志文件。"), true); return false; }
    QList<Transaction> records;
    TransactionRepository repository(m_database);
    if (!repository.findAll(records)) { showResult(repository.lastError(), true); return false; }
    if (!allRecords) {
        QList<Transaction> filtered;
        for (const auto &record : records) if (m_filter.matches(record)) filtered.append(record);
        records = filtered;
    }
    QString error;
    if (!CsvExporter::write(path, records, error)) { showResult(error, true); return false; }
    showResult(QStringLiteral("已导出 %1 笔消费：\n%2").arg(records.size()).arg(path));
    return true;
}
bool DataManagementWidget::backupTo(const QString &path)
{
    if (!m_available || m_busy) return false;
    QString error;
    if (!m_backup.backup(path, error)) { showResult(error, true); return false; }
    showResult(QStringLiteral("完整备份已保存：\n%1").arg(path));
    return true;
}
bool DataManagementWidget::requestRestore(const QString &path)
{
    if (!m_available || m_busy) return false;
    if (hasOpenEditor()) { showResult(QStringLiteral("请先保存或取消打开的表单，再恢复备份。"), true); return false; }
    BackupInfo info;
    QString error;
    if (!m_backup.prepareRestore(path, info, error)) { showResult(QStringLiteral("备份检查失败：%1").arg(error), true); return false; }
    auto *confirmation = new QMessageBox(QMessageBox::Question, QStringLiteral("恢复完整备份"),
        QStringLiteral("备份文件：%1\n\n消费 %2 笔，预算 %3 个月，愿望 %4 条。\n许愿目标 %5 个，忍住记录 %6 条。\n购物原则 %7 条，品牌 %8 个，决策会话 %9 次。\n购买复盘 %10 项，价格商品 %11 个，价格记录 %12 条。\n导入索引 %13 条，月度标记 %14 条，月度故事 %15 份。\n将替换当前数据，并先自动保存当前数据的备份。\n筛选、未保存的预算和决策草稿会重置。确定恢复吗？")
            .arg(path).arg(info.expenseCount).arg(info.budgetCount).arg(info.wishCount).arg(info.poolCount).arg(info.savingCount).arg(info.ruleCount).arg(info.brandCount).arg(info.sessionCount).arg(info.reviewCount).arg(info.trackCount).arg(info.priceCount).arg(info.importKeyCount).arg(info.tagCount).arg(info.storyCount),
        QMessageBox::Yes | QMessageBox::No, this);
    confirmation->setObjectName(QStringLiteral("restoreConfirmationDialog"));
    confirmation->setDetailedText(confirmation->text());
    confirmation->setText(QStringLiteral("使用“%1”替换当前全部业务数据？\n\n包括账单、预算、清单、复盘、商品资料、截图和价格历史。\n备份中有 %2 笔消费、%3 条愿望；完整数量可展开详情查看。\n恢复前会自动备份当前数据，未保存的草稿会重置。")
        .arg(QFileInfo(path).fileName()).arg(info.expenseCount).arg(info.wishCount));
    confirmation->setTextFormat(Qt::PlainText);
    confirmation->setAttribute(Qt::WA_DeleteOnClose);
    confirmation->button(QMessageBox::Yes)->setText(QStringLiteral("恢复"));
    confirmation->button(QMessageBox::No)->setText(QStringLiteral("取消"));
    confirmation->setDefaultButton(QMessageBox::No); confirmation->setEscapeButton(QMessageBox::No);
    m_busy = true; updateControls();
    connect(confirmation, &QMessageBox::finished, this, [this, confirmation](int) {
        const bool accepted = confirmation->standardButton(confirmation->clickedButton()) == QMessageBox::Yes;
        if (!accepted) {
            m_backup.discardPrepared(); showResult(QStringLiteral("已取消恢复，当前数据保持不变。"));
        } else {
            QString safetyBackup, error;
            if (!m_backup.restorePrepared(safetyBackup, error)) {
                showResult(error + (safetyBackup.isEmpty() ? QString() : QStringLiteral("\n恢复前备份：%1").arg(safetyBackup)), true);
                m_backup.discardPrepared();
            } else {
                emit databaseRestored();
                showResult(QStringLiteral("数据已恢复。恢复前的自动备份：\n%1").arg(safetyBackup));
            }
        }
        m_busy = false; updateControls();
    });
    confirmation->open();
    return true;
}
