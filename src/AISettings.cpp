#include "AISettings.h"
#include <QSettings>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVariant>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#endif

QString AISettings::validationError() const
{
    if (!configurationError.isEmpty()) return configurationError;
    const QUrl url(baseUrl.trimmed());
    const bool local = url.host() == QStringLiteral("localhost") || url.host() == QStringLiteral("127.0.0.1") || url.host() == QStringLiteral("::1");
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
        || (url.scheme() != QStringLiteral("https") && !(local && url.scheme() == QStringLiteral("http"))))
        return QStringLiteral("请填写有效的 HTTPS 服务地址，本机测试地址可用 HTTP。");
    if (model.trimmed().isEmpty() || model.size()>200) return QStringLiteral("请填写支持图片的模型名称。");
    if (apiKey.contains('\r') || apiKey.contains('\n') || apiKey.size()>4096) return QStringLiteral("API Key 格式无效。");
    return {};
}
QUrl AISettings::endpoint() const
{
    QString value=baseUrl.trimmed(); while (value.endsWith('/')) value.chop(1);
    if (!value.endsWith(QStringLiteral("/chat/completions"))) value += QStringLiteral("/chat/completions");
    return QUrl(value);
}

AISettings AISettingsStore::loadPersonal(const QString &path, QString *error)
{
    if (error) error->clear();
    AISettings result;
    if (path.isEmpty()) return result;
    QSettings store(path,QSettings::IniFormat);
    result.useTrial=store.value(QStringLiteral("ai/useTrial"),false).toBool();
    result.baseUrl=store.value(QStringLiteral("ai/baseUrl"),result.baseUrl).toString();
    result.model=store.value(QStringLiteral("ai/model"),result.model).toString();
    const auto host=QUrl(result.baseUrl.trimmed()).host().toLower();
    const bool officialQwen=host==QStringLiteral("dashscope.aliyuncs.com") || host==QStringLiteral("dashscope-intl.aliyuncs.com") || host==QStringLiteral("dashscope-us.aliyuncs.com");
    // Upgrade only the previous default on official Qwen endpoints. Reading
    // settings does not rewrite the file or alter a user's custom provider.
    const bool legacyChoice=store.value(QStringLiteral("ai/modelSelectionVersion"),0).toInt()<1;
    if (result.model.trimmed().isEmpty() || (legacyChoice && officialQwen && result.model.trimmed()==QStringLiteral("qwen-vl-plus"))) result.model=AISettings{}.model;
    const QByteArray protectedKey=QByteArray::fromBase64(store.value(QStringLiteral("ai/protectedKey")).toByteArray());
    if (!protectedKey.isEmpty()) {
#ifdef Q_OS_WIN
        DATA_BLOB input{DWORD(protectedKey.size()),reinterpret_cast<BYTE *>(const_cast<char *>(protectedKey.constData()))}, output{};
        if (CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)) {
            result.apiKey=QString::fromUtf8(reinterpret_cast<char *>(output.pbData),output.cbData);
            SecureZeroMemory(output.pbData,output.cbData); LocalFree(output.pbData);
        } else if (error) *error=QStringLiteral("这台电脑无法读取已保存的密钥，请重新填写。");
#else
        if (error) *error=QStringLiteral("此平台不支持读取 Windows 保存的密钥，请重新填写。");
#endif
    }
    if (!store.contains(QStringLiteral("ai/useTrial")) && protectedKey.isEmpty()) {
        const auto shared=trial();
        result.useTrial=shared.configurationError.isEmpty() && shared.trialDefaultForNewUsers;
    }
    return result;
}

AISettings AISettingsStore::trial(QString *error)
{
    if (error) error->clear();
    AISettings result; result.useTrial=true;
    // Tests supply a fixture path through the application object; production
    // reads only the optional configuration beside the executable.
    const auto overridePath=QCoreApplication::instance()?QCoreApplication::instance()->property("kuraTrialConfigPath").toString():QString();
    const auto path=overridePath.isEmpty()?QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("kura-trial.json")):overridePath;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) result.configurationError=QStringLiteral("此运行包未提供 Kura AI 试用，请切换为自己的 API Key。");
    else if (file.size()>32768) result.configurationError=QStringLiteral("Kura AI 试用配置无效，请切换为自己的 API Key。");
    else {
        const auto document=QJsonDocument::fromJson(file.readAll()); const auto object=document.object();
        if (!document.isObject() || object.value("format").toInt()!=1 || !object.value("baseUrl").isString() || !object.value("model").isString() || !object.value("apiKey").isString())
            result.configurationError=QStringLiteral("Kura AI 试用配置无效，请切换为自己的 API Key。");
        else {
            result.baseUrl=object.value("baseUrl").toString(); result.model=object.value("model").toString(); result.apiKey=object.value("apiKey").toString();
            result.trialDefaultForNewUsers=object.value("defaultForNewUsers").toBool(false);
            if (!result.validationError().isEmpty() || result.apiKey.trimmed().isEmpty()) result.configurationError=QStringLiteral("Kura AI 试用配置无效，请切换为自己的 API Key。");
        }
    }
    if (!result.configurationError.isEmpty()) result.apiKey.clear();
    if (error) *error=result.configurationError;
    return result;
}

AISettings AISettingsStore::load(const QString &path, QString *error)
{
    auto result=loadPersonal(path,error);
    // Selecting a trial never overwrites, or silently falls back to, a
    // separately saved personal credential.
    return result.useTrial?trial(error):result;
}

bool AISettingsStore::exportTrial(const QString &path,const AISettings &settings,QString &error,bool defaultForNewUsers)
{
    error=settings.validationError();
    if (error.isEmpty() && settings.apiKey.trimmed().isEmpty()) error=QStringLiteral("本机尚未保存个人 API Key，无法生成试用配置。");
    if (!error.isEmpty()) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { error=QStringLiteral("无法写入试用配置文件。"); return false; }
    const auto bytes=QJsonDocument(QJsonObject{{"format",1},{"baseUrl",settings.baseUrl.trimmed()},{"model",settings.model.trimmed()},{"apiKey",settings.apiKey.trimmed()},{"defaultForNewUsers",defaultForNewUsers}}).toJson(QJsonDocument::Indented);
    if (file.write(bytes)!=bytes.size() || !file.commit()) { error=QStringLiteral("试用配置保存失败。"); return false; }
    return true;
}

bool AISettingsStore::save(const QString &path, const AISettings &settings, QString &error)
{
    error=settings.validationError(); if (!error.isEmpty()) return false;
    if (path.isEmpty()) { error=QStringLiteral("无法确定本机设置文件位置。"); return false; }
    QByteArray protectedKey;
    if (!settings.apiKey.trimmed().isEmpty()) {
#ifdef Q_OS_WIN
        QByteArray raw=settings.apiKey.trimmed().toUtf8();
        DATA_BLOB input{DWORD(raw.size()),reinterpret_cast<BYTE *>(raw.data())}, output{};
        if (!CryptProtectData(&input,L"Kura AI",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)) {
            raw.fill(0); error=QStringLiteral("无法加密保存 API Key，请稍后重试。"); return false;
        }
        raw.fill(0); protectedKey=QByteArray(reinterpret_cast<char *>(output.pbData),output.cbData); LocalFree(output.pbData);
#else
        error=QStringLiteral("此平台暂不支持安全保存密钥。"); return false;
#endif
    }
    QSettings store(path,QSettings::IniFormat);
    store.setValue(QStringLiteral("ai/baseUrl"),settings.baseUrl.trimmed());
    store.setValue(QStringLiteral("ai/model"),settings.model.trimmed());
    store.setValue(QStringLiteral("ai/modelSelectionVersion"),1);
    store.setValue(QStringLiteral("ai/useTrial"),settings.useTrial);
    store.setValue(QStringLiteral("ai/protectedKey"),protectedKey.toBase64());
    store.sync();
    if (store.status()!=QSettings::NoError) { error=QStringLiteral("设置保存失败，请检查目录权限。"); return false; }
    return true;
}
