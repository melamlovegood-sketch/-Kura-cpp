# 仓库整理与分支记录

整理日期：2026-10-28；远端同步完成：2026-10-29。仓库：[melamlovegood-sketch/-Kura-cpp](https://github.com/melamlovegood-sketch/-Kura-cpp)。

## 整理前的问题

- 远端默认分支为 `feature/b-main-window`，已包含 0.16.6；远端 `main` 则停在较早的消费时间／备注实现。
- README 标注 0.16.0，使用说明标注 0.13.0，发布说明开头仍写 0.12.6。
- 使用、设计、历史验收与开发接口集中在首页，文档目录缺少导航；本地启动 CMD 与 GitHub 源码下载的区别不清楚。

## 整理内容与远端状态

重写首页，增加文档索引、协作约定和 PR 模板；更新使用、构建与验证说明；补充构建产物与本地配置忽略规则。原 README 存档为 [历史参考](HISTORICAL_README.md)，现有专题文档路径保留。

**远端迁移已完成。** GitHub 默认分支与本地 `origin/HEAD` 均已改为 `main`，仓库简介已补充。完整 0.17.0 源码与整理提交已通过普通快进推送同步到主线，原提交历史保留。

后续更新前重新获取远端并确认没有独立新增工作，使用普通推送（禁止 force push）：

```powershell
git fetch origin
# 确认 origin/main 仍为本地 main 的祖先；非零退出时先核对并集成新提交
git merge-base --is-ancestor origin/main main
git push origin main:main
```

仓库简介：“Kura · C++17 / Qt 6 理性消费与记账助手，支持双预算、愿望冷静期、AI 辅助决策与月度复盘。”旧功能分支仅保留用于追溯贡献，后续开发从 `main` 开始。

## 历史分支清单

以下为 2026-10-28 整理前的远端快照，不是当前主线状态。已使用 Git 祖先关系核对：快照中各分支的提交都包含在当时完整集成基线 `e8ce330` 中，无分支独有提交被遗漏。分支暂时保留，便于组员核对贡献；未删除或重命名，也未改写历史。

| 分支 | 当时提交 | 用途／状态 |
| --- | --- | --- |
| `main` | `6665663` | 整理前旧主线，比当时集成基线少 10 个提交 |
| `feature/b-main-window` | `e8ce330` | 当时默认分支，完整 0.16.6 集成基线 |
| `feature/release-polish` | `41ef485` | 早期发布整理与验收说明 |
| `feature/b-category-summary` | `c03913d` | B 的分类汇总、笔数、占比及月度变化 |
| `feature/add-transaction-dialog` | `3cfecd4` | 消费录入弹窗 |
| `feature/sqlite-persistence` | `e6d793e` | SQLite 持久化 |
| `feature/complete-bookkeeping` | `27d0998` | 消费编辑、删除、筛选与汇总 |
| `feature/monthly-budget` | `3406df6` | 月度预算与超支提示 |
| `feature/spending-trends` | `4254940` | 年度统计与趋势 |
| `feature/wishlist-cooling` | `5da93c3` | 愿望清单、冷静期与购买记账 |
| `feature/data-management` | `23398a7` | CSV 导出与数据库备份恢复 |
| `feature/transaction-details` | `6665663` | 消费时间与备注 |

本地 `integration/unified-20261028` 是整合工作分支；另有旧 UI／详情任务的工作树，不属于本次清理目标。后续新功能按 [开发协作](../CONTRIBUTING.md) 从统一主线创建短期分支。

整理期间另一个任务在同一工作区开始 0.17.0 自定义分类开发，第一轮整理保留其全部未提交修改。该任务随后提交 `fc420da` 并完成统一验收；2026-10-29 远端同步时已包含这轮功能和本次文档整理。当前工作区继续保留原整合分支，`main` 指向同一集成结果。
