#include "DecisionChatWidget.h"
#include "AppTheme.h"
#include "ShellWidgets.h"
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QInputMethodEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QTextOption>
#include <QTimer>
#include <QToolButton>
#include <functional>
#include <cmath>

namespace {
class ChatMark : public QWidget {
public:
    explicit ChatMark(QWidget *parent):QWidget(parent) { setFixedSize(16,16); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing); p.scale(0.8,0.8);
        p.setPen(QPen(palette().color(QPalette::WindowText),1.15,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPainterPath path; path.moveTo(9,2); path.lineTo(11,7); path.lineTo(16,9); path.lineTo(11,11); path.lineTo(9,16); path.lineTo(7,11); path.lineTo(2,9); path.lineTo(7,7); path.closeSubpath(); p.drawPath(path);
        p.drawLine(16,1,16,5); p.drawLine(14,3,18,3);
    }
};

class ChatInput : public QPlainTextEdit {
public:
    using QPlainTextEdit::QPlainTextEdit;
    std::function<void()> submit;
protected:
    void inputMethodEvent(QInputMethodEvent *event) override { m_composing=!event->preeditString().isEmpty(); QPlainTextEdit::inputMethodEvent(event); }
    void keyPressEvent(QKeyEvent *event) override {
        if ((event->key()==Qt::Key_Return || event->key()==Qt::Key_Enter) && !(event->modifiers()&Qt::ShiftModifier) && !m_composing) {
            event->accept(); if (submit) submit(); return;
        }
        QPlainTextEdit::keyPressEvent(event);
    }
private:
    bool m_composing=false;
};

// Text grows with the page. Model-provided links and images never open or load
// external resources while rendering a reply.
class ChatMarkdown : public QTextBrowser {
public:
    explicit ChatMarkdown(const QString &markdown,QWidget *parent):QTextBrowser(parent) {
        setObjectName("decisionChatAnswerText"); setFrameShape(QFrame::NoFrame);
        setOpenLinks(false); setOpenExternalLinks(false);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed); setMinimumWidth(0);
        document()->setDocumentMargin(0);
        document()->setDefaultStyleSheet("h1 {font-size:18px;} h2 {font-size:17px;} h3 {font-size:14px;} p {margin-top:0px;margin-bottom:12px;line-height:155%;} ul,ol {margin-top:4px;margin-bottom:12px;} li {margin-bottom:7px;line-height:155%;} blockquote {margin-left:12px;} pre {white-space:pre-wrap;}");
        document()->setMarkdown(markdown,QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub)|QTextDocument::MarkdownNoHTML);
        for (auto block=document()->begin();block.isValid();block=block.next()) {
            QTextCursor cursor(block); auto format=block.blockFormat();
            format.setLineHeight(170,QTextBlockFormat::ProportionalHeight); format.setBottomMargin(block.textList()?8:14);
            cursor.setBlockFormat(format);
            if (format.headingLevel()>0) {
                cursor.setPosition(block.position()); cursor.setPosition(block.position()+block.length()-1,QTextCursor::KeepAnchor);
                QTextCharFormat heading; heading.setFontWeight(QFont::Medium); heading.setProperty(QTextFormat::FontPixelSize,format.headingLevel()<=2?17:14); cursor.mergeCharFormat(heading);
            }
        }
        auto option=document()->defaultTextOption(); option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); document()->setDefaultTextOption(option);
        connect(document()->documentLayout(),&QAbstractTextDocumentLayout::documentSizeChanged,this,[this] { fit(); });
        QTimer::singleShot(0,this,[this] { fit(); });
    }
protected:
    QVariant loadResource(int,const QUrl &) override { return {}; }
    void resizeEvent(QResizeEvent *event) override { QTextBrowser::resizeEvent(event); fit(); }
private:
    void fit() {
        if (m_fitting) return; m_fitting=true;
        document()->setDefaultFont(font()); document()->setTextWidth(qMax(80,viewport()->width()));
        const int height=int(std::ceil(document()->size().height()))+4;
        if (this->height()!=height) setFixedHeight(height);
        m_fitting=false;
    }
    bool m_fitting=false;
};

QPair<QString,QString> splitAnswer(const QString &text)
{
    const QRegularExpression heading(QStringLiteral("(?:^|\n)(?:#{1,6}\\s*|\\*\\*)?(?:补充分析|完整分析|详细分析)(?:\\*\\*)?[^\n]*(?:\n|$)"));
    const auto match=heading.match(text);
    if (match.hasMatch() && match.capturedStart()>80) return {text.left(match.capturedStart()).trimmed(),text.mid(match.capturedEnd()).trimmed()};
    if (text.size()<=850) return {text,{}};
    int cut=text.lastIndexOf(QStringLiteral("\n\n"),850);
    if (cut<300) cut=text.lastIndexOf(QChar(0x3002),850)+1;
    if (cut<300) return {text,{}};
    return {text.left(cut).trimmed(),text.mid(cut).trimmed()};
}

void clearLayout(QVBoxLayout *layout)
{
    while (auto *item=layout->takeAt(0)) { if (item->widget()) delete item->widget(); delete item; }
}
}

DecisionChatWidget::DecisionChatWidget(QWidget *parent):QFrame(parent)
{
    setObjectName("decisionChatWidget"); setProperty("card",true); setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Maximum);
    auto *layout=new QVBoxLayout(this); layout->setContentsMargins(20,18,20,14); layout->setSpacing(0); layout->setAlignment(Qt::AlignTop);
    auto *header=new QHBoxLayout;
    header->addWidget(new ChatMark(this)); auto *title=ShellWidgets::label(QStringLiteral("决策助手"),this); title->setObjectName("decisionChatTitle"); header->addWidget(title,1);
    m_toggle=new QToolButton(this); m_toggle->setObjectName("executionAdviceToggle"); m_toggle->setAccessibleName(QStringLiteral("展开或折叠决策助手"));
    m_toggle->setCheckable(true); m_toggle->setChecked(true); m_toggle->setArrowType(Qt::UpArrow); header->addWidget(m_toggle); layout->addLayout(header);
    m_body=new QWidget(this); m_body->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Maximum); auto *bodyLayout=new QVBoxLayout(m_body); bodyLayout->setContentsMargins(0,0,0,0); bodyLayout->setSpacing(14); bodyLayout->setAlignment(Qt::AlignTop);
    auto *context=ShellWidgets::label(QStringLiteral("结合这次购买的预算与购物原则"),m_body,"eyebrow"); context->setObjectName("decisionChatContext"); bodyLayout->addWidget(context);
    auto *divider=new QFrame(m_body); divider->setObjectName("executionDivider"); divider->setFixedHeight(1); bodyLayout->addWidget(divider);
    m_empty=new QWidget(m_body); auto *emptyLayout=new QVBoxLayout(m_empty); emptyLayout->setContentsMargins(4,20,4,10); emptyLayout->setSpacing(10);
    auto *emptyTitle=ShellWidgets::label(QStringLiteral("还有哪里拿不准？"),m_empty); emptyTitle->setObjectName("decisionChatEmptyTitle"); emptyTitle->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyTitle);
    auto *emptyHint=ShellWidgets::label(QStringLiteral("预算、用途，或一个替代选择，都可以聊聊。"),m_empty,"eyebrow"); emptyHint->setAlignment(Qt::AlignCenter); emptyLayout->addWidget(emptyHint); bodyLayout->addWidget(m_empty);
    m_transcript=new QWidget(m_body); m_transcript->setObjectName("decisionChatMessages"); m_messageLayout=new QVBoxLayout(m_transcript); m_messageLayout->setContentsMargins(0,0,0,0); m_messageLayout->setSpacing(22); bodyLayout->addWidget(m_transcript);
    m_suggestions=new QWidget(m_body); m_suggestionLayout=new QBoxLayout(QBoxLayout::LeftToRight,m_suggestions); m_suggestionLayout->setContentsMargins(0,0,0,0); m_suggestionLayout->setSpacing(8);
    m_analyze=new QPushButton(QStringLiteral("帮我分析这次购买  ↗"),m_suggestions); m_analyze->setObjectName("generateAIBriefButton");
    m_budget=new QPushButton(QStringLiteral("这笔钱会影响什么？  ↗"),m_suggestions); m_budget->setObjectName("decisionBudgetPromptButton");
    m_alternative=new QPushButton(QStringLiteral("先帮我比较替代方案  ↗"),m_suggestions); m_alternative->setObjectName("decisionAlternativePromptButton");
    for (auto *button:{m_analyze,m_budget,m_alternative}) { button->setProperty("chatPrompt",true); m_suggestionLayout->addWidget(button); }
    m_suggestionLayout->addStretch(); bodyLayout->addWidget(m_suggestions);
    auto *composer=new QFrame(m_body); composer->setObjectName("decisionChatComposer"); composer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed); auto *composerLayout=new QVBoxLayout(composer); composerLayout->setContentsMargins(12,10,12,9); composerLayout->setSpacing(7);
    auto *input=new ChatInput(composer); m_input=input; input->setObjectName("decisionQuestionEdit"); input->setAccessibleName(QStringLiteral("给决策助手的消息")); input->setPlaceholderText(QStringLiteral("继续问，或说说你在纠结什么…"));
    input->setFixedHeight(52); input->setTabChangesFocus(true); input->setFrameShape(QFrame::NoFrame); composerLayout->addWidget(input);
    auto *sendRow=new QHBoxLayout;
    auto *hint=ShellWidgets::label(QStringLiteral("Enter 发送 · Shift + Enter 换行"),composer,"eyebrow"); hint->setObjectName("decisionChatKeyHint"); sendRow->addWidget(hint,1);
    m_send=new QPushButton(QStringLiteral("发送 ↑"),composer); m_send->setObjectName("askDecisionAIButton"); m_send->setProperty("primary",true); sendRow->addWidget(m_send); composerLayout->addLayout(sendRow); bodyLayout->addWidget(composer);
    auto *statusRow=new QHBoxLayout; m_status=ShellWidgets::label(QStringLiteral("帮你想清楚，决定仍由你。"),m_body,"eyebrow"); m_status->setObjectName("decisionChatStatus"); statusRow->addWidget(m_status,1);
    m_retry=new QPushButton(QStringLiteral("重试"),m_body); m_retry->setObjectName("decisionChatRetryButton"); m_retry->setProperty("executionQuiet",true); m_retry->hide(); statusRow->addWidget(m_retry); bodyLayout->addLayout(statusRow); layout->addWidget(m_body);
    connect(m_toggle,&QToolButton::toggled,this,[this](bool expanded) { m_body->setVisible(expanded); m_toggle->setArrowType(expanded?Qt::UpArrow:Qt::DownArrow); });
    input->submit=[this] { submitDraft(); };
    connect(m_input,&QPlainTextEdit::textChanged,this,[this] {
        const int height=qBound(52,int(m_input->document()->size().height())*m_input->fontMetrics().lineSpacing()+8,140);
        m_input->setFixedHeight(height); updateControls();
    });
    connect(m_send,&QPushButton::clicked,this,[this] { if (m_busy) emit cancelRequested(); else submitDraft(); });
    connect(m_analyze,&QPushButton::clicked,this,[this] { emit requestSubmitted(QStringLiteral("帮我分析这次购买"),true,false); });
    connect(m_budget,&QPushButton::clicked,this,[this] { emit requestSubmitted(QStringLiteral("这笔钱对本月预算有什么影响？"),false,false); });
    connect(m_alternative,&QPushButton::clicked,this,[this] { emit requestSubmitted(QStringLiteral("请帮我比较替代方案，先从我的实际用途出发。"),false,false); });
    connect(m_retry,&QPushButton::clicked,this,&DecisionChatWidget::retry);
    renderMessages(); updateControls();
}

void DecisionChatWidget::setAvailable(bool available) { m_available=available; updateControls(); }
bool DecisionChatWidget::isExpanded() const { return m_toggle->isChecked(); }
void DecisionChatWidget::setExpanded(bool expanded) { m_toggle->setChecked(expanded); }

void DecisionChatWidget::submitDraft()
{
    const auto text=m_input->toPlainText().trimmed();
    if (!m_available || m_busy || text.isEmpty()) return;
    if (text.size()>2000) { m_status->setText(QStringLiteral("问题最多 2000 字，请精简后发送。")); AppTheme::setTone(m_status,"warning"); return; }
    emit requestSubmitted(text,false,false);
}

void DecisionChatWidget::beginRequest(const QString &question,bool brief,bool regenerate)
{
    const bool samePending=!m_messages.isEmpty() && m_messages.last().role=="user" && m_messages.last().text==question;
    m_regenerateIndex=-1;
    if (regenerate) for (int i=m_messages.size()-1;i>=0;--i) if (m_messages[i].role=="assistant") { m_regenerateIndex=i; break; }
    if (!brief && !regenerate && !samePending) m_messages.append({"user",question,{},false});
    m_question=question; m_brief=brief; m_busy=true; m_failed=false; m_lastError.clear();
    if (m_input->toPlainText().trimmed()==question) m_input->clear();
    setExpanded(true); m_status->clear(); AppTheme::setTone(m_status,{}); renderMessages(); updateControls(); reveal(m_input,true);
}

void DecisionChatWidget::completeRequest(const QString &answer)
{
    if (!m_busy) return;
    const Message message{"assistant",answer,m_question,m_brief};
    if (m_regenerateIndex>=0 && m_regenerateIndex<m_messages.size()) m_messages[m_regenerateIndex]=message;
    else m_messages.append(message);
    m_regenerateIndex=-1; m_busy=false; m_failed=false;
    m_status->setText(QStringLiteral("帮你想清楚，决定仍由你。")); AppTheme::setTone(m_status,{}); renderMessages(); updateControls();
}

void DecisionChatWidget::failRequest(const QString &message)
{
    m_busy=false; m_failed=true; m_lastError=message;
    if (!m_brief && m_input->toPlainText().isEmpty()) m_input->setPlainText(m_question);
    m_status->setText(message); AppTheme::setTone(m_status,"warning"); renderMessages(); updateControls();
}

void DecisionChatWidget::clearConversation(const QString &notice)
{
    m_messages.clear(); m_busy=false; m_failed=false; m_regenerateIndex=-1; m_question.clear(); m_lastError.clear();
    m_status->setText(notice.isEmpty()?QStringLiteral("帮你想清楚，决定仍由你。"):notice); AppTheme::setTone(m_status,{}); renderMessages(); updateControls();
}

void DecisionChatWidget::retry()
{
    if (m_available && !m_busy && !m_question.isEmpty()) emit requestSubmitted(m_question,m_brief,m_regenerateIndex>=0);
}

void DecisionChatWidget::updateControls()
{
    m_input->setEnabled(m_available); m_send->setEnabled(m_available && (m_busy || !m_input->toPlainText().trimmed().isEmpty()));
    const auto label=m_busy?QStringLiteral("停止 □"):QStringLiteral("发送 ↑");
    if (m_send->text()!=label) { m_send->setText(label); m_send->setProperty("primary",!m_busy); m_send->style()->unpolish(m_send); m_send->style()->polish(m_send); }
    for (auto *button:{m_analyze,m_budget,m_alternative}) button->setEnabled(m_available && !m_busy);
    m_analyze->setVisible(m_messages.isEmpty()); m_alternative->setVisible(!m_messages.isEmpty()); m_suggestions->setVisible(!m_busy);
    m_retry->setVisible(m_failed && !m_question.isEmpty()); m_retry->setEnabled(m_available && !m_busy);
    for (auto *button:findChildren<QPushButton *>("decisionChatRegenerateButton")) button->setEnabled(m_available && !m_busy);
}

void DecisionChatWidget::renderMessages()
{
    clearLayout(m_messageLayout); m_empty->setVisible(m_messages.isEmpty() && !m_busy); m_transcript->setVisible(!m_messages.isEmpty() || m_busy);
    int lastAssistant=-1; for (int i=0;i<m_messages.size();++i) if (m_messages[i].role=="assistant") lastAssistant=i;
    for (int i=0;i<m_messages.size();++i) {
        const auto message=m_messages[i]; auto *row=new QWidget(m_transcript); auto *rowLayout=new QVBoxLayout(row); rowLayout->setContentsMargins(0,0,0,0); rowLayout->setSpacing(7);
        if (message.role=="user") {
            row->setObjectName("decisionChatUserMessage");
            auto *role=ShellWidgets::label(QStringLiteral("你"),row,"eyebrow"); role->setAlignment(Qt::AlignRight); rowLayout->addWidget(role);
            auto *line=new QHBoxLayout; line->setContentsMargins(0,0,0,0); line->addStretch(1);
            auto *bubble=new QFrame(row); bubble->setProperty("chatUserBubble",true); bubble->setMaximumWidth(qMax(140,int(width()*.85)-40));
            auto *bubbleLayout=new QVBoxLayout(bubble); bubbleLayout->setContentsMargins(13,10,13,10);
            auto *text=ShellWidgets::label(message.text,bubble); text->setObjectName("decisionChatUserText"); text->setMinimumWidth(0); text->setTextInteractionFlags(Qt::TextSelectableByMouse); bubbleLayout->addWidget(text); line->addWidget(bubble); rowLayout->addLayout(line);
            text->ensurePolished(); bubble->setFixedWidth(qMin(qMax(140,int(width()*.85)-40),text->fontMetrics().horizontalAdvance(message.text)+28));
        } else {
            row->setObjectName("decisionChatAssistantMessage");
            auto *roleRow=new QHBoxLayout; roleRow->setSpacing(7); roleRow->addWidget(new ChatMark(row)); auto *role=ShellWidgets::label(QStringLiteral("KURA"),row); role->setObjectName("decisionChatRole"); roleRow->addWidget(role); roleRow->addStretch(); rowLayout->addLayout(roleRow);
            const auto parts=splitAnswer(message.text); rowLayout->addWidget(new ChatMarkdown(parts.first,row));
            if (!parts.second.isEmpty()) {
                auto *toggle=new QToolButton(row); toggle->setObjectName("decisionChatExpandAnswerButton"); toggle->setText(QStringLiteral("展开完整分析")); toggle->setProperty("executionDisclosure",true); toggle->setArrowType(Qt::RightArrow); toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); toggle->setCheckable(true);
                auto *details=new ChatMarkdown(parts.second,row); details->setObjectName("decisionChatAnswerDetails"); details->hide(); rowLayout->addWidget(toggle,0,Qt::AlignLeft); rowLayout->addWidget(details);
                connect(toggle,&QToolButton::toggled,details,[toggle,details](bool open) { details->setVisible(open); toggle->setText(open?QStringLiteral("收起完整分析"):QStringLiteral("展开完整分析")); toggle->setArrowType(open?Qt::DownArrow:Qt::RightArrow); });
            }
            auto *actions=new QHBoxLayout; actions->setSpacing(14);
            auto *copy=new QPushButton(QStringLiteral("复制"),row); copy->setObjectName("decisionChatCopyButton"); copy->setProperty("executionQuiet",true); actions->addWidget(copy);
            connect(copy,&QPushButton::clicked,this,[this,text=message.text] { QTextDocument plain; plain.setMarkdown(text,QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub)|QTextDocument::MarkdownNoHTML); QApplication::clipboard()->setText(plain.toPlainText()); m_status->setText(QStringLiteral("已复制这条回复。")); });
            if (i==lastAssistant && i==m_messages.size()-1) {
                auto *again=new QPushButton(QStringLiteral("重新回答"),row); again->setObjectName("decisionChatRegenerateButton"); again->setProperty("executionQuiet",true); again->setEnabled(m_available && !m_busy); actions->addWidget(again);
                connect(again,&QPushButton::clicked,this,[this,message] { emit requestSubmitted(message.question.isEmpty()?QStringLiteral("帮我分析这次购买"):message.question,message.brief,true); });
            }
            actions->addStretch(); rowLayout->addLayout(actions);
        }
        m_messageLayout->addWidget(row);
    }
    if (m_busy) {
        auto *thinking=ShellWidgets::label(QStringLiteral("KURA · 正在帮你梳理…"),m_transcript,"muted"); thinking->setObjectName("decisionChatThinkingLabel"); m_messageLayout->addWidget(thinking);
    }
}

QString DecisionChatWidget::serialize() const
{
    if (m_messages.isEmpty() && m_input->toPlainText().isEmpty()) return {};
    QJsonArray messages;
    for (const auto &message:m_messages) messages.append(QJsonObject{{"role",message.role},{"text",message.text},{"question",message.question},{"brief",message.brief}});
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"format","kura-decision-chat"},{"version",1},{"messages",messages},{"draft",m_input->toPlainText()},{"expanded",isExpanded()}}).toJson(QJsonDocument::Compact));
}

void DecisionChatWidget::restoreConversation(const QString &saved)
{
    clearConversation(); m_input->clear(); setExpanded(true);
    const auto document=QJsonDocument::fromJson(saved.toUtf8());
    if (document.isObject() && document["format"]==QStringLiteral("kura-decision-chat") && document["version"].toInt()==1 && document["messages"].isArray()) {
        for (const auto &value:document["messages"].toArray()) {
            const auto object=value.toObject(); const auto role=object["role"].toString(),text=object["text"].toString();
            if ((role=="user" || role=="assistant") && !text.isEmpty()) m_messages.append({role,text,object["question"].toString(),object["brief"].toBool()});
        }
        m_input->setPlainText(document["draft"].toString()); setExpanded(document["expanded"].toBool(true));
    } else if (!saved.trimmed().isEmpty()) {
        // Read old single-turn text without showing its internal briefing prompt.
        const int marker=saved.indexOf(QStringLiteral("\n\nAI："));
        if (saved.startsWith(QStringLiteral("你：")) && marker>=0) {
            const auto question=saved.mid(2,marker-2).trimmed(); const bool brief=question.startsWith(QStringLiteral("请根据事实给出简短的决策简报"));
            if (!brief) m_messages.append({"user",question,{},false});
            m_messages.append({"assistant",saved.mid(marker+5).trimmed(),brief?QStringLiteral("帮我分析这次购买"):question,brief});
        } else m_messages.append({"assistant",saved,QStringLiteral("帮我分析这次购买"),true});
    }
    renderMessages(); updateControls();
}

QString DecisionChatWidget::plainText() const
{
    QStringList lines; for (const auto &message:m_messages) lines << message.text; return lines.join(QStringLiteral("\n\n"));
}

QJsonArray DecisionChatWidget::requestHistory(bool regenerate) const
{
    int last=-1;
    for (int i=m_messages.size()-1;i>=0;--i) if (m_messages[i].role=="assistant") { last=i; break; }
    if (regenerate) --last;
    QJsonArray history;
    for (int i=0;i<=last;++i) {
        const auto &message=m_messages[i];
        if (message.role=="assistant" && message.brief) history.append(QJsonObject{{"role","user"},{"content",message.question.isEmpty()?QStringLiteral("帮我分析这次购买"):message.question}});
        history.append(QJsonObject{{"role",message.role},{"content",message.text}});
    }
    int length=0; QJsonArray recent;
    for (int i=history.size()-1;i>=0 && recent.size()<8;--i) { const auto value=history[i].toObject(); const int size=value.value("content").toString().size(); if (length+size>24000) break; recent.prepend(value); length+=size; }
    while (!recent.isEmpty() && recent.first()["role"].toString()!="user") recent.removeFirst();
    // Regeneration must not repeat the user turn being regenerated.
    if (regenerate && !recent.isEmpty() && recent.last()["role"].toString()=="user") recent.removeLast();
    return recent;
}

void DecisionChatWidget::reveal(QWidget *target,bool follow)
{
    if (!follow) return;
    QTimer::singleShot(0,this,[this,target] { for (auto *parent=parentWidget();parent;parent=parent->parentWidget()) if (auto *scroll=qobject_cast<QScrollArea *>(parent)) { if (isVisible()) scroll->ensureWidgetVisible(target,0,24); break; } });
}

void DecisionChatWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event); m_suggestionLayout->setDirection(width()<480?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
    for (auto *bubble:findChildren<QFrame *>()) if (bubble->property("chatUserBubble").toBool()) if (auto *text=bubble->findChild<QLabel *>("decisionChatUserText")) bubble->setFixedWidth(qMin(qMax(140,int(width()*.85)-40),text->fontMetrics().horizontalAdvance(text->text())+28));
    fitContent();
}

bool DecisionChatWidget::event(QEvent *event)
{
    const bool handled=QFrame::event(event);
    if (event->type()==QEvent::LayoutRequest || event->type()==QEvent::Show) fitContent();
    return handled;
}

void DecisionChatWidget::fitContent()
{
    if (m_reflowScheduled || !layout()) return;
    m_reflowScheduled=true;
    QTimer::singleShot(0,this,[this] {
        layout()->activate();
        const int wanted=layout()->hasHeightForWidth()?layout()->totalHeightForWidth(width()):layout()->sizeHint().height();
        if (wanted>0 && height()!=wanted) setFixedHeight(wanted);
        for (auto *parent=parentWidget();parent && !qobject_cast<QScrollArea *>(parent);parent=parent->parentWidget()) {
            if (parent->layout()) parent->layout()->invalidate(); parent->updateGeometry();
        }
        m_reflowScheduled=false;
    });
}
