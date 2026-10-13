# Kura C++ 实现说明

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
