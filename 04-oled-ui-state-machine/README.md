# 0805--17.2-OLED界面状态机：页面切换、长按返回与息屏唤醒

## 实验目标

基于 STM32F103C8T6 实现温湿度、CO₂、光照和土壤湿度采集，通过 OLED 多页面显示、SW1 短按切页与长按返回、30 秒息屏唤醒、串口查询及手动/自动联动控制，构成完整的环境监测终端。

## 硬件与配置

| 功能 | 配置或引脚 | 来源 |
| --- | --- | --- |
| MCU | STM32F103C8T6，LQFP48 | `Template.ioc` |
| OLED | I2C1，PB6/SCL、PB7/SDA，驱动地址 `0x78` | `Template.ioc`、`Core/Src/oled.c` |
| SW1 | PB12，低电平按下 | `Core/Inc/main.h`、`MDK-ARM/UserCode/app_main.c` |
| DHT22 | PC15 | `Core/Inc/dht22.h` |
| 光照 / 土壤湿度 | ADC1 IN0/PA0、IN1/PA1，DMA 循环采集 | `Template.ioc` |
| CO₂ | USART2，PA2/PA3 | `Template.ioc`、`Core/Src/co2.c` |
| 查询与控制命令 | USART1，PA9/PA10 | `Template.ioc`、`MDK-ARM/UserCode/app_main.c` |
| 联动输出 | TIM1 CH1/PA8、TIM4 CH3/PB8、LED3/PB15、蜂鸣器/PB9 | `Template.ioc`、`Core/Inc/main.h` |

## 主要文件

- `MDK-ARM/Template.uvprojx`：Keil MDK 工程。
- `Template.ioc`：STM32CubeMX 配置。
- `Core/Src/main.c`：初始化外设后进入 `app_main()`。
- `MDK-ARM/UserCode/app_main.c`：传感器轮询、串口命令、联动控制、按键扫描、页面状态机和息屏逻辑。
- `Core/Src/oled.c`、`Core/Inc/oled.h`：OLED 初始化、文本与帧缓冲显示。
- `Core/Src/dht22.c`、`Core/Src/co2.c`：温湿度和 CO₂ 采集。
- `MDK-ARM/UserCode/light.c`、`soil.c`：光照和土壤湿度换算。

## 实现流程

1. `main()` 完成 GPIO、DMA、定时器、串口、ADC 和 I2C 初始化，随后调用 `app_main()`。
2. `app_main()` 初始化传感器、OLED、PWM、串口命令接收和 ADC DMA。
3. 主循环按不同周期读取光照、土壤湿度、CO₂、温湿度，并执行自动联动。
4. `App_KeyScan()` 区分短按和 1500 ms 长按；`App_HandleKeyEvent()` 完成页面切换、返回主页和熄屏唤醒。
5. `App_CheckScreenTimeout()` 在 30 秒无操作后清屏；下一次有效按键仅唤醒并保留原页面。
6. `App_ProcessCommand()` 处理查询命令与手动/自动控制命令。

## 打开与构建

1. 使用 STM32CubeMX 打开 `Template.ioc` 查看配置。
2. 使用 Keil MDK 打开 `MDK-ARM/Template.uvprojx`。
3. 选择工程目标后编译、下载，并在 STM32F103C8T6 开发板上验收。

## 验证状态

| 证据层级 | 状态 | 说明 |
| --- | --- | --- |
| 源码存在 | 已确认 | 归档含 `.ioc`、`.uvprojx`、启动文件、应用源码、驱动和头文件。 |
| 静态检查 | 已确认 | 本次归档检查了状态机、按键、OLED、传感器、串口和联动配置。 |
| 编译 | 用户确认通过 | 用户在本次归档请求中明确说明项目已经完成编译。当前会话未重新编译。 |
| 烧录 | 用户确认通过 | 用户明确说明已经烧录。当前会话未重新下载。 |
| 开发板验收 | 用户确认通过 | 用户明确说明已完成上板验收，功能范围为标题与实验目标所述完整环境监测终端。 |

## 已知边界

- 本次归档没有重新运行 Keil 构建、下载或真机测试，运行证据来自用户当前确认。
- 构建产物、J-Link 日志、Keil 用户配置和临时文件未纳入归档。
- `Core/Inc/main.h` 中 SW1 的注释仍保留早期“假设按键接在 PB12”的措辞；实际工程配置、代码与用户验收均使用 PB12，本次为保持已验收源工程原样而未改写源码。
