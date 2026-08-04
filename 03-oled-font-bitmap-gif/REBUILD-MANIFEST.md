# STM32 Rebuild Manifest: OLED 文字、图片与 GIF 取模显示

## Identity

- Project: STM32F103 OLED 文字、图片与 GIF 取模显示
- Archive number: `03`
- Archive slug: `oled-font-bitmap-gif`
- Answer-key repository: `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs`
- Pushed branch: `main`
- Pushed commit: 由本清单所在的已推送 Git 提交确定；精确值记录在本地重建目录索引
- Answer-key directory: `03-oled-font-bitmap-gif`
- Direct GitHub manifest URL: `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/REBUILD-MANIFEST.md`
- Archived local answer key: `E:\TEMPLATE\stm32f103-learning-labs\03-oled-font-bitmap-gif`
- Completed working project: `E:\TEMPLATE\Template`
- Default practice root: `E:\TEMPLATE\oled-font-bitmap-gif-learning`

## Direct answer-key files

| Purpose | Relative path | Direct GitHub URL |
|---|---|---|
| Overview | `README.md` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/README.md` |
| Startup | `Core/Src/main.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Core/Src/main.c` |
| Main feature logic | `MDK-ARM/UserCode/app_main.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/MDK-ARM/UserCode/app_main.c` |
| OLED implementation | `Core/Src/oled.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Core/Src/oled.c` |
| OLED interface | `Core/Inc/oled.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Core/Inc/oled.h` |
| UTF-8 font data | `Core/Inc/FontDotMatrix16.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Core/Inc/FontDotMatrix16.c` |
| Bitmap and GIF data | `Core/Inc/bmp.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Core/Inc/bmp.h` |
| I2C1 implementation | `Core/Src/i2c.c` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Core/Src/i2c.c` |
| GPIO and pin definitions | `Core/Inc/main.h` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Core/Inc/main.h` |
| CubeMX configuration | `Template.ioc` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/Template.ioc` |
| Keil project | `MDK-ARM/Template.uvprojx` | `https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs/blob/main/03-oled-font-bitmap-gif/MDK-ARM/Template.uvprojx` |

## Hardware boundary

- MCU: STM32F103C8T6（由工程与 CubeMX 配置支持）
- Peripherals and buses: I2C1，100 kHz，7 位寻址；SSD1306 OLED 驱动地址常量为 `0x78`
- Inputs: 本实验活动逻辑无输入；PB12 按键只服务于已禁用的小恐龙代码
- Outputs: SSD1306 128×64 单色 OLED；PB6=`I2C1_SCL`，PB7=`I2C1_SDA`
- Explicitly unused hardware: 活动实验不使用 PB15 LED3、蜂鸣器、传感器、串口、ADC、DMA、TIM/PWM

## Function navigation map

| Stage | Practice file | `Ctrl+F` search anchor | Occurrence and operation location |
|---|---|---|---|
| 启动链路 | `Core/Src/main.c` | `MX_I2C1_Init();` | `main()` 内的调用点；随后调用 `app_main()` |
| I2C1 配置 | `Core/Src/i2c.c` | `void MX_I2C1_Init(void)` | 定义；核对 100 kHz、7 位寻址及 `HAL_I2C_Init()` |
| OLED 底层写入 | `Core/Src/oled.c` | `OLED_I2C_ADDRESS` | 文件顶部常量与 `OLED_Write()`；保持 `HAL_I2C_Mem_Write()` 路径 |
| OLED 初始化 | `Core/Src/oled.c` | `HAL_StatusTypeDef OLED_Init(void)` | 定义；按顺序发送 SSD1306 初始化命令 |
| 帧缓冲和位图 | `Core/Src/oled.c` | `void OLED_DrawBitmap` | 定义；横向扫描、每行按字节、最高位在前，再由 `OLED_Refresh()` 整屏发送 |
| UTF-8 中文 | `Core/Src/oled.c` | `void OLED_DrawUtf8Text16` | 定义；按 UTF-8 长度查找 16×16 字模，并写入帧缓冲 |
| 中文字模 | `Core/Inc/FontDotMatrix16.c` | `g_font_dot_matrix_16_index` | 数据定义；目标字为“您、好、陈、工”，每项 32 字节 |
| 静态图与 GIF | `Core/Inc/bmp.h` | `g_image_dot_1_24bit_32x32` | 静态播放图标；同文件的 `g_gif_astronaut_frames` 保存 4 帧动画 |
| 画面组合 | `MDK-ARM/UserCode/app_main.c` | `static HAL_StatusTypeDef App_DrawMouldingDemo` | 活动定义；中文、播放图标和当前 GIF 帧在这里组合 |
| 活动主循环 | `MDK-ARM/UserCode/app_main.c` | `#define GIF_FRAME_INTERVAL_MS` | 该锚点之后的 `app_main()` 才是活动入口；使用 `HAL_GetTick()` 每 150 ms 换帧 |

### Navigation traps

- `MDK-ARM/UserCode/app_main.c` 中共有三个名为 `app_main()` 的定义：前两个分别属于旧环境监测和 OLED 小恐龙代码，均位于 `#if 0`；活动入口是 `GIF_FRAME_INTERVAL_MS` 之后的最后一个定义。
- 不要只搜索 `void app_main(void)` 后修改第一个结果；先搜索唯一锚点 `static HAL_StatusTypeDef App_DrawMouldingDemo`。
- `OLED_ShowText()` 仍是单字节英文接口；中文显示必须走 `OLED_DrawUtf8Text16()`。
- `Core/Inc/FontDotMatrix16.c` 虽位于 `Inc` 目录，但它是需要加入 Keil 构建的 C 源文件；`MDK-ARM/Template.uvprojx` 已包含该项目项。
- `/* USER CODE BEGIN */` 与 `/* USER CODE END */` 是 CubeMX 生成代码保护标记；重建时不要在无保护区域随意加入业务逻辑。

## Ordered rebuild stages

1. **恢复 I2C1 硬件链路**：文件为 `Template.ioc`、`Core/Src/i2c.c`、`Core/Src/main.c`。确认 PB6/PB7、100 kHz、7 位寻址，并在 `main()` 中执行 `MX_I2C1_Init()`。本阶段设备观察：OLED 能完成初始化，不进入 `Error_Handler()`。
2. **恢复 SSD1306 写入和清屏**：文件为 `Core/Src/oled.c`、`Core/Inc/oled.h`。实现命令/数据写入、位置设置、初始化和清屏。设备观察：清屏后无随机残留或花屏。
3. **建立 128×64 帧缓冲**：文件为 `Core/Src/oled.c`。实现 `OLED_FrameClear()`、`OLED_DrawPixel()`、`OLED_DrawBitmap()` 与 `OLED_Refresh()`。设备观察：可显示一个固定测试位图。
4. **加入 16×16 UTF-8 中文字模**：文件为 `Core/Inc/FontDotMatrix16.c`、`Core/Inc/FontDotMatrix16.h`、`Core/Src/oled.c`。实现 UTF-8 字符长度识别、索引查找和 16×16 点阵绘制。设备观察：顶部完整显示“您好陈工”，无拆字节空白。
5. **加入 32×32 静态图片**：文件为 `Core/Inc/bmp.h`、`MDK-ARM/UserCode/app_main.c`。绘制 `g_image_dot_1_24bit_32x32`。设备观察：左下出现播放三角形。
6. **加入 GIF 连续帧**：文件为 `Core/Inc/bmp.h`、`MDK-ARM/UserCode/app_main.c`。用 `HAL_GetTick()` 和 150 ms 间隔循环 4 帧。设备观察：右下宇航员持续变化，主循环不被 `HAL_Delay()` 阻塞。
7. **组合并验收完整画面**：文件为 `MDK-ARM/UserCode/app_main.c`、`MDK-ARM/Template.uvprojx`。确认活动入口、字模源项目项和三类画面同时存在。设备观察：中文、播放图标和宇航员动画同时正常。

## Known answer-key evidence and limits

- Source present: 已确认。归档快照包含 `.ioc`、`.uvprojx`、启动入口、I2C/OLED 驱动、字模、静态图与 GIF 帧。
- Static inspection: 已确认。活动入口、I2C1 引脚与速率、SSD1306 地址、UTF-8 字模路径、位图数据长度、4 帧循环和 Keil 项目引用均已核对。
- Build verified: 用户在归档前确认当前工程编译通过；原始 Keil 日志、错误/警告计数和退出码未在当前记录中保留。
- Download verified: 用户确认当前工程烧录和校验通过；原始下载日志未保留。
- Device verified: 用户确认“您好陈工”、播放图标与宇航员 GIF 动画全部通过。
- Limits: 这些证据只属于本归档答案工程；以后在默认练习目录重建时，必须重新编译、下载并进行开发板验证，不能继承本次结果。

## Catalog handoff

归档推送成功后，在 `C:\Users\ASUS\.codex\skills\stm32-project-rebuild-coach\references\archive-index.md` 以编号 `03` 和 slug `oled-font-bitmap-gif` 登记本清单、本清单 GitHub URL、默认练习目录与实际推送提交。此文件是权威交接清单，本地索引只负责发现。
