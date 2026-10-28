#pragma once
#include <QFont>
class QWidget;

namespace AppTypography {
void initialize();
QFont sans(int pixels, QFont::Weight weight = QFont::Normal);
QFont serif(int pixels);
QFont resolved(QFont font);
void apply(QWidget *root);
}
