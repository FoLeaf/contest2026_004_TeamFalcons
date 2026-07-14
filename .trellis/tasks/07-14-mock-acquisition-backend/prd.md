# Mock acquisition backend for UI without RS485 hardware

## Goal

在无扩展板/无传感器时，VelaGuard 仍能在首页展示**实时采集读数**，并为后续 #02/#03 预留可切换的采集后端接口（`mock` / 未来 `uart`）。

## Requirements

- R1: 定义最小采集 API：`init` / `poll` / 当前读数与质量。
- R2: 实现 **mock** 后端：1 个模拟温度点，1–2s 更新，带 scale 后工程值与 unit。
- R3: 首页 ACQUISITION 区显示 mock 读数（值、单位、质量/来源），不再固定 “Not configured”。
- R4: Kconfig 选择后端：`mock`（默认 test）与 `none`/`uart` 占位（uart 本任务不实现业务）。
- R5: 不引入真实 UART/Modbus/nanoMODBUS；不占用 D0/D1。
- R6: mock 轮询不得阻塞 LVGL 主循环（timer 或非阻塞 poll）。

## Acceptance Criteria

- [x] 存在 `vg_acq_*` 接口与 mock 实现源文件。
- [x] 默认 mock 时首页显示变化中的温度类读数。
- [x] 质量字段至少区分 good / 未配置。
- [x] README/issue #02 可引用 mock 作为无硬件路径。
- [x] 不破坏 Issue #01 身份/启动/uptime 行为（uptime timer 仍 1s；acq 同 timer 刷新）。

## Out of Scope

- nanoMODBUS、真 RS485、告警规则引擎完整版、网络。
