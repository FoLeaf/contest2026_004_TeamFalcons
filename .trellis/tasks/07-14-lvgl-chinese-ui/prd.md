# Enable LVGL Chinese UI strings for VelaGuard home

## Goal

为 VelaGuard 首页启用 LVGL 中文显示，将用户可见状态文案改为中文；日志/设备 ID/固件号等协议字段可保留英文或数字。

## Requirements

- R1: 启用内置 `lv_font_simsun_16_cjk`（Kconfig + 构建脚本）。
- R2: 首页标题与状态区中文：采集、告警、网络、音频、时间、存储等。
- R3: 中文字符必须落在 SimSun16 CJK 字库内，避免空白方框。
- R4: 不破坏 mock 采集/告警数值刷新。
- R5: 本任务不做完整 i18n 框架。

## Acceptance Criteria

- [ ] 构建启用 `CONFIG_LV_FONT_SIMSUN_16_CJK`
- [ ] 真机首页可见中文状态文案
- [ ] 温度数值与告警变色仍正常
- [ ] 串口日志仍可读（可中英混合）

## Notes

字库为 LVGL 内置子集，非全汉字；新增文案前需核对字形是否在字库中。
