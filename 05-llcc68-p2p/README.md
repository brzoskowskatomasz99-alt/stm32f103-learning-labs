# 0806--LLCC68 P2P 双板通信验证

## 实验目标

基于两块 STM32F103C8T6 与两块 Ra-01SC（LLCC68），使用同一套 LoRa 参数构建终端 TX 和网关 RX，实现定时发送 `LLCC68-P2P:<序号>`，并通过串口区分初始化、发射完成、前导码、包头和接收完成阶段。

本快照保留最终终端 TX 角色。当前真机证据确认终端持续产生 `SENT`，但网关在 60.006 秒内没有检测到 PREAMBLE，因此无线 P2P **尚未通过**。

## 硬件与配置

| 功能 | 配置或引脚 | 来源 |
| --- | --- | --- |
| MCU | STM32F103C8T6 | `Template.ioc`、`MDK-ARM/Template.uvprojx` |
| LoRa 模块 | Ra-01SC，LLCC68，410–525 MHz 型号 | 当前实验记录与 `Drivers/LLCC68/` |
| SPI1 | PA5/SCK、PA6/MISO、PA7/MOSI；主机、8 bit、Mode 0、软件 NSS、预分频 32 | `Template.ioc`、`Core/Src/spi.c` |
| LLCC68 NSS | PA4，默认高电平 | `Core/Inc/main.h`、`Core/Src/gpio.c` |
| LLCC68 RESET | PB1，默认高电平 | `Core/Inc/main.h`、`Core/Src/gpio.c` |
| LLCC68 BUSY | PB0，输入 | `Core/Inc/main.h`、`Core/Src/gpio.c` |
| LLCC68 DIO1 | PB10，EXTI 上升沿；当前 P2P 逻辑使用 IRQ 状态轮询，不依赖 DIO1 回调 | `Template.ioc`、`Core/Src/llcc68_p2p.c` |
| 串口日志 | USART1，PA9/TX、PA10/RX，115200、8N1、无流控 | `Core/Src/usart.c`、当前采集记录 |
| 公共射频参数 | 433 MHz、SF7、125 kHz、CR 4/5、前导码 8、显式包头、CRC 开、标准 IQ、0 dBm | `Core/Inc/llcc68_p2p_config.h` |
| RF 通路初始化 | DIO2 内部 RF switch 控制；430–440 MHz image calibration | `Core/Src/llcc68_p2p.c` |

## 主要文件

- `MDK-ARM/Template.uvprojx`：Keil MDK 工程。
- `Template.ioc`：STM32CubeMX 外设与引脚配置。
- `Core/Src/main.c`：初始化 SPI1、USART1 和 GPIO，调用 P2P 初始化与轮询入口。
- `Core/Inc/llcc68_p2p_config.h`：TX/RX 编译角色与唯一一套公共 LoRa 参数；归档时角色为 TX。
- `Core/Src/llcc68_p2p.c`：公共射频初始化、TX 状态机、RX 轮询与分阶段日志。
- `Core/Src/llcc68_hal_stm32.c`、`Core/Inc/llcc68_hal_stm32.h`：SPI、NSS、RESET、BUSY 的 STM32 HAL 适配。
- `Drivers/LLCC68/`：LLCC68 驱动源码和接口。
- `Evidence/`：本次最终双串口采集的原始日志。

## 实现流程

1. `main()` 完成 GPIO、USART1 与 SPI1 初始化，调用 `LLCC68_P2P_Init()`。
2. 公共初始化依次执行 reset、GetStatus、standby、DIO2 RF switch、430–440 MHz image calibration、LoRa 包类型、433 MHz 频率、调制和包参数配置。
3. TX 角色每 2000 ms 写入 `LLCC68-P2P:<序号>`，启动发送并轮询 TX_DONE；成功后打印 `SENT SEQ=<n>`。
4. RX 角色进入连续接收并轮询 IRQ；分别打印 PREAMBLE、HEADER VALID、HEADER ERROR、CRC ERROR 和 RECV。
5. 只有网关出现 `RECV` 才算 P2P 通过；终端 `SENT` 不能单独证明无线信号已被接收。

## 打开、构建与双板准备

1. 使用 STM32CubeMX 打开 `Template.ioc` 检查引脚与 SPI1 配置。
2. 使用 Keil MDK 打开 `MDK-ARM/Template.uvprojx`。
3. 当前 `Core/Inc/llcc68_p2p_config.h` 为 TX；构建并保存终端固件。
4. 将 `LLCC68_P2P_ROLE` 单独切换为 `LLCC68_P2P_ROLE_RX`，重新全量构建并保存网关固件。
5. 分别向终端与网关下载对应固件，再切回所需工作角色；每次以构建日志和串口初始化角色为准，不能只凭文件时间推断。
6. 先启动网关 RX，再启动终端 TX，同时采集两端 USART1 日志至少 30 秒。

## 验证状态

| 证据层级 | 状态 | 说明 |
| --- | --- | --- |
| 源码存在 | 已确认 | 快照包含 `.ioc`、`.uvprojx`、启动文件、HAL 适配、P2P 代码和 LLCC68 驱动。 |
| 静态检查 | 已确认 | 已核对角色、公共参数、DIO2 RF switch、433 MHz image calibration、TX/RX 状态机、引脚和工程文件引用。 |
| 编译 | 已确认通过 | 2026-08-06 的 TX 全量 Rebuild 实际编译 `llcc68_p2p.c`，结果 `0 Error(s), 1 Warning(s)`；唯一 Warning 为既有 `App_IsAlarmActive` 未引用。 |
| 烧录 | 终端已确认通过 | 用户确认 J-Link 连接终端后下载；J-Link 识别 STM32F103C8、VTarget 3.300 V，并报告 Programming Done、Verify OK、Application running。网关本轮未重新烧录。 |
| 真机行为 | 部分验证，P2P 未通过 | COM10 连续出现 `SENT SEQ=80` 至 `108`；COM8 只有 `RX INIT OK FREQ=433000000`，60.006 秒内无 PREAMBLE、HEADER 或 RECV。 |

## 原始观察结果

- `Evidence/COM10-terminal-TX-60s.log`：终端连续发送完成日志。
- `Evidence/COM8-gateway-RX-60s.log`：网关只有 RX 初始化日志。
- 最终接收阶段：**无 PREAMBLE**。
- 两块模块按课堂要求未连接外置天线，功率保持 0 dBm；这一测试条件会限制对射频链路故障的进一步判定。

## 已知边界与待验证项

- 归档保存的是可检查、可重建的当前工程，不把 `SENT` 写成无线接收成功。
- 当前快照宏为 TX；重建双板实验时必须分别生成 TX 与 RX 固件并避免烧错板。
- 本次串口采集未捕获终端 `INIT OK`，因此只能确认持续 SENT，不能声称日志包含终端复位初始化行。
- 网关修复后 RX 固件的历史烧录由当前实验记录支持，但本轮最终下载仅针对终端。
- P2P 最终通过、外置天线条件、射频端硬件状态和模块互换定位均待后续独立验证。
