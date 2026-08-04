# STM32F103 OLED 文字、图片与 GIF 取模显示

本实验在 STM32F103C8T6 与 SSD1306 OLED 工程上完成三类取模显示：16×16 UTF-8 中文“您好陈工”、32×32 播放图标，以及由 4 帧点阵组成的宇航员 GIF 动画。实验保留原有英文显示、温度图标、清屏和测试图案接口。

## 硬件与配置

| 功能 | STM32 引脚或参数 | 配置与作用 |
| --- | --- | --- |
| OLED SCL | PB6 | I2C1_SCL，复用开漏输出 |
| OLED SDA | PB7 | I2C1_SDA，复用开漏输出 |
| I2C1 | 100 kHz、7 位地址 | SSD1306 驱动使用地址常量 `0x78` |
| OLED | 128×64 单色屏 | 8 页、每页 128 字节的帧缓冲刷新 |

当前活动实验不读取按键，也不使用 LED、蜂鸣器、传感器、串口、ADC 或 PWM。工程中保留的环境监测和 OLED 小恐龙代码位于 `#if 0` 区域，不属于本实验的活动逻辑。

## 工程入口与关键文件

- CubeMX 配置：`Template.ioc`
- Keil 工程：`MDK-ARM/Template.uvprojx`
- 启动入口：`Core/Src/main.c`
- 活动实验入口：`MDK-ARM/UserCode/app_main.c`
- OLED 驱动：`Core/Src/oled.c`、`Core/Inc/oled.h`
- 中文字模：`Core/Inc/FontDotMatrix16.c`、`Core/Inc/FontDotMatrix16.h`
- 图片和 GIF 帧：`Core/Inc/bmp.h`
- I2C1 配置：`Core/Src/i2c.c`、`Core/Inc/i2c.h`

启动流程：

```text
HAL_Init
  -> SystemClock_Config
  -> MX_GPIO_Init
  -> MX_I2C1_Init
  -> app_main
```

## 实现流程

1. `OLED_Init()` 通过 `HAL_I2C_Mem_Write()` 初始化 SSD1306。
2. OLED 驱动维护 1 KB 帧缓冲，提供画点、矩形、横向扫描位图和整屏刷新。
3. `OLED_DrawUtf8Text16()` 按 UTF-8 字符长度查找 16×16 字模；每个汉字使用 32 字节，按 16 列×2 页解释。
4. `App_DrawMouldingDemo()` 在顶部绘制“您好陈工”，左下绘制 32×32 播放图标，右下绘制当前宇航员动画帧。
5. 活动 `app_main()` 使用 `HAL_GetTick()` 每 150 ms 切换一次 GIF 帧，并重新刷新 OLED，不使用阻塞式动画延时。

## 打开、构建与下载

1. 使用 Keil MDK 打开 `MDK-ARM/Template.uvprojx`。
2. 按 `F7` 编译工程。
3. 连接 STM32F103C8T6 开发板，按 `F8` 下载并运行。
4. 观察 OLED 顶部中文、左下播放图标和右下宇航员动画。

## 验证记录

| 证据层级 | 状态 | 证据与边界 |
| --- | --- | --- |
| 源码存在 | 已确认 | 快照包含 `.ioc`、`.uvprojx`、活动入口、OLED/I2C 驱动、中文字模及图片/GIF 点阵 |
| 静态检查 | 已确认 | 活动入口唯一；工程引用 `FontDotMatrix16.c`；字模、静态图和 4 帧 GIF 数据长度与绘制接口一致 |
| 当前构建 | 用户确认通过 | 用户在归档前明确表示全部验证通过；原始 Keil 构建日志和退出码未在当前记录中保留 |
| 当前下载 | 用户确认通过 | 用户确认烧录与校验通过；原始下载日志未在当前记录中保留 |
| 开发板行为 | 用户确认通过 | “您好陈工”、播放图标和宇航员 GIF 动画均在 OLED 上验证通过 |

实际观察结论：本实验要求的文字取模、静态图片取模和 GIF 连续帧显示均由用户在目标开发板上确认通过。

## 已知边界

- 当前字模文件保留 PortHelper 生成的 143 项索引，活动画面只使用“您、好、陈、工”；用户已选择保持当前实现，不再精简为单独的 `oledfont.h`。
- GIF 使用从课件素材选取并转换的 4 帧 32×32 单色点阵，不是完整 103 帧素材。
- 原始编译、下载日志与精确退出码未随本轮验收提供，因此归档只记录用户确认，不补写不存在的原始证据。
- AXF、HEX、对象文件、构建日志、个人 Keil 配置和 J-Link 日志未纳入快照。
