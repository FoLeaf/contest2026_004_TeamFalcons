# Implement: 首页设置入口与网络设置页

## Ordered checklist

1. [ ] 固化资源和构建前提：为三个用户 SVG 增加可重复的 LVGL 9.1 A8/C descriptor
   转换产物与声明；补充中文 16/20 px 字库字符；在 Windows 构建脚本启用 DNS
   resolver 与 cJSON。
2. [ ] 扩展维护的 netinit carrier patch：增加线程安全的 runtime IPv4 policy API，
   让 DHCP/静态策略在立即应用和 carrier recovery 时使用同一个所有者；同步应用到
   `../apps` checkout，并保持 patch helper 幂等。
3. [ ] 扩展 `vg_config`：定义 versioned network config，严格 cJSON 解析、共享 IPv4
   组合校验、DHCP 默认创建、临时文件写入和原子提交/取消接口。
4. [ ] 新增 `vg_network`：启动工作线程，加载持久配置，调用 netinit runtime API，
   发布锁保护状态快照，处理异步 apply、成功提交与失败回滚。
5. [ ] 抽取 UI 主题与导航公共组件，确保一个常驻服务 timer 持续采集/告警，screen
   删除时清空所有页面对象引用。
6. [ ] 重排首页：顶部显示品牌、真实 Ethernet 状态/IP 和纯设置图标入口；构建模式
   与固件版本移入身份区域；移除硬编码网络块并保持本地状态优先级。
7. [ ] 实现设置首页：固定返回/标题栏、七类滚动列表，只有网络设置可进入，其余
   明确显示“暂未开放”且不注册执行事件。
8. [ ] 实现网络设置页：真实状态、DHCP/静态切换、四个静态字段、Ethernet/Wi-Fi
   素材、全屏数字键盘、字段与组合错误、确认对话框、应用中/成功/失败状态。
9. [ ] 更新 README 与静态回归 harness，覆盖状态来源、配置合同、netinit 单所有者、
   字库字符、资源声明、页面导航、timer 生命周期和构建 Kconfig。
10. [ ] 执行格式、静态检查、维护补丁正反向检查和 openvela 固件构建；检查产物大小，
    审计 480x272 文案、对比度、触摸热区和所有状态。
11. [ ] 在硬件上验证拔线、插线 DHCP、静态配置、静态重连、无效输入、取消、应用
    回滚、重复页面导航、无触摸降级与 NSH 可用性；记录无法在本会话完成的物理门禁。

## Validation commands

```bash
python3 harness/velaguard_settings_check.py
python3 harness/velaguard_issue1_check.py
python3 harness/qspi_boot_flow_check.py --artifacts .debug
bash -n scripts/apply-openvela-eth-mii-patch.sh
git -C ../apps apply --reverse --check \
  "$PWD/scripts/openvela-netinit-carrier-poll.patch"
powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -File scripts/windows_build_openvela.ps1 -Rebuild incremental
```

根据实现触及的外部 apps C 文件，再运行 NuttX/apps 的 `checkpatch.sh`；最终构建若
由当前 Linux/WSL 环境直接执行，则使用项目已验证的等价 build 命令并记录产物地址。

## Review gates

- [ ] `network.json` 只有一个解析/校验所有者，UI 不直接解析或写文件。
- [ ] netinit 是 DHCP/静态活动地址的唯一底层所有者，没有第二套 DHCP monitor。
- [ ] UI 线程不执行 DHCP、文件写入、fsync 或阻塞式网络应用。
- [ ] 静态配置在 carrier loss/recovery 后保持静态策略，DHCP 配置仍自动续接。
- [ ] 每个 apply 失败点都能说明正式文件与运行配置最终处于哪个版本。
- [ ] 页面切换不重复创建 timer，不更新已删除对象，采集/告警在设置页仍运行。
- [ ] 首页入口只显示用户设置图标，网络状态真实且不会挤压 480x272 内容。
- [ ] 七类设置顺序、占位状态、键盘、确认和错误状态符合 PRD。
- [ ] 新中文文案无缺字，三个 SVG 均由可追踪源生成并实际使用。
- [ ] `design-taste-frontend` pre-flight 的适用项通过：单一主题/强调色/圆角系统、
  无装饰动效、按钮对比度、输入/错误对比度、完整状态循环、触屏热区。

## Risky files and rollback points

| Area | Risk | Rollback point |
|---|---|---|
| `scripts/openvela-netinit-carrier-poll.patch`, `../apps/netutils/netinit/*` | DHCP 重连或静态策略回归 | 恢复旧 carrier patch/API；默认 DHCP 路径保持可编译 |
| `vg_config.*`, `network.json` | 配置损坏或不兼容 | 删除 `network.json` 回到 DHCP 默认；正式文件在 rename 前不覆盖 |
| `vg_network.*` | 线程竞态或 UI 卡顿 | 禁用运行时提交，保留只读网络快照和 netinit 默认 DHCP |
| UI screen lifecycle | timer use-after-free | 回退为单首页，同时保留后�� network/config 模块 |
| fonts/assets | flash 增长、缺字、图像格式错误 | 回退新增 descriptor；保留源 SVG，重新生成 A8 小尺寸资源 |

## Start gate

实现前必须由用户审阅 `prd.md`、`design.md` 与本文件并明确同意开始；随后运行
`task.py start`，再按 inline 模式加载 `trellis-before-dev`。
