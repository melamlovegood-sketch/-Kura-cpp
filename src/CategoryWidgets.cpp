#include "CategoryWidgets.h"
#include "CategoryCatalog.h"
#include "CategoryRepository.h"
#include "DatabaseManager.h"
#include "ShellWidgets.h"
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QVBoxLayout>

namespace {
class Binding:public QObject {
public:
    Binding(QComboBox *combo,bool purpose,const QString &path):QObject(combo),m_combo(combo),m_purpose(purpose),m_path(path) {
        for(int i=0;i<combo->count();++i)m_prefix.append({combo->itemText(i),combo->itemData(i)});
        combo->installEventFilter(this);refresh();
    }
    void refresh(const QString &oldKey={},const QString &newKey={}) {
        auto selected=m_combo->currentData().toString();
        if(!oldKey.isEmpty() && selected==oldKey)selected=newKey;
        const bool hadSelection=m_combo->currentIndex()>=0;
        if(!m_purpose && !selected.isEmpty())selected=CategoryCatalog::productKey(selected);
        QList<UserCategory> custom;
        QString connection;
        for(auto *w=static_cast<QWidget *>(m_combo);w;w=w->parentWidget())if(w->property("categoryDatabase").isValid()){connection=w->property("categoryDatabase").toString();break;}
        if(connection.isEmpty() && !m_path.isEmpty()) {
            auto path=m_path; if(path.endsWith(".ui.ini"))path.chop(7);
            for(const auto &name:QSqlDatabase::connectionNames())if(QSqlDatabase::database(name,false).databaseName()==path){connection=name;break;}
        }
        if(QSqlDatabase::contains(connection)) {CategoryRepository repository(QSqlDatabase::database(connection,false));repository.load(custom,m_combo->property("categoryIncludeInactive").toBool());}
        QSignalBlocker blocker(m_combo);m_combo->clear();
        for(const auto &prefix:m_prefix)m_combo->addItem(prefix.first,prefix.second);
        for(const auto &key:CategoryCatalog::defaultKeys())m_combo->addItem(CategoryCatalog::purposeLabel(key),key);
        for(const auto &category:custom)m_combo->addItem(category.name+(category.active?QString():QStringLiteral("（已停用）")),category.key);
        if(m_purpose)m_combo->addItem(QStringLiteral("未分类"),"unclassified");
        if(CategoryCatalog::isCustom(selected) && m_combo->findData(selected)<0)m_combo->addItem(CategoryCatalog::label(selected)+QStringLiteral("（已停用）"),selected);
        int index=m_combo->findData(selected);
        if(index<0)index=m_prefix.isEmpty()?m_combo->findData(m_purpose?"unclassified":"other"):0;
        m_combo->setCurrentIndex(index);
        if(hadSelection && !oldKey.isEmpty() && oldKey!=newKey && selected==newKey) {
            blocker.unblock();emit m_combo->currentIndexChanged(index);
        }
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::Show || event->type()==QEvent::MouseButtonPress || event->type()==QEvent::FocusIn)refresh();
        return QObject::eventFilter(object,event);
    }
private:
    QComboBox *m_combo;bool m_purpose;QString m_path;
    QList<QPair<QString,QVariant>> m_prefix;
};
Binding *binding(QComboBox *combo){for(auto *child:combo->children())if(auto *b=dynamic_cast<Binding *>(child))return b;return nullptr;}
}
void CategoryWidgets::bind(QComboBox *combo,bool purpose,const QString &path){if(!binding(combo))new Binding(combo,purpose,path);}
void CategoryWidgets::select(QComboBox *combo,const QString &key,bool purpose) {
    if(auto *b=binding(combo))b->refresh();
    const auto selected=purpose?key:CategoryCatalog::productKey(key);
    if(CategoryCatalog::isCustom(selected) && combo->findData(selected)<0)combo->addItem(CategoryCatalog::label(selected)+QStringLiteral("（已停用）"),selected);
    combo->setCurrentIndex(combo->findData(selected));
}
void CategoryWidgets::refreshAll(QWidget *root,const QString &oldKey,const QString &newKey) {
    for(auto *combo:root->findChildren<QComboBox *>())if(auto *b=binding(combo))b->refresh(oldKey,newKey);
}
CategorySettingsWidget::CategorySettingsWidget(DatabaseManager *database,QWidget *parent):QWidget(parent),m_database(database)
{
    setObjectName("categorySettingsWidget");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(16);
    auto *defaults=ShellWidgets::label(QStringLiteral("默认分类优先显示"),this,"heading");layout->addWidget(defaults);
    layout->addWidget(ShellWidgets::label(QStringLiteral("吃喝  ·  玩乐  ·  服饰鞋包  ·  交通  ·  其他"),this));
    layout->addWidget(ShellWidgets::label(QStringLiteral("默认五类始终保留。你自己的分类会排在后面，记账、清单和商品核对共用。"),this,"muted"));
    auto *addRow=new QHBoxLayout;m_name=new QLineEdit(this);m_name->setObjectName("newCategoryNameEdit");m_name->setMaxLength(20);m_name->setPlaceholderText(QStringLiteral("例如：宠物、书籍、运动"));
    auto *add=new QPushButton(QStringLiteral("添加分类"),this);add->setObjectName("addCategoryButton");add->setProperty("primary",true);addRow->addWidget(m_name,1);addRow->addWidget(add);layout->addLayout(addRow);
    m_error=ShellWidgets::label({},this,"error");m_error->setObjectName("categorySettingsError");layout->addWidget(m_error);
    m_rows=new QVBoxLayout;m_rows->setSpacing(12);layout->addLayout(m_rows);
    layout->addWidget(ShellWidgets::label(QStringLiteral("重命名会同步更新已有记录的分类名称。停用只让分类退出新建选项，已有记录与统计会继续保留，也可重新启用。"),this,"muted"));
    add->setEnabled(database!=nullptr);connect(add,&QPushButton::clicked,this,[this]{
        if(!m_database)return;CategoryRepository repository(m_database->database());
        if(!repository.add(m_name->text())){m_error->setText(repository.lastError());return;}
        m_name->clear();m_error->clear();refresh();emit categoriesChanged({},{});
    });
}
void CategorySettingsWidget::showEvent(QShowEvent *event){QWidget::showEvent(event);refresh();}
void CategorySettingsWidget::refresh()
{
    if(!m_database || !m_database->database().isOpen())return;
    QList<UserCategory> categories;CategoryRepository repository(m_database->database());
    if(!repository.load(categories,true)){m_error->setText(repository.lastError());return;}
    while(auto *item=m_rows->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    if(categories.isEmpty())m_rows->addWidget(ShellWidgets::label(QStringLiteral("还没有自定义分类，先用默认五类也很好。"),this,"muted"));
    for(const auto &category:categories) {
        auto *row=new QFrame(this);row->setProperty("card",true);auto *line=new QHBoxLayout(row);line->setContentsMargins(14,12,14,12);line->setSpacing(10);
        auto *name=new QLineEdit(category.name,row);name->setMaxLength(20);name->setObjectName(QStringLiteral("categoryName_%1").arg(category.id));line->addWidget(name,1);
        auto *save=new QPushButton(QStringLiteral("保存名称"),row);save->setObjectName(QStringLiteral("renameCategory_%1").arg(category.id));line->addWidget(save);
        auto *toggle=new QPushButton(category.active?QStringLiteral("停用"):QStringLiteral("重新启用"),row);toggle->setObjectName(QStringLiteral("toggleCategory_%1").arg(category.id));line->addWidget(toggle);
        connect(save,&QPushButton::clicked,this,[this,category,name]{CategoryRepository repository(m_database->database());const auto updated=CategoryCatalog::customKey(name->text());if(!repository.rename(category.key,name->text())){m_error->setText(repository.lastError());return;}m_error->clear();refresh();emit categoriesChanged(category.key,updated);});
        connect(toggle,&QPushButton::clicked,this,[this,category]{CategoryRepository repository(m_database->database());if(!repository.setActive(category.key,!category.active)){m_error->setText(repository.lastError());return;}m_error->clear();refresh();emit categoriesChanged({},{});});
        m_rows->addWidget(row);
    }
}
