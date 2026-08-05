# STM32 Rebuild Manifest

## Identity

- Project: 0805--17.2-OLED界面状态机：页面切换、长按返回与息屏唤醒
- Archive number: 04
- Archive slug: `oled-ui-state-machine`
- Answer-key repository: `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs`
- Pushed branch: `main`
- Pushed commit: 本清单由归档提交引入，准确提交号见仓库中本文件的 Git 历史与本次归档报告
- Answer-key directory: `04-oled-ui-state-machine`
- Direct GitHub manifest URL: `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/REBUILD-MANIFEST.md`
- Archived local answer key: `E:\TEMPLATE\stm32f103-learning-labs\04-oled-ui-state-machine`
- Completed working project: `E:\TEMPLATE\Template`（只读来源，未修改）
- Default practice root: `E:\TEMPLATE\oled-ui-state-machine-learning`（仅建议，未创建）

## Direct answer-key files

| Purpose | Relative path | Direct GitHub URL |
|---|---|---|
| Overview | `README.md` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/README.md` |
| Startup | `Core/Src/main.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/Core/Src/main.c` |
| Main feature logic | `MDK-ARM/UserCode/app_main.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/MDK-ARM/UserCode/app_main.c` |
| OLED implementation | `Core/Src/oled.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/Core/Src/oled.c` |
| OLED interface | `Core/Inc/oled.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/Core/Inc/oled.h` |
| Sensor implementations | `Core/Src/dht22.c`, `Core/Src/co2.c`, `MDK-ARM/UserCode/light.c`, `MDK-ARM/UserCode/soil.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/tree/main/04-oled-ui-state-machine` |
| GPIO and pin definitions | `Core/Inc/main.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/Core/Inc/main.h` |
| CubeMX configuration | `Template.ioc` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/Template.ioc` |
| Keil project | `MDK-ARM/Template.uvprojx` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/04-oled-ui-state-machine/MDK-ARM/Template.uvprojx` |

## Hardware boundary

- MCU: STM32F103C8T6，LQFP48。
- Peripherals and buses: I2C1 OLED（PB6/PB7，驱动地址 `0x78`）；ADC1 IN0/IN1 + DMA；USART1 命令；USART2 CO₂；TIM1 CH1 与 TIM4 CH3 PWM。
- Inputs: SW1/PB12；DHT22/PC15；光照/PA0；土壤湿度/PA1；CO₂/USART2 RX PA3。
- Outputs: OLED；USART1 TX PA9；TIM1 CH1/PA8；TIM4 CH3/PB8；LED3/PB15；BEEP/PB9。
- Explicitly unused hardware: 源码未给出可以可靠断言的完整未使用硬件清单，待重建时按 `.ioc` 与实际接线确认。

## Function navigation map

| Stage | Practice file | `Ctrl+F` search anchor | Occurrence and operation location |
|---|---|---|---|
| 启动入口 | `Core/Src/main.c` | `app_main();` | 调用点；在 `USER CODE BEGIN 2` 后进入应用。 |
| 页面状态声明 | `MDK-ARM/UserCode/app_main.c` | `APP_PAGE_HOME = 0` | `AppPage` 枚举定义；附近还有 `AppKeyEvent` 和页面运行状态。 |
| 按键扫描 | `MDK-ARM/UserCode/app_main.c` | `static AppKeyEvent App_KeyScan` | 唯一定义；通过按下时长产生短按或长按事件。 |
| 页面切换与唤醒 | `MDK-ARM/UserCode/app_main.c` | `static void App_HandleKeyEvent` | 唯一定义；短按切页、长按回主页，熄屏时第一次按键只唤醒。 |
| 30 秒息屏 | `MDK-ARM/UserCode/app_main.c` | `static void App_CheckScreenTimeout` | 唯一定义；搜索 `APP_SCREEN_TIMEOUT_MS` 可定位 30000 ms 参数。 |
| 页面绘制分派 | `MDK-ARM/UserCode/app_main.c` | `static HAL_StatusTypeDef App_DrawCurrentPage` | 唯一定义；按 `current_page` 分派主页、温湿度页、光照土壤页。 |
| 串口命令 | `MDK-ARM/UserCode/app_main.c` | `static void App_ProcessCommand` | 唯一定义；包含查询、手动输出控制与 `AUTO`。 |
| 自动联动 | `MDK-ARM/UserCode/app_main.c` | `static void App_ApplyAutomaticControl` | 唯一定义；依据传感器有效状态更新 PWM、LED3 和蜂鸣器状态。 |
| 传感器调度 | `MDK-ARM/UserCode/app_main.c` | `void app_main(void)` | 唯一定义；主循环附近依次调用 `Light_ReadLux`、`Soil_ReadHumidityLevel`、`CO2_get_data`、`DHT22_ReadData`。 |
| OLED 驱动 | `Core/Src/oled.c` | `HAL_StatusTypeDef OLED_Init` | 初始化定义；同文件搜索 `OLED_ShowText`、`OLED_FrameClear`、`OLED_Refresh`。 |

### Navigation traps

- `Core/Src/main.c` 中有一个位于 `#if 0` 内的旧 `fputc()`，活动定义在 `MDK-ARM/UserCode/app_main.c`；不要修改失活版本。
- 活动 `app_main()` 只在 `MDK-ARM/UserCode/app_main.c` 定义一次；`Core/Src/main.c` 只有调用点。
- CubeMX 生成文件含 `USER CODE BEGIN/END` 标记；重建时自定义代码应留在用户区或 `MDK-ARM/UserCode` 中。
- `Core/Inc/main.h` 的 SW1 注释带有早期“假设”措辞，但活动代码和工程边界均使用 PB12；重建时仍需以实际接线验证。

## Ordered rebuild stages

1. **工程骨架与启动**：打开 `Template.ioc` 和 `MDK-ARM/Template.uvprojx`，确认 MCU、时钟和 `main()` 到 `app_main()` 的调用链；先要求新练习工程独立编译。
2. **OLED 基础显示**：在 `Core/Src/oled.c` 与 `Core/Inc/oled.h` 重建 I2C 写入、初始化、清屏和文本显示；开发板应能稳定显示单页测试内容。
3. **传感器采集**：逐个加入 DHT22、CO₂、光照和土壤湿度模块；每次只接入一个数据源并观察合理读数，再进入下一项。
4. **串口查询**：在 `app_main.c` 重建 USART1 空闲接收、命令规范化和 `GET` 查询；串口应返回对应传感器值。
5. **手动与自动联动**：重建 PWM/GPIO 输出与 `AUTO` 切换；分别验证手动命令和传感器驱动的自动输出。
6. **页面状态机**：重建 `AppPage`、三个绘制函数和 `App_DrawCurrentPage()`；上电应进入主页，数据页应显示当前有效值。
7. **短按与长按**：重建 `App_KeyScan()` 和 `App_HandleKeyEvent()`；验证短按切页、1500 ms 长按回主页。
8. **息屏与唤醒**：重建 `App_CheckScreenTimeout()`；验证 30 秒无操作清屏，第一次有效按键恢复熄屏前页面且不额外切页。
9. **完整回归**：重新编译、烧录并逐项验收采集、显示、按键、息屏、串口以及手动/自动联动。

## Known answer-key evidence and limits

| Evidence | Status | Boundary |
|---|---|---|
| Source present | 已确认 | 快照包含 `.ioc`、`.uvprojx`、启动文件、应用源码、驱动和头文件。 |
| Static inspection | 已确认 | 归档时检查了活动入口、状态机、按键、息屏、传感器、串口、PWM 和引脚配置。 |
| Build output | 用户确认通过 | 当前归档会话未重新运行 Keil；用户明确说明本项目已经完成编译。 |
| Flash/download | 用户确认通过 | 当前归档会话未重新下载；用户明确说明已经烧录。 |
| Device observation | 用户确认通过 | 用户明确说明已经上板验收标题和功能概括所列完整功能。 |

这些答案工程证据不能替代以后新建练习工程的重新编译、烧录和开发板验证。

## Catalog handoff

归档推送并验证成功后，将本清单登记到 `C:\Users\ASUS\.codex\skills\stm32-project-rebuild-coach\references\archive-index.md`。本文件是权威交接资料，本地目录只保存指针。
