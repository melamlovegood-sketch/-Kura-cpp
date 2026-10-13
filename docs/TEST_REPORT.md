# Kura C++ 0.8.0 测试记录

验证日期：2026-10-13。环境：Windows 11 x64、GCC 15.2、Qt 6.10.1、MSYS2 UCRT64、CMake/Ninja。测试数据库全部为独立临时文件，不读取正式消费数据。

## 自动回归

初次 Debug 构建执行 9 组测试，9/9 通过。接入 B 的补充提交和分类复盘后，最终 Release 构建执行 9 组，9/9 通过，总用时约 24 秒；交付以本次 Release 检查为准。

| 测试组 | 主要验证内容 | Release 结果 |
|---|---|---|
| category_summary_tests | B 的六类金额、笔数、占比、上月变化、月界、空分类、筛选隔离、增改删刷新 | 通过 |
| repository_tests | 时间、备注、CRUD、持久化、迁移、编号和数据库异常 | 通过 |
| window_tests | 表单、确认取消、筛选、汇总及失败保留输入 | 通过 |
| summary_tests | 日期范围、整数金额、跨月跨年和空结果 | 通过 |
| budget_tests | 月份隔离、零与未设置、超支、联动及失败保留输入 | 通过 |
| statistics_tests | 年度趋势、月均、最高月份、跳转及加载失败保留快照 | 通过 |
| wishlist_tests | 冷静期边界、购买事务、重复购买、放弃和页面更新 | 通过 |
| data_management_tests | CSV、WAL 快照、整库恢复、回滚、旧备份与取消 | 通过 |
| release_tests | 演示金额与状态、拒绝覆盖已有数据、优先级迁移、分类复盘、800×600 布局 | 通过 |

## 界面检查

已生成并查看 1180×860 下记账、统计、愿望、数据管理的 Qt 页面截图，以及 800×600 记账页的顶部和底部截图。关键按钮、日期筛选、分类金额、优先级及数据管理入口可显示；紧凑布局使用滚动访问下方内容。截图来自虚构演示数据。

## 运行依赖检查

在独立运行目录中放置 Release 程序和同 Kit 的 DLL／插件，设置 `qt.conf` 指向本地插件，并将 PATH 限制为 Windows 系统目录、清除额外 Qt 插件路径。category_summary_tests、data_management_tests、release_tests 均通过，程序 `--version` 正常退出。

图形自动测试使用 Qt 的 offscreen 平台。这证明相同组件及 SQLite 在运行目录的依赖齐备，不等同于另一台电脑的人工验收。完整程序包不包含测试 exe 或 Qt6Test.dll。

## 尚需人工记录

另一台 Windows 电脑启动、真实桌面与投影缩放、Excel 实际显示、组员交叉操作及教师验收尚需按 ACCEPTANCE.md 填写。未声称完成这些检查。课程真实名单、贡献度百分比和个人报告需由组员提供。
