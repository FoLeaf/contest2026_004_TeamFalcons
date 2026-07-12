# VelaGuard ISSUE1 application

This directory contains the first bootable VelaGuard product slice for the
STM32H750B-DK. It owns stable Device ID initialization, startup directory and
event creation, the LVGL industrial home screen, and the NSH-preserving process
entry point.

ISSUE1 intentionally contains no Modbus, networking, MQTT, AI, audio playback,
runtime configuration, or OTA behavior.

The Windows VS Code tasks in the repository root build and flash this app with
the external-QSPI workflow. Test mode uses a compile-time Device ID override;
production mode derives the ID from the STM32 96-bit UID.
