# 0.12.7 版本统一记录

2026-10-28，针对多个对话修改、各自交付演示程序造成的版本混用，确立同一份源码、完整验收和固定启动入口。

## 核对结果

- 当前桌面应用源码：项目根目录下 `-Kura-cpp/`。主目录已包含近期各功能修改，没有通过旧副本整文件覆盖。
- `Kura/` 是原网页项目，`阶段演示/` 是 10 月 13 日历史交付，不是另一个待合并的最新版桌面源码。
- `C:/Users/Fall/.codex/tmp/kura-execution-source-20261027` 是指向主目录的目录联接，实际编辑的是同一份文件。
- 独立检查了 `kura-chat-01a17d9e/source`、`kura-price-screenshot-source-20261028`、`kura-product-source-20261028`、`kura-aesthetic-*`、`kura-fonts-*`、`kura-glyph-probe-source-*`。它们没有主目录缺失的文件；有差异的文件修改时间均早于主目录。另逐项对照商品资料、截图记价和近期对话的功能要求，沿用主目录中已整合的实现。
- Git 注册的两个旧临时工作区没有剩余源码文件；分支提交均已包含在当前主目录的祖先历史中，不将其删除状态合并回来。
- 此前 `aesthetic-preview`、`billing-focused-run`、`planning-focused-run`、`secondary-focused-run` 以及不同后缀 ZIP 都是各自生成时的静态快照，不会随主源码变化。仅按包名或修改时间启动容易看到不同功能组合。
- “评估应用界面交互并反馈精简建议”对话在本次核对期间完成统一清单；本次发布以它完成后的源码为基础。

## 当前统一功能

| 范围 | 保留的最新行为 | 主要实现与验收 |
| --- | --- | --- |
| 主页与购买 | 两个入口共用购买流程；输入、核对、选择去向；可手动填，研究交接保留资料 | HomeWidget、DecisionDialog；decision、shell、workflow |
| 账单 | AI／手动录入、最近记录、日历／月度／半年图表、双预算；完整记录页聚焦筛选 | BillingWorkspace、BillingCharts、ExpenseComposer；billing、window、budget |
| 导入 | 选择文件、核对、完成；CSV／XLSX；判重、批量分类、固定底部按钮 | BillImportDialog、BillParser；import、supplement_ui、secondary_ui |
| 统一清单 | 待购和蹲价合一；每件商品独立价格历史、目标价、链接；冷静期只限制购买 | WishlistWidget、UnifiedWishlistRepository、WishPriceDialog；planning_layout、wishlist、price_record |
| 排序 | 默认按添加时间；可拖动／上下移动并保存手动顺序；到期不重排 | WishlistWidget、wishlist_order；planning_layout |
| 执行 | 渐进决策简报、模式、计时与研究记录；购物原则／AI 按需展开；购买和加入清单保留事务保护 | ExecutionWidget、ExecutionRepository；execution_ui、workflow、decision_chat |
| 复盘 | 消费人格、故事、值得／后悔列表；购买后复盘和年度统计为次级入口 | MonthlyReviewWidget、MonthlyStoryTemplate；monthly_review、review_layout、statistics |
| 弹窗与商品详情 | 原生圆角卡片、固定操作区；核心字段优先；资料默认查看、按需编辑和 AI 讨论 | DialogUi、ModernSelectors、ProductDetailsDialog；dialog、product_details、secondary_ui |
| 设置与数据 | 四个设置入口、三套主题；原则与品牌独立编辑；导入／导出／备份／恢复 | AppearanceWidget、DataManagementWidget；shell、data_management |
| 持久化兼容 | 保留原消费、商品资料、截图、价格历史；旧蹲价迁移；历史归档不计为新放弃；旧备份恢复 | DatabaseBackupService、ProductRepository；repository、data_management、workflow |

## 唯一启动与更新入口

上一级目录：

- `启动最新版.cmd`：使用原应用的正式数据库路径。
- `体验最新版.cmd`：临时演示数据库，退出后删除。
- `更新最新版.cmd`：重新编译、完整回归、打包并更新固定入口。

脚本使用 `build/current.json` 指向通过验证的版本目录。旧包原样保留，正式数据库不搬移、不覆盖。每个新包保存 `build-manifest.json`（源码逐文件 SHA-256、程序 SHA-256、通过的测试组数）和 `verified-tests.log`。启动入口会验证程序与源码；源码变化时先重新构建，校验失败则不启动旧程序。

`scripts/Build-Unified.ps1` 使用英文目录联接 `LOCALAPPDATA/KuraBuild/source` 构建主源码，并独占发布锁。测试失败或构建、打包期间源码有变化，均不更新 `current.json`。编译输出是 `LOCALAPPDATA/KuraBuild/build/`，与其他对话已有构建目录分开；实际输出目录也使用英文路径，避免 MSYS2 工具解析目录联接后的中文路径失败。

## 备份与验证范围

合并前源码备份保存在上一级 `版本整合备份/20261028-122854/`，含主目录与各独立副本的 ZIP、差异清单；最后一轮功能完成后的源码另存快照。备份不包含用户业务数据库和密钥。

验收运行项目全部 25 组测试，测试结果以当前运行包附带的日志为准。AI 自动化测试使用本地模拟服务，真实服务连通性需使用者自行配置验证。本次不请求真实账单或用户密钥。
