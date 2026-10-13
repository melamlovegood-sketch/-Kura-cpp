# Kura C++ Windows 运行包

0.8.0 运行包包含 KuraCpp.exe、匹配的 Qt 和 MinGW 依赖、平台与 SQLite 插件、Demo.cmd、使用说明、演示步骤、测试与验收记录、示例截图。包内不包含个人数据库或测试程序。

## 本次构建环境

Windows 11 x64，MSYS2 UCRT64，GCC 15.2，Qt 6.10.1，CMake 和 Ninja。程序包使用 Release 构建；测试用独立临时数据库。

## 重新构建与打包

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

脚本在 `build/releases/KuraCpp-0.8.0-windows-x64` 下创建目录和 ZIP，并输出 SHA256。同版本目标已存在时停止，不自动删除或覆盖旧发布包。只复制 DLL、已知插件目录、文档、截图和构建好的程序，不复制源码目录中的 SQLite 数据。

```powershell
./scripts/Measure-Code.ps1
```

该命令给出手写 C/C++、PowerShell、CMake 的文件数及非空行数，口径和限制见 CODE_METRICS.md。代码比例不是成员贡献比例。

## 验证范围

本机自动测试、界面预览和限制 PATH 后的运行依赖检查见 TEST_REPORT.md。另一台 Windows 电脑、现场投影环境、人工 Excel 检查仍需组员按 ACCEPTANCE.md 填写。

最终课程提交需另行加入真实名单、个人报告、组报告、互评、设计确认及教师通知，并按课程要求命名 RAR。此运行 ZIP 可以作为其中的程序目录，不能当作这些材料已经完成。
