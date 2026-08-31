# Repository Guidelines

## Project Structure & Module Organization

`F407_IndustrialMonitor` 是面向学习的工业数据采集与监控终端，当前目标是逐步完成基于 FreeRTOS 的功能。MCU 为 STM32F407ZGT6，开发板为正点原子 STM32F407 Explorer；工程使用 STM32CubeMX、HAL、Keil MDK-ARM 和 ARM Compiler 6，系统时钟为 168 MHz。

- `Core/`：CubeMX 自动生成的启动、时钟、外设初始化和中断代码。
- `Drivers/`：STM32 HAL 与 CMSIS；不得修改其源码。
- `APP/`：业务逻辑；新增业务功能优先放入此处。
- `BSP/`：板级硬件封装。
- `COMMON/`：可复用通用模块。
- `Docs/`：项目文档。
- `MDK-ARM/F407_IndustrialMonitor.uvprojx`：Keil µVision 工程文件。

## Build, Test, and Development Commands

在 Keil µVision 中打开 `MDK-ARM/F407_IndustrialMonitor.uvprojx`，按 `F7` 构建。命令行构建可在 Keil MDK 命令提示符中运行：

```powershell
UV4.exe -b MDK-ARM\F407_IndustrialMonitor.uvprojx -j0
```

构建产物位于 `MDK-ARM/F407_IndustrialMonitor/`，且应保持为 Git 忽略项。使用 ST-Link V2 通过 SWD 下载和调试；USART1 调试串口为 115200 8N1（PA9 为 TX、PA10 为 RX）。LED 定义为 `PF9 = LED_RED`、`PF10 = LED_GREEN`。

## Coding Style & Naming Conventions

默认使用 C，不使用 C++，并使用 STM32 HAL API。新增业务代码应优先按职责放入 `APP/`、`BSP/` 或 `COMMON/`。应用代码使用四空格缩进；公共应用函数沿用 `App_Init` 这类 `App_` 前缀，CubeMX 初始化函数沿用 `MX_<PERIPHERAL>_Init`。模块保持 `foo.c` / `foo.h` 配对，文件内辅助函数优先使用 `static`。代码应清晰、易解释，适合嵌入式面试学习，避免过度抽象。

每次修改前先阅读相关代码。不得随意修改 CubeMX 自动生成初始化代码；若必须修改 `Core/` 中的 CubeMX 文件，应尽可能只在 `/* USER CODE BEGIN/END */` 区域内修改。未经明确要求不得修改 `F407_IndustrialMonitor.ioc`，也不得修改 HAL Driver 或 CMSIS 源码。单次变更只实现一个明确阶段的功能，不要同时推进多个阶段。

## Testing Guidelines

当前未配置自动化测试框架或覆盖率目标。提交代码前至少应完成构建；涉及硬件或时序的变更，应记录实际使用的开发板、编译/烧录结果和串口或 LED 观察结果。硬件信息不确定时必须明确说明，不得猜测；不得声称代码已经在开发板验证，除非用户提供真实的编译、烧录和运行结果。

## Commit & Pull Request Guidelines

提交应聚焦单一改动，使用如 `feat: add sensor sampling` 或 `fix: handle UART timeout` 的简洁 Conventional Commit 标题。变更说明应如实记录已执行的检查和未验证的硬件步骤。不要提交 `.hex`、`.axf`、对象文件或本地 Keil 用户设置。
