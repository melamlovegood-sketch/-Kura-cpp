#include "AppearanceWidget.h"
#include "PricePerspectiveWidget.h"
#include "AISettings.h"
#include "AppTheme.h"
#include "ShellWidgets.h"
#include "BuildInfo.h"
#include "ModernSelectors.h"
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QApplication>
#include <QClipboard>
#include <QShowEvent>
#include <QDesktopServices>
#include <QUrlQuery>
#include <QButtonGroup>
#include <QPushButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QBuffer>
#include <QImage>
#include <QPainter>

namespace {
// QPushButton's native size hint accounts only for its own caption. These rows
// contain two lines and a status, so let their layout determine the height.
class SettingsEntryButton final : public QPushButton {
public:
    explicit SettingsEntryButton(QWidget *parent):QPushButton(parent) { setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred); }
    QSize sizeHint() const override { return layout()?layout()->sizeHint():QPushButton::sizeHint(); }
    QSize minimumSizeHint() const override { return layout()?layout()->minimumSize():QPushButton::minimumSizeHint(); }
};
}

AppearanceWidget::AppearanceWidget(const QString &settingsPath,QWidget *parent) : QWidget(parent),m_settingsPath(settingsPath),m_connection(this)
{
    setObjectName(QStringLiteral("appearanceWidget"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 30, 28, 28); layout->setSpacing(20);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    auto *heading=new QHBoxLayout;
    m_back=new QPushButton(QStringLiteral("← 设置"),this); m_back->setObjectName("settingsBackButton");
    m_back->setProperty("link",true); heading->addWidget(m_back);
    m_title=ShellWidgets::label(QStringLiteral("设置"),this,"heading"); m_title->setObjectName("settingsTitleLabel");
    heading->addWidget(m_title,1); layout->addLayout(heading);
    connect(m_back,&QPushButton::clicked,this,[this] { showSection(0); });
    m_home=new QWidget(this); m_home->setObjectName("settingsHome");
    auto *entries=new QVBoxLayout(m_home); entries->setContentsMargins(0,0,0,0); entries->setSpacing(14);
    entries->addWidget(ShellWidgets::label(QStringLiteral("把显示、价格参考和数据保存调成你习惯的方式。"),m_home,"muted"));
    m_unsavedAI=new QPushButton(QStringLiteral("AI 配置有未保存的修改 · 继续编辑 →"),m_home); m_unsavedAI->setObjectName("unsavedAISettingsButton"); m_unsavedAI->setProperty("link",true); entries->addWidget(m_unsavedAI,0,Qt::AlignLeft);
    connect(m_unsavedAI,&QPushButton::clicked,this,[this] { showSection(2); });
    const auto group=[&](const QString &title) {
        auto *column=new QVBoxLayout; column->setSpacing(0);
        entries->addWidget(ShellWidgets::label(title,m_home,"eyebrow"));
        auto *box=new QFrame(m_home); box->setProperty("settingsGroup",true); box->setLayout(column); column->setContentsMargins(6,6,6,6); entries->addWidget(box);
        return column;
    };
    const auto entry=[&](QVBoxLayout *column,const QString &title,const QString &description,const char *name,QLabel **summary) {
        auto *button=new SettingsEntryButton(column->parentWidget()); button->setObjectName(QLatin1String(name)); button->setProperty("settingsEntry",true); button->setAccessibleName(title); button->setAccessibleDescription(description);
        auto *row=new QHBoxLayout(button); row->setContentsMargins(16,14,16,14); row->setSpacing(16);
        auto *text=new QWidget(button); auto *copy=new QVBoxLayout(text); copy->setContentsMargins(0,0,0,0); copy->setSpacing(5);
        auto *label=ShellWidgets::label(title,text); label->setProperty("settingsEntryTitle",true); copy->addWidget(label);
        auto *detail=ShellWidgets::label(description,text,"muted"); detail->setProperty("settingsEntryDescription",true); copy->addWidget(detail); row->addWidget(text,1);
        *summary=ShellWidgets::label(QString(),button); (*summary)->setObjectName(QString::fromLatin1(name)+"Summary"); (*summary)->setProperty("settingsStatus",true); row->addWidget(*summary);
        auto *arrow=ShellWidgets::label(QStringLiteral("›"),button); arrow->setProperty("settingsArrow",true); row->addWidget(arrow);
        for (auto *child:button->findChildren<QWidget *>()) child->setAttribute(Qt::WA_TransparentForMouseEvents);
        button->setCursor(Qt::PointingHandCursor); column->addWidget(button); return button;
    };
    const auto divider=[](QVBoxLayout *column) { auto *line=new QFrame; line->setFixedHeight(1); line->setProperty("settingsDivider",true); column->addWidget(line); };
    auto *preferences=group(QStringLiteral("使用偏好"));
    auto *appearanceEntry=entry(preferences,QStringLiteral("外观"),QStringLiteral("主题、字号与动画"),"settingsAppearanceButton",&m_appearanceSummary);
    divider(preferences);
    auto *priceEntry=entry(preferences,QStringLiteral("价格换算"),QStringLiteral("用餐费或工作时间理解价格"),"settingsPricePerspectiveButton",&m_priceSummary);
    auto *services=group(QStringLiteral("服务与数据"));
    auto *aiEntry=entry(services,QStringLiteral("AI 服务"),QStringLiteral("模型配置与连接测试"),"settingsAIButton",&m_serviceSummary);
    divider(services);
    auto *manage=entry(services,QStringLiteral("数据管理"),QStringLiteral("导入、导出与备份恢复"),"settingsDataButton",&m_dataSummary);
    m_dataSummary->setText(QStringLiteral("尚未备份"));
    connect(priceEntry,&QPushButton::clicked,this,[this] { showSection(3); });
    connect(appearanceEntry,&QPushButton::clicked,this,[this] { showSection(1); });
    connect(aiEntry,&QPushButton::clicked,this,[this] { showSection(2); });
    connect(manage,&QPushButton::clicked,this,&AppearanceWidget::dataRequested);
    auto *footer=new QHBoxLayout; footer->setContentsMargins(4,6,4,0);
    footer->addWidget(ShellWidgets::label(QStringLiteral("KURA · 蔵  /  v")+QStringLiteral(KURA_VERSION),m_home,"muted")); footer->addStretch();
    auto *feedbackButton=new QPushButton(QStringLiteral("问题反馈 →"),m_home); feedbackButton->setObjectName("settingsFeedbackButton"); feedbackButton->setProperty("link",true); footer->addWidget(feedbackButton);
    footer->addSpacing(16);
    auto *aboutButton=new QPushButton(QStringLiteral("关于与使用说明 →"),m_home); aboutButton->setObjectName("settingsAboutButton"); aboutButton->setProperty("link",true); footer->addWidget(aboutButton); entries->addLayout(footer);
    connect(aboutButton,&QPushButton::clicked,this,[this] { showSection(4); });
    connect(feedbackButton,&QPushButton::clicked,this,[this] { showSection(5); });
    layout->addWidget(m_home);
    QFrame *appearance;
    auto *card = ShellWidgets::card(this, &appearance);
    m_appearanceSection=appearance; appearance->setObjectName("settingsAppearanceSection");
    card->addWidget(ShellWidgets::label(QStringLiteral("主题"), appearance, "eyebrow"));
    auto *row = new QHBoxLayout;
    auto *themeGroup = new QButtonGroup(this); themeGroup->setExclusive(true);
    const QStringList ids = {"warm", "cool", "dark", "system"};
    const QStringList names = {QStringLiteral("暖色\n米白 · 温润"), QStringLiteral("冷色\n浅灰 · 清爽"), QStringLiteral("夜间\n深灰 · 柔和"), QStringLiteral("跟随系统\n自动切换")};
    const QStringList swatches = {"#f7f3eb", "#f0f2f5", "#211e1a", "#b8b8b8"};
    for (int i = 0; i < ids.size(); ++i) {
        auto *button = new QPushButton(names[i], appearance);
        button->setObjectName(ids[i] + QStringLiteral("ThemeButton"));
        button->setProperty("themeChoice", true); button->setProperty("themeId", ids[i]);
        QPixmap swatch(40,40); swatch.fill(Qt::transparent);
        QPainter painter(&swatch); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#9c9080"),1)); painter.setBrush(QColor(swatches[i])); painter.drawEllipse(QRectF(2,2,36,36)); painter.end();
        button->setIcon(QIcon(swatch)); button->setIconSize(QSize(20,20));
        button->setCursor(Qt::PointingHandCursor);
        button->setCheckable(true); themeGroup->addButton(button); row->addWidget(button, 1); m_choices.append(button);
        connect(button, &QPushButton::clicked, this, [this, id = ids[i]] { emit themeSelected(id); });
    }
    card->addLayout(row);
    auto *display=new QFormLayout; display->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_fontScale=new ModernComboBox(appearance); m_fontScale->setObjectName("settingsFontScaleComboBox");
    m_fontScale->addItem(QStringLiteral("标准 · 100%"),100); m_fontScale->addItem(QStringLiteral("大 · 115%"),115); m_fontScale->addItem(QStringLiteral("特大 · 130%"),130);
    int scale=100; bool reduced=false;
    if (!settingsPath.isEmpty()) { QSettings saved(settingsPath,QSettings::IniFormat); scale=saved.value("appearance/fontScale",100).toInt(); reduced=saved.value("appearance/reducedMotion",false).toBool(); }
    m_fontScale->setCurrentIndex(qMax(0,m_fontScale->findData(scale))); display->addRow(QStringLiteral("界面字号"),m_fontScale); card->addLayout(display);
    m_reducedMotion=new QCheckBox(QStringLiteral("减少动画"),appearance); m_reducedMotion->setObjectName("settingsReducedMotionCheckBox"); m_reducedMotion->setChecked(reduced); card->addWidget(m_reducedMotion);
    card->addWidget(ShellWidgets::label(QStringLiteral("立即应用并保留。减少动画会跳过开屏动画，并直接显示许愿池进度。"),appearance,"muted"));
    const auto displayChanged=[this] { emit displayOptionsChanged(m_fontScale->currentData().toInt(),m_reducedMotion->isChecked()); refreshSummary(); };
    connect(m_fontScale,&QComboBox::currentIndexChanged,this,displayChanged); connect(m_reducedMotion,&QCheckBox::toggled,this,displayChanged);
    layout->addWidget(appearance);
    QFrame *aiCard;
    auto *ai=ShellWidgets::card(this,&aiCard);
    m_aiSection=aiCard; aiCard->setObjectName("settingsAISection");
    ai->addWidget(ShellWidgets::label(QStringLiteral("还没有 API Key？先试用 Kura AI"),aiCard,"eyebrow"));
    ai->addWidget(ShellWidgets::label(QStringLiteral("测试期间由项目提供 AI 服务，无需注册或填写密钥。点击 AI 功能时，相关文字或截图会发送到试用服务；试用结束后可切换为自己的 API Key。"),aiCard,"muted"));
    auto *trialActions=new QHBoxLayout;
    m_useTrial=new QPushButton(QStringLiteral("启用 Kura AI 试用"),aiCard); m_useTrial->setObjectName("useTrialAIButton");
    m_usePersonal=new QPushButton(QStringLiteral("使用自己的 API Key"),aiCard); m_usePersonal->setObjectName("usePersonalAIButton");
    trialActions->addWidget(m_useTrial); trialActions->addWidget(m_usePersonal); ai->addLayout(trialActions);
    m_trialStatus=ShellWidgets::label(QString(),aiCard,"muted"); m_trialStatus->setObjectName("aiTrialStatusLabel"); ai->addWidget(m_trialStatus);
    ai->addWidget(ShellWidgets::label(QStringLiteral("个人配置"),aiCard,"eyebrow"));
    auto *form=new QFormLayout;
    QString loadError; const auto saved=AISettingsStore::loadPersonal(m_settingsPath,&loadError); m_savedAI=saved;
    m_baseUrl=new QLineEdit(saved.baseUrl,aiCard); m_baseUrl->setObjectName("aiBaseUrlEdit");
    m_model=new QLineEdit(saved.model,aiCard); m_model->setObjectName("aiModelEdit");
    m_key=new QLineEdit(saved.apiKey,aiCard); m_key->setObjectName("aiApiKeyEdit"); m_key->setEchoMode(QLineEdit::Password); m_key->setPlaceholderText(QStringLiteral("填写你的 API Key"));
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *keyRow=new QWidget(aiCard); auto *keyLayout=new QHBoxLayout(keyRow); keyLayout->setContentsMargins(0,0,0,0); keyLayout->addWidget(m_key,1);
    auto *showKey=new QPushButton(QStringLiteral("显示"),keyRow); showKey->setObjectName("showAIKeyButton"); showKey->setCheckable(true); keyLayout->addWidget(showKey);
    connect(showKey,&QPushButton::toggled,this,[this,showKey](bool show) { m_key->setEchoMode(show?QLineEdit::Normal:QLineEdit::Password); showKey->setText(show?QStringLiteral("隐藏"):QStringLiteral("显示")); });
    form->addRow(QStringLiteral("服务地址"),m_baseUrl); form->addRow(QStringLiteral("模型"),m_model); form->addRow(QStringLiteral("API Key"),keyRow); ai->addLayout(form);
    ai->addWidget(ShellWidgets::label(QStringLiteral("默认使用通义千问，也可填写兼容服务。文字与截图共用模型，请选支持图片的模型。密钥在本机加密保存，不写入业务备份。"),aiCard,"muted"));
    auto *keyHelpActions=new QHBoxLayout;
    auto *applyQwen=new QPushButton(QStringLiteral("免费申请千问 API Key ↗"),aiCard); applyQwen->setObjectName("applyQwenApiKeyButton"); applyQwen->setProperty("link",true);
    auto *keyGuideToggle=new QPushButton(QStringLiteral("查看申请教程 ▾"),aiCard); keyGuideToggle->setObjectName("qwenApiKeyGuideButton"); keyGuideToggle->setProperty("link",true); keyGuideToggle->setCheckable(true);
    keyHelpActions->addWidget(applyQwen); keyHelpActions->addWidget(keyGuideToggle); keyHelpActions->addStretch(); ai->addLayout(keyHelpActions);
    auto *quotaHint=ShellWidgets::label(QStringLiteral("Key 可免费创建；模型免费额度与有效期以百炼控制台为准，额度外调用可能计费。"),aiCard,"muted"); quotaHint->setObjectName("qwenFreeQuotaHintLabel"); ai->addWidget(quotaHint);
    auto *keyGuide=new QWidget(aiCard); keyGuide->setObjectName("qwenApiKeyGuidePanel");
    auto *guide=new QVBoxLayout(keyGuide); guide->setContentsMargins(0,0,0,0); guide->setSpacing(10);
    guide->addWidget(ShellWidgets::label(QStringLiteral("1. 打开阿里云百炼官方入口，注册或登录，选择华北2（北京），按页面提示开通服务。"),keyGuide));
    guide->addWidget(ShellWidgets::label(QStringLiteral("2. 在 API Key 管理中创建普通百炼 API Key，并复制生成的密钥。无需购买套餐或配置电脑环境变量。"),keyGuide));
    guide->addWidget(ShellWidgets::label(QStringLiteral("3. 在免费额度页面检查所选模型的剩余额度与到期时间；支持时提前开启“免费额度用完即停”。模型、地域和账号的实际权益以控制台为准。"),keyGuide));
    guide->addWidget(ShellWidgets::label(QStringLiteral("4. 回到 Kura，切换“使用自己的 API Key”，将密钥粘贴到上方。北京地域的推荐配置如下："),keyGuide));
    auto *recommended=ShellWidgets::label(QStringLiteral("服务地址：%1\n模型：%2（支持文字与截图）").arg(AISettings{}.baseUrl,AISettings{}.model),keyGuide,"muted"); recommended->setObjectName("qwenRecommendedSettingsLabel"); recommended->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard); guide->addWidget(recommended);
    guide->addWidget(ShellWidgets::label(QStringLiteral("5. 点击“保存 AI 设置”，再测试文字连接和虚构截图。连接测试也会消耗模型调用额度。"),keyGuide));
    auto *guideLinks=new QHBoxLayout;
    auto *officialGuide=new QPushButton(QStringLiteral("官方图文教程 ↗"),keyGuide); officialGuide->setObjectName("qwenOfficialGuideButton"); officialGuide->setProperty("link",true);
    auto *freeQuota=new QPushButton(QStringLiteral("查看免费额度 ↗"),keyGuide); freeQuota->setObjectName("qwenFreeQuotaButton"); freeQuota->setProperty("link",true);
    guideLinks->addWidget(officialGuide); guideLinks->addWidget(freeQuota); guideLinks->addStretch(); guide->addLayout(guideLinks);
    ai->addWidget(keyGuide); keyGuide->hide();
    connect(keyGuideToggle,&QPushButton::toggled,keyGuide,&QWidget::setVisible);
    connect(keyGuideToggle,&QPushButton::toggled,this,[keyGuideToggle](bool expanded) { keyGuideToggle->setText(expanded?QStringLiteral("收起申请教程 ▴"):QStringLiteral("查看申请教程 ▾")); });
    const auto openQwenHelp=[this](const QString &address) {
        if (!QDesktopServices::openUrl(QUrl(address))) { m_aiStatus->setText(QStringLiteral("无法打开浏览器，请手动访问：%1").arg(address)); AppTheme::setTone(m_aiStatus,"warning"); }
    };
    connect(applyQwen,&QPushButton::clicked,this,[openQwenHelp] { openQwenHelp(QStringLiteral("https://bailian.console.aliyun.com/cn-beijing/model/settings/api-key")); });
    connect(officialGuide,&QPushButton::clicked,this,[openQwenHelp] { openQwenHelp(QStringLiteral("https://help.aliyun.com/zh/model-studio/get-api-key/")); });
    connect(freeQuota,&QPushButton::clicked,this,[openQwenHelp] { openQwenHelp(QStringLiteral("https://bailian.console.aliyun.com/cn-beijing/costing-balance/free-quota")); });
    auto *saveAI=new QPushButton(QStringLiteral("保存 AI 设置"),aiCard); saveAI->setObjectName("saveAISettingsButton"); ai->addWidget(saveAI,0,Qt::AlignLeft);
    m_saveAI=saveAI;
    auto *probes=new QHBoxLayout; m_probeText=new QPushButton(QStringLiteral("测试文字连接"),aiCard); m_probeText->setObjectName("testAITextButton"); m_probeImage=new QPushButton(QStringLiteral("测试虚构截图"),aiCard); m_probeImage->setObjectName("testAIImageButton"); m_cancelProbe=new QPushButton(QStringLiteral("取消等待"),aiCard); m_cancelProbe->setObjectName("cancelAIConnectionTestButton"); probes->addWidget(m_probeText); probes->addWidget(m_probeImage); probes->addWidget(m_cancelProbe); ai->addLayout(probes); ai->addWidget(ShellWidgets::label(QStringLiteral("测试使用当前选择的 AI 服务；个人模式使用当前填写的配置。截图测试只发送程序生成的虚构样例，不读取真实账单。"),aiCard,"muted"));
    m_aiStatus=ShellWidgets::label(loadError,aiCard); m_aiStatus->setObjectName("aiSettingsStatusLabel"); ai->addWidget(m_aiStatus);
    m_lastTest=ShellWidgets::label(QString(),aiCard,"muted"); m_lastTest->setObjectName("aiLastTestLabel"); ai->addWidget(m_lastTest);
    for (auto *edit:{m_baseUrl,m_model,m_key}) connect(edit,&QLineEdit::textChanged,this,[this] { if (!m_testing) { m_aiStatus->setText(aiDirty()?QStringLiteral("有未保存的修改。测试使用当前输入，正式调用前请保存。") : QString()); AppTheme::setTone(m_aiStatus,"muted"); refreshSummary(); updateTestSummary(); } });
    connect(m_probeText,&QPushButton::clicked,this,[this] { testConnection(false); }); connect(m_probeImage,&QPushButton::clicked,this,[this] { testConnection(true); }); connect(m_cancelProbe,&QPushButton::clicked,this,[this] { m_connection.cancel(); m_testing=false; m_aiStatus->setText(QStringLiteral("已取消等待，填写的配置保持不变。")); updateConnectionControls(); });
    connect(&m_connection,&AIService::failed,this,[this](const QString &error) { m_testing=false; recordTest(false); m_aiStatus->setText(error); AppTheme::setTone(m_aiStatus,"danger"); updateConnectionControls(); });
    connect(&m_connection,&AIService::textReady,this,[this](const QString &) { m_testing=false; recordTest(true); m_aiStatus->setText(QStringLiteral("文字连接成功，用时 %1 秒；图片能力请另测。").arg(QString::number(m_connectionTimer.elapsed()/1000.0,'f',1))); AppTheme::setTone(m_aiStatus,"success"); updateConnectionControls(); });
    connect(&m_connection,&AIService::parsed,this,[this](const ParsedProduct &product) { m_testing=false; const bool matched=product.name.contains(QStringLiteral("笔记本")) && product.amountCents==1234; recordTest(matched); m_aiStatus->setText(matched?QStringLiteral("截图样例识别通过：商品和 12.34 元金额均匹配。真实截图仍需逐次核对。"):QStringLiteral("已收到图片响应，但样例商品或金额不匹配，请核对模型的视觉能力。")); AppTheme::setTone(m_aiStatus,matched?"success":"warning"); updateConnectionControls(); }); updateConnectionControls();
    connect(m_useTrial,&QPushButton::clicked,this,[this] { selectTrial(true); });
    connect(m_usePersonal,&QPushButton::clicked,this,[this] { selectTrial(false); });
    connect(saveAI,&QPushButton::clicked,this,[this] {
        const AISettings settings=currentAI();
        QString error; const bool ok=AISettingsStore::save(m_settingsPath,settings,error);
        if (ok) m_savedAI=settings;
        refreshSummary(); updateTestSummary();
        m_aiStatus->setText(ok?QStringLiteral("已保存。现在可以从主页解析文字或截图。"):error);
        AppTheme::setTone(m_aiStatus,ok?QStringLiteral("success"):QStringLiteral("danger"));
    });
    layout->addWidget(aiCard);
    auto *priceSettings=new PricePerspectiveWidget(settingsPath,this); m_priceSection=priceSettings; layout->addWidget(priceSettings);
    connect(priceSettings,&PricePerspectiveWidget::saved,this,&AppearanceWidget::pricePerspectiveChanged);
    QFrame *about;
    auto *info=ShellWidgets::card(this,&about); m_aboutSection=about; about->setObjectName("settingsAboutSection");
    info->addWidget(ShellWidgets::label(QStringLiteral("KURA · 蔵"),about,"heading"));
    const QString version=QStringLiteral("版本 %1 · 构建于 %2").arg(QStringLiteral(KURA_VERSION),QStringLiteral(KURA_BUILD_DATE));
    auto *versionLabel=ShellWidgets::label(version,about,"muted"); versionLabel->setObjectName("settingsVersionLabel"); info->addWidget(versionLabel);
    info->addWidget(ShellWidgets::label(QStringLiteral("少一点冲动，多一点真正想留下的东西。"),about));
    info->addWidget(ShellWidgets::label(QStringLiteral("从这里开始"),about,"eyebrow"));
    auto *tour = new QPushButton(QStringLiteral("重看新手引导"),about);
    tour->setObjectName("replayOnboardingButton"); info->addWidget(tour,0,Qt::AlignLeft);
    connect(tour,&QPushButton::clicked,this,&AppearanceWidget::onboardingRequested);
    info->addWidget(ShellWidgets::label(QStringLiteral("想买东西：从首页输入商品，核对后选择开始思考或加入清单。\n\n记录消费：在账单页手动记账，或导入已有账单。\n\n购物原则：在执行页展开购物原则，直接添加和编辑。\n\n回看这个月：在复盘页查看消费故事，并标记值得或后悔的支出。"),about));
    auto *copyVersion=new QPushButton(QStringLiteral("复制版本信息"),about); copyVersion->setObjectName("copyVersionInfoButton"); info->addWidget(copyVersion,0,Qt::AlignLeft);
    auto *copyStatus=ShellWidgets::label(QString(),about,"muted"); copyStatus->setObjectName("versionCopyStatusLabel"); info->addWidget(copyStatus);
    connect(copyVersion,&QPushButton::clicked,this,[version,copyStatus] { QApplication::clipboard()->setText(QStringLiteral("KURA / ")+version); copyStatus->setText(QStringLiteral("版本信息已复制。")); });
    layout->addWidget(about);
    QFrame *feedback;
    auto *contact=ShellWidgets::card(this,&feedback); m_feedbackSection=feedback; feedback->setObjectName("settingsFeedbackSection");
    contact->addWidget(ShellWidgets::label(QStringLiteral("遇到问题，或有想补充的功能？欢迎发邮件告诉我。"),feedback));
    contact->addWidget(ShellWidgets::label(QStringLiteral("联系邮箱"),feedback,"eyebrow"));
    const QString email=QStringLiteral("meilintan@bupt.edu.cn");
    auto *emailLabel=ShellWidgets::label(email,feedback); emailLabel->setObjectName("feedbackEmailLabel"); emailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard); contact->addWidget(emailLabel);
    contact->addWidget(ShellWidgets::label(QStringLiteral("反馈时可以附上操作步骤、遇到的问题和截图，方便定位。"),feedback,"muted"));
    auto *contactActions=new QHBoxLayout;
    auto *copyEmail=new QPushButton(QStringLiteral("复制邮箱"),feedback); copyEmail->setObjectName("copyFeedbackEmailButton"); contactActions->addWidget(copyEmail);
    auto *composeEmail=new QPushButton(QStringLiteral("写邮件反馈"),feedback); composeEmail->setObjectName("composeFeedbackEmailButton"); composeEmail->setProperty("primary",true); contactActions->addWidget(composeEmail); contactActions->addStretch(); contact->addLayout(contactActions);
    auto *contactStatus=ShellWidgets::label(QString(),feedback,"muted"); contactStatus->setObjectName("feedbackStatusLabel"); contact->addWidget(contactStatus);
    connect(copyEmail,&QPushButton::clicked,this,[email,contactStatus] { QApplication::clipboard()->setText(email); contactStatus->setText(QStringLiteral("邮箱已复制。")); });
    connect(composeEmail,&QPushButton::clicked,this,[email,version,contactStatus] {
        QUrl mail; mail.setScheme(QStringLiteral("mailto")); mail.setPath(email);
        QUrlQuery query; query.addQueryItem(QStringLiteral("subject"),QStringLiteral("KURA 问题反馈")); query.addQueryItem(QStringLiteral("body"),version+QStringLiteral("\n\n问题描述：\n\n操作步骤：\n")); mail.setQuery(query);
        contactStatus->setText(QDesktopServices::openUrl(mail)?QStringLiteral("已请求打开邮件应用，请填写反馈后自行发送。"):QStringLiteral("无法打开邮件应用，请复制邮箱后手动发送。"));
    });
    layout->addWidget(feedback); layout->addStretch();
    updateTestSummary();
    showSection(0);
}

void AppearanceWidget::showHome() { showSection(0); }

void AppearanceWidget::reloadOnboardingSettings()
{
    if (!aiDirty() && !m_testing) {
        m_savedAI=AISettingsStore::loadPersonal(m_settingsPath);
        m_baseUrl->setText(m_savedAI.baseUrl); m_model->setText(m_savedAI.model); m_key->setText(m_savedAI.apiKey);
        updateConnectionControls(); updateTestSummary();
    }
    if (auto *price=qobject_cast<PricePerspectiveWidget *>(m_priceSection)) price->reload();
    refreshSummary();
}

void AppearanceWidget::showSection(int section)
{
    m_home->setVisible(section==0); m_appearanceSection->setVisible(section==1); m_aiSection->setVisible(section==2); m_priceSection->setVisible(section==3); m_aboutSection->setVisible(section==4); m_feedbackSection->setVisible(section==5);
    m_key->setEchoMode(QLineEdit::Password); if (auto *toggle=findChild<QPushButton *>("showAIKeyButton")) toggle->setChecked(false);
    refreshSummary();
    m_back->setVisible(section!=0);
    m_title->setText(section==1?QStringLiteral("外观"):section==2?QStringLiteral("AI 服务"):section==3?QStringLiteral("价格换算"):section==4?QStringLiteral("关于 KURA"):section==5?QStringLiteral("问题反馈"):QStringLiteral("设置"));
    updateGeometry();
}

void AppearanceWidget::setTheme(const QString &theme)
{
    m_selectedTheme=theme;
    for (auto *choice : m_choices) choice->setChecked(choice->property("themeId").toString() == theme);
    refreshSummary();
}
void AppearanceWidget::updateConnectionControls()
{
    const bool trial=m_savedAI.useTrial;
    for (auto *edit:{m_baseUrl,m_model,m_key}) edit->setEnabled(!m_testing && !trial);
    findChild<QPushButton *>("showAIKeyButton")->setEnabled(!m_testing && !trial);
    m_probeText->setEnabled(!m_testing); m_probeImage->setEnabled(!m_testing); m_saveAI->setEnabled(!m_testing && !trial); m_cancelProbe->setVisible(m_testing); m_cancelProbe->setEnabled(m_testing);
    QString trialError; const auto shared=AISettingsStore::trial(&trialError);
    m_useTrial->setEnabled(!m_testing && !trial && trialError.isEmpty()); m_usePersonal->setEnabled(!m_testing && trial);
    m_useTrial->setText(trial?QStringLiteral("已启用 Kura AI 试用"):QStringLiteral("启用 Kura AI 试用"));
    m_trialStatus->setText(!trialError.isEmpty()?trialError:trial?QStringLiteral("正在使用 Kura AI 试用 · %1 · %2。下方保留你的个人配置，切回后可编辑。").arg(shared.endpoint().host(),shared.model):QStringLiteral("Kura AI 试用可用。点击启用即保存选择，不会自动发送请求，也不会覆盖个人配置。"));
}
void AppearanceWidget::testConnection(bool image)
{
    if (m_testing) return; m_imageProbe=image; const AISettings settings=effectiveAI(); m_testing=true; m_connectionTimer.start(); m_aiStatus->setText(QStringLiteral("正在测试当前配置…")); AppTheme::setTone(m_aiStatus,"muted"); updateConnectionControls(); bool started=false;
    if (image) {
        QImage sample(680,260,QImage::Format_RGB32); sample.fill(Qt::white); QPainter painter(&sample); painter.setPen(Qt::black); QFont font(QStringLiteral("Microsoft YaHei UI")); font.setPixelSize(26); painter.setFont(font); painter.drawText(QRect(28,24,624,212),Qt::TextWordWrap,QStringLiteral("联调样例（虚构，无真实消费）\n商品：测试笔记本\n金额：12.34 元")); painter.end(); QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); sample.save(&buffer,"JPEG",85); started=m_connection.analyze(settings,QStringLiteral("请只识别这张虚构样例图片里的商品和金额。"),bytes);
    } else started=m_connection.advise(settings,QStringLiteral("这是连接测试，请只回复简短中文连接成功。"),QStringLiteral("请回复：连接成功。"));
    if (!started) { m_testing=false; updateConnectionControls(); }
}

AISettings AppearanceWidget::currentAI() const
{
    AISettings value; value.baseUrl=m_baseUrl->text(); value.model=m_model->text(); value.apiKey=m_key->text(); value.useTrial=m_savedAI.useTrial; return value;
}
AISettings AppearanceWidget::effectiveAI() const
{
    return m_savedAI.useTrial?AISettingsStore::trial():currentAI();
}
void AppearanceWidget::selectTrial(bool enabled)
{
    if (m_testing) return;
    QString error;
    if (enabled) AISettingsStore::trial(&error);
    auto selected=m_savedAI; selected.useTrial=enabled;
    if (error.isEmpty() && AISettingsStore::save(m_settingsPath,selected,error)) {
        m_savedAI=selected;
        m_key->setEchoMode(QLineEdit::Password); findChild<QPushButton *>("showAIKeyButton")->setChecked(false);
        m_aiStatus->setText(enabled?QStringLiteral("已启用 Kura AI 试用，可以直接使用 AI 功能。尚未发送任何请求。"):QStringLiteral("已切回个人配置；修改后请保存 AI 设置。"));
        AppTheme::setTone(m_aiStatus,"success");
    } else { m_aiStatus->setText(error); AppTheme::setTone(m_aiStatus,"danger"); }
    updateConnectionControls(); refreshSummary(); updateTestSummary();
}
bool AppearanceWidget::aiDirty() const
{
    const auto value=currentAI(); return value.baseUrl!=m_savedAI.baseUrl || value.model!=m_savedAI.model || value.apiKey!=m_savedAI.apiKey;
}
QString AppearanceWidget::configHash(const AISettings &value) const
{
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonArray{value.baseUrl.trimmed(),value.model.trimmed(),value.apiKey.trimmed()}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}
void AppearanceWidget::recordTest(bool success)
{
    if (!m_settingsPath.isEmpty()) {
        QSettings settings(m_settingsPath,QSettings::IniFormat);
        const QString prefix=m_imageProbe?QStringLiteral("aiTest/image/"):QStringLiteral("aiTest/text/");
        settings.setValue(prefix+"hash",configHash(effectiveAI())); settings.setValue(prefix+"success",success); settings.setValue(prefix+"at",QDateTime::currentDateTime()); settings.sync();
    }
    updateTestSummary(); refreshSummary();
}
void AppearanceWidget::updateTestSummary()
{
    QStringList results;
    if (!m_settingsPath.isEmpty()) {
        QSettings settings(m_settingsPath,QSettings::IniFormat);
        for (const auto &kind:{QStringLiteral("text"),QStringLiteral("image")}) {
            const auto prefix="aiTest/"+kind+"/";
            if (settings.value(prefix+"hash").toString()!=configHash(effectiveAI())) continue;
            const auto at=settings.value(prefix+"at").toDateTime();
            results<<QStringLiteral("%1%2 · %3").arg(kind=="text"?QStringLiteral("文字"):QStringLiteral("截图"),settings.value(prefix+"success").toBool()?QStringLiteral("测试通过"):QStringLiteral("测试未通过"),at.toLocalTime().toString("MM-dd HH:mm"));
        }
    }
    m_lastTest->setText(results.isEmpty()?QStringLiteral("当前配置尚未测试。保存配置不会自动发起连接。"):results.join(QStringLiteral("\n")));
}
void AppearanceWidget::refreshSummary()
{
    const QString theme=m_selectedTheme=="cool"?QStringLiteral("冷色"):m_selectedTheme=="dark"?QStringLiteral("夜间"):m_selectedTheme=="system"?QStringLiteral("跟随系统"):QStringLiteral("暖色");
    m_appearanceSummary->setText(theme+QStringLiteral(" · %1%").arg(m_fontScale->currentData().toInt()));
    const auto price=PricePerspectiveStore::load(m_settingsPath);
    m_priceSummary->setText(price.mode==PricePerspective::Off?QStringLiteral("未开启"):!price.ready()?QStringLiteral("待完善资料"):price.mode==PricePerspective::Student?QStringLiteral("餐费换算"):QStringLiteral("工作时间换算"));
    const auto saved=AISettingsStore::load(m_settingsPath);
    m_serviceSummary->setText(saved.useTrial?(saved.configurationError.isEmpty()?QStringLiteral("Kura AI 试用"):QStringLiteral("试用不可用")):aiDirty()?QStringLiteral("有未保存修改"):saved.apiKey.isEmpty()?QStringLiteral("未配置"):QStringLiteral("已配置"));
    m_unsavedAI->setVisible(aiDirty());
}
void AppearanceWidget::setBackupSummary(const QString &summary) { m_dataSummary->setText(summary); }
void AppearanceWidget::showEvent(QShowEvent *event) { QWidget::showEvent(event); refreshSummary(); updateTestSummary(); }
