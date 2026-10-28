# Kura C++ 实现说明

0.12.0 增加原生 ZIP／OOXML 读取与 CSV 编码解析、BillImportRepository 和后台预览界面、MonthlyReviewRepository 与月度页面。SupplementSchema 同时用于升级、备份核验和恢复字段。导入以稳定索引加事务保证判重，删除消费保留索引墓碑；月度事实指纹覆盖相关记录与预算，生成后核对再保存。AIService 增加结构化分类和故事通道。详见 IMPORT_MONTHLY_REVIEW.md。

0.11.0 新增 WorkflowSchema、ShoppingProfileRepository、ExecutionRepository、PurchaseRepository、ReviewRepository 和 PriceRepository，以及对应原生 Qt 页面。SQLite 保存点支持购买、复盘安排、会话结束和品牌反馈的原子提交及嵌套操作。WorkflowSchema 同时供升级、备份结构核验与恢复字段列表使用，避免新表遗漏。AIService 增加纯文本分析通道，仍复用既有鉴权、超时、取消和响应边界。完整业务规则见 WORKFLOW_PHASE3.md。

0.10.0 新增 CategoryCatalog、DecisionSchema、WishPoolRepository、AISettings／AIService、DecisionDialog 和 WishPoolWidget。保留六类汇总并增加细分类映射，两项预算与合计同次写入。截图用 BLOB 随业务记录保存；目标和忍住记录分表持久化，暂存金额在设置目标的事务中归集。购买／放弃／删除愿望时同时关闭对应目标；完整备份恢复覆盖五张业务表。AI 通过异步 Qt Network 请求配置的兼容接口，输出先校验并供用户修改，只有明确选择才持久化。密钥使用当前 Windows 用户的 DPAPI，与业务备份分离。完整规则见 DECISION_FLOW.md。

0.9.0 新增 HomeWidget、AppearanceWidget 和 ShellWidgets，用 QStackedWidget 管理既有业务页面和新增页面。MainWindow 提供固定页头、底部导航和首页购买入口；AppTheme 统一三套主题及原生矢量图标。业务对象继续保留，切换页面与主题不会重建表单。设置通过独立 INI 文件保存。完整范围见 UI_REFRESH.md；以下为既有业务实现说明。

程序用 C++17 与 Qt Widgets 实现本地消费记录、预算、趋势和延迟购买流程，Qt SQL 访问 SQLite。没有 Web 前端或云服务。

| 层次 | 组件 | 职责 |
|---|---|---|
| 界面 | MainWindow、各页 Widget、编辑 Dialog | 输入、选择、提示、刷新与页面导航 |
| 模型和规则 | Transaction、WishItem、MonthlyBudget、TransactionFilter | 校验、整数金额、状态及范围 |
| 聚合 | ExpenseSummary、ExpenseStatistics、CategorySummary | 完整快照上的全局、年度、分类汇总 |
| 持久化 | DatabaseManager、各 Repository | 建表与兼容升级、参数化 SQL、错误返回 |
| 数据管理 | CsvExporter、DatabaseBackupService | CSV、一致备份、验证和事务恢复 |
| 演示与外观 | DemoData、AppTheme | 隔离示例数据、共享字体颜色与控件风格 |

交易 id 用于定位编辑和删除，不能以表格行号代替。列表是筛选后的快照，预算和分类统计使用全部记录。金钱用 qint64 的整数分保存。消费时间是本地钟面时间；冷静期时间是 UTC 时间戳，显示时转换为本地时间。

购买先在事务中领取仍待决定且已到期的愿望，再插入消费并保存关联编号。任何一步失败都会回滚；提交成功才关闭表单。恢复先冻结并检查来源快照，锁住写入、保存恢复前备份，然后在同一事务中替换消费、预算、愿望与编号信息。

0.8.0 为旧愿望增加 priority 列，默认 2 表示中优先级。旧版完整备份在私有副本中升级，不修改来源文件。界面统一保持既有操作和对象接口；宽窗口中分类面板在右侧，小窗口中置于记录下方，通过滚动查看。

B 的分类模块以原分支两次提交合并，计算和测试保留在 CategorySummary 和 category_summary_tests 中。本月金额在记账页显示；按指定年月计算的金额、笔数、占比和上月变化在统计页“分类复盘”显示。金额和变化始终用整数分，占比由金额比值计算，仅在显示时舍入。
