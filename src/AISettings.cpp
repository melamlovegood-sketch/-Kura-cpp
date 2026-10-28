#include "AISettings.h"
#include <QSettings>
#include <QByteArray>
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

AISettings AISettingsStore::load(const QString &path, QString *error)
{
    if (error) error->clear();
    AISettings result;
    if (path.isEmpty()) return result;
    QSettings store(path,QSettings::IniFormat);
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
    return result;
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
    store.setValue(QStringLiteral("ai/protectedKey"),protectedKey.toBase64());
    store.sync();
    if (store.status()!=QSettings::NoError) { error=QStringLiteral("设置保存失败，请检查目录权限。"); return false; }
    return true;
}
