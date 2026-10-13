# Kura C++

基于 C++17、Qt 6 Widgets、Qt SQL、SQLite 和 CMake 的课程项目。

## 当前功能

点击“记一笔”，填写消费名称、金额、分类和日期。保存成功后关闭弹窗并刷新列表；重新打开程序时自动读取消费记录。

- 消费名称不能为空，保存时去除首尾空格，最多输入 100 个字符。
- 金额必须大于 0，最多两位小数，上限为 99,999,999.99 元。
- 分类：餐饮、交通、购物、娱乐、学习、其他。
- 日期默认为今天，可以通过日历选择。
- SQLite 自动生成编号，列表按消费日期倒序、同一天按编号倒序显示。
- 保存失败时显示原因，保留表单内容，可以修正问题后再次保存。
- 暂未实现编辑、删除、筛选、统计、预算等功能。

## 数据保存位置

数据保存在 Qt 的本机应用数据目录 `QStandardPaths::AppLocalDataLocation` 下的 `kura.sqlite`，与源码和程序所在目录无关。Windows 通常为：

```text
%LOCALAPPDATA%\Kura\KuraCpp\kura.sqlite
```

鼠标悬停在主窗口底部状态栏，可以查看实际文件路径。升级或移动程序不会主动清空数据；备份时先关闭程序，再复制数据库文件。

第一次运行会自动创建目录和表。数据库打不开、文件损坏或缺少 SQLite 驱动时，主窗口会提示错误并禁用新增按钮，不会偷偷覆盖或清空数据；解决问题后重新启动。

上一阶段只存在内存中的记录没有落盘，因此不能从旧程序恢复。实际数据库文件及构建目录已被 Git 忽略，不会随源码提交。

## 编译运行

在 Qt Creator 中打开 `CMakeLists.txt`，选择包含 Qt 6 Widgets、SQL、Test 的桌面 Kit，配置、构建并运行。不需要单独安装 SQLite 服务。

也可以在已配置好 Qt 和编译器的终端运行：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="你的 Qt 6 Kit 目录"
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC 多配置构建可以在构建时加 `--config Debug`，测试时加 `-C Debug`。如果仅构建程序、不运行测试，可在配置时加 `-DBUILD_TESTING=OFF`，此时不需要 Qt Test。

Qt Kit 和编译器必须匹配，例如 MSVC Kit 配 MSVC、MinGW Kit 配对应的 MinGW。生成的程序位于构建目录；MSVC 多配置生成器通常在 `Debug` 或 `Release` 子目录。

如果使用 MSYS2 的 Qt/MinGW，遇到 `moc: Cannot create ... Illegal byte sequence`，请在纯英文路径中构建项目；这类工具可能无法处理中文目录。

给没有 Qt 环境的电脑提供程序时，应使用对应 Kit 的 `windeployqt` 部署运行库，检查包含 `Qt6Sql` 和 `sqldrivers/qsqlite` 插件。MSYS2 还需补齐它们依赖的 MinGW/第三方 DLL。交付完整程序目录，不能只发一个 exe。

## 代码结构

| 文件 | 职责 |
|---|---|
| `src/Transaction.h/.cpp` | 消费数据、统一分类、数据校验 |
| `src/DatabaseManager.h/.cpp` | 数据路径、数据库连接、建表、连接清理 |
| `src/TransactionRepository.h/.cpp` | 参数绑定、新增记录、查询列表、数据库错误 |
| `src/AddTransactionDialog.h/.cpp` | 表单、输入校验、保存请求和失败提示 |
| `src/mainwindow.h/.cpp` | 连接表单和数据接口、加载和刷新列表 |
| `src/main.cpp` | 应用名称、程序入口 |
| `tests/` | 数据持久化、金额精度和界面失败路径测试 |

金额以整数“分”保存，`28.00` 元就是 `2800` 分。业务计算优先使用 `amountCents()`；`amount()` 只是读取以元表示的值，列表金额通过整数拼接显示两位小数。

当前表结构：

```sql
transactions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    description TEXT NOT NULL,
    amount_cents INTEGER NOT NULL,
    category TEXT NOT NULL,
    date TEXT NOT NULL
)
```

实际建表语句还包含名称长度、金额范围和类型、日期长度等约束。日期存储为 `yyyy-MM-dd`，查询和插入统一由 Repository 执行。

## 给 B 的接续说明

### 已经可用的接口

```cpp
DatabaseManager database; // 默认本机应用数据路径
if (!database.initialize()) {
    // 读取 database.lastError() 并提示用户。
}
TransactionRepository repository(database);

Transaction item(0, QStringLiteral("午饭"), 2800,
                 QStringLiteral("餐饮"), QDate::currentDate());
if (repository.add(item)) {
    // item.id() 已回填 SQLite 生成的编号。
}

QList<Transaction> records;
if (!repository.findAll(records)) {
    // 使用 repository.lastError() 区分查询失败和正常的空列表。
}
```

`Transaction` 构造函数第三个参数单位是“分”，不是“元”。新记录编号必须为 0。已经保存的对象不能再次调用 `add()`，避免无意重复插入。

弹窗保存流程：

```text
点击保存
→ AddTransactionDialog::accept() 校验输入
→ 发出 saveRequested 信号（此时不会关闭弹窗）
→ MainWindow 读取 dialog->transaction()
→ m_repository.add(transaction)
→ 失败：dialog->showSaveError(...)，保留输入
→ 成功：dialog->completeSave()，然后 reloadTransactions(transaction.id())
```

数据库写入成功、但列表刷新失败时，主窗口会明确提示“记录已保存”，避免用户重试插入导致重复记录。查询失败也不会先清空现有表格。

### B 接下来做编辑和删除

1. 在 `TransactionRepository` 增加 `update(const Transaction &)` 和 `remove(qint64 id)`，使用绑定参数和明确的成功/失败结果；检查目标编号是否存在。
2. 在主窗口增加编辑、删除入口；删除前让用户确认，取消不操作数据库。
3. 选中记录的数据库编号已存在第 0 列的 `Qt::UserRole`：

```cpp
const int row = m_transactionTable->currentRow();
if (row >= 0) {
    const qint64 id = m_transactionTable->item(row, 0)->data(Qt::UserRole).toLongLong();
    // 用 id 修改/删除，不能把 row 当成数据库编号。
}
```

4. 主窗口的 `m_transactions` 是最近一次成功加载的记录，可按编号查找数据来预填编辑弹窗。
5. 修改或删除成功后调用 `reloadTransactions()`；失败时提示错误，保留界面数据。
6. 给新接口补充“取消不变、失败不变、重启后仍正确”的测试。新增源文件时，在 CMake 对应的 `kura_core` 或 `kura_widgets` 列表中登记。

## 验收和测试

1. 启动程序，点击“记一笔”，确认弹窗正常出现，日期为今天。
2. 输入“午饭 / 28 / 餐饮”，保存，确认表格新增一行且金额为 `¥28.00`。
3. 再添加“地铁 / 4 / 交通”，确认两条记录都存在。
4. 填写后点击取消、按 Esc 或关闭弹窗，确认记录数量不变。
5. 消费名称留空或只输入空格，保存时应提示错误并保持弹窗打开。
6. 金额为 0 时不能保存；输入 `0.01` 可以保存。
7. 更改分类和日期后保存，确认表格与输入一致。
8. 多次打开弹窗，确认表单重置且不会重复添加记录。
9. 关闭程序后重新打开，确认消费记录仍在。
10. 暂时占用数据库导致保存失败，确认表单不关闭、输入不丢；解除占用后重试只新增一次。

自动测试使用临时数据库，不读写实际个人消费数据：

- `repository_tests`：跨进程保存与读取、稳定编号、金额边界、整数精度、中文和引号、非法数据、数据库错误、连接隔离与清理。
- `window_tests`：启动加载、回车保存、取消及输入校验、锁定数据库时保留输入及重试、初始化失败提示、刷新失败保留列表。
