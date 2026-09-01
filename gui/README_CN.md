# VelaGuard HMI 模拟器（Windows / LVGL 9.1）

基于官方 [`lv_port_pc_vscode`](https://github.com/lvgl/lv_port_pc_vscode) 的 `release/v9.1` 分支，已适配本机 Windows，并接入 **VelaGuard C1 HMI**（480×272 壳 / 主题 / 总览 / 详情 / 趋势）。

## 环境组成

| 组件 | 本机路径 / 版本 |
|------|-----------------|
| LVGL | `lvgl/` **9.1.0** |
| 编译器 | `D:\Develop\llvm-mingw-20250227-ucrt-x86_64`（带 pthread/unistd） |
| CMake | `D:\Develop\STM32CubeCLT\CMake` |
| SDL2 | `third_party/SDL2`（2.30.10 MinGW） |
| 调试器 | `D:\Develop\mingw64\bin\gdb.exe` |

## 一键构建与运行

在 **PowerShell** 中：

```powershell
cd D:\Study\Embeded\Velaguard\GUI

$env:PATH = "D:\Develop\llvm-mingw-20250227-ucrt-x86_64\bin;D:\Develop\STM32CubeCLT\CMake\bin;" + $env:PATH

cmake --preset mingw-debug
cmake --build --preset mingw-debug -j 8
.\bin\main.exe
```

启动后窗口为 **480×272**，默认进入 VelaGuard 首页（非 `lv_demo_widgets`）。

### 演示快捷键

| 键 | 场景 |
|----|------|
| `1` | 正常 |
| `2` | 预警 |
| `3` | 严重 |
| `4` | 离线 |
| `5` | MiMo 不可用 |
| `6` | OTA 进行中 |
| `Esc` / 顶栏 `<` | 返回上一页 |

## VSCode 使用

1. 打开 `simulator.code-workspace`
2. 推荐扩展：`ms-vscode.cpptools`、`ms-vscode.cmake-tools`
3. `Ctrl+Shift+B` 构建，或 F5 调试
4. 产物：`bin/main.exe`（构建后会复制 `SDL2.dll` 到同目录）

## 目录结构（C1）

```
GUI/
├── bin/                      # main.exe + SDL2.dll
├── build/                    # CMake 构建目录（gitignore）
├── main/
│   ├── inc/vg_display.h      # 分辨率与布局常量
│   ├── src/main.c            # 入口：hal_init(480,272) + vg_app_init()
│   └── ui/                   # VelaGuard HMI（必须入库；.gitignore 仅忽略根级 /ui/）
│       ├── app/              # vg_app_init、键 1–6 场景切换
│       ├── shell/            # 状态栏、content host、返回栈、toast
│       ├── theme/            # 工业暗色 tokens + 字体
│       ├── fonts/            # CJK 子集 vg_font_ui_14
│       ├── model/            # mock 传感器 / 告警 / 网络 / 场景
│       ├── pages/            # home / device / trend
│       └── widgets/          # chip / device_card / metric_row
├── third_party/SDL2/         # 本地 SDL2 MinGW 包
├── lvgl/                     # LVGL 9.1
├── lv_conf.h
├── CMakeLists.txt
├── CMakePresets.json
└── simulator.code-workspace
```

## 配置要点

- `lv_conf.h`：`LV_USE_FLOAT=1`；Montserrat 12/14/16/20/24；`LV_FONT_SIMSUN_16_CJK` + `LV_FONT_FMT_TXT_LARGE`（CJK 兜底）
- `CMakeLists.txt`：优先 `third_party/SDL2`；`main/ui/**/*.c` 编入 `main` 目标
- `.gitignore`：忽略 `build/`、`bin/`、**仅根级** `/ui/`（不要写裸 `ui/`，否则会误忽略 `main/ui/`）

## 注意

1. **必须用 llvm-mingw**，不要用缺 pthread/unistd 的 mingw64 工具链做主构建。
2. FreeRTOS 在 Windows 本机默认 **关闭**。
3. C2/C3 页面（告警详情、诊断、日志、OTA 等）入口会 toast「后续版本」；导航 API 已预留。
