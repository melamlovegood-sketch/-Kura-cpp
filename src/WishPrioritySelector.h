#pragma once
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

// A new wish deliberately starts without a selection. Existing wishes retain
// their saved high/medium/low meaning; no dates are inferred from old data.
inline QWidget *makeWishPrioritySelector(QWidget *parent, const QString &prefix, QButtonGroup *&group)
{
    auto *widget = new QWidget(parent);
    widget->setObjectName(prefix + "PrioritySelector");
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    auto *title = new QLabel(QStringLiteral("现在有多需要？（必选）"), widget);
    title->setWordWrap(true);
    layout->addWidget(title);
    group = new QButtonGroup(widget);
    group->setObjectName(prefix + "PriorityGroup");
    const QStringList choices = {QStringLiteral("高优先 · 近期确实要用"),
        QStringLiteral("中优先 · 有明确用途，可以等等"), QStringLiteral("低优先 · 暂时想要，不着急")};
    for (int i = 0; i < choices.size(); ++i) {
        auto *button = new QPushButton(choices[i], widget);
        button->setObjectName(prefix + "Priority" + QString::number(i + 1) + "Button");
        button->setCheckable(true);
        button->setAutoDefault(false);
        button->setProperty("coolingPreset", true);
        group->addButton(button, i + 1);
        layout->addWidget(button);
    }
    return widget;
}
