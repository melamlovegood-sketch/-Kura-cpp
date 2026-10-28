# Kura · 理性消费与记账助手

当前统一版本 **0.17.0**：默认五类统一、自定义分类管理及执行页入口精简，详见 [分类管理](docs/CATEGORIES.md)。

基于 **C++17 / Qt 6 / SQLite** 的 Windows 桌面课程项目。把记账、预算、购买前思考、愿望冷静期与月度复盘放在一起，帮助使用者看清消费，也给冲动购买留一点思考时间。

最近已验收基线：**0.17.0**。开发中的源码版本以 [CMakeLists.txt](CMakeLists.txt) 为准，功能与版本变化见 [版本统一记录](docs/VERSION_UNIFICATION.md)。只有通过统一构建与回归的产物才作为最新版分发。

**[使用说明](docs/USER_GUIDE.md) · [课堂演示](docs/DEMO_PERSONAS.md) · [文档索引](docs/README.md) · [开发协作](CONTRIBUTING.md) · [构建与发布](docs/RELEASE.md)**

## 可以做什么

| 页面 | 主要功能 |
| --- | --- |
| 主页 | 查看必要生活／自由消费预算、许愿池和待处理事项，继续未完成的分析 |
| 账单 | 手动或 AI 辅助录入，预览导入微信／支付宝 CSV、XLSX，筛选记录与查看消费图表 |
| 清单 | 管理待购物品、优先级与冷静期，维护商品资料、截图、价格记录和目标价 |
| 执行 | 购买前快速思考或研究计时，参考本地消费简报，按需使用 AI 分析与追问 |
| 复盘 | 标记值得／后悔的消费，查看月度人格故事、忍住金额和栗子成就 |
| 设置 | 主题与字号、新手引导、价格换算、AI 服务、数据导出及完整备份恢复 |

基础记账、清单、预算和备份可离线使用。AI 功能在用户主动操作时请求配置的服务，解析结果需确认后保存；个人 API Key 与业务数据库分开保存。配置方法见 [千问 Key 教程](docs/QWEN_API_KEY_GUIDE.md)。

## 使用与体验

已有完整项目工作区时，使用仓库上一级的 **“启动最新版.cmd”** 或 **“体验最新版.cmd”**；“更新最新版.cmd”负责重新构建和验收。这些 CMD 属于本地工作区，不包含在 GitHub 源码仓库中。

拿到安装包或免安装包时，按 [安装说明](docs/INSTALLER.md) 操作。免安装包需完整解压，再运行 `KuraCpp.exe`；`Demo.cmd` 使用临时虚构数据，不修改正式账本。安装包与源码 ZIP 是不同产物，GitHub 的 **Download ZIP 仅包含源码**。

从源码启动已有验收包：

```powershell
# 在本仓库根目录执行；首次构建需要本机 Qt 工具链及运行依赖
./scripts/Start-Current.ps1
./scripts/Start-Current.ps1 -Demo
```

启动脚本校验并打开最近一次已验收版本。源码有变动时，运行 `./scripts/Build-Unified.ps1` 更新；严格核对源码与运行包使用 `./scripts/Start-Current.ps1 -CheckOnly`。首次克隆与构建步骤见 [构建与发布](docs/RELEASE.md)。

## 源码结构

```text
src/             Qt 页面、业务模型、数据仓储与 AI 服务
tests/           Qt Test 自动回归与本地 AI 模拟服务
resources/       字体、图标与 Qt 资源
examples/        虚构账单与导入样例
scripts/         构建、校验、打包与安装器脚本
docs/            使用、设计、验收与历史记录
CMakeLists.txt   构建目标、依赖和版本号
CONTRIBUTING.md  分支与协作约定
AGENTS.md        自动化协作规则
```

构建输出和个人数据不提交到仓库；`build/*-run/`、预览图和旧 ZIP 是历史快照。桌面功能只在本仓库维护，上一级的 `Kura/` 为原网页参考，`阶段演示/` 为历史交付。

## 开发与验收

依赖为 CMake 3.16+、C++17、Qt 6.7+（Widgets / SQL / Network；测试另需 Test）、Zlib 和匹配的编译器；当前 Windows 验证环境为 MSYS2 UCRT64 / Qt 6.10.1 / Ninja。

日常编译与测试使用独立构建目录；MSYS2 遇到中文路径问题时，创建指向本仓库的英文目录联接。发布必须执行 `scripts/Build-Unified.ps1`，完整回归及源码指纹核对成功后才更新固定入口。AI 自动验收使用本地模拟服务。

使用验收见 [验收清单](docs/ACCEPTANCE.md)，现有验证证据见 [测试记录](docs/TEST_REPORT.md)。整理前 README 中的详细接口与旧验收示例保留在 [历史参考](docs/HISTORICAL_README.md)。
