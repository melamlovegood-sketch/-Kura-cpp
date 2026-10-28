# Windows 测试安装包

先在唯一源码目录执行 `./scripts/Build-Unified.ps1`，通过完整回归后再运行：

```powershell
./scripts/Build-Installer.ps1 -InnoSetupCompiler "$env:LOCALAPPDATA/KuraBuild/tools/InnoSetup/ISCC.exe"
```

编译器使用 Inno Setup 6.7.3（从 https://jrsoftware.org/isdl.php 获取）。脚本与统一发布共用独占锁，要求当前发布与源码指纹匹配，核对程序哈希、完整测试日志及必需运行依赖；复制白名单内的程序、DLL、插件、文档、样例与许可，检查打包期间源码及负载没有变化。产物和负载文件哈希记录在 `build/installers/时间戳目录/installer-manifest.json`。含共享 AI 配置的当前发布需要显式传入 `-IncludeTrialAI`；普通包不含密钥和本机数据库。

安装包适用于 Windows 10/11 x64，默认安装到当前用户的 `%LOCALAPPDATA%/Programs/Kura`，不需要管理员权限。创建开始菜单普通／演示入口和可选桌面快捷方式，支持 Windows 设置中的卸载。正式数据仍使用应用原有的 `%LOCALAPPDATA%/Kura/KuraCpp`，升级和卸载都不删除用户账本、设置或备份。演示模式使用独立临时数据。

同一 Windows 账户中的开发运行、免安装版和安装版沿用同一个数据目录，重新安装后看到旧记录属于正常的数据保留；全新账户不会带入这些记录。`onboarding_tests::newUserDefaultStorageIsEmptyAndExistingUserKeepsRecords` 使用 Qt 测试存储和独立随机身份验证默认目录下的空账本、新手引导、旧数据续用及另一新身份隔离，绝不访问或清理正式数据。

0.16.1 的桌面／开始菜单快捷方式、EXE、窗口／任务栏及安装程序统一使用原 Kura 松鼠与栗子矢量标识。`resources/icons/kura.svg` 是母版，`scripts/Generate-AppIcon.mjs` 使用 sharp 导出含 16–256px 八档尺寸的 ICO，Qt 资源与 Windows RC 同时嵌入。

分发前用独立安装目标静默安装，核对全部负载哈希及开始菜单入口，限制 PATH 验证 `--version` 和演示窗口启动，再卸载确认程序与快捷方式清理。自动验收不调用真实 AI 服务。真实异机兼容与人工体验由测试用户完成。

运行许可来自 MSYS2 UCRT64 `share/licenses`（可通过 `-RuntimeLicenseDirectory` 指定其他匹配环境）；Qt LGPL v3 原文来自 https://github.com/qt/qtbase/blob/v6.10.1/LICENSES/LGPL-3.0-only.txt 。Qt 动态库可独立替换，Qt 6.10.1 对应源码见 https://download.qt.io/archive/qt/6.10/6.10.1/single/ 。简体中文安装器翻译来自 Inno Setup 官方翻译索引 https://jrsoftware.org/files/istrans/ 指向的 `jrsoftware/issrc/Files/Languages/ChineseSimplified.isl`，文件头保留作者信息。
