# 执行页简洁版

0.12.8 历史核对后恢复首屏直接输入“买什么”，回车或下一步直接进入模式选择。共用文字／截图购买弹窗作为次级入口保留，并携带已填商品名；简报页的名称编辑与首屏同步。新增回归保护真实入口，避免其他功能调整再次用按钮替换掉渐进输入。详见 [精修历史核对](UI_HISTORY_AUDIT.md)。

2026-10-28，按用户实际使用后的反馈，重新对照原网页 `Kura/src/pages/Execution.tsx`、`DecisionBrief.tsx`、`ShoppingPrinciples.tsx` 和 `generateDecisionBrief.ts`，在原生 Qt Widgets 中还原渐进引导。

首屏只有小标题「执行层」和两张卡片：「买什么？」的单一输入框，以及默认展开的「购物原则」。空输入时提交按钮禁用，填写商品名后点击「生成决策简报」或按回车，才出现下一屏。移除英文标语、大标题、01／02／03 编号、提前出现的模式与计时状态。购物原则支持直接添加、编辑，使用与设置相同的数据库记录。

提交后显示简短的「决策简报」和「决策模式」两张卡片。金额、用途、品类、品牌位于可选补充中，不要求先填写金额；可通过「重新输入」返回。模式建议沿用原网页：预计金额大于 500 元，或名称包含数码／电子类关键词时建议研究模式，否则建议快速决策，用户可以自行选择。进入简报不会创建决策会话，也不自动调用付费 AI。

点击「开始」后才出现当前模式的内容。快速模式显示倒计时和「提前决策」，倒计时结束或主动提前决策后出现最终操作；不会自动购买。研究模式显示三项确认和可选研究记录。开始后可在两种模式间切换，保留会话编号、记录和原有截止时间，重启恢复当前模式。未填写金额时不显示大号「金额待确认」或空预算面板；填写后按真实的基础生活／可支配品类计算影响。AI 问答与预算参考默认折叠。

点击「修改商品」可以更新当前决策的名称、用途、金额和品类，不重新开始计时。用途与品牌选择存入现有研究记录 JSON，兼容旧记录，不新增数据库字段。研究勾选立即保存，文字记录使用保存按钮，切换步骤与做决定前也会保存。

「加入清单，稍后再想」打开预填当前商品的确认表单。取消不写入愿望，也不结束决策。确认时，愿望新增与当前会话结束作为一个 SQLite 事务提交；失败回滚，重复提交不会重复新增。该操作不产生消费。购买仍由既有确认表单填写实际金额，成功后记账并安排后续复盘。

暖色、冷色、深色沿用统一主题，内容宽度与原网页一致为 640px（含左右内边距）。小窗口使用页面滚动，底部导航保持可用。`execution_ui_tests` 现含 8 个场景，覆盖初始渐进显示、仅输入商品名、购物原则保存与取消、两种模式切换、过期计时不产生消费、预算变化、重启恢复、加入清单事务和小窗口布局。既有 workflow／shell 测试继续覆盖购买、AI 上下文与外壳导航。

MSYS2 Qt 的 moc 不支持本项目的中文构建路径，可通过英文目录联接构建同一份源码：

```powershell
cmake -S C:/Users/Fall/.codex/tmp/kura-execution-source-20261027 -B C:/Users/Fall/.codex/tmp/kura-execution-source-20261027/build-simple -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DBUILD_TESTING=ON
cmake --build C:/Users/Fall/.codex/tmp/kura-execution-source-20261027/build-simple --parallel 4
ctest --test-dir C:/Users/Fall/.codex/tmp/kura-execution-source-20261027/build-simple --output-on-failure
```

`build/preview/execution-guided-*.png` 为独立测试数据库生成的新版界面截图。之前的 `execution-simple-*` 为历史样板实现。

本次 Release 完整回归 19/19 通过。最后收紧无金额页面后，执行页、workflow 和 shell 三组再次回归。新版独立运行目录为 `build/execution-guided-run/`，包含 Qt SQL／SQLite 等依赖；`KuraCpp.exe` 打开正式数据，`Demo.cmd` 使用隔离演示数据。
