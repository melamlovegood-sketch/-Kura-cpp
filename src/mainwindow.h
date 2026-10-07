#pragma once

#include <QMainWindow>

class QPushButton;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QPushButton *m_addButton = nullptr;
    QTableWidget *m_transactionTable = nullptr;
};
