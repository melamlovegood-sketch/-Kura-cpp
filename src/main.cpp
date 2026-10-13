#include <QApplication>
#include <QCommandLineParser>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include "DemoData.h"

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Kura"));
    app.setApplicationName(QStringLiteral("KuraCpp"));
    app.setApplicationVersion(QStringLiteral("0.8.0"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Kura C++ 个人消费决策与预算管理"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({QStringLiteral("demo"), QStringLiteral("使用独立临时数据库启动课堂演示，退出后删除演示数据。")});
    parser.process(app);
    if (parser.isSet(QStringLiteral("demo"))) {
        QTemporaryDir directory;
        DatabaseManager demo(directory.filePath(QStringLiteral("demo.sqlite")));
        QString error;
        if (!directory.isValid() || !demo.initialize() || !DemoData::populate(demo,error)) {
            QMessageBox::critical(nullptr,QStringLiteral("演示启动失败"),error.isEmpty() ? demo.lastError() : error);
            return 1;
        }
        MainWindow window(demo.databasePath());
        window.setWindowTitle(QStringLiteral("Kura C++ · 演示模式"));
        auto *banner = new QLabel(QStringLiteral("演示模式 · 临时数据，退出后删除"),window.statusBar());
        banner->setObjectName(QStringLiteral("demoBanner"));
        window.statusBar()->addPermanentWidget(banner);
        window.show();
        return app.exec();
    }
    MainWindow window;
    window.show();
    return app.exec();
}
