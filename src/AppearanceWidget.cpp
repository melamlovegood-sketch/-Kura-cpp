#include "AppearanceWidget.h"
#include "AISettings.h"
#include "AppTheme.h"
#include "ShellWidgets.h"
#include <QButtonGroup>
#include <QPushButton>
#include <QFormLayout>
#include <QLineEdit>
#include <QBuffer>
#include <QImage>
#include <QPainter>

AppearanceWidget::AppearanceWidget(const QString &settingsPath,QWidget *parent) : QWidget(parent),m_settingsPath(settingsPath),m_connection(this)
{
    setObjectName(QStringLiteral("appearanceWidget"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(20);
    setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    auto *heading=new QHBoxLayout;
    m_back=new QPushButton(QStringLiteral("← 设置"),this); m_back->setObjectName("settingsBackButton");
    m_back->setProperty("link",true); heading->addWidget(m_back);
    m_title=ShellWidgets::label(QStringLiteral("设置"),this,"heading"); m_title->setObjectName("settingsTitleLabel");
    heading->addWidget(m_title,1); layout->addLayout(heading);
    connect(m_back,&QPushButton::clicked,this,[this] { showSection(0); });
    m_home=new QWidget(this); m_home->setObjectName("settingsHome");
    auto *entries=new QVBoxLayout(m_home); entries->setContentsMargins(0,0,0,0); entries->setSpacing(8);
    const auto entry=[&](const QString &text,const char *name) {
        auto *button=new QPushButton(text+QStringLiteral("  →"),m_home); button->setObjectName(QLatin1String(name)); button->setAccessibleName(text);
        button->setMinimumHeight(52); button->setStyleSheet(QStringLiteral("QPushButton { text-align:left; padding:12px 18px; }"));
        button->setCursor(Qt::PointingHandCursor); entries->addWidget(button); return button;
    };
    auto *appearanceEntry=entry(QStringLiteral("外观"),"settingsAppearanceButton");
    auto *aiEntry=entry(QStringLiteral("AI 服务"),"settingsAIButton");
    auto *profile=entry(QStringLiteral("购物原则与品牌"),"shoppingProfileButton");
    auto *manage=entry(QStringLiteral("数据管理"),"settingsDataButton");
    connect(appearanceEntry,&QPushButton::clicked,this,[this] { showSection(1); });
    connect(aiEntry,&QPushButton::clicked,this,[this] { showSection(2); });
    connect(profile,&QPushButton::clicked,this,&AppearanceWidget::shoppingProfileRequested);
    connect(manage,&QPushButton::clicked,this,&AppearanceWidget::dataRequested);
    layout->addWidget(m_home);
    QFrame *appearance;
    auto *card = ShellWidgets::card(this, &appearance);
    m_appearanceSection=appearance; appearance->setObjectName("settingsAppearanceSection");
    card->addWidget(ShellWidgets::label(QStringLiteral("主题"), appearance, "eyebrow"));
    auto *row = new QHBoxLayout;
    auto *group = new QButtonGroup(this); group->setExclusive(true);
    const QStringList ids = {"warm", "cool", "dark"};
    const QStringList names = {QStringLiteral("暖色\n米白 · 温润"), QStringLiteral("冷色\n浅灰 · 清爽"), QStringLiteral("夜间\n深灰 · 护眼")};
    const QStringList swatches = {"#f5f0e8", "#f0f2f5", "#141414"};
    for (int i = 0; i < ids.size(); ++i) {
        auto *button = new QPushButton(names[i], appearance);
        button->setObjectName(ids[i] + QStringLiteral("ThemeButton"));
        button->setProperty("themeChoice", true); button->setProperty("themeId", ids[i]);
        QPixmap swatch(40,40); swatch.fill(Qt::transparent);
        QPainter painter(&swatch); painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#9c9080"),1)); painter.setBrush(QColor(swatches[i])); painter.drawEllipse(QRectF(2,2,36,36)); painter.end();
        button->setIcon(QIcon(swatch)); button->setIconSize(QSize(20,20));
        button->setCursor(Qt::PointingHandCursor);
        button->setCheckable(true); group->addButton(button); row->addWidget(button, 1); m_choices.append(button);
        connect(button, &QPushButton::clicked, this, [this, id = ids[i]] { emit themeSelected(id); });
    }
    card->addLayout(row);
    card->addWidget(ShellWidgets::label(QStringLiteral("立即应用，下次打开时保留。"), appearance, "muted"));
    layout->addWidget(appearance);
    QFrame *aiCard;
    auto *ai=ShellWidgets::card(this,&aiCard);
    m_aiSection=aiCard; aiCard->setObjectName("settingsAISection");
    auto *form=new QFormLayout;
    QString loadError; const auto saved=AISettingsStore::load(m_settingsPath,&loadError);
    m_baseUrl=new QLineEdit(saved.baseUrl,aiCard); m_baseUrl->setObjectName("aiBaseUrlEdit");
    m_model=new QLineEdit(saved.model,aiCard); m_model->setObjectName("aiModelEdit");
    m_key=new QLineEdit(saved.apiKey,aiCard); m_key->setObjectName("aiApiKeyEdit"); m_key->setEchoMode(QLineEdit::Password); m_key->setPlaceholderText(QStringLiteral("填写你的 API Key"));
    form->addRow(QStringLiteral("服务地址"),m_baseUrl); form->addRow(QStringLiteral("模型"),m_model); form->addRow(QStringLiteral("API Key"),m_key); ai->addLayout(form);
    ai->addWidget(ShellWidgets::label(QStringLiteral("默认使用通义千问，也可填写兼容服务。文字与截图共用模型，请选支持图片的模型。密钥在本机加密保存，不写入业务备份。"),aiCard,"muted"));
    auto *saveAI=new QPushButton(QStringLiteral("保存 AI 设置"),aiCard); saveAI->setObjectName("saveAISettingsButton"); ai->addWidget(saveAI,0,Qt::AlignLeft);
    m_saveAI=saveAI;
    auto *probes=new QHBoxLayout; m_probeText=new QPushButton(QStringLiteral("测试文字连接"),aiCard); m_probeText->setObjectName("testAITextButton"); m_probeImage=new QPushButton(QStringLiteral("测试虚构截图"),aiCard); m_probeImage->setObjectName("testAIImageButton"); m_cancelProbe=new QPushButton(QStringLiteral("取消等待"),aiCard); m_cancelProbe->setObjectName("cancelAIConnectionTestButton"); probes->addWidget(m_probeText); probes->addWidget(m_probeImage); probes->addWidget(m_cancelProbe); ai->addLayout(probes); ai->addWidget(ShellWidgets::label(QStringLiteral("测试使用当前填写的配置，会调用模型并按服务计费。截图测试只发送程序生成的虚构样例，不读取真实账单。"),aiCard,"muted"));
    m_aiStatus=ShellWidgets::label(loadError,aiCard); m_aiStatus->setObjectName("aiSettingsStatusLabel"); ai->addWidget(m_aiStatus);
    for (auto *edit:{m_baseUrl,m_model,m_key}) connect(edit,&QLineEdit::textChanged,this,[this] { if (!m_testing) { m_aiStatus->setText(QStringLiteral("配置已修改。测试使用当前输入，正式调用前请保存设置。")); AppTheme::setTone(m_aiStatus,"muted"); } });
    connect(m_probeText,&QPushButton::clicked,this,[this] { testConnection(false); }); connect(m_probeImage,&QPushButton::clicked,this,[this] { testConnection(true); }); connect(m_cancelProbe,&QPushButton::clicked,this,[this] { m_connection.cancel(); m_testing=false; m_aiStatus->setText(QStringLiteral("已取消等待，填写的配置保持不变。")); updateConnectionControls(); });
    connect(&m_connection,&AIService::failed,this,[this](const QString &error) { m_testing=false; m_aiStatus->setText(error); AppTheme::setTone(m_aiStatus,"danger"); updateConnectionControls(); });
    connect(&m_connection,&AIService::textReady,this,[this](const QString &) { m_testing=false; m_aiStatus->setText(QStringLiteral("文字连接成功，用时 %1 秒；图片能力请另测。").arg(QString::number(m_connectionTimer.elapsed()/1000.0,'f',1))); AppTheme::setTone(m_aiStatus,"success"); updateConnectionControls(); });
    connect(&m_connection,&AIService::parsed,this,[this](const ParsedProduct &product) { m_testing=false; const bool matched=product.name.contains(QStringLiteral("笔记本")) && product.amountCents==1234; m_aiStatus->setText(matched?QStringLiteral("截图样例识别通过：商品和 12.34 元金额均匹配。真实截图仍需逐次核对。"):QStringLiteral("已收到图片响应，但样例商品或金额不匹配，请核对模型的视觉能力。")); AppTheme::setTone(m_aiStatus,matched?"success":"warning"); updateConnectionControls(); }); updateConnectionControls();
    connect(saveAI,&QPushButton::clicked,this,[this] {
        AISettings settings; settings.baseUrl=m_baseUrl->text(); settings.model=m_model->text(); settings.apiKey=m_key->text();
        QString error; const bool ok=AISettingsStore::save(m_settingsPath,settings,error);
        m_aiStatus->setText(ok?QStringLiteral("已保存。现在可以从主页解析文字或截图。"):error);
        AppTheme::setTone(m_aiStatus,ok?QStringLiteral("success"):QStringLiteral("danger"));
    });
    layout->addWidget(aiCard);
    layout->addStretch();
    auto *about = ShellWidgets::label(QStringLiteral("KURA · 蔵"), m_home, "muted");
    about->setAlignment(Qt::AlignCenter); entries->addSpacing(16); entries->addWidget(about);
    showSection(0);
}

void AppearanceWidget::showSection(int section)
{
    m_home->setVisible(section==0); m_appearanceSection->setVisible(section==1); m_aiSection->setVisible(section==2);
    m_back->setVisible(section!=0);
    m_title->setText(section==1?QStringLiteral("外观"):section==2?QStringLiteral("AI 服务"):QStringLiteral("设置"));
    updateGeometry();
}

void AppearanceWidget::setTheme(const QString &theme)
{
    for (auto *choice : m_choices) choice->setChecked(choice->property("themeId").toString() == theme);
}
void AppearanceWidget::updateConnectionControls()
{
    for (auto *edit:{m_baseUrl,m_model,m_key}) edit->setEnabled(!m_testing); m_probeText->setEnabled(!m_testing); m_probeImage->setEnabled(!m_testing); m_saveAI->setEnabled(!m_testing); m_cancelProbe->setVisible(m_testing); m_cancelProbe->setEnabled(m_testing);
}
void AppearanceWidget::testConnection(bool image)
{
    if (m_testing) return; AISettings settings; settings.baseUrl=m_baseUrl->text(); settings.model=m_model->text(); settings.apiKey=m_key->text(); m_testing=true; m_connectionTimer.start(); m_aiStatus->setText(QStringLiteral("正在测试当前配置…")); AppTheme::setTone(m_aiStatus,"muted"); updateConnectionControls(); bool started=false;
    if (image) {
        QImage sample(680,260,QImage::Format_RGB32); sample.fill(Qt::white); QPainter painter(&sample); painter.setPen(Qt::black); QFont font(QStringLiteral("Microsoft YaHei UI")); font.setPixelSize(26); painter.setFont(font); painter.drawText(QRect(28,24,624,212),Qt::TextWordWrap,QStringLiteral("联调样例（虚构，无真实消费）\n商品：测试笔记本\n金额：12.34 元")); painter.end(); QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); sample.save(&buffer,"JPEG",85); started=m_connection.analyze(settings,QStringLiteral("请只识别这张虚构样例图片里的商品和金额。"),bytes);
    } else started=m_connection.advise(settings,QStringLiteral("这是连接测试，请只回复简短中文连接成功。"),QStringLiteral("请回复：连接成功。"));
    if (!started) { m_testing=false; updateConnectionControls(); }
}
