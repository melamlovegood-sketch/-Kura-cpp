#pragma once
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace ShellWidgets {
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
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);
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
