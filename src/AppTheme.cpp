#include "AppTheme.h"
#include <QFont>
#include <QPalette>
#include <QWidget>

void AppTheme::apply(QWidget *root)
{
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSizeF(10);
    root->setFont(font);
    QPalette palette = root->palette();
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#f4f7fb")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#1f2d40")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#1f2d40")));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f7f9fc")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#dce9ff")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#163d75")));
    palette.setColor(QPalette::Mid, QColor(QStringLiteral("#d8e1ee")));
    root->setPalette(palette);
    root->setStyleSheet(QStringLiteral(R"(
        QMainWindow { background: #f4f7fb; }
        QTabWidget::pane { background: #ffffff; border: 1px solid #dce4ef; border-radius: 8px; }
        QTabBar::tab { padding: 10px 22px; margin-right: 4px; color: #52627b; background: #eaf0f8; border-top-left-radius: 7px; border-top-right-radius: 7px; }
        QTabBar::tab:selected { color: #1f5cae; background: #ffffff; }
        QGroupBox { background: #ffffff; border: 1px solid #dce4ef; border-radius: 8px; margin-top: 12px; padding: 12px; }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; color: #344b69; }
        QPushButton { min-height: 22px; padding: 6px 14px; border: 1px solid #cbd7e6; border-radius: 6px; background: #ffffff; color: #263b55; }
        QPushButton:hover { background: #edf4ff; border-color: #8fb3e4; }
        QPushButton:pressed { background: #dce9ff; }
        QPushButton:focus { border: 1px solid #346fe0; }
        QPushButton#addTransactionButton, QPushButton#addWishButton, QPushButton#saveButton, QPushButton#saveWishButton, QPushButton#saveBudgetButton, QPushButton#exportCsvButton { background: #2864c7; border-color: #2864c7; color: white; }
        QPushButton#deleteTransactionButton, QPushButton#deleteWishButton { color: #b42318; }
        QPushButton:disabled { color: #8493a6; background: #eef2f7; border-color: #dde4ee; }
        QPushButton#addTransactionButton:disabled, QPushButton#addWishButton:disabled, QPushButton#saveButton:disabled, QPushButton#saveWishButton:disabled, QPushButton#saveBudgetButton:disabled, QPushButton#exportCsvButton:disabled, QPushButton#deleteTransactionButton:disabled, QPushButton#deleteWishButton:disabled { color: #8493a6; background: #eef2f7; border-color: #dde4ee; }
        QLineEdit, QComboBox, QAbstractSpinBox, QPlainTextEdit { border: 1px solid #cbd7e6; border-radius: 5px; padding: 5px; background: white; color: #1f2d40; }
        QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus, QPlainTextEdit:focus { border-color: #346fe0; }
        QAbstractSpinBox { padding-right: 20px; min-height: 22px; }
        QComboBox { padding-right: 24px; min-height: 22px; }
        QAbstractSpinBox:disabled, QComboBox:disabled { color: #8493a6; background: #f0f3f8; }
        QTableWidget { border: 1px solid #dce4ef; border-radius: 6px; background: white; alternate-background-color: #f7f9fc; gridline-color: #e8edf5; selection-background-color: #dce9ff; selection-color: #163d75; }
        QTableWidget::item { padding: 5px; }
        QHeaderView::section { background: #edf2f8; color: #405571; border: none; border-bottom: 1px solid #dce4ef; padding: 7px; }
        QScrollArea { border: none; background: transparent; }
        QStatusBar { color: #63738a; background: #f4f7fb; }
        QToolTip { background: #ffffff; color: #1f2d40; border: 1px solid #cbd7e6; padding: 5px; }
    )"));
}
