# Kura 离线字体（0.16.0）

全部字体编译到 Qt 资源中，首次启动、断网、换电脑都不需要下载或安装。

| 用途 | 拉丁字符 | 简体中文 |
|---|---|---|
| 导航、表单、正文、商品名 | DM Sans | Source Han Sans CN VF（思源黑体中国大陆字形） |
| 展示金额、品牌 | DM Serif Display | 思源黑体，保证“约”、币种与金额的功能可读性 |
| 首页寄语、月度故事标题、执行引导、许愿目标 | DM Serif Display | LXGW WenKai Lite（霞鹜文楷轻便版） |

- 思源黑体：Adobe `source-han-sans`，版本 2.005R，`Variable/OTF/Subset/SourceHanSansCN-VF.otf`。CN 是官方地区字形版本，未按当前文案裁切，仍可显示用户输入。wght 轴显式设置，避免默认 ExtraLight。
- 霞鹜文楷轻便版：`lxgw/LxgwWenKai-Lite`，版本 v1.501，`fonts/TTF/LXGWWenKaiLite-Regular.ttf`。原文件内嵌，未修改字体名称或轮廓；罕见字回退思源黑体。
- 拉丁字体保留既有 DM Sans 4.004 与 DM Serif Display 5.200。
- 四份 `*-OFL.txt` 为对应字体的 SIL OFL 授权，统一发布脚本同时复制到运行包 `licenses/fonts/`。
- `AppTypography` 与主题 QSS 使用一致的字体栈，拉丁字体在前、中文字体在后。无需网页 CSS、CDN、WOFF2 或系统级字体安装。

## 先前的网页字形核对记录

# Kura 字体

原网页的 `index.html` 加载 DM Sans 与 DM Serif Display 的正常和斜体。本目录包含同版本 TTF，编译到 `kura.qrc` 中，不依赖联网下载。

- DM Sans 4.004：`https://github.com/google/fonts/tree/main/ofl/dmsans`
- DM Serif Display 5.200：`https://github.com/google/fonts/tree/main/ofl/dmserifdisplay`
- OFL 授权分别见 `DMSans-OFL.txt` 和 `DMSerifDisplay-OFL.txt`，适用于对应字体的正常和斜体。

中文正文在 Windows 上使用已安装的 Microsoft YaHei；衬线中文使用 SimSun。这些 Windows 字体不在本目录内，也不随程序分发。字重、光学尺寸与实际字形验证见 `docs/TYPOGRAPHY.md`。
