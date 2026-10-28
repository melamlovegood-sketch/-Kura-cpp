#include "ModernSelectors.h"
#include "ShoppingProfileDialog.h"
#include "ShellWidgets.h"
#include "CategoryCatalog.h"
#include "AppTheme.h"
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>

ShoppingProfileDialog::ShoppingProfileDialog(DatabaseManager &database,QWidget *parent):QDialog(parent),m_repository(database)
{
    setObjectName("shoppingProfileDialog"); setWindowTitle(QStringLiteral("购物原则与信任品牌")); resize(760,560);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(20,20,20,20); layout->setSpacing(12);
    layout->addWidget(ShellWidgets::label(QStringLiteral("用自己的经验，做下一次决定"),this,"heading"));
    auto *tabs=new QTabWidget(this); tabs->setObjectName("shoppingProfileTabs");
    auto *rulesPage=new QWidget; auto *rulesLayout=new QVBoxLayout(rulesPage);
    rulesLayout->addWidget(ShellWidgets::label(QStringLiteral("每行一条购物原则，最多 30 条。例如：先确认用途，再比较价格。"),rulesPage,"muted"));
    m_rules=new QPlainTextEdit(rulesPage); m_rules->setObjectName("shoppingRulesEdit"); m_rules->setPlaceholderText(QStringLiteral("写下你实际认可的规则。没有内容时留空即可。")); rulesLayout->addWidget(m_rules,1);
    auto *saveRules=new QPushButton(QStringLiteral("保存购物原则"),rulesPage); saveRules->setObjectName("saveShoppingRulesButton"); rulesLayout->addWidget(saveRules,0,Qt::AlignRight); tabs->addTab(rulesPage,QStringLiteral("购物原则"));
    auto *brandsPage=new QWidget; auto *brandsLayout=new QVBoxLayout(brandsPage);
    auto *brandHeading=new QHBoxLayout;
    auto *newBrand=new QPushButton(QStringLiteral("＋ 添加品牌"),brandsPage); newBrand->setObjectName("newBrandButton"); brandHeading->addWidget(newBrand); brandHeading->addStretch();
    auto *about=new QPushButton(QStringLiteral("反馈如何使用？"),brandsPage); about->setObjectName("brandFeedbackHelpButton"); about->setProperty("link",true); brandHeading->addWidget(about); brandsLayout->addLayout(brandHeading);
    m_table=new QTableWidget(0,4,brandsPage); m_table->setObjectName("brandTable"); m_table->setHorizontalHeaderLabels({QStringLiteral("品类"),QStringLiteral("品牌"),QStringLiteral("购买反馈"),QStringLiteral("备注")}); m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection); m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->verticalHeader()->hide(); m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); brandsLayout->addWidget(m_table,1);
    auto *buttons=new QHBoxLayout;
    m_editBrand=new QPushButton(QStringLiteral("编辑品牌"),brandsPage); m_editBrand->setObjectName("editBrandButton");
    m_archiveBrand=new QPushButton(QStringLiteral("停用品牌"),brandsPage); m_archiveBrand->setObjectName("archiveBrandButton"); m_archiveBrand->setProperty("link",true);
    buttons->addWidget(m_archiveBrand); buttons->addStretch(); buttons->addWidget(m_editBrand); brandsLayout->addLayout(buttons); tabs->addTab(brandsPage,QStringLiteral("信任品牌")); layout->addWidget(tabs,1);
    m_status=ShellWidgets::label(QString(),this); m_status->setObjectName("shoppingProfileStatusLabel"); layout->addWidget(m_status);
    auto *close=new QPushButton(QStringLiteral("完成"),this); layout->addWidget(close,0,Qt::AlignRight); connect(close,&QPushButton::clicked,this,&QDialog::accept);
    QStringList rules; if (m_repository.loadRules(rules)) m_rules->setPlainText(rules.join('\n')); else m_status->setText(m_repository.lastError());
    refreshBrands();
    connect(m_table,&QTableWidget::itemSelectionChanged,this,&ShoppingProfileDialog::loadSelection);
    connect(newBrand,&QPushButton::clicked,this,[this] { openBrandEditor(false); });
    connect(m_editBrand,&QPushButton::clicked,this,[this] { openBrandEditor(true); });
    connect(m_table,&QTableWidget::cellDoubleClicked,this,[this](int,int) { openBrandEditor(true); });
    connect(about,&QPushButton::clicked,this,[this] { QMessageBox::information(this,QStringLiteral("购买反馈"),QStringLiteral("列表统计已完成复盘中的“值得”和“后悔”次数。\n每次“值得”让品牌偏好加 1，“后悔”减 1；同一项复盘只更新一次。\n偏好只辅助决策，不代表品牌质量。编辑中的高级选项可调整原有偏好，范围 -100 至 100。\n停用会保留历史，再添加同品类同名品牌可恢复。")); });
    connect(saveRules,&QPushButton::clicked,this,[this] {
        QStringList rules; for (const auto &line:m_rules->toPlainText().split('\n')) if (!line.trimmed().isEmpty()) rules << line.trimmed();
        const bool ok=m_repository.saveRules(rules); m_status->setText(ok?QStringLiteral("购物原则已保存。"):m_repository.lastError()); AppTheme::setTone(m_status,ok?"success":"danger"); if (ok) emit profileChanged();
    });
    connect(m_archiveBrand,&QPushButton::clicked,this,[this] {
        if (!m_selected) return; const auto id=m_selected;
        auto *confirmation=new QMessageBox(QMessageBox::Question,QStringLiteral("停用品牌"),QStringLiteral("停用后不再推荐这个品牌，已有复盘历史会保留。确定吗？"),QMessageBox::Yes|QMessageBox::No,this); confirmation->setObjectName("archiveBrandConfirmation"); confirmation->setDefaultButton(QMessageBox::No); confirmation->setAttribute(Qt::WA_DeleteOnClose);
        connect(confirmation,&QMessageBox::finished,this,[this,confirmation,id](int) { if (confirmation->standardButton(confirmation->clickedButton())!=QMessageBox::Yes) return; if (!m_repository.archiveBrand(id)) { m_status->setText(m_repository.lastError()); return; } resetBrand(); refreshBrands(); emit profileChanged(); }); confirmation->open();
    });
}
bool ShoppingProfileDialog::refreshBrands(qint64 selectedId)
{
    QList<BrandEntry> loaded; if (!m_repository.loadBrands(loaded)) { m_status->setText(m_repository.lastError()); return false; } m_brands=loaded;
    m_table->setRowCount(0); m_table->setRowCount(loaded.size());
    for (int row=0;row<loaded.size();++row) { const auto &brand=loaded[row]; const QStringList values={CategoryCatalog::label(brand.category),brand.name,QStringLiteral("值得 %1 · 后悔 %2").arg(brand.worthCount).arg(brand.regretCount),brand.note}; for (int column=0;column<values.size();++column) m_table->setItem(row,column,new QTableWidgetItem(values[column])); m_table->item(row,0)->setData(Qt::UserRole,brand.id); if (brand.id==selectedId) m_table->selectRow(row); } loadSelection(); return true;
}
void ShoppingProfileDialog::loadSelection()
{
    const int row=m_table->currentRow();
    m_selected=row>=0 && m_table->item(row,0) && m_table->item(row,0)->isSelected()?m_table->item(row,0)->data(Qt::UserRole).toLongLong():0;
    m_editBrand->setEnabled(m_selected>0); m_archiveBrand->setEnabled(m_selected>0);
}
void ShoppingProfileDialog::resetBrand() { m_selected=0; m_table->clearSelection(); loadSelection(); }
void ShoppingProfileDialog::openBrandEditor(bool existing)
{
    if (auto *open=findChild<QDialog *>("brandEditorDialog"); open && open->isVisible()) { open->raise(); return; }
    BrandEntry selected; selected.category="online_shopping";
    if(existing) { if(!m_selected) return; bool found=false; for(const auto &brand:m_brands) if(brand.id==m_selected) { selected=brand; found=true; break; } if(!found) return; }
    auto *dialog=new QDialog(this); dialog->setObjectName("brandEditorDialog"); dialog->setWindowTitle(existing?QStringLiteral("编辑品牌"):QStringLiteral("添加品牌")); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(480,390);
    auto *layout=new QVBoxLayout(dialog); auto *form=new QFormLayout;
    auto *name=new QLineEdit(selected.name,dialog); name->setObjectName("brandNameEdit"); name->setMaxLength(80);
    auto *category=new ModernComboBox(dialog); category->setObjectName("brandCategoryComboBox"); for(const auto &key:CategoryCatalog::keys()) category->addItem(CategoryCatalog::label(key),key); category->setCurrentIndex(category->findData(selected.category));
    auto *note=new QLineEdit(selected.note,dialog); note->setObjectName("brandNoteEdit"); note->setMaxLength(500);
    form->addRow(QStringLiteral("品牌名称"),name); form->addRow(QStringLiteral("品类"),category); form->addRow(QStringLiteral("备注 · 选填"),note); layout->addLayout(form);
    auto *advanced=new QPushButton(QStringLiteral("高级选项"),dialog); advanced->setCheckable(true); advanced->setProperty("link",true); advanced->setObjectName("brandAdvancedButton"); layout->addWidget(advanced,0,Qt::AlignLeft);
    auto *weight=new QSpinBox(dialog); weight->setObjectName("brandWeightSpinBox"); weight->setRange(-100,100); weight->setValue(selected.weight); weight->setPrefix(QStringLiteral("偏好 ")); weight->hide(); layout->addWidget(weight);
    connect(advanced,&QPushButton::toggled,weight,&QWidget::setVisible);
    auto *error=ShellWidgets::label(QString(),dialog,"error"); error->setObjectName("brandEditorErrorLabel"); error->hide(); layout->addWidget(error);
    auto *buttons=new QHBoxLayout; auto *cancel=new QPushButton(QStringLiteral("取消"),dialog); auto *save=new QPushButton(QStringLiteral("保存品牌"),dialog); save->setObjectName("saveBrandButton"); save->setProperty("primary",true); buttons->addWidget(cancel); buttons->addStretch(); buttons->addWidget(save); layout->addLayout(buttons);
    connect(cancel,&QPushButton::clicked,dialog,&QDialog::reject);
    connect(save,&QPushButton::clicked,dialog,[this,dialog,selected,name,category,note,weight,error] {
        auto brand=selected; brand.name=name->text(); brand.category=category->currentData().toString(); brand.note=note->text(); brand.weight=weight->value();
        if(!m_repository.saveBrand(brand)) { error->setText(m_repository.lastError()); error->show(); return; }
        dialog->accept(); refreshBrands(brand.id); m_status->setText(QStringLiteral("品牌已保存。")); emit profileChanged();
    }); dialog->open(); name->setFocus();
}
