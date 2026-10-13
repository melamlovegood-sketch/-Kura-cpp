# Kura C++

基于 C++17、Qt 6 Widgets 和 CMake 的课程项目。

## 当前功能

点击“记一笔”，填写消费名称、金额、分类和日期。保存后，消费记录会显示在主窗口表格中；取消或关闭弹窗不会新增记录。

- 消费名称不能为空，保存时去除首尾空格，最多输入 100 个字符。
- 金额必须大于 0，最多两位小数，上限为 99,999,999.99 元。
- 分类：餐饮、交通、购物、娱乐、学习、其他。
- 日期默认为今天，可以通过日历选择。
- 当前记录只在本次运行期间保留，关闭程序后不会保存。

## 编译运行

在 Qt Creator 中打开 `CMakeLists.txt`，选择包含 Qt 6 Widgets 的桌面 Kit，配置、构建并运行。

也可以在已配置好 Qt 和编译器的终端运行：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="你的 Qt 6 Kit 目录"
cmake --build build
```

Qt Kit 和编译器必须匹配，例如 MSVC Kit 配 MSVC、MinGW Kit 配对应的 MinGW。生成的程序位于构建目录；MSVC 多配置生成器通常在 `Debug` 或 `Release` 子目录。

如果使用 MSYS2 的 Qt/MinGW，遇到 `moc: Cannot create ... Illegal byte sequence`，请在纯英文路径中构建项目；这类工具可能无法处理中文目录。

## 本次代码

- `src/AddTransactionDialog.h/.cpp`：表单、输入校验和输入结果读取。
- `src/mainwindow.h/.cpp`：B 的主窗口布局，以及弹窗与表格的简单连接。
- `src/main.cpp`：程序入口。

弹窗暂时独立于 A 的 `Transaction` 数据类。只有收到 `accepted` 信号后才能读取本次有效输入：

```cpp
dialog->description(); // QString，消费名称
dialog->amount();      // double，金额
dialog->category();    // QString，分类
dialog->date();        // QDate，日期
```

后续接入数据类时，可在主窗口的 `accepted` 回调中用这些值构造 `Transaction`，再交给记录列表管理。

## 手动验收

1. 启动程序，点击“记一笔”，确认弹窗正常出现，日期为今天。
2. 输入“午饭 / 28 / 餐饮”，保存，确认表格新增一行且金额为 `¥28.00`。
3. 再添加“地铁 / 4 / 交通”，确认两条记录都存在。
4. 填写后点击取消、按 Esc 或关闭弹窗，确认记录数量不变。
5. 消费名称留空或只输入空格，保存时应提示错误并保持弹窗打开。
6. 金额为 0 时不能保存；输入 `0.01` 可以保存。
7. 更改分类和日期后保存，确认表格与输入一致。
8. 多次打开弹窗，确认表单重置且不会重复添加记录。
