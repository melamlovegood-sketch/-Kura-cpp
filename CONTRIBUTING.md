# 开发与协作

本仓库是 Kura Windows 桌面应用的唯一源码。先阅读 [AGENTS.md](AGENTS.md) 与 [版本统一记录](docs/VERSION_UNIFICATION.md)。

## 主线与分支

整理后的主线约定为 `main`：保存已集成的完整源码，GitHub 默认分支也应指向它。当前远端迁移状态及旧分支用途见 [仓库整理记录](docs/REPOSITORY_STATUS.md)。

新增工作从最新主线创建短期分支：`feature/<功能>`、`fix/<问题>` 或 `docs/<主题>`。先提交当前工作，再执行：

```powershell
git fetch origin
git switch main
git pull --ff-only origin main
git switch -c feature/your-topic
```

通过 Pull Request 合入主线，说明用户会看到的变化和实际完成的验证。旧分支用于追溯阶段实现，不作为新功能的起点；分支名称不代表版本新旧。

## 修改前后

1. 修改前运行 `git status --short`，保留已有工作，检查同一文件是否有其他对话正在修改。
2. 在当前源码逐项合并，不能用临时副本、旧演示包整文件覆盖。
3. 并行任务使用不同构建目录；MSYS2 中文路径不兼容时只创建英文目录联接，不复制源码另行开发。
4. 按改动范围执行现有测试。发布必须经过统一脚本的完整回归，不能把专项通过写成全部通过。
5. 更新相关使用说明；版本变更同步 `CMakeLists.txt`、README 与版本记录。

## 提交内容

- 提交源码、测试、必要资源、虚构样例和文档。
- 不提交构建目录、EXE／DLL、安装包、压缩包、个人账单、数据库、设置、API Key 或共享 AI 凭据。
- 第三方资源保留原有许可；不擅自给整个课程项目新增开源许可证。
- 中文 PowerShell 脚本保留 UTF-8 BOM，并通过 `scripts/Test-LauncherCompatibility.ps1` 的 Windows PowerShell 5.1 检查。

## 发布

按 [构建与发布](docs/RELEASE.md) 执行 `scripts/Build-Unified.ps1`。它通过完整回归、源码指纹与打包检查后更新 `build/current.json`；编译或打包期间源码变化时停止提升版本。

普通分发不包含 AI 共享凭据。带 `-IncludeTrialAI` 的小组测试产物仅限授权测试用户，不能作为公开 GitHub Release 附件。安装包规则见 [安装说明](docs/INSTALLER.md)。
