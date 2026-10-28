#include "OnboardingDialog.h"
#include "OnboardingTour.h"
#include "KuraMascot.h"
#include "AppTypography.h"
#include "BudgetRepository.h"
#include "DatabaseManager.h"
#include "ModernSelectors.h"
#include "PricePerspectiveWidget.h"
#include "EmptyAmountSpinBox.h"
#include "ShellWidgets.h"
#include "ShoppingProfileRepository.h"
#include "WishlistRepository.h"
#include "WishPoolRepository.h"
#include "WishPrioritySelector.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QList>
#include <QApplication>

// A regular layout containing only the visible form. QStackedLayout reserves
// every page's height-for-width, which leaves large gaps on shorter forms.
class OnboardingPages final : public QWidget {
public:
    explicit OnboardingPages(QWidget *parent):QWidget(parent) {
        m_layout=new QVBoxLayout(this); m_layout->setContentsMargins(0,0,0,0); m_layout->setSpacing(0);
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    }
    void addWidget(QWidget *page) { m_pages.append(page); m_layout->addWidget(page); page->setVisible(m_pages.size()==1); }
    int count() const { return m_pages.size(); }
    QWidget *widget(int index) const { return m_pages.at(index); }
    void setCurrentIndex(int index) { for (int i=0;i<m_pages.size();++i) m_pages[i]->setVisible(i==index); m_layout->invalidate(); updateGeometry(); }
private:
    QVBoxLayout *m_layout;
    QList<QWidget *> m_pages;
};

namespace {
QDoubleSpinBox *amount(QWidget *owner,const char *name) {
    auto *field=new EmptyAmountSpinBox(owner); field->setObjectName(QLatin1String(name));
    field->setRange(0,double(Transaction::MaximumAmountCents)/100); field->setDecimals(2);
    field->setButtonSymbols(QAbstractSpinBox::NoButtons); field->setMinimumHeight(42); return field;
}
void field(QVBoxLayout *layout,const QString &title,QWidget *widget) {
    layout->addWidget(ShellWidgets::label(title,widget->parentWidget(),"secondary")); layout->addWidget(widget);
}
}

OnboardingDialog::OnboardingDialog(DatabaseManager &database,const QString &settingsPath,QWidget *parent,bool resume)
    :QWidget(parent),m_database(database),m_settingsPath(settingsPath),m_connection(this)
{
    setObjectName("onboardingDialog"); setFocusPolicy(Qt::StrongFocus);
    parent->installEventFilter(this); setGeometry(parent->rect());
    m_form=new QWidget(this); m_form->setProperty("page",true); m_form->setAttribute(Qt::WA_StyledBackground);
    auto *outer=new QVBoxLayout(m_form); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
    auto *top=new QHBoxLayout; top->setContentsMargins(24,12,24,4);
    m_progress=ShellWidgets::label({},m_form,"muted"); m_progress->setObjectName("onboardingProgress"); top->addWidget(m_progress,1);
    auto *later=new QPushButton(QStringLiteral("稍后再说 ×"),m_form); later->setProperty("link",true); later->setObjectName("onboardingCloseButton"); top->addWidget(later); outer->addLayout(top);
    connect(later,&QPushButton::clicked,this,[this] { finish(false); });
    m_scroll=new QScrollArea(m_form); m_scroll->setObjectName("onboardingSetupScroll"); m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame); m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); outer->addWidget(m_scroll,1);
    auto *body=new QWidget; body->setProperty("page",true); auto *vertical=new QVBoxLayout(body); vertical->setContentsMargins(28,18,28,28); vertical->addStretch();
    auto *row=new QHBoxLayout; row->addStretch(); m_content=new QWidget(body); m_content->setMaximumWidth(640); m_content->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    auto *layout=new QVBoxLayout(m_content); layout->setContentsMargins(0,0,0,0); layout->setSpacing(16);
    m_mascot=new KuraMascot(m_content); m_mascot->setObjectName("onboardingMascot"); m_mascot->setFixedSize(126,138); layout->addWidget(m_mascot,0,Qt::AlignHCenter);
    m_title=ShellWidgets::label({},m_content); m_title->setObjectName("onboardingTitle");
    const int scale=window()->property("fontScale").isValid()?window()->property("fontScale").toInt():100;
    m_title->setFont(AppTypography::editorial(qRound(24*scale/100.)));
    m_title->setStyleSheet(QString("font-family:'DM Serif Display','LXGW WenKai Lite','Source Han Sans CN VF'; font-size:%1px; font-weight:400;").arg(qRound(24*scale/100.))); layout->addWidget(m_title);
    m_subtitle=ShellWidgets::label({},m_content,"secondary"); m_subtitle->setObjectName("onboardingBody"); layout->addWidget(m_subtitle);
    m_forms=new OnboardingPages(m_content); m_forms->setObjectName("onboardingForms"); layout->addWidget(m_forms);
    m_error=ShellWidgets::label({},m_content,"error"); m_error->setObjectName("onboardingError"); m_error->hide(); layout->addWidget(m_error);
    auto *actions=new QHBoxLayout; actions->setSpacing(12);
    m_previous=new QPushButton(QStringLiteral("上一步"),m_content); m_previous->setProperty("link",true); m_previous->setObjectName("onboardingPreviousButton"); actions->addWidget(m_previous);
    m_skip=new QPushButton(QStringLiteral("先跳过这一步"),m_content); m_skip->setProperty("link",true); m_skip->setObjectName("onboardingSkipButton"); actions->addWidget(m_skip); actions->addStretch();
    m_next=new QPushButton(m_content); m_next->setProperty("primary",true); m_next->setMinimumHeight(42); m_next->setObjectName("onboardingNextButton"); actions->addWidget(m_next); layout->addLayout(actions);
    row->addWidget(m_content,1); row->addStretch(); vertical->addLayout(row); vertical->addStretch(); m_scroll->setWidget(body);
    buildForms();
    connect(m_previous,&QPushButton::clicked,this,[this] { cancelTest(); showStep(m_step-1); });
    connect(m_skip,&QPushButton::clicked,this,[this] { cancelTest(); showStep(m_step+1); });
    connect(m_next,&QPushButton::clicked,this,&OnboardingDialog::next);
    auto *escape=new QShortcut(QKeySequence(Qt::Key_Escape),this); escape->setContext(Qt::WidgetWithChildrenShortcut); connect(escape,&QShortcut::activated,this,[this] { finish(false); });
    connect(&m_connection,&AIService::textReady,this,[this](const QString &) { if (!m_testing) return; cancelTest(); m_error->setText(QStringLiteral("连接成功啦！确认后会把配置保存在本机。")); AppTheme::setTone(m_error,"muted"); m_error->show(); });
    connect(&m_connection,&AIService::failed,this,[this](const QString &message) { if (!m_testing) return; cancelTest(); m_error->setText(message); AppTheme::setTone(m_error,"danger"); m_error->show(); });
    if (resume) m_step=qBound(0,QSettings(settingsPath,QSettings::IniFormat).value("onboarding/setupStep",0).toInt(),6);
    showStep(m_step); m_form->setGeometry(rect());
}

void OnboardingDialog::buildForms()
{
    const auto page=[this] { auto *widget=new QWidget(m_forms); auto *layout=new QVBoxLayout(widget); layout->setContentsMargins(0,0,0,0); layout->setSpacing(12); layout->setAlignment(Qt::AlignTop); m_forms->addWidget(widget); return layout; };
    auto *welcome=page();
    welcome->addWidget(ShellWidgets::label(QStringLiteral("记下花销，让冲动先冷静一下；攒出心愿，再回头看看哪些花得真香。"),welcome->parentWidget()));
    welcome->addWidget(ShellWidgets::label(QStringLiteral("每一步都能跳过，之后也能在设置里补上。"),welcome->parentWidget(),"muted"));
    auto *ai=page(); auto *aiPage=ai->parentWidget(); const auto saved=AISettingsStore::loadPersonal(m_settingsPath);
    m_provider=new ModernComboBox(aiPage); m_provider->setObjectName("onboardingAIProvider"); m_provider->addItems({QStringLiteral("通义千问 · 支持截图"),QStringLiteral("OpenAI"),QStringLiteral("自定义兼容服务")});
    m_provider->setCurrentIndex(saved.baseUrl.contains("dashscope.aliyuncs.com")?0:saved.baseUrl.contains("api.openai.com")?1:2); field(ai,QStringLiteral("选择 AI 服务"),m_provider);
    m_key=new QLineEdit(saved.apiKey,aiPage); m_key->setObjectName("onboardingAIKey"); m_key->setEchoMode(QLineEdit::Password); m_key->setPlaceholderText(QStringLiteral("API Key 会在本机加密保存")); m_key->setMaxLength(4096); field(ai,QStringLiteral("API Key"),m_key);
    auto *advanced=new QPushButton(QStringLiteral("服务地址与模型 ▸"),aiPage); advanced->setCheckable(true); advanced->setProperty("link",true); advanced->setObjectName("onboardingAIAdvancedButton"); ai->addWidget(advanced,0,Qt::AlignLeft);
    auto *advancedBody=new QWidget(aiPage); auto *advancedForm=new QFormLayout(advancedBody); advancedForm->setContentsMargins(0,0,0,0); advancedForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_url=new QLineEdit(saved.baseUrl,advancedBody); m_url->setObjectName("onboardingAIUrl"); m_model=new QLineEdit(saved.model,advancedBody); m_model->setObjectName("onboardingAIModel"); advancedForm->addRow(QStringLiteral("服务地址"),m_url); advancedForm->addRow(QStringLiteral("模型"),m_model); ai->addWidget(advancedBody); advancedBody->hide();
    connect(advanced,&QPushButton::toggled,advancedBody,&QWidget::setVisible);
    connect(m_provider,&QComboBox::currentIndexChanged,this,[this](int index) { cancelTest(); if (index<2) { m_url->setText(index==0?QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1"):QStringLiteral("https://api.openai.com/v1")); m_model->setText(index==0?QStringLiteral("qwen3-vl-plus"):QStringLiteral("gpt-4o-mini")); } });
    for (auto *edit:{m_key,m_url,m_model}) connect(edit,&QLineEdit::textChanged,this,[this] { cancelTest(); m_error->hide(); });
    auto *aiActions=new QHBoxLayout; m_test=new QPushButton(QStringLiteral("测试连接"),aiPage); m_test->setObjectName("onboardingAITestButton"); aiActions->addWidget(m_test);
    m_trial=new QPushButton(QStringLiteral("先用 Kura AI 试用"),aiPage); m_trial->setObjectName("onboardingTrialButton"); m_trial->setCheckable(true); aiActions->addWidget(m_trial); ai->addLayout(aiActions);
    QString trialError; AISettingsStore::trial(&trialError); m_trial->setEnabled(trialError.isEmpty()); m_trial->setChecked(saved.useTrial && trialError.isEmpty());
    m_trial->setToolTip(trialError.isEmpty()?QStringLiteral("确认后保存选择；使用 AI 功能时，相关文字或截图才会发送到试用服务。"):trialError);
    const auto trialText=[](bool enabled) { return enabled?QStringLiteral("已选择 Kura AI 试用"):QStringLiteral("先用 Kura AI 试用"); };
    m_trial->setText(trialText(m_trial->isChecked()));
    connect(m_trial,&QPushButton::toggled,this,[this,trialText](bool trial) { cancelTest(); m_trial->setText(trialText(trial)); for (auto *edit:{m_key,m_url,m_model}) edit->setEnabled(!trial); m_provider->setEnabled(!trial); });
    if (m_trial->isChecked()) { for (auto *edit:{m_key,m_url,m_model}) edit->setEnabled(false); m_provider->setEnabled(false); }
    connect(m_test,&QPushButton::clicked,this,&OnboardingDialog::testAI);
    auto *trialHint=ShellWidgets::label({},aiPage,"muted"); trialHint->setObjectName("onboardingAITrialHint"); ai->addWidget(trialHint);
    const auto updateTrialHint=[trialHint](bool enabled) { trialHint->setText(enabled?QStringLiteral("已选择项目方提供的 Kura AI，无需填写 Key，可直接确认下一步。使用 AI 时会发送相关文字或截图；保存不会发起请求，测试连接只发送一条虚构问候。"):QStringLiteral("没有 Key 也没关系，可以先跳过。试用可用时可直接选择。保存不会自动发送请求；测试连接只发送一条虚构问候。")); };
    updateTrialHint(m_trial->isChecked()); connect(m_trial,&QPushButton::toggled,this,updateTrialHint);
    auto *link=new QLabel(QStringLiteral("<a href=\"https://bailian.console.aliyun.com/cn-beijing/model/settings/api-key\">去阿里云百炼申请 Key ↗</a>"),aiPage); link->setOpenExternalLinks(true); link->setWordWrap(true); ai->addWidget(link);
    auto *budget=page(); m_basic=amount(budget->parentWidget(),"onboardingBasicBudget"); m_discretionary=amount(budget->parentWidget(),"onboardingFreeBudget");
    MonthlyBudgetStatus status; BudgetRepository budgets(m_database); if (budgets.loadStatus(m_month,status) && status.hasBudget) { m_basic->setValue(double(status.basicLimitCents)/100); m_discretionary->setValue(double(status.hasSplit?status.discretionaryLimitCents:status.limitCents)/100); }
    field(budget,QStringLiteral("必要生活额度（元） · 吃饭、通勤等日常"),m_basic); field(budget,QStringLiteral("自由消费额度（元） · 留给喜欢的东西"),m_discretionary);
    budget->addWidget(ShellWidgets::label(QStringLiteral("这是 %1 月的两份额度，分开填写。必要生活剩余不会自动转进自由消费；之后随时可以调整。").arg(m_month.month()),budget->parentWidget(),"muted"));
    auto *price=page(); m_perspective=new PricePerspectiveWidget(m_settingsPath,price->parentWidget(),true); price->addWidget(m_perspective);
    auto *wish=page(); auto *wishPage=wish->parentWidget(); m_wishName=new QLineEdit(wishPage); m_wishName->setMaxLength(140); m_wishName->setObjectName("onboardingWishName"); m_wishName->setPlaceholderText(QStringLiteral("比如：一双 Nike 699 块")); field(wish,QStringLiteral("写下这件心动的东西"),m_wishName);
    m_price=amount(wishPage,"onboardingWishPrice"); field(wish,QStringLiteral("预计价格（元） · 确认一下金额"),m_price); connect(m_wishName,&QLineEdit::editingFinished,this,&OnboardingDialog::parseWishText);
    auto *priority=makeWishPrioritySelector(wishPage,"onboardingWish",m_priority);
    auto *priorityRow=new QHBoxLayout; priorityRow->setSpacing(10);
    const QStringList priorityLabels={QStringLiteral("高优先\n近期确实要用"),QStringLiteral("中优先\n有用途，可以等等"),QStringLiteral("低优先\n暂时想要，不着急")};
    for (int i=1;i<=3;++i) {
        auto *button=qobject_cast<QPushButton *>(m_priority->button(i));
        button->setAccessibleName(button->text()); button->setToolTip(button->text());
        priority->layout()->removeWidget(button); button->setText(priorityLabels[i-1]); priorityRow->addWidget(button,1);
    }
    qobject_cast<QVBoxLayout *>(priority->layout())->addLayout(priorityRow); wish->addWidget(priority);
    WishPoolRepository pool(m_database); WishPoolSnapshot snapshot; const bool loaded=pool.load(snapshot);
    m_goal=new QCheckBox(QStringLiteral("把它设为我的许愿目标"),wishPage); m_goal->setObjectName("onboardingWishGoal"); m_goal->setChecked(loaded && !snapshot.id); wish->addWidget(m_goal);
    if (snapshot.id) wish->addWidget(ShellWidgets::label(QStringLiteral("当前目标是“%1”。勾选后会切换目标，原目标与忍住记录会保留为历史。").arg(snapshot.name),wishPage,"muted"));
    wish->addWidget(ShellWidgets::label(QStringLiteral("确认后加入待购清单，先冷静 3 天。它只是一件心愿，还没有产生消费。"),wishPage,"muted"));
    const auto pending=m_settingsPath.isEmpty()?0:QSettings(m_settingsPath,QSettings::IniFormat).value("onboarding/pendingWishId").toLongLong();
    if (pending) { QList<WishItem> items; WishlistRepository repository(m_database); if (repository.findAll(items)) for (const auto &item:items) if (item.id==pending && item.status==WishStatus::Waiting) { m_savedWish=item; m_savedWishKey=item.name+"|"+QString::number(item.estimatedCents)+"|"+QString::number(item.priority); m_wishName->setText(item.name); m_price->setValue(double(item.estimatedCents)/100); if (auto *button=m_priority->button(item.priority)) button->setChecked(true); m_goal->setChecked(snapshot.wishId==pending); } }
    auto *rules=page(); m_rules=new QPlainTextEdit(rules->parentWidget()); m_rules->setObjectName("onboardingRules"); m_rules->setPlaceholderText(QStringLiteral("比如：买之前先想想家里有没有类似的。\n一行一条，用自己的话写就好。")); m_rules->setMinimumHeight(120); m_rules->setMaximumHeight(180); m_rules->setTabChangesFocus(true);
    QStringList existingRules; ShoppingProfileRepository profiles(m_database); if (profiles.loadRules(existingRules)) m_rules->setPlainText(existingRules.join('\n')); rules->addWidget(m_rules); rules->addWidget(ShellWidgets::label(QStringLiteral("我会把它放进执行页的购物原则。用 AI 分析时，也能把你的原则带进思考里。"),rules->parentWidget(),"muted"));
    auto *ready=page(); QFrame *card; auto *import=ShellWidgets::card(ready->parentWidget(),&card); import->addWidget(ShellWidgets::label(QStringLiteral("要不要带上过去几个月的账单？"),card,"heading")); import->addWidget(ShellWidgets::label(QStringLiteral("可选导入 CSV / XLSX。先核对，再保存，让我们的第一天就有迹可循。"),card,"secondary"));
    auto *goImport=new QPushButton(QStringLiteral("去导入账单 →"),card); goImport->setObjectName("onboardingImportButton"); import->addWidget(goImport,0,Qt::AlignLeft); ready->addWidget(card); connect(goImport,&QPushButton::clicked,this,[this] { emit importRequested(); });
    auto *explore=new QPushButton(QStringLiteral("我先自己探索"),ready->parentWidget()); explore->setObjectName("onboardingExploreButton"); explore->setProperty("link",true); ready->addWidget(explore,0,Qt::AlignLeft); connect(explore,&QPushButton::clicked,this,[this] { finish(false); });
}

void OnboardingDialog::showStep(int step)
{
    m_step=qBound(0,step,6); m_forms->setCurrentIndex(m_step);
    const QStringList titles={QStringLiteral("嗨，我是 Kura！"),QStringLiteral("给我的 AI 小帮手接上电"),QStringLiteral("这个月，给生活和心动各留多少？"),QStringLiteral("把钱换成你熟悉的小刻度"),QStringLiteral("你现在最想买的一件东西是什么？"),QStringLiteral("一起写下你的消费小原则"),QStringLiteral("好啦，Kura 的小窝准备好啦！")};
    const QStringList bodies={QStringLiteral("你的小松鼠消费搭子，报到啦。\n尾巴负责囤栗子，我陪你留下真正喜欢的宝贝。"),QStringLiteral("配置后，我能帮你识别截图、一起分析商品。也可以先跳过，记账和清单都能离线用。"),QStringLiteral("先照顾好日常，再安心留一点给喜欢。确认后保存到本月预算。"),QStringLiteral("选个身份，商品价格就能换算成餐费或工作时间。只是参考，不会改变账单。"),QStringLiteral("说一句就好。我会先把它放进清单，陪你慢慢攒，不催你立刻买。"),QStringLiteral("不是给自己立规矩，用大白话记下你认可的消费方式就好。"),QStringLiteral("记账、冷静一下、攒心愿、回头复盘。我都在。接下来让我带你认认路吧？")};
    m_title->setText(titles[m_step]); m_subtitle->setText(bodies[m_step]);
    m_progress->setText(m_step==0?QStringLiteral("KURA · 初次见面"):m_step==6?QStringLiteral("KURA · 准备出发"):QStringLiteral("布置小窝 · %1 / 5").arg(m_step));
    m_previous->setVisible(m_step>0); m_skip->setVisible(m_step>0 && m_step<6);
    m_next->setText(m_step==0?QStringLiteral("好呀，一起开始 →"):m_step==6?QStringLiteral("让 Kura 带我逛一圈 →"):QStringLiteral("确认，下一步 →"));
    m_mascot->setVisible(m_step==0 || m_step==6); m_mascot->setFixedSize(m_step==0?(height()<680?64:126):76,m_step==0?(height()<680?70:138):84);
    m_error->hide(); AppTheme::setTone(m_error,"danger");
    for (int i=0;i<m_forms->count();++i) m_forms->widget(i)->setSizePolicy(QSizePolicy::Preferred,i==m_step?QSizePolicy::Preferred:QSizePolicy::Ignored);
    m_forms->setMinimumHeight(0); m_forms->updateGeometry(); m_scroll->verticalScrollBar()->setValue(0); rememberStep();
}
void OnboardingDialog::rememberStep() {
    if (m_settingsPath.isEmpty()) return;
    QSettings settings(m_settingsPath,QSettings::IniFormat); settings.setValue("onboarding/setupStep",m_step);
}
void OnboardingDialog::next() {
    if (m_step==6) { beginTour(); return; }
    if (saveStep()) showStep(m_step+1);
}
AISettings OnboardingDialog::aiValues(bool effective) const {
    if (m_trial->isChecked()) {
        if (effective) return AISettingsStore::trial();
        auto value=AISettingsStore::loadPersonal(m_settingsPath); value.useTrial=true; return value;
    }
    AISettings value; value.baseUrl=m_url->text().trimmed(); value.model=m_model->text().trimmed(); value.apiKey=m_key->text().trimmed(); return value;
}
void OnboardingDialog::cancelTest() {
    m_testing=false; m_connection.cancel(); if (m_test) m_test->setText(QStringLiteral("测试连接"));
}
void OnboardingDialog::testAI() {
    if (m_testing) { cancelTest(); return; }
    const auto value=aiValues(true); QString error=value.validationError();
    if (error.isEmpty() && value.apiKey.isEmpty()) error=QStringLiteral("先填入 API Key，或选择可用的试用，再让我打个招呼吧。");
    if (!error.isEmpty()) { m_error->setText(error); m_error->show(); return; }
    m_testing=true; m_test->setText(QStringLiteral("取消测试")); m_error->hide();
    if (!m_connection.advise(value,QStringLiteral("仅用于连接测试，请简短回复。"),QStringLiteral("你好，请回复 OK。"))) { cancelTest(); m_error->setText(QStringLiteral("连接未能开始，请检查配置。")); m_error->show(); }
}
bool OnboardingDialog::saveStep() {
    QString error;
    if (m_testing) error=QStringLiteral("连接测试还在进行，可以先取消测试，或跳过这一步。");
    else if (m_step==1) {
        const auto value=aiValues(); error=value.validationError();
        if (error.isEmpty() && !value.useTrial && value.apiKey.isEmpty()) error=QStringLiteral("还没填 Key 呢，可以点左边先跳过，之后再来接上 AI。");
        if (error.isEmpty() && AISettingsStore::save(m_settingsPath,value,error)) emit preferencesSaved();
    } else if (m_step==2) {
        if (!m_basic->hasAcceptableInput() || !m_discretionary->hasAcceptableInput()) error=QStringLiteral("请填写有效的预算金额。");
        else { m_basic->interpretText(); m_discretionary->interpretText(); BudgetRepository repository(m_database); if (!repository.saveSplit(m_month,qRound64(m_basic->value()*100),qRound64(m_discretionary->value()*100))) error=repository.lastError(); }
    } else if (m_step==3) {
        if (!m_perspective->save()) return false;
        emit preferencesSaved();
    } else if (m_step==4) return saveWish();
    else if (m_step==5) {
        QStringList rules; for (const auto &line:m_rules->toPlainText().split('\n')) if (!line.trimmed().isEmpty()) rules << line.trimmed();
        ShoppingProfileRepository repository(m_database); if (!repository.saveRules(rules)) error=repository.lastError();
    }
    if (!error.isEmpty()) { m_error->setText(error); m_error->show(); m_scroll->ensureWidgetVisible(m_error); return false; }
    return true;
}
void OnboardingDialog::parseWishText() {
    const auto text=m_wishName->text().trimmed();
    static const QRegularExpression expression(QStringLiteral("^(.*?)\\s*(?:[¥￥]\\s*(\\d+(?:\\.\\d{1,2})?)|(\\d+(?:\\.\\d{1,2})?)\\s*(?:元|块))\\s*$"));
    const auto match=expression.match(text);
    if (match.hasMatch() && !match.captured(1).trimmed().isEmpty()) {
        const auto number=match.captured(2).isEmpty()?match.captured(3):match.captured(2);
        bool ok=false; const double value=number.toDouble(&ok);
        if (ok && value>0 && value<=m_price->maximum()) { m_wishName->setText(match.captured(1).trimmed()); m_price->setValue(value); }
    }
}
bool OnboardingDialog::saveWish() {
    parseWishText(); QString error;
    if (!m_price->hasAcceptableInput()) error=QStringLiteral("请确认心愿的预计价格。");
    m_price->interpretText();
    WishItem item=m_savedWish; item.name=m_wishName->text().trimmed(); item.estimatedCents=qRound64(m_price->value()*100); item.priority=m_priority->checkedId();
    if (error.isEmpty()) error=item.inputError();
    if (error.isEmpty() && item.priority<1) error=QStringLiteral("点一下现在有多需要，再把它放进清单吧。");
    const auto key=item.name+"|"+QString::number(item.estimatedCents)+"|"+QString::number(item.priority);
    if (error.isEmpty()) {
        WishlistRepository repository(m_database);
        if (!item.id) { if (!repository.add(item)) error=repository.lastError(); else m_savedWish=item; }
        else if (key!=m_savedWishKey) { if (!repository.update(item)) error=repository.lastError(); else m_savedWish=item; }
        if (error.isEmpty()) {
            m_savedWishKey=key;
            if (!m_settingsPath.isEmpty()) { QSettings settings(m_settingsPath,QSettings::IniFormat); settings.setValue("onboarding/pendingWishId",m_savedWish.id); settings.sync(); }
            if (m_goal->isChecked()) { WishPoolRepository pool(m_database); if (!pool.setGoal(m_savedWish.id,true)) error=QStringLiteral("心愿已在清单中，但许愿目标暂未设置成功：%1。可以重试，或取消勾选先保留心愿。").arg(pool.lastError()); }
        }
    }
    if (!error.isEmpty()) { m_error->setText(error); m_error->show(); m_scroll->ensureWidgetVisible(m_error); return false; }
    return true;
}
void OnboardingDialog::beginTour() {
    if (m_tour) return;
    cancelTest(); m_form->hide(); emit tourStarted(); m_tour=new OnboardingTour(this);
    connect(m_tour,&OnboardingTour::pageRequested,this,&OnboardingDialog::pageRequested);
    connect(m_tour,&OnboardingTour::finished,this,&OnboardingDialog::finish); m_tour->start();
}
void OnboardingDialog::finish(bool completed) {
    if (m_finishing) return;
    m_finishing=true; cancelTest(); hide(); emit finished(completed); deleteLater();
}
bool OnboardingDialog::eventFilter(QObject *object,QEvent *event) {
    if (object==parentWidget() && event->type()==QEvent::Resize) setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(object,event);
}
void OnboardingDialog::resizeEvent(QResizeEvent *) {
    if (m_form) m_form->setGeometry(rect());
    if (m_mascot && m_step==0) m_mascot->setFixedSize(height()<680?64:126,height()<680?70:138);
    if (m_tour) m_tour->setGeometry(rect());
}

bool OnboardingDialog::focusNextPrevChild(bool next) {
    QList<QWidget *> fields;
    for (auto *widget:findChildren<QWidget *>())
        if (widget->isVisibleTo(this) && widget->isEnabled() && (widget->focusPolicy() & Qt::TabFocus)) fields.append(widget);
    if (fields.isEmpty()) { setFocus(); return true; }
    const int index=fields.indexOf(QApplication::focusWidget());
    const int target=index<0?(next?0:fields.size()-1):(index+(next?1:fields.size()-1))%fields.size();
    fields[target]->setFocus(next?Qt::TabFocusReason:Qt::BacktabFocusReason); return true;
}
