# Backend Development Guidelines

> Best practices for backend development in this project.

---

## Overview

This directory contains guidelines for backend development. Fill in each file with your project's specific conventions.

---

## Guidelines Index

| Guide | Description | Status |
|-------|-------------|--------|
| [Directory Structure](./directory-structure.md) | Module organization and file layout | To fill |
| [Database Guidelines](./database-guidelines.md) | ORM patterns, queries, migrations | To fill |
| [Error Handling](./error-handling.md) | Error types, handling strategies | To fill |
| [Quality Guidelines](./quality-guidelines.md) | Code standards, forbidden patterns | To fill |
| [Logging Guidelines](./logging-guidelines.md) | Structured logging and contest AI log secret-redaction contract | Partial |
| [STM32H750 QSPI Toolchain](./stm32h750-qspi-toolchain.md) | Windows build/flash/debug plus MB1381 B01 MII Ethernet and QSPI-XIP contracts | Active |
| [Trellis Session Task Lifecycle](./trellis-session-task-lifecycle.md) | Claude/OpenCode session lifecycle, planning-safe selection, activation, and idempotent OpenCode Task context injection | Active |
| [VelaGuard MQTT / AI Bridge Contract](./velaguard-mqtt-ai-bridge-contract.md) | Topic tree, diagnosis JSON, host `ai_bridge_stub` CLI, req_id consistency | Active |

---

## How to Fill These Guidelines

For each guideline file:

1. Document your project's **actual conventions** (not ideals)
2. Include **code examples** from your codebase
3. List **forbidden patterns** and why
4. Add **common mistakes** your team has made

The goal is to help AI assistants and new team members understand how YOUR project works.

---

**Language**: All documentation should be written in **English**.
