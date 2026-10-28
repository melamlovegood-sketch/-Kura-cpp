#pragma once
#include <QIcon>
#include <QString>
#include <QColor>
class QWidget;
namespace AppTheme {
QString normalized(const QString &theme);
void apply(QWidget *root, const QString &theme = QStringLiteral("warm"));
void setTone(QWidget *widget, const QString &tone);
QIcon icon(const QString &name, const QString &theme, bool active = false);
QColor color(const QWidget *widget,const QString &role);
}
