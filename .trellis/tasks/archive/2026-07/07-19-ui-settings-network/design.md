# Design: 首页设置入口与网络设置页

## Design read

这是面向现场操作员的固定 480x272 工业设备界面，属于现有产品 UI 的保留式
改造，不是网页设置后台。沿用首页的深色表面、青色强调色和语义告警色。

- `DESIGN_VARIANCE: 4`
- `MOTION_INTENSITY: 2`
- `VISUAL_DENSITY: 7`
- 统一规则：面板 6 px 圆角，交互按钮 6 px 圆角，图标位于至少 40x32 px 热区；
  仅按下态使用轻微明度变化，不做装饰动画。

## Architecture and boundaries

| Owner | Responsibility | Must not own |
|---|---|---|
| `vg_config` | `network.json` schema、默认值、严格解析、校验、临时文件与原子提交 | socket/ioctl、LVGL 对象 |
| `netinit` runtime policy | Ethernet 活动 IPv4 策略、DHCP/静态应用、carrier 重连后的重新应用 | VelaGuard 文件路径、UI 文案 |
| `vg_network` | 加载配置、异步提交、运行状态快照、错误与回滚编排 | LVGL 对象、字段级表单状态 |
| UI navigation/pages | 页面生命周期、草稿字段、确认、反馈、只读状态渲染 | 直接解析 JSON、直接调用 netlib/ioctl |

保持一个网络策略所有者。现有 netinit carrier monitor 继续管理底层接口，不另写
第二套 DHCP 轮询器；通过通用运行时 IPv4 policy API 接受 VelaGuard 配置。

## Data contracts

### Persistent configuration

`/data/velaguard/configs/network.json`：

```json
{
  "version": 1,
  "mode": "dhcp",
  "ipv4": "",
  "netmask": "",
  "gateway": "",
  "dns": ""
}
```

- 首次启动或文件不存在时写入 DHCP 默认配置，不虚构可能与现场网络冲突的静态
  地址。DHCP 模式允许四个静态字段为空，并在用户曾保存静态配置后保留这些值；
  静态模式要求四个字段全部有效。
- 使用 cJSON 做类型和必填字段检查；`inet_pton()` 只负责语法转换，连续掩码、
  主机地址、同网段网关等组合规则由一个共享校验函数负责。
- 未知 `version`、缺字段、错误类型或非法组合返回错误，不静默接受部分配置。
- 不包含 SSID、密码、Token、API Key 等凭据。

### Runtime netinit API

扩展 `netutils/netinit` 公共头文件，定义不依赖 VelaGuard 的通用合同：

```c
enum netinit_ipv4_mode
{
  NETINIT_IPV4_DHCP,
  NETINIT_IPV4_STATIC
};

struct netinit_ipv4_config
{
  enum netinit_ipv4_mode mode;
  struct in_addr address;
  struct in_addr netmask;
  struct in_addr router;
  struct in_addr dns;
};

int netinit_set_ipv4_config(const struct netinit_ipv4_config *config);
```

- netinit 内部锁保护期望配置与 `g_use_dhcpc`，carrier monitor 和运行时 setter
  读取同一策略。
- DHCP 策略清除旧静态地址并触发/重试租约；静态策略停止 DHCP，设置地址、
  掩码、路由、DNS。
- carrier loss 清除活动地址和 resolver，但保留期望策略；carrier recovery 根据
  当前策略重新 DHCP 或重新应用静态配置。
- setter 返回参数或同步系统调用错误。无 carrier 时接受策略并等待恢复，不把
  “当前没有网线”当作配置失败。

### VelaGuard network snapshot

`vg_network_get_status()` 只复制受互斥锁保护的快照，UI 不执行网络系统调用：

```c
enum vg_network_state
{
  VG_NETWORK_DOWN,
  VG_NETWORK_ADDRESSING,
  VG_NETWORK_ONLINE,
  VG_NETWORK_APPLYING,
  VG_NETWORK_ERROR
};

struct vg_network_status
{
  enum vg_network_state state;
  enum vg_network_mode mode;
  bool carrier;
  char ipv4[INET_ADDRSTRLEN];
  int last_error;
};
```

`vg_network` 独立工作线程轮询接口 flag 与地址，并处理 UI 提交请求。LVGL 定时器
只读取快照，因此 DHCP 或文件 I/O 不会卡住触摸与首页刷新。

## Apply transaction and rollback

```text
UI draft
  -> shared validation
  -> confirmation
  -> vg_network_request_apply(copy)
  -> write network.json.tmp + flush/fsync
  -> netinit_set_ipv4_config(new)
  -> rename(tmp, network.json)
  -> publish success snapshot
```

失败路径：

1. 临时文件失败：删除临时文件，运行配置不变。
2. runtime apply 失败：重新应用内存中的旧配置，删除临时文件。
3. 原子 rename 失败：重新应用旧配置，删除临时文件，正式文件保持原状。
4. 回滚自身失败：状态进入 `VG_NETWORK_ERROR`，保留原正式文件，并同时记录原始
   错误与回滚错误；UI 显示需要从 NSH 检查网络。
5. DHCP 已接受但还未取得租约：事务成功，状态为 `ADDRESSING`，netinit 按现有
   重试周期继续获取地址。

## Screen architecture and lifecycle

使用一个常驻 LVGL 服务定时器。它持续执行采集/告警评估并按当前页面是否存在
决定是否更新标签；页面删除事件统一清空对象引用。导航只替换 screen，不创建
第二个业务定时器。

页面结构：

```text
Home
  status bar: VelaGuard | Ethernet status/IP | setting.svg button
  identity: device, storage, uptime, build mode, firmware
  acquisition/alarm primary region
  audio/time supporting region

Settings
  fixed header: back | 设置
  scroll body:
    网络设置                 可用
    采集与传感器             暂未开放
    告警规则                 暂未开放
    云端与 MQTT              暂未开放
    声音                     暂未开放
    系统                     暂未开放
    固件更新                 暂未开放

Network
  fixed header: back | 网络设置
  status: Ethernet icon, carrier/address/current IP
  mode: DHCP | 静态
  static fields: IP, 掩码, 网关, DNS
  Wi-Fi row: WIFI.svg, 暂未开放
  action: 保存并应用

IPv4 editor
  fixed field title + textarea
  LVGL number keyboard
  cancel / confirm events
```

- 设置页列表与网络页内容区可滚动，标题栏固定。
- 首页设置按钮使用 `setting.svg` 轮廓，图标经 LVGL recolor 提升深色主题对比度，
  不显示文字标签。
- `settings_ethernet.svg` 与 `WIFI.svg` 用于对应网络项。源 SVG 保留在 `res/`；
  生成 A8 或其他 LVGL 9.1 可链接 C descriptor，避免启用运行时 SVG/PNG 解码器。
- 只做无动画 screen load 和按下态反馈，避免 480x272 上的过渡抖动。

## Build and compatibility

- 当前栈为 LVGL 9.1、16-bit color；使用 `lv_screen_load`、`lv_keyboard` number mode、
  `lv_textarea` 和 LVGL 9 image descriptor API。
- 构建配置增加 `CONFIG_NETDB_DNSCLIENT` 与 `CONFIG_NETUTILS_CJSON`；保留现有
  `NETINIT_CARRIER_POLL`，但升级维护补丁使其支持运行时 DHCP/静态策略。
- `scripts/openvela-netinit-carrier-poll.patch` 继续是 apps checkout 修改的可重复
  应用来源；同步更新补丁回归断言，避免只改本机 checkout。
- 无触摸输入时不主动导航，首页与后台网络状态仍工作；NSH 始终保留。

## Rollout and rollback

- 功能局限在 VelaGuard 应用、构建 Kconfig 选择、图像/字体资源以及维护的 netinit
  补丁，不改 URL、设备身份或本地告警合同。
- 软件回滚可移除新 UI/network 源文件并恢复旧 `vg_ui_home.c`；netinit 运行时 API
  是增量接口，不改变未调用它的默认 DHCP 行为。
- 硬件验证失败时仍可通过 NSH `ifconfig`/`renew` 诊断，配置文件可删除以回到 DHCP 默认。

## Risks

| Risk | Mitigation |
|---|---|
| netinit monitor 与 UI 同时改地址 | 所有策略变更只经过 netinit runtime API 与内部锁 |
| DHCP 阻塞 UI | 独立网络线程；UI 只读快照 |
| 静态地址断线后变回 DHCP | carrier recovery 从保留的 runtime policy 重放静态配置 |
| 写入一半或掉电 | 临时文件、flush/fsync、原子 rename，正式文件最后替换 |
| 页面销毁后 timer 悬空 | screen delete 回调清空引用，常驻 timer 仅更新非空引用 |
| 字体/图标缺失 | 字库字符清单回归 + SVG 到 LVGL descriptor 的可重复生成步骤 |
| 触摸误操作 | 40 px 级热区、按下态、应用确认、应用期间禁用提交 |
