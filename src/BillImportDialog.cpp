#include "BillImportDialog.h"
#include "ModernSelectors.h"
#include "CategoryCatalog.h"
#include "ShellWidgets.h"
#include "AppTheme.h"
#include "Transaction.h"
#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <memory>
namespace {
bool allowed(const BillDraft &row) { return row.state!=BillDraft::Duplicate && row.state!=BillDraft::Conflict; }
QString categoryName(const QString &key) { return CategoryCatalog::label(key).replace(QStringLiteral("（旧分类）"),QString()); }
QPushButton *quiet(const QString &text,const char *name,QWidget *parent) { auto *button=new QPushButton(text,parent); button->setObjectName(QLatin1String(name)); button->setProperty("importQuiet",true); button->setCursor(Qt::PointingHandCursor); return button; }
class ImportDelegate final:public QStyledItemDelegate {
public:
    explicit ImportDelegate(QTableWidget *table):QStyledItemDelegate(table),m_table(table) {}
    QWidget *createEditor(QWidget *parent,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        if(index.column()!=5) return QStyledItemDelegate::createEditor(parent,option,index);
        auto *combo=new ModernComboBox(parent); combo->setObjectName("importCategoryEditor"); for(const auto &key:CategoryCatalog::keys()) combo->addItem(categoryName(key),key);
        connect(combo,&QComboBox::activated,combo,[this,combo] { emit const_cast<ImportDelegate *>(this)->commitData(combo); emit const_cast<ImportDelegate *>(this)->closeEditor(combo); }); return combo;
    }
    void setEditorData(QWidget *editor,const QModelIndex &index) const override { if(auto *combo=qobject_cast<QComboBox *>(editor)) combo->setCurrentIndex(combo->findData(index.data(Qt::UserRole).toString())); else QStyledItemDelegate::setEditorData(editor,index); }
    void setModelData(QWidget *editor,QAbstractItemModel *model,const QModelIndex &index) const override { if(auto *combo=qobject_cast<QComboBox *>(editor)) { model->setData(index,combo->currentData(),Qt::UserRole); model->setData(index,categoryName(combo->currentData().toString())); } else QStyledItemDelegate::setModelData(editor,model,index); }
    bool editorEvent(QEvent *event,QAbstractItemModel *model,const QStyleOptionViewItem &option,const QModelIndex &index) override {
        if(event->type()==QEvent::MouseButtonRelease && index.flags().testFlag(Qt::ItemIsEnabled) && static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton) {
            if(index.column()==0 && index.flags().testFlag(Qt::ItemIsUserCheckable)) { model->setData(index,index.data(Qt::CheckStateRole).toInt()==Qt::Checked?Qt::Unchecked:Qt::Checked,Qt::CheckStateRole); return true; }
            if(index.column()==5 && index.flags().testFlag(Qt::ItemIsEditable)) { m_table->edit(index); return true; }
        } return QStyledItemDelegate::editorEvent(event,model,option,index);
    }
    void paint(QPainter *p,const QStyleOptionViewItem &o,const QModelIndex &index) const override {
        if(index.column()!=0 && index.column()!=5 && index.column()!=7) { QStyledItemDelegate::paint(p,o,index); return; }
        p->save(); p->setRenderHint(QPainter::Antialiasing); const auto *w=o.widget; const bool enabled=index.flags().testFlag(Qt::ItemIsEnabled);
        if(o.state.testFlag(QStyle::State_Selected)) p->fillRect(o.rect,AppTheme::color(w,"inset"));
        if(index.column()==0) { const QRectF box(o.rect.center().x()-9,o.rect.center().y()-9,18,18); const bool checked=index.data(Qt::CheckStateRole).toInt()==Qt::Checked; p->setPen(QPen(AppTheme::color(w,checked?"accent":"border"),1)); p->setBrush(AppTheme::color(w,checked?"accent":"card")); p->setOpacity(enabled?1:.35); p->drawRoundedRect(box,5,5); if(checked) { p->setPen(QPen(AppTheme::color(w,"onAccent"),1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p->drawLine(box.topLeft()+QPointF(4,9),box.topLeft()+QPointF(8,13)); p->drawLine(box.topLeft()+QPointF(8,13),box.topLeft()+QPointF(14,5)); } }
        else { const bool category=index.column()==5; const auto pill=QRectF(o.rect).adjusted(7,10,-7,-10); p->setPen(Qt::NoPen); p->setBrush(AppTheme::color(w,"inset")); p->drawRoundedRect(pill,10,10); p->setPen(AppTheme::color(w,enabled?"secondary":"muted")); p->setFont(o.font); const int right=category?21:8; p->drawText(pill.adjusted(10,0,-right,0),Qt::AlignVCenter|Qt::AlignLeft,o.fontMetrics.elidedText(index.data().toString(),Qt::ElideRight,int(pill.width())-10-right)); if(category && enabled) { const QPointF c(pill.right()-12,pill.center().y()); p->setPen(QPen(AppTheme::color(w,"muted"),1.1)); p->drawLine(c+QPointF(-3,-1),c+QPointF(0,2)); p->drawLine(c+QPointF(0,2),c+QPointF(3,-1)); } } p->restore();
    }
private: QTableWidget *m_table;
};
}
BillImportDialog::BillImportDialog(DatabaseManager &database,const QString &settingsPath,QWidget *parent):QDialog(parent),m_repository(database),m_ai(this),m_settingsPath(settingsPath)
{
    setObjectName("billImportDialog"); setWindowTitle(QStringLiteral("导入账单")); setProperty("dialogFixedViewport",true); resize(1020,720); setAcceptDrops(true);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(24,24,24,24); layout->setSpacing(14);
    auto *steps=new QHBoxLayout; steps->setSpacing(8); for(const auto &text:QStringList{QStringLiteral("1  选择文件"),QStringLiteral("2  核对账单"),QStringLiteral("3  导入完成")}) { auto *label=ShellWidgets::label(text,this); label->setProperty("importStep",true); m_steps.append(label); steps->addWidget(label); } steps->addStretch(); auto *optionsButton=quiet(QStringLiteral("导入选项"),"billImportOptionsButton",this); steps->addWidget(optionsButton); layout->addLayout(steps);
    m_options=new QFrame(this); m_options->setObjectName("importOptionsPanel"); auto *options=new QVBoxLayout(m_options); options->setContentsMargins(14,12,14,12); options->setSpacing(10);
    auto *source=new QHBoxLayout; source->addWidget(ShellWidgets::label(QStringLiteral("账单来源"),m_options,"muted")); m_platform=new ModernComboBox(m_options); m_platform->setObjectName("importPlatformComboBox"); m_platform->addItem(QStringLiteral("自动识别（推荐）"),""); m_platform->addItem(QStringLiteral("微信"),"wechat"); m_platform->addItem(QStringLiteral("支付宝"),"alipay"); m_platform->addItem(QStringLiteral("Kura CSV"),"kura"); source->addWidget(m_platform,1); m_reload=quiet(QStringLiteral("重新读取文件"),"reloadBillFileButton",m_options); source->addWidget(m_reload); options->addLayout(source);
    m_transfers=new QCheckBox(QStringLiteral("也导入转账与红包支出"),m_options); m_transfers->setObjectName("includeBillTransfersCheckBox"); m_recent=new QCheckBox(QStringLiteral("只显示近 6 个月"),m_options); m_recent->setObjectName("recentBillsCheckBox"); auto *scope=new QHBoxLayout; scope->addWidget(m_transfers); scope->addWidget(m_recent); scope->addStretch(); options->addLayout(scope); m_options->hide(); layout->addWidget(m_options); connect(optionsButton,&QPushButton::clicked,this,[this] { m_options->setVisible(m_options->isHidden()); });
    m_pages=new QStackedWidget(this); m_pages->setObjectName("billImportPages"); m_pages->setMinimumHeight(0); m_pages->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Ignored); layout->addWidget(m_pages,1);
    auto *start=new QWidget; start->setObjectName("importChoosePage"); auto *startLayout=new QVBoxLayout(start); startLayout->setContentsMargins(0,8,0,8); startLayout->addStretch(); auto *drop=new QFrame(start); drop->setObjectName("importDropCard"); auto *dropLayout=new QVBoxLayout(drop); dropLayout->setContentsMargins(28,24,28,24); dropLayout->setSpacing(13); auto *mark=ShellWidgets::label(QStringLiteral("↥"),drop); mark->setObjectName("importUploadMark"); mark->setAlignment(Qt::AlignCenter); dropLayout->addWidget(mark); auto *title=ShellWidgets::label(QStringLiteral("把账单文件拖到这里"),drop); title->setProperty("importTitle",true); title->setAlignment(Qt::AlignCenter); dropLayout->addWidget(title); auto *hint=ShellWidgets::label(QStringLiteral("支持微信、支付宝导出的 CSV / XLSX，也支持 Kura CSV"),drop,"muted"); hint->setAlignment(Qt::AlignCenter); dropLayout->addWidget(hint); m_choose=new QPushButton(QStringLiteral("选择账单文件"),drop); m_choose->setObjectName("chooseBillFileButton"); m_choose->setProperty("primary",true); dropLayout->addWidget(m_choose,0,Qt::AlignHCenter); startLayout->addWidget(drop); startLayout->addSpacing(12); auto *promise=ShellWidgets::label(QStringLiteral("先预览，再保存。收入与退款会自动排除，重复账单会单独标出。"),start,"muted"); promise->setAlignment(Qt::AlignCenter); startLayout->addWidget(promise); startLayout->addStretch(); m_pages->addWidget(start);
    auto *review=new QWidget; review->setObjectName("importReviewPage"); auto *reviewLayout=new QVBoxLayout(review); reviewLayout->setContentsMargins(0,0,0,0); reviewLayout->setSpacing(10); auto *fileRow=new QHBoxLayout; m_fileName=ShellWidgets::label(QString(),review); m_fileName->setObjectName("importFileNameLabel"); m_fileName->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred); fileRow->addWidget(m_fileName,1); auto *change=quiet(QStringLiteral("更换文件"),"changeBillFileButton",review); fileRow->addWidget(change); reviewLayout->addLayout(fileRow); m_summary=ShellWidgets::label(QString(),review,"muted"); m_summary->setObjectName("billImportSummaryLabel"); reviewLayout->addWidget(m_summary);
    auto *tools=new QHBoxLayout; tools->setSpacing(5); m_newTab=quiet(QStringLiteral("新账单"),"importNewRowsButton",review); m_reviewTab=quiet(QStringLiteral("待核对"),"importNeedsReviewButton",review); m_excludedTab=quiet(QStringLiteral("重复与异常"),"importExcludedRowsButton",review); for(auto *button:{m_newTab,m_reviewTab,m_excludedTab}) { button->setCheckable(true); button->setProperty("importTab",true); tools->addWidget(button); } tools->addStretch(); m_search=new QLineEdit(review); m_search->setObjectName("importSearchEdit"); m_search->setPlaceholderText(QStringLiteral("搜索名称或分类")); m_search->setClearButtonEnabled(true); m_search->setMinimumWidth(120); m_search->setMaximumWidth(220); tools->addWidget(m_search); reviewLayout->addLayout(tools);
    auto *selection=new QHBoxLayout; m_selectAll=new QCheckBox(QStringLiteral("全选当前列表"),review); m_selectAll->setObjectName("importSelectAllCheckBox"); m_selectAll->setTristate(true); selection->addWidget(m_selectAll); selection->addStretch(); m_bulkToggle=quiet(QStringLiteral("批量改分类"),"toggleBillBulkCategoryButton",review); selection->addWidget(m_bulkToggle); m_classify=quiet(QStringLiteral("AI 辅助分类"),"classifyBillsButton",review); m_classify->setToolTip(QStringLiteral("发送当前勾选的前 20 笔名称与金额给你设置的 AI。结果只作建议，点击导入才会保存。")); selection->addWidget(m_classify); m_skipped=quiet(QStringLiteral("查看未导入项"),"showBillSkippedReasonsButton",review); selection->addWidget(m_skipped); reviewLayout->addLayout(selection);
    m_bulkBar=new QFrame(review); m_bulkBar->setObjectName("importBulkBar"); auto *bulk=new QHBoxLayout(m_bulkBar); bulk->setContentsMargins(10,8,10,8); bulk->addWidget(ShellWidgets::label(QStringLiteral("当前列表中已勾选的账单"),m_bulkBar,"muted")); m_bulkCategory=new ModernComboBox(m_bulkBar); m_bulkCategory->setObjectName("bulkBillCategoryComboBox"); for(const auto &key:CategoryCatalog::keys()) m_bulkCategory->addItem(categoryName(key),key); bulk->addWidget(m_bulkCategory,1); m_bulk=quiet(QStringLiteral("应用分类"),"applyBillCategoryButton",m_bulkBar); bulk->addWidget(m_bulk); m_bulkBar->hide(); reviewLayout->addWidget(m_bulkBar);
    m_table=new QTableWidget(0,8,review); m_table->setObjectName("billImportPreviewTable"); m_table->setHorizontalHeaderLabels({QString(),QStringLiteral("日期"),QStringLiteral("时间"),QStringLiteral("消费名称"),QStringLiteral("金额"),QStringLiteral("分类"),QStringLiteral("来源行"),QStringLiteral("核对状态")}); m_table->setItemDelegate(new ImportDelegate(m_table)); m_table->setSelectionBehavior(QAbstractItemView::SelectRows); m_table->setSelectionMode(QAbstractItemView::SingleSelection); m_table->setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed); m_table->verticalHeader()->hide(); m_table->verticalHeader()->setDefaultSectionSize(52); m_table->setShowGrid(false); m_table->setAlternatingRowColors(false); m_table->setMinimumHeight(100); m_table->setColumnHidden(2,true); m_table->setColumnHidden(6,true); auto *header=m_table->horizontalHeader(); header->setSectionResizeMode(QHeaderView::Fixed); m_table->setColumnWidth(0,38); m_table->setColumnWidth(1,106); m_table->setColumnWidth(4,100); m_table->setColumnWidth(5,112); m_table->setColumnWidth(7,106); header->setSectionResizeMode(3,QHeaderView::Stretch); reviewLayout->addWidget(m_table,1); m_empty=ShellWidgets::label(QString(),review,"muted"); m_empty->setObjectName("importEmptyHint"); m_empty->setAlignment(Qt::AlignCenter); reviewLayout->addWidget(m_empty,1); m_empty->hide(); auto *editHint=ShellWidgets::label(QStringLiteral("双击名称修改，点击分类调整。导入按钮会按你当前看到的分类保存。"),review,"muted"); reviewLayout->addWidget(editHint); m_pages->addWidget(review);

    auto *loading=new QWidget; auto *loadingLayout=new QVBoxLayout(loading); loadingLayout->addStretch(); m_loadingHint=ShellWidgets::label(QStringLiteral("正在读取账单…"),loading); m_loadingHint->setProperty("importTitle",true); m_loadingHint->setAlignment(Qt::AlignCenter); loadingLayout->addWidget(m_loadingHint); auto *progress=new QProgressBar(loading); progress->setRange(0,0); progress->setFixedWidth(180); loadingLayout->addWidget(progress,0,Qt::AlignHCenter); loadingLayout->addStretch(); m_pages->addWidget(loading);
    auto *done=new QWidget; done->setObjectName("importDonePage"); auto *doneLayout=new QVBoxLayout(done); doneLayout->addStretch(); auto *tick=ShellWidgets::label(QStringLiteral("✓"),done); tick->setObjectName("importSuccessMark"); tick->setAlignment(Qt::AlignCenter); doneLayout->addWidget(tick); m_doneTitle=ShellWidgets::label(QString(),done); m_doneTitle->setObjectName("importDoneTitle"); m_doneTitle->setProperty("importTitle",true); m_doneTitle->setAlignment(Qt::AlignCenter); doneLayout->addWidget(m_doneTitle); m_doneHint=ShellWidgets::label(QString(),done,"muted"); m_doneHint->setAlignment(Qt::AlignCenter); doneLayout->addWidget(m_doneHint); doneLayout->addStretch(); m_pages->addWidget(done);
    m_error=ShellWidgets::label(QString(),this,"error"); m_error->setObjectName("billImportErrorLabel"); m_error->hide(); layout->addWidget(m_error);
    auto *footer=new QFrame(this); footer->setObjectName("importFooter"); auto *bottom=new QHBoxLayout(footer); bottom->setContentsMargins(0,12,0,0); bottom->setSpacing(10); auto *totals=new QVBoxLayout; totals->setSpacing(3); m_footerSummary=ShellWidgets::label(QString(),footer); m_footerSummary->setObjectName("importFooterSummary"); m_footerHint=ShellWidgets::label(QString(),footer,"muted"); m_footerHint->setObjectName("importFooterHint"); totals->addWidget(m_footerSummary); totals->addWidget(m_footerHint); bottom->addLayout(totals,1);
    m_close=quiet(QStringLiteral("取消"),"closeBillImportButton",footer); m_cancelWork=quiet(QStringLiteral("停止"),"cancelBillImportWorkButton",footer); m_again=quiet(QStringLiteral("再导入一份"),"importAnotherFileButton",footer); m_import=new QPushButton(QString(),footer); m_import->setObjectName("commitBillImportButton"); m_import->setProperty("primary",true); for(auto *button:{m_close,m_cancelWork,m_again,m_import}) bottom->addWidget(button); layout->addWidget(footer);
    connect(m_choose,&QPushButton::clicked,this,&BillImportDialog::chooseFile); connect(change,&QPushButton::clicked,this,&BillImportDialog::chooseFile); connect(m_close,&QPushButton::clicked,this,&BillImportDialog::reject); connect(m_import,&QPushButton::clicked,this,&BillImportDialog::commit); connect(m_skipped,&QPushButton::clicked,this,&BillImportDialog::showSkipped); connect(m_reload,&QPushButton::clicked,this,[this] { if(!m_path.isEmpty()) loadFile(m_path); });
    m_reloadTimer=new QTimer(this); m_reloadTimer->setSingleShot(true); m_reloadTimer->setInterval(200); connect(m_reloadTimer,&QTimer::timeout,this,[this] { if(!m_path.isEmpty()) loadFile(m_path); });
    const auto changed=[this] { if(!m_path.isEmpty() && !m_done) { m_needsReload=true; m_reloadTimer->start(); } updateState(); };
    connect(m_platform,&QComboBox::currentIndexChanged,this,changed); connect(m_transfers,&QCheckBox::toggled,this,changed); connect(m_recent,&QCheckBox::toggled,this,&BillImportDialog::applyFilters); connect(m_search,&QLineEdit::textChanged,this,&BillImportDialog::applyFilters);
    for(int i=0;i<3;++i) { auto *button=QList<QPushButton *>{m_newTab,m_reviewTab,m_excludedTab}[i]; connect(button,&QPushButton::clicked,this,[this,i] { m_filter=i; applyFilters(); }); }
    connect(m_selectAll,&QCheckBox::clicked,this,[this](bool checked) { for(int i=0;i<m_preview.rows.size();++i) if(!m_table->isRowHidden(i) && allowed(m_preview.rows[i])) { m_preview.rows[i].selected=checked; const QSignalBlocker blocker(m_table); m_table->item(i,0)->setCheckState(checked?Qt::Checked:Qt::Unchecked); } updateState(); });
    connect(m_bulkToggle,&QPushButton::clicked,this,[this] { m_bulkBar->setVisible(m_bulkBar->isHidden()); }); connect(m_bulk,&QPushButton::clicked,this,&BillImportDialog::applyCategory); connect(m_classify,&QPushButton::clicked,this,&BillImportDialog::classify);
    connect(m_table,&QTableWidget::itemChanged,this,[this](QTableWidgetItem *item) { if(m_loading || item->row()<0 || item->row()>=m_preview.rows.size()) return; auto &row=m_preview.rows[item->row()]; if(!allowed(row)) return; if(item->column()==0) row.selected=item->checkState()==Qt::Checked; else if(item->column()==3) row.name=item->text().trimmed(); else if(item->column()==5) { row.category=item->data(Qt::UserRole).toString(); row.confirmed=true; } updateState(); });
    connect(m_cancelWork,&QPushButton::clicked,this,[this] { if(m_worker) m_worker->requestInterruption(); m_ai.cancel(); m_aiRunning=false; updateState(); });
    connect(m_again,&QPushButton::clicked,this,[this] { m_done=false; m_needsReload=true; m_preview={}; m_path.clear(); m_aiSuggested.clear(); m_filter=0; m_search->clear(); m_error->hide(); m_table->setRowCount(0); updateState(); });
    connect(&m_ai,&AIService::failed,this,[this](const QString &message) { m_aiRunning=false; m_error->setText(message); m_error->show(); updateState(); });
    connect(&m_ai,&AIService::classified,this,[this](const BillClassifications &result) { m_aiRunning=false; for(auto it=result.categories.cbegin();it!=result.categories.cend();++it) if(it.key()>=0 && it.key()<m_preview.rows.size()) { auto &row=m_preview.rows[it.key()]; row.category=it.value(); row.confirmed=false; m_aiSuggested.insert(it.key()); } render(); }); updateState();
}
BillImportDialog::~BillImportDialog() { if(m_worker) m_worker->requestInterruption(); m_ai.cancel(); }
void BillImportDialog::reject() { m_closed=true; m_reloadTimer->stop(); if(m_worker) m_worker->requestInterruption(); m_ai.cancel(); QDialog::reject(); }
void BillImportDialog::chooseFile() { const auto path=QFileDialog::getOpenFileName(this,QStringLiteral("选择账单文件"),{},QStringLiteral("账单 (*.csv *.xlsx)")); if(!path.isEmpty()) loadFile(path); }
void BillImportDialog::dragEnterEvent(QDragEnterEvent *event) { if(!m_worker && !m_aiRunning && !m_done && event->mimeData()->hasUrls()) { const auto urls=event->mimeData()->urls(); if(urls.size()==1 && urls.first().isLocalFile()) { const auto suffix=QFileInfo(urls.first().toLocalFile()).suffix().toLower(); if(suffix=="csv" || suffix=="xlsx") event->acceptProposedAction(); } } }
void BillImportDialog::dropEvent(QDropEvent *event) { if(event->mimeData()->hasUrls() && event->mimeData()->urls().size()==1 && loadFile(event->mimeData()->urls().first().toLocalFile())) event->acceptProposedAction(); }
bool BillImportDialog::loadFile(const QString &path)
{
    if (m_worker || m_aiRunning || m_done) return false; m_reloadTimer->stop(); m_closed=false; m_path=path; m_needsReload=true; m_error->hide(); m_loadingHint->setText(QStringLiteral("正在读取 %1…").arg(QFileInfo(path).fileName()));
    struct Result { BillParseResult data; QString error; bool ok=false; }; auto result=std::make_shared<Result>(); const auto platform=m_platform->currentData().toString(); const bool transfers=m_transfers->isChecked();
    auto *thread=QThread::create([result,path,platform,transfers] { result->ok=BillParser::parseFile(path,result->data,result->error,platform,transfers,[] { return QThread::currentThread()->isInterruptionRequested(); }); }); m_worker=thread;
    connect(thread,&QThread::finished,this,[this,result,thread] {
        if (m_worker!=thread) return; m_worker=nullptr; if (m_closed) return; if (thread->isInterruptionRequested()) { result->ok=false; result->error=QStringLiteral("已取消读取。"); }
        if (result->ok && result->data.rows.size()>10000) { result->ok=false; result->error=QStringLiteral("单次最多预览 10,000 笔有效支出，请拆分账单后导入。"); }
        if (result->ok && !m_repository.prepare(result->data)) { result->ok=false; result->error=m_repository.lastError(); }
        if (!result->ok) { m_error->setText(result->error+QStringLiteral(" 原预览未写入数据库。")); m_error->show(); if(m_platform->currentData().toString().isEmpty()) m_options->show(); updateState(); emit previewFailed(result->error); return; }
        m_preview=result->data; m_needsReload=false; m_filter=0; m_search->clear(); m_aiSuggested.clear(); m_options->hide(); m_bulkBar->hide(); render(); emit previewReady();
    }); connect(thread,&QThread::finished,thread,&QObject::deleteLater); updateState(); thread->start(); return true;
}
bool BillImportDialog::inRange(const BillDraft &row) const { if (!m_recent->isChecked()) return true; const auto today=QDate::currentDate(); const auto first=QDate(today.year(),today.month(),1); return row.date>=first.addMonths(-5) && row.date<=QDate(today.year(),today.month(),today.daysInMonth()); }

bool BillImportDialog::matchesView(const BillDraft &row) const {
    if(!inRange(row)) return false; const bool excluded=!allowed(row) || row.state==BillDraft::PossibleDuplicate;
    if(m_filter==2 ? !excluded : excluded) return false;
    if(m_filter==1 && row.confirmed && !row.name.trimmed().isEmpty() && row.name.size()<=100) return false;
    const auto text=m_search->text().trimmed(); return text.isEmpty() || row.name.contains(text,Qt::CaseInsensitive) || categoryName(row.category).contains(text,Qt::CaseInsensitive) || row.date.toString(Qt::ISODate).contains(text);
}
void BillImportDialog::render()
{
    m_loading=true; const QSignalBlocker blocker(m_table); m_table->setUpdatesEnabled(false); m_table->setRowCount(0); m_table->setRowCount(m_preview.rows.size());
    for(int i=0;i<m_preview.rows.size();++i) { const auto &row=m_preview.rows[i]; auto *selected=new QTableWidgetItem; selected->setFlags(allowed(row)?Qt::ItemIsUserCheckable|Qt::ItemIsEnabled|Qt::ItemIsSelectable:Qt::ItemIsSelectable); selected->setCheckState(row.selected?Qt::Checked:Qt::Unchecked); m_table->setItem(i,0,selected);
        const QStringList values={row.date.toString("yyyy-MM-dd"),row.time.isValid()?row.time.toString("HH:mm:ss"):QStringLiteral("未记录"),row.name,Transaction::formatAmount(row.cents)};
        for(int j=0;j<values.size();++j) { auto *cell=new QTableWidgetItem(values[j]); cell->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|((j==2 && allowed(row))?Qt::ItemIsEditable:Qt::NoItemFlags)); cell->setToolTip(QStringLiteral("%1 %2\n%3\n原始文件第 %4 行").arg(row.date.toString(Qt::ISODate),values[1],row.originalName).arg(row.sourceRow)); if(j==3) cell->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter); m_table->setItem(i,j+1,cell); }
        auto *category=new QTableWidgetItem(categoryName(row.category)); category->setData(Qt::UserRole,row.category); category->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|(allowed(row)?Qt::ItemIsEditable:Qt::NoItemFlags)); m_table->setItem(i,5,category);
        auto *line=new QTableWidgetItem(QString::number(row.sourceRow)); line->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable); m_table->setItem(i,6,line); auto *status=new QTableWidgetItem; status->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable); m_table->setItem(i,7,status);
    } m_table->setUpdatesEnabled(true); m_loading=false; applyFilters();
}
void BillImportDialog::applyFilters() { for(int i=0;i<m_preview.rows.size();++i) m_table->setRowHidden(i,!matchesView(m_preview.rows[i])); updateState(); }
void BillImportDialog::updateState()
{
    const bool busy=m_worker || m_aiRunning; int selected=0,unconfirmed=0,invalid=0,duplicate=0,possible=0,conflict=0,fresh=0,review=0,visible=0,visibleSelected=0; qint64 sum=0;
    m_loading=true; const QSignalBlocker blocker(m_table);
    for(int i=0;i<m_preview.rows.size();++i) { const auto &row=m_preview.rows[i]; const bool valid=!row.name.trimmed().isEmpty() && row.name.size()<=100;
        if(row.state==BillDraft::Duplicate) ++duplicate; if(row.state==BillDraft::PossibleDuplicate) ++possible; if(row.state==BillDraft::Conflict) ++conflict;
        if(allowed(row) && row.state!=BillDraft::PossibleDuplicate && inRange(row)) { ++fresh; if(!row.confirmed || !valid) ++review; }
        if(row.selected && inRange(row)) { ++selected; sum+=row.cents; if(!row.confirmed) ++unconfirmed; if(!valid || !allowed(row)) ++invalid; }
        if(!m_table->isRowHidden(i) && allowed(row)) { ++visible; if(row.selected) ++visibleSelected; }
        if(auto *status=m_table->item(i,7)) { status->setText(row.state==BillDraft::Duplicate?QStringLiteral("已导入"):row.state==BillDraft::Conflict?QStringLiteral("信息冲突"):row.state==BillDraft::PossibleDuplicate?QStringLiteral("疑似重复"):!valid?QStringLiteral("请修改名称"):row.confirmed?QStringLiteral("已核对"):QStringLiteral("待核对")); status->setToolTip(!valid?QStringLiteral("名称须为 1—100 个字"):row.reason); }
    } m_loading=false;
    int shown=0; for(int i=0;i<m_table->rowCount();++i) if(!m_table->isRowHidden(i)) ++shown;
    m_table->setVisible(shown>0); m_empty->setVisible(shown==0);
    m_empty->setText(!m_search->text().isEmpty()?QStringLiteral("没有找到匹配账单，换个关键词试试。"):fresh==0 && duplicate>0?QStringLiteral("这份账单已经导入过，无需重复保存。\n可以切到“重复与异常”查看明细。"):m_filter==1?QStringLiteral("没有需要核对的分类，可以直接导入所选账单。"):QStringLiteral("这里没有可导入的支出。\n收入、退款与无效记录已自动排除。"));
    m_fileName->setText(m_preview.fileName); m_fileName->setToolTip(m_preview.fileName); m_summary->setText(QStringLiteral("新账单 %1 笔  ·  待核对 %2 笔  ·  已重复 %3 笔").arg(fresh).arg(review).arg(duplicate));
    m_newTab->setText(QStringLiteral("新账单 %1").arg(fresh)); m_reviewTab->setText(QStringLiteral("待核对 %1").arg(review)); m_excludedTab->setText(QStringLiteral("重复与异常 %1").arg(duplicate+possible+conflict));
    const QList<QPushButton *> tabs{m_newTab,m_reviewTab,m_excludedTab}; for(int i=0;i<tabs.size();++i) { tabs[i]->setChecked(m_filter==i); tabs[i]->setEnabled(!busy && !m_done); }
    { const QSignalBlocker selectBlocker(m_selectAll); m_selectAll->setCheckState(visibleSelected==0?Qt::Unchecked:visibleSelected==visible?Qt::Checked:Qt::PartiallyChecked); } m_selectAll->setEnabled(!busy && !m_done && visible>0);
    for(auto *widget:QList<QWidget *>{m_platform,m_transfers,m_recent,m_choose,m_bulkCategory,m_search}) widget->setEnabled(!busy && !m_done);
    m_table->setEnabled(!busy && !m_done); m_reload->setEnabled(!busy && !m_done && !m_path.isEmpty()); m_bulk->setEnabled(!busy && !m_done && !m_needsReload && visibleSelected>0); m_bulkToggle->setEnabled(!busy && visibleSelected>0); m_classify->setEnabled(!busy && !m_done && !m_needsReload && selected>0);
    m_bulkToggle->setVisible(visibleSelected>0); m_classify->setVisible(selected>0);
    if(!visibleSelected) m_bulkBar->hide();
    m_skipped->setVisible(m_preview.filtered+m_preview.invalid>0); m_skipped->setText(QStringLiteral("未导入 %1 笔 ↗").arg(m_preview.filtered+m_preview.invalid));
    const bool hasPreview=!m_preview.fileName.isEmpty(); m_pages->setCurrentIndex(m_done?3:m_worker?2:hasPreview?1:0);
    for(int i=0;i<m_steps.size();++i) { m_steps[i]->setProperty("activeStep",i==(m_done?2:hasPreview?1:0)); m_steps[i]->style()->unpolish(m_steps[i]); m_steps[i]->style()->polish(m_steps[i]); }
    m_import->setVisible(hasPreview || m_done); m_import->setEnabled(m_done || (!busy && !m_needsReload && selected>0 && invalid==0)); m_import->setText(m_done?QStringLiteral("完成"):QStringLiteral("导入 %1 笔").arg(selected)); m_again->setVisible(m_done); m_close->setVisible(!m_done); m_cancelWork->setVisible(busy); m_cancelWork->setEnabled(busy);
    m_footerSummary->setText(m_done?QStringLiteral("已保存到你的账本"):hasPreview?QStringLiteral("将导入 %1 笔 · %2").arg(selected).arg(Transaction::formatAmount(sum)):QStringLiteral("文件只在本机读取"));
    m_footerHint->setText(m_done?QStringLiteral("关闭窗口后仍会保留"):m_aiRunning?QStringLiteral("正在获取分类建议，尚未写入账本…"):m_worker?QStringLiteral("正在读取，尚未写入账本…"):m_needsReload && hasPreview?QStringLiteral("请重新读取文件后再导入"):invalid?QStringLiteral("请先修改所选账单的无效名称"):hasPreview && selected==0?QStringLiteral("当前没有勾选需要导入的账单"):hasPreview?unconfirmed?QStringLiteral("含 %1 笔分类建议；点击导入即按当前分类保存").arg(unconfirmed):QStringLiteral("点击导入后写入账本"):QStringLiteral("不会自动上传或保存原始文件"));
}
void BillImportDialog::applyCategory()
{
    for(int i=0;i<m_preview.rows.size();++i) { auto &row=m_preview.rows[i]; if(row.selected && !m_table->isRowHidden(i) && allowed(row)) { row.category=m_bulkCategory->currentData().toString(); row.confirmed=true; } } m_bulkBar->hide(); render();
}
void BillImportDialog::commit()
{
    if(m_done) { accept(); return; } if(!m_import->isEnabled()) return; m_table->setCurrentCell(-1,-1); m_import->setEnabled(false);
    auto preview=m_preview; for(auto &row:preview.rows) { if(!inRange(row)) row.selected=false; if(row.selected) row.confirmed=true; }
    BillImportOutcome outcome; if(!m_repository.commit(preview,outcome)) { m_error->setText(m_repository.lastError()+QStringLiteral(" 整批未保存，预览和选择已保留。")); m_error->show(); updateState(); return; }
    m_done=true; m_doneTitle->setText(QStringLiteral("已导入 %1 笔账单").arg(outcome.inserted)); m_doneHint->setText(outcome.alreadyImported?QStringLiteral("另有 %1 笔已经导入，本次没有重复保存。").arg(outcome.alreadyImported):QStringLiteral("你的消费记录已保存。现在可以回到账本查看。")); m_error->hide(); updateState(); emit imported(outcome.inserted);
}
void BillImportDialog::showSkipped()
{
    auto *dialog=new QDialog(this); dialog->setWindowTitle(QStringLiteral("这些记录没有导入")); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->resize(600,440); auto *layout=new QVBoxLayout(dialog); layout->addWidget(ShellWidgets::label(QStringLiteral("收入、退款和无效记录已排除。重复与冲突账单可在“重复与异常”中查看。"),dialog,"muted")); auto *text=ShellWidgets::label(m_preview.skippedReasons.isEmpty()?QStringLiteral("没有其他未导入项。"):m_preview.skippedReasons.mid(0,300).join('\n'),dialog); auto *scroll=new QScrollArea(dialog); scroll->setWidgetResizable(true); scroll->setWidget(text); layout->addWidget(scroll,1); auto *close=quiet(QStringLiteral("知道了"),"closeBillSkippedButton",dialog); layout->addWidget(close,0,Qt::AlignRight); connect(close,&QPushButton::clicked,dialog,&QDialog::accept); dialog->open();
}
void BillImportDialog::classify()
{
    if (m_worker || m_aiRunning || m_needsReload || m_done) return; QJsonArray items;
    for (int i=0;i<m_preview.rows.size() && items.size()<20;++i) { const auto &row=m_preview.rows[i]; if (row.selected && inRange(row) && allowed(row) && !m_aiSuggested.contains(i) && !row.name.trimmed().isEmpty() && row.name.size()<=100) items.append(QJsonObject{{"id",QString::number(i)},{"name",row.name},{"amount",Transaction::formatAmount(row.cents)}}); }
    if (items.isEmpty()) { m_error->setText(QStringLiteral("没有尚未分析的勾选条目。可直接确认或手动选择分类。")); m_error->show(); return; }
    m_aiRunning=true; m_error->hide(); updateState(); if (!m_ai.classify(AISettingsStore::load(m_settingsPath),items)) { m_aiRunning=false; updateState(); }
}
