#pragma once
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ShellWidgets {
inline constexpr int PageWidth = 1040;
inline constexpr int PageMargin = 28;
inline constexpr int PageSpacing = 24;

inline void pageLayout(QVBoxLayout *layout)
{
    layout->setContentsMargins(PageMargin, 24, PageMargin, 32);
    layout->setSpacing(PageSpacing);
}

inline QLabel *pageTitle(const QString &text, QWidget *parent, const char *name)
{
    auto *title = new QLabel(text, parent);
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    title->setProperty("pageTitle", true);
    title->setObjectName(QLatin1String(name));
    title->setMinimumHeight(42);
    return title;
}

inline QLabel *label(const QString &text, QWidget *parent, const char *role = nullptr)
{
    auto *result = new QLabel(text, parent);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    if (role) result->setProperty(role, true);
    return result;
}
inline QVBoxLayout *card(QWidget *parent, QFrame **frame = nullptr)
{
    auto *box = new QFrame(parent);
    box->setProperty("card", true);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(14);
    if (frame) *frame = box;
    return layout;
}
inline QScrollArea *centeredPage(QWidget *content, int maximumWidth, QWidget *parent, Qt::Alignment alignment = {})
{
    content->setProperty("page", true);
    content->setMaximumWidth(maximumWidth);
    auto *body = new QWidget;
    body->setProperty("page", true);
    auto *layout = new QHBoxLayout(body);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addStretch(); layout->addWidget(content, 1, alignment); layout->addStretch();
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(body);
    return scroll;
}
}
