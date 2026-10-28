#pragma once
#include <QString>
namespace DialogUi {
// Covers current and future application dialogs without changing their save APIs.
void install();
QString styleSheet();
}
