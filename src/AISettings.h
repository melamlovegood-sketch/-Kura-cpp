#pragma once
#include <QString>
#include <QUrl>
struct AISettings {
    QString baseUrl = QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1");
    QString model = QStringLiteral("qwen3-vl-plus");
    QString apiKey;
    bool useTrial = false;
    bool trialDefaultForNewUsers = false;
    QString configurationError;
    QString validationError() const;
    QUrl endpoint() const;
};
namespace AISettingsStore {
AISettings load(const QString &path, QString *error = nullptr);
AISettings loadPersonal(const QString &path, QString *error = nullptr);
AISettings trial(QString *error = nullptr);
bool exportTrial(const QString &path, const AISettings &settings, QString &error, bool defaultForNewUsers = false);
bool save(const QString &path, const AISettings &settings, QString &error);
}
