# Kura C++ Windows 运行包

0.12.6 运行包包含 KuraCpp.exe、匹配的 Qt（含 Network）、Zlib 和 MinGW 依赖、平台与 SQLite／TLS 插件、Demo.cmd、虚构账单样例、使用说明、测试记录和页面截图。常规包不含个人数据库、API Key 或测试程序；仅显式使用 `Build-Unified.ps1 -IncludeTrialAI` 的小组测试包携带项目方授权的共享 AI 配置，详见 [小组 AI 试用](TRIAL_AI.md)。本次记账工作台见 BILLING_REFRESH.md，消费人格故事见 CONSUMER_PERSONALITIES.md，复盘界面见 REVIEW_REFRESH.md，导入与故事规则见 IMPORT_MONTHLY_REVIEW.md。

## 本次构建环境

0.12.3 清单简洁版见 PLANNING_REFRESH.md；运行包附带 `planning-*.png` 实际窗口截图。

Windows 11 x64，MSYS2 UCRT64，GCC 15.2，Qt 6.10.1，CMake 和 Ninja；源码编译另需匹配 Kit 的 Zlib 开发库。程序包使用 Release 构建；测试用独立临时数据库。

## 重新构建与打包

根目录 CMD 入口使用 Windows PowerShell 5.1。含中文的 `.ps1` 脚本必须保存为 UTF-8 with BOM，避免中文引号被误读后触发解析错误。统一发布前会通过 `scripts/Test-LauncherCompatibility.ps1` 使用系统 PowerShell 5.1 检查所有构建／启动脚本的编码与语法；不能只在 PowerShell 7 中验证。

「启动最新版／体验最新版」打开 `build/current.json` 指向的最近一份已验收运行包，并核对指针、清单与程序哈希。开发中的源码变化不会强制用户在启动时重新编译；使用「更新最新版」完成编译、完整回归与源码一致性检查后才替换固定入口。首次没有已验收运行包时才自动构建。`Start-Current.ps1 -CheckOnly` 仍严格检查运行包是否与当前源码一致，供发布验收使用。

在匹配的 Qt Kit 中执行：

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DBUILD_TESTING=ON
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure
```

MSYS2 的 moc 对中文路径有兼容问题，构建时用英文路径或指向本仓库的英文目录联接。先用匹配 Kit 的 windeployqt 准备完整运行依赖，再补齐该工具未自动复制的 MinGW 和第三方 DLL。

```powershell
./scripts/Package-Windows.ps1 -Executable ./build-release/KuraCpp.exe -RuntimeDirectory C:/path/to/complete-runtime -LicenseDirectory C:/path/to/kit/licenses
```

脚本在 `build/releases/KuraCpp-版本-windows-x64` 下创建目录和 ZIP，并输出 SHA256。同版本目标已存在时停止，不自动删除或覆盖旧发布包。只复制 DLL、已知插件目录、文档、截图和构建好的程序，不复制源码目录中的 SQLite 数据。

```powershell
./scripts/Measure-Code.ps1
```

该命令给出手写 C/C++、PowerShell、CMake 的文件数及非空行数，口径和限制见 CODE_METRICS.md。代码比例不是成员贡献比例。

## 验证范围

本机自动测试、界面预览和限制 PATH 后的运行依赖检查见 TEST_REPORT.md。另一台 Windows 电脑、现场投影环境、人工 Excel 检查仍需组员按 ACCEPTANCE.md 填写。

最终课程提交需另行加入真实名单、个人报告、组报告、互评、设计确认及教师通知，并按课程要求命名 RAR。此运行 ZIP 可以作为其中的程序目录，不能当作这些材料已经完成。
