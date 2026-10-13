#include "DataManagementWidget.h"
#include "DatabaseManager.h"
#include "TransactionRepository.h"
#include "CsvExporter.h"
#include <QAbstractButton>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
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
    auto *title = new QLabel(QStringLiteral("数据导出与备份"), this);
    auto font = title->font(); font.setPointSize(18); font.setBold(true); title->setFont(font);
    layout->addWidget(title);
    auto *exportGroup = new QGroupBox(QStringLiteral("导出消费记录"), this);
    auto *exportLayout = new QVBoxLayout(exportGroup);
    auto *exportHint = new QLabel(QStringLiteral("导出为 CSV，可用 Excel 打开。包含编号、日期、时间、名称、分类、金额和完整备注。"), this);
    exportHint->setWordWrap(true); exportLayout->addWidget(exportHint);
    auto *exportRow = new QHBoxLayout;
    m_exportScope = new QComboBox(this);
    m_exportScope->setObjectName(QStringLiteral("exportScopeComboBox"));
    m_exportScope->addItems({QStringLiteral("全部消费记录"), QStringLiteral("当前已应用的筛选结果")});
    exportRow->addWidget(m_exportScope); exportRow->addStretch();
    m_export = new QPushButton(QStringLiteral("导出 CSV"), this);
    m_export->setObjectName(QStringLiteral("exportCsvButton")); exportRow->addWidget(m_export);
    exportLayout->addLayout(exportRow);
    m_filterLabel = new QLabel(this);
    m_filterLabel->setObjectName(QStringLiteral("exportFilterLabel"));
    m_filterLabel->setWordWrap(true); exportLayout->addWidget(m_filterLabel);
    layout->addWidget(exportGroup);
    auto *backupGroup = new QGroupBox(QStringLiteral("完整备份与恢复"), this);
    auto *backupLayout = new QVBoxLayout(backupGroup);
    auto *backupHint = new QLabel(QStringLiteral("完整备份包含消费、月度预算和愿望清单。恢复会替换这三类数据；操作前会检查文件，并自动保存当前数据的备份。"), this);
    backupHint->setWordWrap(true); backupLayout->addWidget(backupHint);
    auto *backupRow = new QHBoxLayout; backupRow->addStretch();
    m_saveBackup = new QPushButton(QStringLiteral("保存完整备份"), this);
    m_saveBackup->setObjectName(QStringLiteral("saveBackupButton")); backupRow->addWidget(m_saveBackup);
    m_restore = new QPushButton(QStringLiteral("从备份恢复"), this);
    m_restore->setObjectName(QStringLiteral("restoreBackupButton")); backupRow->addWidget(m_restore);
    backupLayout->addLayout(backupRow); layout->addWidget(backupGroup);
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
    for (auto *button : {m_export, m_saveBackup, m_restore}) button->setEnabled(m_available && !m_busy);
    m_exportScope->setEnabled(m_available && !m_busy);
}
void DataManagementWidget::showResult(const QString &message, bool error)
{
    m_result->setStyleSheet(error ? QStringLiteral("color: #b42318;") : QString());
    m_result->setText(message);
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
        QStringLiteral("备份文件：%1\n\n消费 %2 笔，预算 %3 个月，愿望 %4 条。\n将替换当前数据，并先自动保存当前数据的备份。\n筛选和未保存的预算输入会重置。确定恢复吗？")
            .arg(path).arg(info.expenseCount).arg(info.budgetCount).arg(info.wishCount),
        QMessageBox::Yes | QMessageBox::No, this);
    confirmation->setObjectName(QStringLiteral("restoreConfirmationDialog"));
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
