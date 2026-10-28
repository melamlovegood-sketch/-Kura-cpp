# 当前字体方案（0.16.0）

2026-10-28 美学升级改用内嵌思源黑体与霞鹜文楷轻便版。当前字体角色、字号和验收规则见 [美学规范](AESTHETICS.md)。下文为此前 0.15.x 对原网页的核对历史；其中微软雅黑／宋体及 16px 页面标题已由新方案替代，不是当前验收标准。

# 原版字体核对与修正

2026-10-28，参照同级 Kura 网页项目的实际浏览器渲染结果。

## 字体依据

原版 `src/index.css` 的无衬线字体栈为 `'DM Sans', 'PingFang SC', -apple-system, sans-serif`；衬线字体栈为 `'DM Serif Display', Georgia, serif`。`index.html` 从 Google Fonts 加载 DM Sans 的正常／斜体及多种字重，以及 DM Serif Display 的正常／斜体。

在本机 Windows Chromium 中，使用 CDP `CSS.getPlatformFontsForNode` 检查实际参与绘制的字体，不能只检查 CSS 的 `font-family` 或 Qt 的 `QFontInfo`。

| 内容 | 原版在本机实际使用的字体 |
|---|---|
| 中文正文、页面标题 | Microsoft YaHei（微软雅黑，不是 Microsoft YaHei UI） |
| 英文正文、普通数字 | DM Sans，按原样式使用 400／500 等字重 |
| KURA 品牌、金额和衬线数字 | DM Serif Display |
| `font-serif` 中的中文 | SimSun（宋体） |

字体文件版本核对：DM Sans 4.004、DM Serif Display 5.200，与原网页实际加载的 Google Fonts v17 文件相符。正常与斜体四个文件作为 Qt 资源内嵌；中文使用 Windows 已安装的字体，不复制或分发 Windows 字体文件。

## 修正内容

- `AppTypography` 显式设置 DM Sans 的 `wght` 和 `opsz` 轴。此前本机 DirectWrite 在部分 Regular／Medium 文本上实际选择了 Thin；字体族名称相同并不代表字形正确。
- 正文的中文回退改为微软雅黑；衬线混排明确使用 DM Serif Display、Georgia、宋体。
- 页面标题对齐原版 16px / 500；商品名称 15px / 500，清单金额 16px 衬线，复盘数字 22px 衬线，复盘故事正文 15px。品牌保留 22px，首页大金额保留 52px。
- 设置页、弹窗、新建行、切换主题后的控件都通过同一字体规则解析。自绘账单金额使用原版的衬线字体。
- Qt offscreen 平台没有自动发现本机 TTC 中文字体集合，曾错误回退到等线；测试时显式注册 Windows 已安装的微软雅黑、宋体集合，使截图与实际窗口选择相同字形。

## 字形测量

使用相同文字、相同像素字号和字重对照；单位为逻辑像素。

| 样本 | 原网页 Chromium | 修正后 Qt Windows |
|---|---:|---:|
| `Kura / AI Settings 0123456789`，15px / 400 | 208.46977 | 208.25000 |
| `账单 清单 执行 复盘 Settings`，16px / 500 | 206.41794 | 206.31250 |
| `¥1,234.56 KURA`，52px / 400 | 359.68390 | 359.54688 |
| `把心动，留给真正值得的。`，15px / 400 | 180 | 180 |

浏览器与 Qt 仍有抗锯齿和亚像素取整差异，不能宣称逐像素完全相同。字体、实际中文回退、Regular／Medium 字形及关键字号均已逐项核对。

原网页字形报告、修正前后 Qt 字形报告、对比图及页面截图保存在 `build/font-audit/`。新增 `typography_tests` 检查实际 glyph run、变量轴、原版样本文字宽度、斜体、主题切换及后创建的弹窗。

最终 Release 回归 **21/21 通过，用时 45.80 秒**。字体专项同时在 Qt offscreen 与真实 Windows 平台通过；另在 Windows 平台生成七页、三套主题、两个尺寸的 42 张截图并通过布局检查。演示入口为 `build/aesthetic-preview/Demo.cmd`，使用临时演示数据。
