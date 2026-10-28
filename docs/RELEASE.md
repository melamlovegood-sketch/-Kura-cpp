# Windows 构建与发布

最近已验收基线为 **0.16.6**；开发版本取自 `CMakeLists.txt`，不能仅凭版本号认定已发布。发布统一使用 `scripts/Build-Unified.ps1`，通过编译、完整回归及源码指纹核对后，才更新 `build/current.json`。安装包流程见 [INSTALLER.md](INSTALLER.md)。

## 使用已有验收包

完整本地工作区使用上一级“启动最新版.cmd”“体验最新版.cmd”和“更新最新版.cmd”。仓库内的等价入口为：

```powershell
./scripts/Start-Current.ps1             # 启动最近已验收版本
./scripts/Start-Current.ps1 -Demo       # 临时虚构数据
./scripts/Start-Current.ps1 -CheckOnly  # 严格检查源码与运行包一致
./scripts/Build-Unified.ps1            # 重新构建、完整回归并更新固定入口
```

源码变化不会让普通启动强制重编译；更新需显式运行统一构建。没有验收包时，启动脚本会尝试首次构建，但仍需事先准备工具链和匹配的运行依赖。

## 首次克隆与日常开发

GitHub 的 Download ZIP 和 `git clone` 仅获取源码，不提供已编译程序、Qt 运行库、个人数据库或上一级 CMD。依赖为 CMake 3.16+、C++17、Qt 6.7+（Widgets / Sql / Network；测试另需 Test）、Zlib、匹配的编译器及 Ninja。

当前已验证环境是 Windows 11 x64、MSYS2 UCRT64、GCC 15.2、Qt 6.10.1。Qt 与编译器必须来自同一个 Kit。MSYS2 moc 对中文路径有兼容问题，使用指向唯一源码的英文目录联接，不复制源码独立开发。

下面在仓库根目录执行，为日常检查使用独立输出目录。先确认 `source` 联接不存在或已指向本仓库；若它指向其他项目，停止并检查，不覆盖原联接。

```powershell
$kuraRepo = (Get-Location).Path
$kuraAliasRoot = Join-Path $env:LOCALAPPDATA 'KuraBuild'
New-Item -ItemType Directory -Path $kuraAliasRoot -Force | Out-Null
$kuraSource = Join-Path $kuraAliasRoot 'source'
if (-not (Test-Path -LiteralPath $kuraSource)) {
    New-Item -ItemType Junction -Path $kuraSource -Target $kuraRepo | Out-Null
}
$kuraQt = 'C:/msys64/ucrt64'
$env:PATH = "$kuraQt/bin;$env:PATH"
$kuraBuild = Join-Path $kuraAliasRoot 'build-local'
cmake -S $kuraSource -B $kuraBuild -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$kuraQt" -DBUILD_TESTING=ON
cmake --build $kuraBuild --parallel 4
ctest --test-dir $kuraBuild --output-on-failure --parallel 1
```

这套命令用于开发检查，不提升固定发布入口。任一步骤失败时先处理错误，不将产物标为最新版。

## 首次准备运行依赖

`Build-Unified.ps1` 会构建和测试，但当前脚本还要求一份已部署好的、同 Kit 的运行依赖。首次克隆时没有这些被忽略的文件，需先使用匹配的 `windeployqt` 准备它们。例如，在上文编译成功后：

```powershell
$kuraRuntime = Join-Path $kuraRepo 'build/secondary-focused-run'
New-Item -ItemType Directory -Path $kuraRuntime -Force | Out-Null
& "$kuraQt/bin/windeployqt.exe" --release --compiler-runtime --dir $kuraRuntime (Join-Path $kuraBuild 'KuraCpp.exe')
foreach ($kuraDll in @('zlib1.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
    Copy-Item -LiteralPath (Join-Path "$kuraQt/bin" $kuraDll) -Destination $kuraRuntime
}
```

该示例适用于上述 MSYS2 UCRT64 Kit。确认包含 Widgets／SQL／Network 依赖、`platforms/qwindows.dll`、`sqldrivers/qsqlite.dll` 及对应 TLS 插件；第三方 DLL 的实际需求随 Kit 变化，按依赖检查补齐。不要从旧包混入其他版本的 Qt DLL。脚本会核对 Qt6Core 与当前 Kit 的哈希；缺失或不匹配时停止发布。

## 统一发布与安装包

运行 `./scripts/Build-Unified.ps1`。脚本持有独占发布锁，完整回归失败或构建期间源码变化时，不更新固定入口。产物位于 `build/releases/`，随包保存 `build-manifest.json` 和 `verified-tests.log`。更新成功后再运行 `./scripts/Start-Current.ps1 -CheckOnly`。

需要安装包时，在统一发布成功后执行：

```powershell
./scripts/Build-Installer.ps1
```

安装器需要 Inno Setup 6 及匹配的运行许可文件，参数与验证步骤见 [安装说明](INSTALLER.md)。`Package-Windows.ps1` 保留为底层便携包工具，单独运行它不能替代统一验收或更新固定入口。

常规发布不含个人数据、设置和 API Key。仅经项目方明确授权的小组测试可以使用 `-IncludeTrialAI`，详见 [小组 AI 试用](TRIAL_AI.md)。试用包含共享凭据，不上传公开仓库或公开 Release；源码版本标签也不能代替实际构建验收。

## 校验与交付边界

根目录 CMD 使用 Windows PowerShell 5.1；中文 `.ps1` 必须保存为 UTF-8 BOM。统一发布前执行 `scripts/Test-LauncherCompatibility.ps1` 检查编码与语法。字体和运行依赖许可随包保留。

自动测试使用临时数据，AI 验收使用本地模拟服务。异机兼容、投影环境、真实 AI 效果和用户体验按 [验收清单](ACCEPTANCE.md) 实际记录；历史结果见 [测试记录](TEST_REPORT.md)。

课程最终提交还需真实成员信息、个人报告、组报告、互评及教师要求的材料；源码 ZIP、运行包和安装包都不能代替完整课程交付。代码统计可运行 `scripts/Measure-Code.ps1`，统计口径见 [CODE_METRICS.md](CODE_METRICS.md)，代码量不等于个人贡献度。
