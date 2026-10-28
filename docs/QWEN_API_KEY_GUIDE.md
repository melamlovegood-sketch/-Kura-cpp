# 在 Kura 中使用自己的千问 API Key

设置 → AI 服务 → 个人配置下方提供“免费申请千问 API Key”官方入口与可展开的简短教程。申请入口和教程在使用 Kura AI 试用时也可查看；打开链接或展开教程不会修改配置，也不会调用 AI。保留 Kura AI 试用与个人配置切换。

1. 打开[阿里云百炼 API Key 管理（北京）](https://bailian.console.aliyun.com/cn-beijing/model/settings/api-key)，注册或登录阿里云，按页面提示开通百炼，确认地域为华北2（北京）。
2. 创建普通百炼 API Key，复制生成的密钥。无需购买套餐，也不需要设置电脑环境变量。
3. 打开[免费额度页面](https://bailian.console.aliyun.com/cn-beijing/costing-balance/free-quota)，检查要使用的模型是否有赠送额度及其到期时间；支持时提前开启“免费额度用完即停”。Key 创建不收费，但不代表模型一直免费调用；具体权益与开关生效状态以控制台为准。
4. 回到 Kura，选择“使用自己的 API Key”，将密钥粘贴到个人配置中。北京地域可使用服务地址 `https://dashscope.aliyuncs.com/compatible-mode/v1`，模型默认 `qwen3-vl-plus`，支持文字与截图；也可按实际权限选择其他支持图片的模型。地域、地址与 Key 需要匹配。
5. 保存 AI 设置，再测试文字连接与虚构截图。测试同样消耗调用额度；不要将个人 Key 发给他人。

官方说明（2026-10-28 核对）：[获取与配置 API Key](https://help.aliyun.com/zh/model-studio/get-api-key/)、[新人免费额度与用完即停](https://help.aliyun.com/zh/model-studio/new-free-quota)、[北京地域接入信息](https://help.aliyun.com/zh/model-studio/beijing-access-information)。官方目前说明新人免费额度仅限北京地域，额度和有效期按模型分别计算；教程不写死额度数值，后续以官方页面为准。
