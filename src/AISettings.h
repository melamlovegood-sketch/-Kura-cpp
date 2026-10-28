#pragma once
#include <QString>
#include <QUrl>
struct AISettings {
    QString baseUrl = QStringLiteral("https://dashscope.aliyuncs.com/compatible-mode/v1");
    QString model = QStringLiteral("qwen3-vl-plus");
    QString apiKey;
    QString validationError() const;
    QUrl endpoint() const;
};
namespace AISettingsStore {
AISettings load(const QString &path, QString *error = nullptr);
bool save(const QString &path, const AISettings &settings, QString &error);
}
