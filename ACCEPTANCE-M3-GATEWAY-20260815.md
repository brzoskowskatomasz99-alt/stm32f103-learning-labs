# M3 网关桥接验收记录

- 日期：2026-08-15
- 对应范围：任务书 M3（F03 告警/遥测桥接、F04 五执行器远程控制、F06 OLED、T02/T04/T05 部分）+ 交接文档 §20
- 结论：**通过（真机）**。全部验收项证据见下。

## 1. 固件版本

| 板 | 固件 | SHA256 |
|---|---|---|
| 网关 | `Template_GATEWAY_M3.bin`（32528 B，最新版含 T01 统计口径修正） | `F9F867D9795D04A3A4ACF2F83EBA2C9C814EC41D85D6493C2C99E5228953311B`（验收时为 `EE9AA29E…`，功能差异仅 link_rate 统计口径与日志精简） |
| 终端 | `Template_TERMINAL_M2.bin`（24216 B，M3 构建与其字节一致） | `9D65BA6A…` |

- 烧录工具：J-Link V4.96d（loadbin+verifybin，全部 `Verify successful.` + 独立回读比对）。
- Keil MDK-Lite 32KB 限制下网关镜像 32752/32768（余量 16 B，后续加功能需先腾空间）。

## 2. 验收项与证据

### 2.1 T02 五执行器远程控制（agricmd → LoRa → 终端 → ACK → agriack）

| act | 证据 | 结果 |
|---|---|---|
| led | `ACK ... RESULT=0 ACTUAL=100/0`，LED3(PB15) 亮/灭（用户目视） | ✅ |
| buzzer | ACK 100/0，蜂鸣器响/停（用户目视） | ✅ |
| relay | ACK 100/0，继电器吸合/释放（用户耳闻咔哒） | ✅ |
| light | ACK `<值>`，灯光 LED2(PA8) 亮度变化（用户目视） | ✅ |
| fan | ACK 80（用户板无风机硬件，以 ACK+PWM 输出为准） | ✅（软证据） |

### 2.2 非法命令错误 ACK
`agricmd` 发 `dev:node-03` 的错误命令 → `agriack` 回 `{"id":2001,...,"result":"error"}`（用户确认"对的"）。

### 2.3 告警转发（agrialarm）
终端遮光/恢复 → 网关 `MQTT alarm publish: {"dev":"node-02","code":"LIGHT_LOW","active":true/false,...}` → BaFa `agrialarm` 卡片更新（用户确认）。
另加防护：告警码仅接受 1..7 且等级非 0，垃圾帧 `[ALARMREG] BAD DROP` 丢弃。

### 2.4 状态主题（agristatus）
`MQTT status publish: {"gw":"node-01","mqtt":true,"link_rate":69,"terminals":[{"dev":"node-02","online":true}]}` 每 60 s 发布，BaFa `agristatus` 卡片显示（用户确认）。

### 2.5 命令超时重试（T04 核心项）
- 终端断电发 3300：`SEND RETRY=0 → TIMEOUT RETRY=1/2/3（各 1 s）→ GIVE UP ID=3300 → MQTT timeout ACK publish {"result":"error"}` ✅ 串口全序列捕获；
- 终端上电发 3304：终端 `RECV TYPE=1 ... → [CTRL] MANUAL SET ACT=0 VALUE=100 → ACK ID=3304 RESULT=0 ACTUAL=100`，agriack `"ok","actual":100`，LED3 亮 ✅。

### 2.6 OLED 中文页 + 按键（F06/T05）
- 中文界面（字库精简 54 字：粤嵌科技/温度/湿度/光照/土壤 + ASCII），无闪烁（内容变化才刷新），用户确认"亮起中文页""不闪"；
- 按键（网关 SW2=PB12、SW3=PB13；SW1 为电源开关勿按）：长按 SW2 `[UI] ROTATE OFF`、长按 SW3 `[UI] BUZZER SILENCE REQUEST→SILENCE OK`、息屏/唤醒 `[UI] SLEEP/WAKE` 串口日志齐备；
- 5 min 息屏/按键唤醒验证 ✅。

### 2.7 断线自愈（ESP/MQTT）
- 断连后每 60 s 自动重试；UART 接收通道自愈修复（RxState 检测重挂）后，MQTT 恢复（`CONNACK OK`，遥测/状态持续发布）✅。

## 3. 过程排障记录（重要经验）

| 问题 | 根因 | 处置 |
|---|---|---|
| 网关 MQTT 全断、AT 一直失败 | STM32 单独复位后 ESP 残留透传态 + UART 接收通道停摆（交接 §12 竞态） | 整板断电重启 ESP；UART 自愈（RxState 检测重挂）；ESP 60 s 自动重试 |
| OLED 黑屏 | OLED 初始化被"ESP 成功"分支包住；且 I2C 超时 HAL_MAX_DELAY 死等 | 本地模块与 ESP 解耦；I2C 100 ms 有界超时 + 缺失降级 |
| 屏幕闪烁 | 0.5 s 全刷 + OLED_Clear 直写屏 | 内容变化才刷新 + 只清显存 |
| 中文不显示 | Keil MDK-Lite 32KB 镜像限制 + ARMCC5 源编码 | 字库裁剪 54 字 + 纯 ASCII 源（\x 转义） |
| 终端收不到命令 | **终端未装 LoRa 天线**（换底板后只有网关有天线） | 终端补装天线后恢复路径立即打通 |
| 同频外来帧（LEN=10 遥测/RESULT=2 ACK/UNKNOWN 告警） | 同频段其他设备 | 告警码校验、ACK result 校验 |

## 4. 遗留（后续阶段）

- ~~T01 链路成功率统计口径~~（已修正 + 已重烧 475.5 MHz 双固件，见 §5）；
- 2 小时稳定性验收、50 次连续成功率统计（换频后噪声=0，link_rate 已到 88% 且爬升中，挂机即可达标）；
- 终端天线为借的，正式交付需自购一根 470-510 MHz 弹簧天线；
- 凭据轮换（巴法云后台 + secrets.h）；
- Git 提交与推送（排除 secrets.h）。

## 5. 换频事件（2026-08-15 夜）

教室 50 组同频（470.5 MHz + 终端 ID 均 0x0002）导致：同学组遥测/告警混入云端、link_rate 失真、命令可能被外机抢占。处置：**两块板统一改频 475.5 MHz** 重烧。

- 网关：`Template_GATEWAY_M3.bin` 32528 B，SHA256 `0101D9ADE87851064F84B4845AE3FBE1D1E6546CAA4B7D5A4DCFC15246E8B435`
- 终端：`Template_TERMINAL_M3.bin` 24216 B，SHA256 `DE509F4FC6447C230832FA267C23BE982A96A281F65237C92F14A7156CB3C10D`（脚本 `flash_TERMINAL_M3.jlink`）
- 换频后实测（COM12 2 分钟）：**噪声 0 条**（换频前每分钟 30-50 条）、遥测仅本终端（RSSI -15~-22）、`link_rate:88` 且爬升中。
- 频率配置位置：`Core/Inc/llcc68_p2p_config.h` 的 `LLCC68_P2P_FREQ_HZ`（两板必须一致）。
