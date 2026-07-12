# Windows VS Code 编译、烧录与调试 STM32H750B-DK

## 方案边界

STM32H750XBH6 只有 128 KiB 片内 Flash。超过该容量的 openvela 镜像采用：

```text
0x08000000  片内 Flash：QSPI boot stub
0x90000000  外部 QSPI：NuttX/openvela XIP 主镜像
```

本项目固定以下职责，避免再次把不存在的片内空间声明给 OpenOCD：

- WSL：编译 Linux 工具链下的 openvela。
- STM32CubeProgrammer：使用
  `MT25TL01G_STM32H750B-DISCO.stldr` 写入外部 QSPI，再写入片内 boot
  stub。
- OpenOCD：只作为 Cortex-Debug 的 GDB server，不负责 QSPI 下载。

历史提交 `5eb5070` 已在本板验证：xPack OpenOCD 的 `stmqspi` 无法稳定
JEDEC probe/write 双 MT25TL01G，而上述 CubeProgrammer External Loader 可以
正常擦写和校验。

## 1. 环境

在 Windows 安装：

- VS Code，以及 `ms-vscode.cpptools`、`marus25.cortex-debug` 扩展；
- WSL，完整 openvela 工作区位于 WSL 文件系统；
- STM32CubeCLT/STM32CubeProgrammer；
- Windows xPack OpenOCD。

开发板通过 ST-LINK USB 连接到 Windows。即使源码和编译环境位于 WSL，
CubeProgrammer、OpenOCD 和 ST-LINK 都由 Windows 进程访问，不需要把 USB
设备转发给 WSL。

请用 Windows VS Code 直接打开：

```text
\\wsl.localhost\Debian\home\<user>\openvela\contest2026_004_TeamFalcons
```

不要在 WSL Remote 窗口启动 Windows Cortex-Debug。默认工具路径使用
Cortex-Debug 正式注册的 Windows setting，位于 `.vscode/settings.json`；若安装
位置不同，只需修改该文件。源码映射使用 `${workspaceFolder}/..` 指向 openvela
根目录。

PowerShell 脚本也支持环境变量：

| 变量 | 用途 |
|---|---|
| `OPENVELA_WSL_DISTRO` | WSL 发行版，默认 `Debian` |
| `OPENVELA_ROOT_WSL` | WSL 中的 openvela 根目录；默认从参赛仓父目录推导 |
| `OPENVELA_OUT_DIR` | Windows 产物目录，默认参赛仓 `.debug` |
| `STM32_PROGRAMMER_CLI` | `STM32_Programmer_CLI.exe` 完整路径 |
| `STM32_EXTERNAL_LOADER` | `MT25TL01G_STM32H750B-DISCO.stldr` 完整路径 |

## 2. 编译

按 `Ctrl+Shift+B`，运行：

```text
openvela: build QSPI firmware
```

构建脚本会：

1. 将团队仓内的 QSPI 补丁幂等应用到当前 NuttX checkout；
2. 以 `stm32h750b-dk:lvgl` 作为板级驱动基线，但禁用官方
   `lvgldemo` 应用；
3. 启用 `CONFIG_STM32H750B_DK_QSPI_BOOT`；
4. 启用项目自有 `vscode_lab` 并将其设置为自动启动入口；
5. 构建主镜像和内部 boot stub；
6. 复制以下文件到参赛仓 `.debug`：

```text
nuttx.elf  nuttx.hex  nuttx.bin
qspi_bootstub.elf  qspi_bootstub.hex  qspi_bootstub.bin
```

调试构建使用任务 `openvela: build debug QSPI firmware`，它额外启用 `-g3`
和无优化构建。

判断编译正确不要只看任务退出码，还应确认 `.debug` 中六个产物都存在，且
日志显示主镜像位于 `0x9000xxxx`、boot stub 位于 `0x0800xxxx`。

## 3. 下载

运行：

```text
openvela: flash QSPI firmware (CubeProgrammer)
```

脚本在连接硬件前检查：

- 主 HEX 全部位于 `0x90000000..0x97ffffff`；
- boot stub 全部位于 `0x08000000..0x0801ffff`；
- CubeProgrammer CLI 与 External Loader 存在。

随后依次执行：

```text
Cube + External Loader → 写入并校验 QSPI 主镜像
Cube                   → 写入并校验片内 boot stub
Cube                   → 复位
```

已有产物时可使用 `openvela: flash only (CubeProgrammer)`。

没有连接开发板时，可在 Windows PowerShell 只验证工具和镜像布局：

```powershell
scripts\windows_flash_cube.ps1 -NoBuild -ValidateOnly
```

## 4. 断点调试

选择：

```text
openvela: STM32H750B-DK debug after Cube flash
```

按 `F5` 后会先构建并用 CubeProgrammer 下载 debug 镜像，再由 OpenOCD
启动 GDB server。配置使用 `request: attach` 和空 `loadFiles`，因此 GDB 只
加载 `${workspaceFolder}\.debug\nuttx.elf` 的符号，不会再次写 Flash。

若固件已经下载，使用 `openvela: STM32H750B-DK attach only`。

推荐的日常操作顺序是：

1. 普通运行验证：执行 `openvela: flash QSPI firmware
   (CubeProgrammer)`，它会自动编译、烧录并复位。
2. 调试代码：在“运行和调试”中选择 `openvela: STM32H750B-DK debug
   after Cube flash` 后按 `F5`；该配置会先生成并烧录 debug 固件，再 attach。
3. 只增加或移动断点、不改固件：选择 `attach only`，避免重复擦写 QSPI。
4. 改过代码后不要直接使用 `attach only`，否则板上代码与 ELF 符号可能不一致。

连接后若程序正在运行，可先暂停，再在应用源码中下断点并继续。不要用 VS Code
或 GDB 的 Download/Load 命令；本项目的 `loadFiles: []` 正是为了阻止这条错误
路径。

## 5. 运行与触摸验证

ST-LINK 虚拟串口使用 `115200 8N1`。看到 `NuttShell (NSH)` 和 `nsh>` 后，
烧录当前项目固件后，`VS Code Lab` 会自动显示，不需要再运行 openvela 的
`lvgldemo`。界面包含运行秒数、触摸计数、`Breakpoint +1` 按钮和滑块。

在 [vscode_lab_main.c](../app/hello_app/vscode_lab_main.c) 的
`vscode_lab_debug_checkpoint()` 中设置断点，按 `F5` attach，然后触摸
`Breakpoint +1`，即可完成一次从物理触摸到 C 源码断点的实验。

NSH 仍在 `COM7 / 115200 8N1` 上可用。触摸初始化成功时串口会出现
`/dev/input0 open success, maxpoint 1`，应用启动成功后还会输出
`[vscode_lab] UI ready`。本项目已经补齐旧 FT5X06 驱动的
`TSIOC_GETMAXPOINTS` 契约。

## 6. 故障恢复

- `Main QSPI image address range is invalid`：构建未启用 QSPI linker，禁止下载。
- 找不到 External Loader：检查 STM32CubeProgrammer 安装，或设置
  `STM32_EXTERNAL_LOADER`。
- OpenOCD 无法 attach：关闭 CubeProgrammer GUI 和其他 OpenOCD 进程，确认
  ST-LINK 未被占用。
- QSPI 下载成功但不启动：先检查 boot stub 是否写入片内 Flash，再检查复位后
  PC 是否进入 `0x9000xxxx`。若停在 `0x080002aa`，检查 boot stub 是否错误要求
  NuttX 初始 MSP 8 字节对齐；当前实现只要求合法 SRAM 范围和 4 字节对齐。
- 屏幕只有背光：查看串口是否出现 `[vscode_lab] UI ready`；若没有，检查更早的
  板级/LVGL 初始化错误，确认烧录的是刚生成的 `.debug` 产物。
- LVGL 报 `get touch maxpoints failed (errno=25)`：当前 NuttX checkout 未应用
  团队补丁，重新执行项目构建任务，不要只烧录旧 `.debug` 产物。
- 能显示但触摸无效：先看串口是否出现 `/dev/input0 open success`，再检查触摸
  读事件；若点击位置方向错误，再核对 `CONFIG_FT5X06_SWAPXY` 和屏幕旋转配置。
- GDB 中 `lv_nuttx_init()` 显示 `disp=NULL` 但 `indev!=NULL`：不要硬编码
  `/dev/lcd0`；当前 framebuffer 配置应保留 `lv_nuttx_dsc_init()` 选择的
  `/dev/fb0`。
- 芯片保护或连接异常：在 CubeProgrammer GUI 中检查 Option Bytes；记录原值后
  再处理。正常流程不会自动修改 Option Bytes 或执行 mass erase。
