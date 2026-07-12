# VS Code Lab

这是 Team Falcons 自有的 STM32H750B-DK 实验应用，用于验证 Windows VS
Code 下的编译、QSPI 烧录、触摸和源码断点流程，不使用 openvela 提供的
`lvgldemo` 界面。

目录继续通过现有 workspace link 映射到
`packages/demos/contest2026_004_hello_app`，但生成的 NuttX 程序名是
`vscode_lab`。Windows 构建脚本会把它设置为系统入口：完成板级初始化后启动
LVGL UI 任务，同时保留 ST-LINK 串口上的 NSH。

推荐断点函数：

```c
vscode_lab_debug_checkpoint()
```

连接 Cortex-Debug 后触摸屏幕上的 `Breakpoint +1` 即可命中。
