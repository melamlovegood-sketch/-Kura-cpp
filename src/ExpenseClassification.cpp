#include "CategoryWidgets.h"
#include "ExpenseClassification.h"
#include "CategoryCatalog.h"
#include "ModernSelectors.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

ExpenseClassification::ExpenseClassification(const QString &settingsPath,QWidget *parent)
    :QWidget(parent),m_settingsPath(settingsPath) {
    setObjectName("expenseClassification");
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(8);
    auto *modes=new QHBoxLayout; auto *group=new QButtonGroup(this);
    m_byPurpose=new QPushButton(QStringLiteral("按用途记"),this); m_byPurpose->setObjectName("expenseByPurposeButton");
    m_byBudget=new QPushButton(QStringLiteral("按预算记"),this); m_byBudget->setObjectName("expenseByBudgetButton");
    for (auto *button:{m_byPurpose,m_byBudget}) { button->setCheckable(true); button->setProperty("billingSegment",true); group->addButton(button); modes->addWidget(button); }
    modes->addStretch(); layout->addLayout(modes);
    m_fields=new QVBoxLayout; m_fields->setSpacing(8); layout->addLayout(m_fields);
    m_purposePanel=new QWidget(this); auto *purposeRow=new QHBoxLayout(m_purposePanel); purposeRow->setContentsMargins(0,0,0,0);
    purposeRow->addWidget(new QLabel(QStringLiteral("消费用途"),m_purposePanel));
    m_purpose=new ModernComboBox(m_purposePanel); m_purpose->setObjectName("expensePurposeComboBox");
    CategoryWidgets::bind(m_purpose,true,settingsPath);
    m_purpose->setCurrentIndex(m_purpose->findData("unclassified")); purposeRow->addWidget(m_purpose,1);
    m_scopePanel=new QWidget(this); auto *scopeRow=new QHBoxLayout(m_scopePanel); scopeRow->setContentsMargins(0,0,0,0);
    scopeRow->addWidget(new QLabel(QStringLiteral("预算归属"),m_scopePanel)); auto *scopes=new QButtonGroup(this);
    m_basic=new QPushButton(QStringLiteral("必要生活"),m_scopePanel); m_basic->setObjectName("expenseBasicButton");
    m_free=new QPushButton(QStringLiteral("自由消费"),m_scopePanel); m_free->setObjectName("expenseFreeButton");
    m_basic->setToolTip(QStringLiteral("为基本吃饭、通勤、日用品等预留的预算")); m_free->setToolTip(QStringLiteral("用于自己决定的非必需消费，也可以留着不花"));
    for (auto *button:{m_basic,m_free}) { button->setCheckable(true); button->setProperty("billingSegment",true); scopes->addButton(button); scopeRow->addWidget(button,1); }
    m_free->setChecked(true);
    m_more=new QPushButton(QStringLiteral("补充用途 ▾"),this); m_more->setObjectName("expenseMorePurposeButton"); m_more->setProperty("billingQuiet",true); layout->addWidget(m_more,0,Qt::AlignLeft);
    connect(m_byPurpose,&QPushButton::clicked,this,[this] { setBudgetMode(false,true); });
    connect(m_byBudget,&QPushButton::clicked,this,[this] { setBudgetMode(true,true); });
    connect(m_basic,&QPushButton::clicked,this,[this] { setScope(true); }); connect(m_free,&QPushButton::clicked,this,[this] { setScope(false); });
    connect(m_more,&QPushButton::clicked,this,[this] { m_purposePanel->setVisible(m_purposePanel->isHidden()); updateGeometry(); });
    connect(m_purpose,&QComboBox::currentIndexChanged,this,[this] { emit changed(); });
    bool budget=false;
    if (!m_settingsPath.isEmpty()) { QSettings settings(m_settingsPath,QSettings::IniFormat); budget=settings.value("bookkeeping/budgetMode",false).toBool(); }
    setBudgetMode(budget,false);
}
QString ExpenseClassification::purpose() const { return m_purpose->currentData().toString(); }
QString ExpenseClassification::scope() const { return m_basic->isChecked()?QStringLiteral("basic"):QStringLiteral("discretionary"); }
void ExpenseClassification::setScope(bool basic) { m_basic->setChecked(basic); m_free->setChecked(!basic); emit changed(); }
void ExpenseClassification::setValues(const QString &purpose,const QString &scope) {
    CategoryWidgets::select(m_purpose,CategoryCatalog::validPurpose(purpose)?purpose:QStringLiteral("unclassified"),true); setScope(scope=="basic");
    m_purposePanel->setVisible(!m_budgetMode || this->purpose()!="unclassified");
}
void ExpenseClassification::setBudgetMode(bool budget,bool persist) {
    m_budgetMode=budget; m_byPurpose->setChecked(!budget); m_byBudget->setChecked(budget);
    m_fields->removeWidget(m_purposePanel); m_fields->removeWidget(m_scopePanel);
    m_fields->addWidget(budget?m_scopePanel:m_purposePanel); m_fields->addWidget(budget?m_purposePanel:m_scopePanel);
    m_more->setVisible(budget); m_purposePanel->setVisible(!budget || purpose()!="unclassified");
    if (persist && !m_settingsPath.isEmpty()) { QSettings settings(m_settingsPath,QSettings::IniFormat); settings.setValue("bookkeeping/budgetMode",budget); }
    updateGeometry(); emit changed(); emit entryModeChanged(budget);
}
