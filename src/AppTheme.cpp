#include "AppTheme.h"
#include "AppTypography.h"
#include "DialogUi.h"
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QWidget>
#include <QStyle>
#include <QTableWidget>
#include <QAbstractButton>
#include <QMap>

namespace {
struct Colors { QString page, card, inset, ink, secondary, muted, placeholder, border, accent, onAccent, highlight, progress, track; };
Colors colors(const QString &theme)
{
    if (theme == "dark") return {"#141414", "#1e1e1e", "#252525", "#edede8", "#a8a89e", "#6a6a62", "#4a4a44", "#2e2e2e", "#edede8", "#141414", "#35352f", "#a8a89e", "#2e2e2e"};
    if (theme == "cool") return {"#f0f2f5", "#fafbfc", "#e4e8ef", "#2c3340", "#4a5260", "#8a96a8", "#a0aabb", "#d0d4da", "#4a5260", "#f0f2f5", "#dde5ee", "#6b7a90", "#c8cdd6"};
    return {"#f5f2ec", "#fffdf8", "#eee9e0", "#3e372f", "#74695f", "#93877a", "#aaa093", "#ded7cb", "#b87359", "#fffaf5", "#eee1d4", "#ac8d72", "#e7e0d5"};
}
}

QString AppTheme::normalized(const QString &theme)
{
    return theme == "cool" || theme == "dark" ? theme : QStringLiteral("warm");
}

void AppTheme::apply(QWidget *root, const QString &theme)
{
    const auto c = colors(normalized(theme));
    AppTypography::initialize();
    DialogUi::install();
    root->setProperty("kuraTypography", true);
    root->setFont(AppTypography::sans(13));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(c.page));
    palette.setColor(QPalette::WindowText, QColor(c.ink));
    palette.setColor(QPalette::Text, QColor(c.ink));
    palette.setColor(QPalette::ButtonText, QColor(c.ink));
    palette.setColor(QPalette::Button, QColor(c.card));
    palette.setColor(QPalette::Base, QColor(c.card));
    palette.setColor(QPalette::AlternateBase, QColor(c.inset));
    palette.setColor(QPalette::Highlight, QColor(c.highlight));
    palette.setColor(QPalette::HighlightedText, QColor(c.ink));
    palette.setColor(QPalette::Mid, QColor(c.border));
    palette.setColor(QPalette::Link, QColor(c.accent));
    palette.setColor(QPalette::PlaceholderText, QColor(c.placeholder));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(c.muted));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(c.muted));
    root->setPalette(palette);
    QString style = QStringLiteral(R"(
        QWidget { color: @ink; font-family: 'DM Sans', 'Microsoft YaHei'; font-size: 13px; font-weight: 400; }
        QMainWindow, QDialog, QWidget[page="true"], QStackedWidget { background: @page; }
        QLabel { background: transparent; }
        QLabel[muted="true"] { color: @muted; font-size: 13px; }
        QLabel[eyebrow="true"] { color: @muted; font-size: 11px; font-weight: 500; letter-spacing: 1.54px; }
        QLabel[heading="true"] { font-size: 16px; font-weight: 500; }
        QLabel[figure="true"] { font-family: 'DM Serif Display'; font-size: 52px; font-weight: 400; }
        QLabel[secondary="true"] { color: @secondary; }
        QLabel[error="true"], QLabel[tone="danger"] { color: @danger; }
        QLabel[tone="warning"] { color: @warning; }
        QLabel[tone="success"] { color: @success; }
        QLabel[tone="muted"] { color: @muted; }
        QFrame[card="true"] { background: @card; border: 1px solid @border; border-radius: 14px; }
        QFrame[cardAlt="true"] { background: @inset; border: none; border-radius: 14px; }
        QFrame#shellHeader { background: @page; border-bottom: 1px solid @border; }
        QFrame#shellFooter { background: @page; border-top: 1px solid @border; }
        QLabel#brandWordmark { font-family: 'DM Serif Display'; font-size: 22px; font-weight: 400; letter-spacing: -0.22px; }
        QToolButton { background: transparent; border: 1px solid transparent; border-radius: 8px; color: @muted; padding: 6px; font-size: 12px; font-weight: 500; }
        QToolButton:hover { background: @inset; }
        QToolButton:checked { color: @ink; background: transparent; }
        QToolButton:focus { border: 1px solid @accent; }
        QToolButton#settingsButton { padding: 8px; }
        QPushButton { min-height: 22px; padding: 6px 12px; border: 1px solid @border; border-radius: 8px; background: @card; color: @secondary; }
        QPushButton:hover { background: @inset; border-color: @muted; }
        QPushButton:pressed { background: @highlight; }
        QPushButton:focus { border-color: @accent; }
        QPushButton[primary="true"], QPushButton#addTransactionButton, QPushButton#addWishButton,
        QPushButton#saveButton, QPushButton#saveWishButton, QPushButton#saveBudgetButton,
        QPushButton#exportCsvButton { background: @accent; color: @onAccent; border-color: @accent; }
        QPushButton[choice="true"] { text-align: left; padding: 16px; font-size: 14px; }
        QPushButton[themeChoice="true"] { min-height: 64px; }
        QPushButton[themeChoice="true"] { background: @inset; font-size: 13px; }
        QPushButton[themeChoice="true"]:checked { border: 1px solid @accent; background: @accent; color: @onAccent; }
        QPushButton[link="true"] { background: transparent; border: 1px solid transparent; padding: 2px 0; min-height: 18px; color: @muted; font-size: 12px; }
        QPushButton[link="true"]:hover { color: @ink; }
        QPushButton[link="true"]:focus { border-color: @accent; }
        QWidget#homeActionBar { background: @page; border-top: 1px solid @border; }
        QPushButton#homeDecisionButton { background: @inset; border-radius: 12px; color: @secondary; font-size: 15px; font-weight: 500; padding: 0; min-height: 46px; max-height: 46px; }
        QPushButton#homeDecisionButton:hover { color: @ink; border-color: @muted; }
        QLabel#homeBudgetRemainingLabel[empty="true"] { font-family: 'DM Sans', 'Microsoft YaHei'; font-size: 15px; color: @muted; }
        QLabel#poolNameLabel { font-family: 'DM Serif Display'; font-size: 16px; font-weight: 400; }
        QLabel#poolAmountLabel { color: @muted; }
        QPushButton:disabled, QPushButton[primary="true"]:disabled, QPushButton#addTransactionButton:disabled,
        QPushButton#addWishButton:disabled, QPushButton#saveButton:disabled, QPushButton#saveWishButton:disabled,
        QPushButton#saveBudgetButton:disabled, QPushButton#exportCsvButton:disabled { color: @muted; background: @inset; border-color: @border; }
        QPushButton#deleteTransactionButton, QPushButton#deleteWishButton { color: #bf6252; }
        QLineEdit, QComboBox, QAbstractSpinBox, QPlainTextEdit { border: 1px solid @border; border-radius: 8px; padding: 7px; background: @card; color: @ink; selection-background-color: @highlight; selection-color: @ink; }
        QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus, QPlainTextEdit:focus { border-color: @accent; }
        QAbstractSpinBox { padding-right: 20px; min-height: 24px; }
        QComboBox { padding-right: 24px; min-height: 24px; }
        QAbstractSpinBox:disabled, QComboBox:disabled { color: @muted; background: @inset; }
        QComboBox QAbstractItemView { background: @card; color: @ink; selection-background-color: @highlight; }
        QGroupBox { background: @card; border: 1px solid @border; border-radius: 12px; margin-top: 12px; padding: 14px; }
        QGroupBox::title { subcontrol-origin: margin; left: 16px; padding: 0 7px; color: @muted; }
        QTableWidget { border: 1px solid @border; border-radius: 10px; background: @card; alternate-background-color: @card; gridline-color: @border; selection-background-color: @highlight; selection-color: @ink; }
        QTableWidget::item { padding: 8px; border-bottom: 1px solid @border; }
        QHeaderView::section { background: @card; color: @muted; border: none; border-bottom: 1px solid @border; padding: 9px; font-size: 11px; font-weight: 400; }
        QTableCornerButton::section { background: @inset; border: none; }
        QScrollArea { border: none; background: @page; }
        QScrollBar:vertical { background: transparent; width: 5px; margin: 0; }
        QScrollBar::handle:vertical { background: @border; min-height: 30px; border-radius: 2px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QProgressBar { background: @track; border: none; border-radius: 1px; min-height: 2px; max-height: 2px; }
        QProgressBar::chunk { background: @progress; border-radius: 1px; }
        QProgressBar#poolProgress { background: #f5deb3; }
        QProgressBar#poolProgress::chunk { background: #d6a44f; }
        QTabWidget::pane { background: @page; border: none; }
        QTabBar::tab { padding: 9px 20px; margin: 4px 2px; color: @muted; background: @inset; border-radius: 8px; font-size: 12px; }
        QTabBar::tab:selected { color: @ink; background: @card; }
        QTabWidget#reviewSectionsTabs::pane { border: none; background: @page; }
        QLabel#monthlyReviewTitleLabel { font-size: 16px; font-weight: 500; }
        QLabel#monthlyStoryTitleLabel { font-size: 16px; font-weight: 500; }
        QLabel#monthlyStoryText { color: @secondary; font-size: 15px; font-weight: 400; }
        QLabel#monthlyStoryPersonaLabel { color: @secondary; font-size: 16px; font-weight: 500; }
        QLabel#monthlyStoryInterludeLabel { color: @secondary; font-size: 13px; }
        QLabel#monthlyStoryNextStepLabel { color: @ink; font-size: 13px; }
        QWidget#monthlyReviewWidget QLabel[eyebrow="true"] { color: @secondary; font-size: 11px; letter-spacing: 1.54px; font-weight: 500; }
        QWidget#monthlyReviewWidget QLabel[muted="true"] { color: @secondary; }
        QLabel#monthlyStorySourceLabel[tone="warning"] { color: @warning; }
        QLabel#monthlyReviewExpenseNameLabel { font-size: 15px; }
        QLabel#monthlyReviewExpenseNoteLabel { font-size: 13px; }
        QLabel#monthlyStorySourceLabel { font-size: 11px; }
        QLabel[reviewFigure="true"] { font-family: 'DM Serif Display'; font-size: 22px; font-weight: 400; }
        QLabel[reviewPrice="true"] { font-family: 'DM Serif Display'; font-size: 15px; }
        QDateEdit#monthlyReviewMonthEdit { border: none; background: transparent; padding: 0; min-height: 30px; font-size: 14px; }
        QDateEdit#monthlyReviewMonthEdit:focus { background: @inset; }
        QPushButton[reviewMonthArrow="true"] { border: none; background: transparent; padding: 0; min-height: 0; color: @muted; font-size: 25px; }
        QPushButton[reviewLink="true"] { border: none; background: transparent; color: @muted; padding: 3px 4px; min-height: 22px; font-size: 12px; }
        QPushButton[reviewLink="true"]:hover, QPushButton[reviewMonthArrow="true"]:hover { background: @inset; color: @ink; }
        QPushButton[reviewLink="true"]:focus, QPushButton[reviewMonthArrow="true"]:focus { border: 1px solid @accent; }
        QPushButton[reviewLink="true"]:disabled, QPushButton[reviewMonthArrow="true"]:disabled { background: transparent; color: @muted; }
        QPushButton[reviewTab="true"] { border: none; background: transparent; color: @muted; padding: 5px 13px; min-height: 23px; font-size: 13px; border-radius: 7px; }
        QPushButton[reviewTab="true"]:checked { background: @inset; color: @ink; }
        QPushButton[reviewTab="true"]:focus { border: 1px solid @accent; }
        QToolButton#monthlyStoryMoreButton { padding: 2px 7px; font-size: 16px; }
        QToolButton#monthlyStoryMoreButton::menu-indicator { image: none; }
        QMenu { background: @card; color: @ink; border: 1px solid @border; padding: 5px; }
        QMenu::item { padding: 8px 15px; }
        QMenu::item:selected { background: @inset; }
        QFrame[reviewDivider="true"] { border: none; background: @border; }
        QStatusBar { background: @page; color: @muted; font-size: 11px; }
        QStatusBar::item { border: none; }
        QToolTip { background: @card; color: @ink; border: 1px solid @border; padding: 6px; }
        /* The execution flow keeps one active card and uses quiet disclosures
           for optional notes, AI and reference details in every theme. */
        QLabel#executionHeading { font-size: 16px; font-weight: 500; }
        QLineEdit#executionNameEdit { background: transparent; border: none; border-bottom: 1px solid @border; border-radius: 0; padding: 3px 0; min-height: 28px; font-size: 15px; }
        QLineEdit#executionNameEdit:focus { border-bottom-color: @muted; }
        QLabel#executionModeProductLabel { color: @secondary; font-size: 15px; }
        QLabel#executionTimerLabel { font-family: 'DM Serif Display'; font-size: 48px; font-weight: 400; }
        QPushButton[executionQuiet="true"] { background: transparent; border: none; padding: 3px 0; min-height: 22px; color: @muted; font-size: 11px; }
        QPushButton[executionQuiet="true"]:hover { color: @ink; }
        QPushButton[executionQuiet="true"]:focus { border-bottom: 1px solid @accent; }
        QToolButton[executionDisclosure="true"] { background: transparent; color: @muted; border: none; border-radius: 4px; padding: 7px 0; min-height: 20px; text-align: left; font-size: 12px; }
        QToolButton[executionDisclosure="true"]:hover { color: @ink; }
        QToolButton[executionDisclosure="true"]:focus { border-bottom: 1px solid @accent; }
        QToolButton[executionDisclosure="true"]:checked { background: transparent; color: @ink; }
        QFrame#executionDecisionCard, QFrame#executionInputCard, QFrame#executionModeCard, QFrame#executionRulesCard, QFrame#executionOpeningBriefCard { border-radius: 16px; }
        QFrame#executionBudgetPanel { background: @inset; border: none; border-radius: 12px; }
        QFrame#executionDivider { background: @border; border: none; }
        QLabel#executionProductTitle { font-size: 15px; font-weight: 500; }
        QLabel#executionProductPrice { font-family: 'DM Serif Display'; font-size: 22px; font-weight: 400; }
        QLabel#executionProductPurposeLabel { font-size: 12px; }
        QLabel[executionAmount="true"] { font-family: 'DM Serif Display'; font-size: 22px; font-weight: 400; }
        QLabel#executionBudgetTitleLabel { font-size: 13px; }
        QPushButton[executionModeChoice="true"] { padding: 12px; text-align: left; font-size: 14px; }
        QPushButton[executionModeChoice="true"]:checked { background: @inset; border-color: @accent; }
        QPushButton[executionModeChip="true"] { padding: 8px 12px; background: transparent; color: @secondary; font-size: 14px; }
        QPushButton[executionModeChip="true"]:checked { border-color: @accent; background: @inset; color: @ink; }
        QWidget#executionWidget QCheckBox { spacing: 10px; padding: 4px 0; font-size: 13px; }
        QWidget#executionWidget QCheckBox::indicator { width: 15px; height: 15px; border: 1px solid @border; border-radius: 3px; background: @card; }
        QWidget#executionWidget QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
        QWidget#executionWidget QCheckBox:checked { color: @muted; }
        QPushButton#executionWishButton, QPushButton#executionBuyButton, QPushButton#executionNextButton, QPushButton#startExecutionButton { min-height: 30px; }
        QFrame#decisionChatWidget { border-radius: 18px; }
        QLabel#decisionChatTitle { font-size: 15px; font-weight: 500; }
        QLabel#decisionChatContext { font-size: 11px; color: @muted; }
        QToolButton#executionAdviceToggle { padding: 3px; min-width: 22px; min-height: 22px; background: transparent; border: none; }
        QToolButton#executionAdviceToggle:focus { border: 1px solid @accent; }
        QLabel#decisionChatEmptyTitle { font-size: 18px; font-weight: 500; }
        QLabel#decisionChatRole { font-family: 'DM Serif Display'; color: @secondary; font-size: 12px; }
        QFrame[chatUserBubble="true"] { background: @inset; border: none; border-radius: 12px; border-bottom-right-radius: 3px; }
        QLabel#decisionChatUserText { color: @ink; font-size: 14px; }
        QTextBrowser#decisionChatAnswerText, QTextBrowser#decisionChatAnswerDetails { background: transparent; border: none; padding: 0; color: @secondary; font-size: 14px; selection-background-color: @highlight; selection-color: @ink; }
        QPushButton[chatPrompt="true"] { background: @card; border: 1px solid @border; border-radius: 7px; padding: 4px 8px; min-height: 22px; color: @secondary; font-size: 11px; }
        QPushButton[chatPrompt="true"]:hover { background: @inset; }
        QFrame#decisionChatComposer { background: @card; border: 1px solid @border; border-radius: 12px; }
        QPlainTextEdit#decisionQuestionEdit { background: transparent; border: none; border-radius: 0; padding: 0; color: @ink; font-size: 14px; }
        QPlainTextEdit#decisionQuestionEdit:focus { border-bottom: 1px solid @muted; }
        QLabel#decisionChatKeyHint { font-size: 10px; color: @muted; }
        QPushButton#askDecisionAIButton { border-radius: 7px; min-height: 24px; padding: 5px 11px; font-size: 12px; }
        QLabel#decisionChatStatus { font-size: 11px; color: @muted; }
        QLabel#decisionChatStatus[tone="warning"] { color: @warning; }
        QPushButton#decisionChatCopyButton, QPushButton#decisionChatRegenerateButton { font-size: 11px; min-height: 20px; }
        /* The planning page is one narrow list; details live in the open row. */
        QLabel#planningTitleLabel { font-size: 16px; font-weight: 500; }
        QStackedWidget#planningAddButtons { background: transparent; }
        QPushButton[planningAdd="true"], QPushButton#addWishButton[planningAdd="true"] { background: transparent; color: @secondary; border: 1px solid transparent; padding: 5px 0; min-height: 24px; font-size: 13px; }
        QPushButton[planningAdd="true"]:hover, QPushButton#addWishButton[planningAdd="true"]:hover { color: @ink; background: transparent; }
        QPushButton[planningAdd="true"]:focus, QPushButton#addWishButton[planningAdd="true"]:focus { border-color: @accent; }
        QPushButton[planningAdd="true"]:disabled, QPushButton#addWishButton[planningAdd="true"]:disabled { background: transparent; color: @muted; border-color: transparent; }
        QFrame#planningSegments { background: @inset; border: none; border-radius: 18px; }
        QPushButton[planningTab="true"] { background: transparent; color: @muted; border: 1px solid transparent; border-radius: 13px; padding: 0; min-height: 44px; font-size: 13px; font-weight: 500; }
        QPushButton[planningTab="true"]:checked { background: @card; color: @ink; }
        QPushButton[planningTab="true"]:focus { border-color: @accent; }
        QTabWidget#planningTabs::pane { border: none; background: @page; }
        QToolButton#wishlistMoreButton, QToolButton#priceMoreButton, QToolButton#wishProductMoreButton, QToolButton#priceProductMoreButton { padding: 0 3px; font-size: 17px; min-height: 22px; }
        QToolButton#wishlistMoreButton::menu-indicator, QToolButton#priceMoreButton::menu-indicator, QToolButton#wishProductMoreButton::menu-indicator, QToolButton#priceProductMoreButton::menu-indicator { image: none; }
        QTableWidget[productList="true"] { background: @card; border: 1px solid @border; border-radius: 18px; selection-background-color: @card; }
        QTableWidget[productList="true"]::item { padding: 0; border: none; background: transparent; }
        QWidget[productRow="true"] { background: @card; }
        QWidget[productRow="true"][firstProduct="true"] { border-top-left-radius: 17px; border-top-right-radius: 17px; }
        QWidget[productRow="true"][lastProduct="true"] { border-bottom-left-radius: 17px; border-bottom-right-radius: 17px; }
        QPushButton[productToggle="true"] { background: transparent; border: none; border-radius: 0; padding: 0; min-height: 100px; }
        QPushButton[productToggle="true"]:hover { background: @card; }
        QPushButton[productToggle="true"]:focus { border: 1px solid @accent; border-radius: 14px; }
        QWidget[productRow="true"][lastProduct="false"] { border-bottom: 1px solid @border; }
        QListWidget#wishlistOrderList { background: @card; color: @ink; border: 1px solid @border; border-radius: 10px; outline: none; }
        QListWidget#wishlistOrderList::item { padding: 8px 12px; border-bottom: 1px solid @border; }
        QListWidget#wishlistOrderList::item:selected { background: @inset; color: @ink; }
        QLabel[productName="true"] { font-size: 15px; font-weight: 500; }
        QLabel[productAmount="true"] { font-family: 'DM Serif Display'; font-size: 16px; font-weight: 400; }
        QLabel[productArrow="true"] { color: @muted; font-size: 21px; }
        QLabel[planningEmpty="true"] { background: @card; color: @muted; border: 1px solid @border; border-radius: 18px; padding: 24px; }
        /* Bookkeeping stays in the foreground; records and charts are secondary. */
        QLabel#billingPageTitle { font-size: 16px; font-weight: 500; }
        QLabel#bookkeepingTitle { font-size: 16px; font-weight: 500; }
        QLabel[billingSubtitle="true"] { font-size: 17px; font-weight: 500; }
        QLabel[billingAmount="true"] { font-family: 'DM Serif Display'; font-size: 15px; font-weight: 400; }
        QLabel[billingBalance="true"] { font-family: 'DM Serif Display'; font-size: 34px; font-weight: 400; }
        QFrame[billingCard="true"], QWidget#expenseComposer { background: @card; border: 1px solid @border; border-radius: 16px; }
        QFrame[billingInset="true"] { background: @inset; border: none; border-radius: 10px; }
        QFrame[billingDivider="true"] { background: @border; border: none; }
        QFrame[billingConfirmation="true"] { background: transparent; border: none; border-top: 1px solid @border; border-radius: 0; }
        QFrame[billingPopup="true"] { background: @card; border: 1px solid @border; border-radius: 12px; }
        QStackedWidget#bookkeepingEntryModes, QStackedWidget#billingChartViews { background: transparent; }
        QPushButton[billingSegment="true"] { background: transparent; color: @muted; border: 1px solid transparent; border-radius: 7px; padding: 5px 13px; min-height: 23px; }
        QPushButton[billingSegment="true"]:checked { background: @card; color: @ink; border-color: @border; }
        QPushButton[billingSegment="true"]:focus { border-color: @accent; }
        QPushButton[billingQuiet="true"], QPushButton[billingExample="true"] { background: transparent; color: @muted; border: 1px solid transparent; padding: 3px 5px; min-height: 22px; font-size: 12px; }
        QPushButton[billingQuiet="true"]:hover, QPushButton[billingExample="true"]:hover { color: @ink; background: @inset; }
        QPushButton[billingQuiet="true"]:focus, QPushButton[billingExample="true"]:focus { border-color: @accent; }
        QPushButton[billingExample="true"] { background: @inset; border-radius: 6px; padding: 2px 9px; min-height: 20px; font-size: 11px; }
        QPushButton[billingChoice="true"] { background: transparent; border: 1px solid transparent; border-radius: 7px; padding: 6px; }
        QPushButton[billingChoice="true"]:checked, QPushButton[billingChoice="true"]:hover { background: @inset; color: @ink; }
        QPushButton[billingChoice="true"]:focus { border-color: @accent; }
        QPlainTextEdit#bookkeepingInput { background: @card; border: 1px solid @border; border-radius: 10px; padding: 11px; font-size: 14px; }
        QPlainTextEdit#bookkeepingInput:focus { border-color: @accent; }
        QLabel#billingRecentFootnote { color: @muted; font-size: 11px; padding: 11px 18px; }
        QProgressBar[billingProgress="true"] { background: @inset; min-height: 4px; max-height: 4px; border-radius: 2px; }
        QProgressBar[billingProgress="true"]::chunk { background: @accent; border-radius: 2px; }
        QProgressBar[billingProgress="true"][overBudget="true"]::chunk { background: @danger; }
    )");
    style += DialogUi::styleSheet();
    const QList<QPair<QString, QString>> tokens = {{"@onAccent", c.onAccent}, {"@highlight", c.highlight},
        {"@secondary", c.secondary}, {"@progress", c.progress}, {"@track", c.track},
        {"@accent", c.accent}, {"@border", c.border}, {"@muted", c.muted}, {"@inset", c.inset},
        {"@page", c.page}, {"@card", c.card}, {"@ink", c.ink}};
    for (const auto &token : tokens) style.replace(token.first, token.second);
    style.replace("font-family: 'DM Serif Display';", "font-family: 'DM Serif Display', Georgia, SimSun;");
    const bool dark = normalized(theme) == QStringLiteral("dark");
    style.replace("@danger", dark ? "#ef9b8b" : "#a94030");
    style.replace("@warning", dark ? "#d8b66e" : "#8b651b");
    style.replace("@success", dark ? "#8dbba1" : "#38624b");
    root->setStyleSheet(style);
    AppTypography::apply(root);
    for (auto *table : root->findChildren<QTableWidget *>()) table->setShowGrid(false);
    for (auto *button : root->findChildren<QAbstractButton *>()) button->setCursor(Qt::PointingHandCursor);
    root->setProperty("theme", normalized(theme));
}

void AppTheme::setTone(QWidget *widget, const QString &tone)
{
    widget->setProperty("tone", tone);
    widget->style()->unpolish(widget); widget->style()->polish(widget); widget->update();
}

QColor AppTheme::color(const QWidget *widget,const QString &role)
{
    QString theme=QStringLiteral("warm");
    for (auto *current=widget;current;current=current->parentWidget()) if (current->property("theme").isValid()) { theme=current->property("theme").toString(); break; }
    const auto c=colors(normalized(theme));
    const QMap<QString,QString> values={{"page",c.page},{"card",c.card},{"inset",c.inset},{"ink",c.ink},{"secondary",c.secondary},{"muted",c.muted},{"border",c.border},{"accent",c.accent},{"onAccent",c.onAccent},{"progress",c.progress},{"track",c.track},{"danger",theme==QStringLiteral("dark")?QStringLiteral("#ef9b8b"):QStringLiteral("#a94030")}};
    return QColor(values.value(role,c.ink));
}

QIcon AppTheme::icon(const QString &name, const QString &theme, bool active)
{
    const auto c = colors(normalized(theme));
    QPixmap pixmap(48, 48); pixmap.fill(Qt::transparent);
    QPainter p(&pixmap); p.setRenderHint(QPainter::Antialiasing); p.scale(2, 2);
    p.setPen(QPen(QColor(active ? c.ink : c.muted), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath path;
    if (name == "brand") {
        // Original Kura squirrel mark, drawn as native vector paths.
        p.scale(24.0/150, 24.0/165);
        p.setPen(QPen(QColor(c.ink), 3.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawEllipse(QRectF(51,23,22,26));
        path.moveTo(51,37); path.cubicTo(53,30,71,30,73,37);
        path.moveTo(62,23); path.lineTo(62,18); path.cubicTo(62,15,65,13,67,15);
        p.drawPath(path); path = QPainterPath(); p.translate(8,22);
        path.moveTo(103,87); path.cubicTo(99,88,93,90,89,91); path.lineTo(88,95);
        path.cubicTo(91,96,95,98,98,105); path.lineTo(91,115); path.cubicTo(91,115,95,115,100,115);
        path.cubicTo(104,116,103,122,103,122); path.lineTo(58,122); path.cubicTo(58,122,54,97,74,80);
        path.cubicTo(73,70,74,63,78,59); path.lineTo(78,48); path.lineTo(87,55);
        path.cubicTo(96,54,102,63,103,70); path.lineTo(92,74); path.lineTo(91,81); path.lineTo(99,80);
        path.lineTo(102,75); path.cubicTo(109,77,111,85,103,87); path.closeSubpath();
        path.moveTo(49,122); path.cubicTo(38,120,31,114,31,102); path.cubicTo(31,88,39,59,16,63);
        path.lineTo(15,60); path.cubicTo(19,51,27,42,40,42); path.cubicTo(54,42,61,51,61,68);
        path.cubicTo(61,88,48,89,49,122); path.closeSubpath();
        p.drawPath(path); return QIcon(pixmap);
    }
    if (name == "home") {
        path.moveTo(3, 10); path.lineTo(12, 3); path.lineTo(21, 10); path.moveTo(5, 9); path.lineTo(5, 21);
        path.lineTo(10, 21); path.lineTo(10, 14); path.lineTo(14, 14); path.lineTo(14, 21); path.lineTo(19, 21); path.lineTo(19, 9);
    } else if (name == "billing") {
        p.drawRoundedRect(QRectF(3, 5, 18, 15), 3, 3); p.drawRoundedRect(QRectF(14, 10, 8, 6), 1, 1); p.drawPoint(17, 13);
    } else if (name == "wishlist") {
        for (int y : {5, 12, 19}) { p.drawRoundedRect(QRectF(3, y-1, 3, 3), .5, .5); p.drawLine(10, y, 21, y); }
    } else if (name == "execution") {
        path.moveTo(4, 7); path.lineTo(20, 7); path.lineTo(19, 21); path.lineTo(5, 21); path.closeSubpath();
        path.moveTo(8, 8); path.lineTo(8, 6); path.cubicTo(8, 1, 16, 1, 16, 6); path.lineTo(16, 8);
    } else if (name == "review") {
        path.moveTo(12, 5); path.cubicTo(8, 2, 4, 3, 2, 4); path.lineTo(2, 20); path.cubicTo(5, 18, 9, 18, 12, 21);
        path.cubicTo(15, 18, 19, 18, 22, 20); path.lineTo(22, 4); path.cubicTo(18, 2, 15, 3, 12, 5); path.lineTo(12, 21);
    } else {
        p.drawEllipse(QRectF(5, 5, 14, 14)); p.drawEllipse(QRectF(9, 9, 6, 6));
        for (int i=0; i<8; ++i) { p.save(); p.translate(12,12); p.rotate(i*45); p.drawLine(0,-7,0,-10); p.restore(); }
    }
    p.drawPath(path); return QIcon(pixmap);
}
