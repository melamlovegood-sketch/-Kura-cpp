#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Kura"));
    app.setApplicationName(QStringLiteral("KuraCpp"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));
    MainWindow window;
    window.show();
    return app.exec();
}
