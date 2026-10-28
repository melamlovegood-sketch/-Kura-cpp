#include <QApplication>
#include <QCommandLineParser>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QIcon>
#include "AppTypography.h"
#include "DemoMode.h"
#include "SplashScreen.h"
#include "BuildInfo.h"
#include "AISettings.h"
#include <QTextStream>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Kura"));
    app.setApplicationName(QStringLiteral("KuraCpp"));
    app.setApplicationVersion(QStringLiteral(KURA_VERSION));
    AppTypography::initialize();
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/kura.ico")));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Kura C++ 个人消费决策与预算管理"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({QStringLiteral("demo"), QStringLiteral("使用独立临时数据库启动课堂演示，退出后删除演示数据。")});
    parser.addOption({QStringLiteral("demo-profile"),QStringLiteral("直接进入演示身份：student 或 worker。"),QStringLiteral("profile")});
    parser.addOption({QStringLiteral("export-trial-ai"),QStringLiteral("将本机已保存的个人 AI 配置导出到指定试用包；包含可撤销的共享密钥。"),QStringLiteral("file")});
    parser.addOption({QStringLiteral("trial-ai-settings"),QStringLiteral("仅导出试用配置时，从指定的本机设置文件读取 AI 配置。"),QStringLiteral("file")});
    parser.process(app);
    if (parser.isSet(QStringLiteral("export-trial-ai"))) {
        const auto settingsPath=parser.isSet(QStringLiteral("trial-ai-settings"))?parser.value(QStringLiteral("trial-ai-settings")):DatabaseManager::defaultDatabasePath()+QStringLiteral(".ui.ini");
        QString error; const auto settings=AISettingsStore::loadPersonal(settingsPath,&error);
        if (!error.isEmpty() || !AISettingsStore::exportTrial(parser.value(QStringLiteral("export-trial-ai")),settings,error,true)) {
            QTextStream(stderr)<<error<<Qt::endl; return 1;
        }
        return 0;
    }
    if (parser.isSet(QStringLiteral("trial-ai-settings"))) parser.showHelp(1);
    const auto profile=parser.value(QStringLiteral("demo-profile"));
    if (parser.isSet(QStringLiteral("demo-profile")) && profile!="student" && profile!="worker") parser.showHelp(1);
    if (parser.isSet(QStringLiteral("demo")) || parser.isSet(QStringLiteral("demo-profile"))) return DemoMode::run(app,profile);
    MainWindow window;
    window.show();
    SplashScreen::present(&window);
    QTimer::singleShot(0,&window,&MainWindow::startFirstRunGuide);
    return app.exec();
}
