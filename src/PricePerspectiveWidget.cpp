#include "PricePerspectiveWidget.h"
#include "ShellWidgets.h"
#include "AppTheme.h"
#include "EmptyAmountSpinBox.h"
#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>

namespace {
QDoubleSpinBox *amount(QWidget *parent,const char *name,const QString &hint=QStringLiteral("未填写（元）")) {
    auto *field=new EmptyAmountSpinBox(parent,hint); field->setObjectName(QLatin1String(name)); field->setRange(0,99999999.99); field->setDecimals(2); field->setButtonSymbols(QAbstractSpinBox::NoButtons); field->setSingleStep(10); return field;
}
}
PricePerspectiveWidget::PricePerspectiveWidget(const QString &settingsPath,QWidget *parent,bool compact):QWidget(parent),m_settingsPath(settingsPath) {
    setProperty("onboardingCompact",compact);
    setObjectName("pricePerspectiveWidget"); setProperty("page",true); setAttribute(Qt::WA_StyledBackground); auto *layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(16);
    layout->addWidget(ShellWidgets::label(QStringLiteral("把价格换算成你熟悉的尺度。只作参考，不改变任何预算或账单。"),this,"muted"));
    const auto p=PricePerspectiveStore::load(settingsPath); m_mode=p.mode;
    auto *choices=new QHBoxLayout; auto *group=new QButtonGroup(this);
    const QStringList labels={QStringLiteral("不开启"),QStringLiteral("学生"),QStringLiteral("工作党")};
    const QStringList names={"pricePerspectiveOffButton","pricePerspectiveStudentButton","pricePerspectiveWorkerButton"};
    for (int i=0;i<3;++i) { auto *button=new QPushButton(labels[i],this); button->setObjectName(names[i]); button->setCheckable(true); button->setProperty("billingSegment",true); group->addButton(button,i); button->setChecked(int(m_mode)==i); choices->addWidget(button,1); connect(button,&QPushButton::clicked,this,[this,i] { m_mode=PricePerspective::Mode(i); m_status->clear(); updatePreview(); }); } layout->addLayout(choices);
    m_student=new QWidget(this); auto *student=new QFormLayout(m_student); student->setContentsMargins(0,0,0,0); student->setSpacing(12);
    m_food=amount(m_student,"pricePerspectiveDailyFoodSpinBox"); m_food->setValue(double(p.dailyFoodCents)/100);
    m_meals=new QSpinBox(m_student); m_meals->setObjectName("pricePerspectiveMealsSpinBox"); m_meals->setRange(1,10); m_meals->setButtonSymbols(QAbstractSpinBox::NoButtons); m_meals->setSuffix(QStringLiteral(" 顿")); m_meals->setValue(p.mealsPerDay);
    student->addRow(QStringLiteral("每日伙食预算（元）"),m_food); student->addRow(QStringLiteral("每天几顿饭"),m_meals); layout->addWidget(m_student);
    m_worker=new QWidget(this); auto *worker=new QFormLayout(m_worker); worker->setContentsMargins(0,0,0,0); worker->setSpacing(12);
    m_salary=amount(m_worker,"pricePerspectiveSalarySpinBox"); m_salary->setValue(double(p.monthlyIncomeCents)/100); m_salary->setSingleStep(1000);
    m_days=new QDoubleSpinBox(m_worker); m_days->setObjectName("pricePerspectiveWorkDaysSpinBox"); m_days->setRange(1,31); m_days->setButtonSymbols(QAbstractSpinBox::NoButtons); m_days->setDecimals(2); m_days->setSuffix(QStringLiteral(" 天")); m_days->setValue(p.workDaysPerMonth);
    m_hours=new QDoubleSpinBox(m_worker); m_hours->setObjectName("pricePerspectiveHoursSpinBox"); m_hours->setRange(0.5,24); m_hours->setButtonSymbols(QAbstractSpinBox::NoButtons); m_hours->setDecimals(2); m_hours->setSingleStep(0.5); m_hours->setSuffix(QStringLiteral(" 小时")); m_hours->setValue(p.dailyWorkHours);
    worker->addRow(QStringLiteral("月到手工资（元）"),m_salary); worker->addRow(QStringLiteral("每月工作天数"),m_days); worker->addRow(QStringLiteral("每天工作时长"),m_hours); layout->addWidget(m_worker);
    QFrame *preview; auto *previewLayout=ShellWidgets::card(this,&preview); m_preview=preview; preview->setObjectName("pricePerspectivePreview");
    m_reference=ShellWidgets::label(QString(),preview); m_reference->setObjectName("pricePerspectiveReferenceLabel"); previewLayout->addWidget(m_reference);
    auto *sampleRow=new QFormLayout; m_sample=amount(preview,"pricePerspectiveSampleSpinBox",QStringLiteral("请输入参考价格（元）")); m_sample->setValue(100); sampleRow->addRow(QStringLiteral("试一个商品价格（元）"),m_sample); previewLayout->addLayout(sampleRow);
    m_example=ShellWidgets::label(QString(),preview); m_example->setObjectName("pricePerspectiveExampleLabel"); previewLayout->addWidget(m_example);
    m_basis=ShellWidgets::label(QString(),preview,"muted"); m_basis->setObjectName("pricePerspectiveBasisLabel"); previewLayout->addWidget(m_basis); layout->addWidget(preview);
    layout->addWidget(ShellWidgets::label(QStringLiteral("这组数据只保存在本机，不发送给 AI。切换身份或关闭换算，会保留已填写的数据。"),this,"muted"));
    auto *saveButton=new QPushButton(QStringLiteral("保存设置"),this); saveButton->setObjectName("savePricePerspectiveButton"); saveButton->setProperty("primary",true); layout->addWidget(saveButton,0,Qt::AlignLeft);
    m_status=ShellWidgets::label(QString(),this,"muted"); m_status->setObjectName("pricePerspectiveStatusLabel"); layout->addWidget(m_status);
    connect(saveButton,&QPushButton::clicked,this,&PricePerspectiveWidget::save);
    saveButton->setVisible(!compact);
    for (auto *field:{m_food,m_salary,m_days,m_hours,m_sample}) connect(field,&QDoubleSpinBox::valueChanged,this,[this] { m_status->clear(); updatePreview(); });
    connect(m_meals,&QSpinBox::valueChanged,this,[this] { m_status->clear(); updatePreview(); }); updatePreview();
}
PricePerspective PricePerspectiveWidget::values() const {
    PricePerspective p; p.mode=m_mode; p.dailyFoodCents=qRound64(m_food->value()*100); p.mealsPerDay=m_meals->value(); p.monthlyIncomeCents=qRound64(m_salary->value()*100); p.workDaysPerMonth=m_days->value(); p.dailyWorkHours=m_hours->value(); return p;
}
void PricePerspectiveWidget::updatePreview() {
    m_student->setVisible(m_mode==PricePerspective::Student); m_worker->setVisible(m_mode==PricePerspective::Worker); m_preview->setVisible(!property("onboardingCompact").toBool() && m_mode!=PricePerspective::Off);
    const auto p=values(); m_reference->setText(p.ready()?p.reference():m_mode==PricePerspective::Student?QStringLiteral("填写每日伙食预算后显示换算"):QStringLiteral("填写月到手工资后显示换算"));
    const auto quote=p.quote(qRound64(m_sample->value()*100)); m_example->setText(quote.primary+(quote.secondary.isEmpty()?QString():"\n"+quote.secondary)); m_basis->setText(quote.basis); updateGeometry();
}
bool PricePerspectiveWidget::save() {
    for (auto *field:{m_food,m_salary,m_days,m_hours}) { if (!field->hasAcceptableInput()) { m_status->setText(QStringLiteral("请填写有效的金额或工作时间。")); AppTheme::setTone(m_status,"danger"); return false; } field->interpretText(); }
    m_meals->interpretText(); const auto p=values(); QString error; if (!PricePerspectiveStore::save(m_settingsPath,p,error)) { m_status->setText(error); AppTheme::setTone(m_status,"danger"); return false; }
    m_status->setText(p.mode==PricePerspective::Off?QStringLiteral("已关闭换算，填写的数据已保留。"):p.ready()?QStringLiteral("已保存，商品价格旁会显示换算参考。"):QStringLiteral("已保存；填写完整后才会显示换算提醒。")); AppTheme::setTone(m_status,"muted"); emit saved(); return true;
}

void PricePerspectiveWidget::reload() {
    const auto p=PricePerspectiveStore::load(m_settingsPath); m_mode=p.mode;
    const QStringList names={"pricePerspectiveOffButton","pricePerspectiveStudentButton","pricePerspectiveWorkerButton"};
    findChild<QPushButton *>(names.at(int(m_mode)))->setChecked(true);
    m_food->setValue(double(p.dailyFoodCents)/100); m_meals->setValue(p.mealsPerDay);
    m_salary->setValue(double(p.monthlyIncomeCents)/100); m_days->setValue(p.workDaysPerMonth); m_hours->setValue(p.dailyWorkHours);
    m_status->clear(); updatePreview();
}
