# STM32F103 OLED I2C Demo

基于 STM32F103C8T6、STM32CubeMX 和 Keil MDK 的 I2C OLED 实验工程。

## 当前运行内容

- 使用 I2C1 驱动 OLED：PB6 为 SCL，PB7 为 SDA。
- OLED 驱动位于 `Core/Src/oled.c` 和 `Core/Inc/oled.h`。
- 当前入口仅初始化 I2C1，并执行 OLED 显示实验。
- 工程目录仍保留 DHT22、CO2 等早期实验模块，但它们不属于当前运行入口。

## 工程入口

- CubeMX 配置：`Template.ioc`
- Keil 工程：`MDK-ARM/Template.uvprojx`
- 主程序：`Core/Src/main.c`

## 构建与运行

1. 使用 Keil MDK 打开 `MDK-ARM/Template.uvprojx`。
2. 编译工程。
3. 连接 STM32F103C8T6 开发板并下载程序。
4. 观察 OLED 显示结果。

> 仓库中的源码与工程配置可供检查；实际编译、下载和开发板显示结果需要在本机 Keil 与硬件环境中验证。
