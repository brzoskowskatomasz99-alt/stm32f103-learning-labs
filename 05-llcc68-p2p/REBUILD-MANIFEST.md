# STM32 Rebuild Manifest

## Identity

- Project: 0806--LLCC68 P2P 双板通信验证
- Archive number: 05
- Archive slug: `llcc68-p2p`
- Answer-key repository: `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs`
- Pushed branch: `main`
- Pushed commit: 本清单由归档提交引入，准确提交号见仓库中本文件的 Git 历史与本次归档报告
- Answer-key directory: `05-llcc68-p2p`
- Direct GitHub manifest URL: `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/REBUILD-MANIFEST.md`
- Archived local answer key: `E:\TEMPLATE\stm32f103-learning-labs\05-llcc68-p2p`
- Completed working project: `E:\TEMPLATE\Template`（只读来源，未修改）
- Default practice root: `E:\TEMPLATE\llcc68-p2p-learning`（仅建议，未创建）

## Direct answer-key files

| Purpose | Relative path | Direct GitHub URL |
|---|---|---|
| Overview | `README.md` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/README.md` |
| Startup | `Core/Src/main.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Core/Src/main.c` |
| Main P2P logic | `Core/Src/llcc68_p2p.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Core/Src/llcc68_p2p.c` |
| P2P configuration | `Core/Inc/llcc68_p2p_config.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Core/Inc/llcc68_p2p_config.h` |
| HAL adapter implementation | `Core/Src/llcc68_hal_stm32.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Core/Src/llcc68_hal_stm32.c` |
| HAL adapter interface | `Core/Inc/llcc68_hal_stm32.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Core/Inc/llcc68_hal_stm32.h` |
| SPI implementation | `Core/Src/spi.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Core/Src/spi.c` |
| GPIO and pin definitions | `Core/Inc/main.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Core/Inc/main.h` |
| LLCC68 driver | `Drivers/LLCC68/` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/tree/main/05-llcc68-p2p/Drivers/LLCC68` |
| Serial evidence | `Evidence/` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/tree/main/05-llcc68-p2p/Evidence` |
| CubeMX configuration | `Template.ioc` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/Template.ioc` |
| Keil project | `MDK-ARM/Template.uvprojx` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/05-llcc68-p2p/MDK-ARM/Template.uvprojx` |

## Hardware boundary

- MCU: 两块 STM32F103C8T6，分别作为终端 TX 与网关 RX。
- Peripherals and buses: SPI1 主机（PA5/SCK、PA6/MISO、PA7/MOSI）；LLCC68 NSS/PA4、RESET/PB1、BUSY/PB0、DIO1/PB10；USART1/PA9、PA10，115200、8N1、无流控。
- Inputs: LLCC68 BUSY/PB0；DIO1/PB10 虽配置为 EXTI，但当前 P2P 主路径使用 IRQ 状态轮询。
- Outputs: SPI1 SCK/MOSI、NSS/PA4、RESET/PB1；USART1 TX/PA9；Ra-01SC 射频端。
- Explicitly unused hardware: 当前 P2P 入口注释了 `app_main()` 与一次性 `LLCC68_Diag_RunOnce()`；环境监测外设不属于本次无线验收。外置天线按课堂条件未连接。

## Function navigation map

| Stage | Practice file | `Ctrl+F` search anchor | Occurrence and operation location |
|---|---|---|---|
| 启动入口 | `Core/Src/main.c` | `LLCC68_P2P_Init();` | 活动调用点；位于外设初始化之后。附近的 `LLCC68_Diag_RunOnce()` 与 `app_main()` 均被注释。 |
| 主循环入口 | `Core/Src/main.c` | `LLCC68_P2P_Process();` | 活动调用点；位于 `while (1)` 内，持续推进非阻塞 TX/RX 状态机。 |
| 角色与公共参数 | `Core/Inc/llcc68_p2p_config.h` | `#define LLCC68_P2P_ROLE` | 角色宏定义；当前值为 TX。附近是唯一一套频率、SF、BW、CR、前导码、CRC、IQ、功率和校准范围。 |
| HAL 上下文 | `Core/Src/llcc68_hal_stm32.c` | `const llcc68_hal_stm32_context_t llcc68_hal_stm32_context` | 唯一定义；绑定 hspi1、NSS、RESET、BUSY 与超时。 |
| SPI 写命令 | `Core/Src/llcc68_hal_stm32.c` | `llcc68_hal_write` | 官方 HAL 回调定义；先等 BUSY 低，再拉低 NSS 并调用 HAL SPI。 |
| SPI 读命令 | `Core/Src/llcc68_hal_stm32.c` | `llcc68_hal_read` | 官方 HAL 回调定义；包含命令发送、dummy byte 与分块收取。 |
| 公共射频初始化 | `Core/Src/llcc68_p2p.c` | `static bool llcc68_p2p_configure_radio` | 唯一定义；依次 reset、GetStatus、standby、DIO2 RF switch、image calibration、频率与 LoRa 参数。 |
| TX 载荷与启动 | `Core/Src/llcc68_p2p.c` | `static void llcc68_p2p_tx_send` | 仅在 TX 条件编译分支中定义；构造 `LLCC68-P2P:<n>`、写缓冲并调用 set_tx。 |
| TX 状态机 | `Core/Src/llcc68_p2p.c` | `static void llcc68_p2p_process_tx` | 仅在 TX 分支中定义；等待 2000 ms、轮询 TX_DONE 并输出 SENT。 |
| RX 轮询 | `Core/Src/llcc68_p2p.c` | `static void llcc68_p2p_process_rx` | 仅在 RX 分支中定义；连续接收，处理 RX_DONE、CRC、TIMEOUT、PREAMBLE 与 HEADER 事件。 |
| 公共初始化入口 | `Core/Src/llcc68_p2p.c` | `bool LLCC68_P2P_Init` | 唯一定义；按编译角色输出 TX 或 RX INIT OK。 |

### Navigation traps

- `llcc68_p2p.c` 同时保存 TX 和 RX 源码，但条件编译一次只生成一个角色；不要把“源码中存在 RX 代码”误当成当前 AXF 是 RX。
- 归档时 `LLCC68_P2P_ROLE` 为 TX；双板重建必须分别构建、保存并烧录两个角色，不能让后一次构建覆盖前一个固件后再凭文件名猜测。
- `SENT` 来自 LLCC68 TX_DONE，只证明芯片侧发送流程完成；P2P 成功必须由网关 `RECV` 证明。
- `DIO1` 在 CubeMX 中配置为 EXTI，但当前代码把 DIO IRQ 引脚掩码设为 0，并通过 `llcc68_get_irq_status()` 轮询；不要先去寻找 DIO1 回调。
- DIO2 用于模块内部 RF switch，由 LLCC68 命令启用，不是当前 STM32 引脚表中的独立 GPIO。
- `Core/Src/main.c` 保留环境监测初始化，但 `app_main()` 调用已注释；本实验活动逻辑是 P2P 入口。
- CubeMX 生成文件含 `USER CODE BEGIN/END` 标记；重建时需保持用户代码区域边界。

## Ordered rebuild stages

1. **工程骨架与角色概念**：打开 `Template.ioc` 与 `MDK-ARM/Template.uvprojx`，确认 MCU、SPI1、USART1 和 `main()` 的 P2P 调用链；先区分“一套源码、两个编译角色”。
2. **SPI1 与硬件引脚**：在 `Core/Src/spi.c`、`Core/Src/gpio.c` 和 `Core/Inc/main.h` 重建 PA4/PA5/PA6/PA7、PB0/PB1/PB10 配置；先重新验证 RESET 与 GetStatus。
3. **LLCC68 HAL 适配**：在 `llcc68_hal_stm32.c` 重建 BUSY 有界等待、NSS 时序、SPI 读写和 reset/wakeup 回调；用驱动返回值作为软件边界。
4. **公共 LoRa 参数**：在 `llcc68_p2p_config.h` 建立唯一参数源，保持 433 MHz、SF7、125 kHz、CR 4/5、前导码 8、CRC 开、标准 IQ和 0 dBm。
5. **公共射频初始化**：在 `llcc68_p2p_configure_radio()` 按顺序重建 DIO2 RF switch、430–440 MHz image calibration 与 LoRa 配置；分别确认 TX/RX INIT OK。
6. **终端 TX 状态机**：重建 `llcc68_p2p_tx_send()` 和 `llcc68_p2p_process_tx()`；开发板应每 2 秒输出递增 SENT，但此时只算终端发送侧通过。
7. **网关 RX 轮询**：切换为 RX 构建，重建连续接收、PREAMBLE、HEADER、CRC、RX_DONE 与 RECV 日志；先确认 RX INIT OK。
8. **双固件管理**：分别全量构建并明确保存 TX 与 RX 固件，先烧网关再烧终端；每块板用串口角色日志核对，防止烧错。
9. **双板无线验收**：同步采集两端至少 30 秒；只有网关出现 `PREAMBLE → HEADER VALID → RECV "LLCC68-P2P:<n>"` 才通过。
10. **失败分级**：若未收到，严格记录无 PREAMBLE、有 PREAMBLE 无 HEADER VALID、HEADER ERROR、HEADER VALID 无 RX_DONE 或 RECV 成功，不在同一轮同时改多个射频变量。

## Known answer-key evidence and limits

| Evidence | Status | Boundary |
|---|---|---|
| Source present | 已确认 | 快照含 `.ioc`、`.uvprojx`、P2P 源码、STM32 HAL 适配与 LLCC68 驱动。 |
| Static inspection | 已确认 | 已核对入口、条件编译角色、公共参数、初始化顺序、TX/RX 状态机、引脚和工程引用。 |
| Build output | TX 已确认通过 | 2026-08-06 全量 Rebuild 实际编译 P2P 文件并得到 `0 Error(s), 1 Warning(s)`；未把旧 RX 构建日志升级为本轮当前构建证据。 |
| Flash/download | 终端 TX 已确认通过 | 用户确认 J-Link 接终端后执行；Programming Done、Verify OK、Application running。网关本轮未重烧。 |
| Device observation | 部分通过，P2P 失败 | 60.006 秒内终端连续 SENT 80–108；网关只有 RX INIT OK，无 PREAMBLE、HEADER 或 RECV。 |

当前答案工程只证明终端侧 TX 流程和网关初始化可运行，不证明无线链路成功。以后新建练习工程必须重新构建、烧录并取得网关 RECV；归档中的历史证据不能代替新练习的真机验收。

## Catalog handoff

归档推送并验证成功后，将本清单登记到 `C:\Users\ASUS\.codex\skills\stm32-project-rebuild-coach\references\archive-index.md`。本文件是权威交接资料，本地目录只保存指针。
