# M3 网关桥接：实现完成，待烧录验收计划

- 日期：2026-08-15（无硬件条件下完成全部代码/测试/编译/出包）
- 结论：**代码与构建完成**；真机验收待板子回到手边后执行。

## 1. 本轮交付（无硬件）

| 模块 | 文件 | 说明 |
|---|---|---|
| 告警转发 | `bridge_mqtt.c`（FormatAlarmJson）、`mqtt.c`、`alarm_registry.c` | TYPE=3 → `agrialarm/up` JSON + 活动告警登记表（OLED P4 用） |
| 五执行器命令桥 | `bridge_mqtt.c`（ParseCommandJson） | `act`:led/buzzer/relay/light/fan；非法命令发布 `{"result":"error"}` |
| 命令可靠性 | `command_link.c` | 1 s 超时、最多 3 次重发、cmd_id 匹配 ACK、超时错误 ACK |
| 在线表/状态 | `terminal_table.c`、`link_stats.c`、`gateway_data.c` | 30 s 判离线；50 帧滑动成功率；`agristatus/up`（60 s） |
| OLED | `ui_oled.c`、`ui_hal.c`、`ui_oled_hal.h` | P0-P4、SW1/SW2 短长按、5 min 息屏/唤醒、报警优先、静音请求 |
| 配置 | `gateway_config.h` | 超时/重发/离线/状态周期/OLED 参数 |
| 测试 | `tests/command_link`、`terminal_table`、`link_stats`、`alarm_registry`、`ui_oled`（+bridge_mqtt 扩展） | 10 套全部 PASS |
| 文档 | `README.md`（重写）、`PROTOCOL.md`（新建） | 编译/烧录/引脚/协议/主题 |
| 凭据 | `secrets.h`（gitignore）+ `secrets_template.h` | SSID/密码/BEMFA_UID 迁出源码 |

## 2. 构建产物

| 固件 | 文件 | 大小 | SHA256 |
|---|---|---|---|
| 网关 M3 | `MDK-ARM/Template/Template_GATEWAY_M3.bin` | 32528 B | `F9F867D9795D04A3A4ACF2F83EBA2C9C814EC41D85D6493C2C99E5228953311B` |
| 终端 M3 | `MDK-ARM/Template/Template_TERMINAL_M3.bin` | 24216 B | `9D65BA6A…`（**与已验收的 M2 v3 字节一致 → 终端无需重烧**） |

- 双角色 Keil 编译 0 error 0 warning（`build_gateway_m3.log`/`build_terminal_m3.log`）。
- 烧录脚本：`MDK-ARM/flash_GATEWAY_M3.jlink`。
- 凭据迁移后网关产物哈希不变（4E641E9C 前后一致，字节透明）。
- 2026-08-15 追加修复（F7870632 版，在 1CA55137 基础上）：
  ⑦ **网关本地模块与 ESP 解耦**：OLED/在线表/告警登记/命令链路初始化不再依赖 ESP 成功（此前 ESP 初始化失败会导致 OLED 黑屏、MQTT 永不启动）；
  ⑧ **ESP/MQTT 断线自动重试**：每 60 s 重试一次（T04 断网恢复）；
  ⑨ ui_oled.c 中文字面量改为 \x 转义字节（ARMCC5 纯 ASCII 源要求）。
  ① 断 MQTT 时 LoRa 帧处理（ACK 匹配/告警登记/在线表/UI 数据）继续工作；
  ② esp.c 出错日志不再回显含凭据的 AT+CWJAP；
  ③ **OLED I2C 超时由 HAL_MAX_DELAY（无限）改为 100 ms，且 OLED 缺失/故障时 UI 降级为无操作**；
  ④ **OLED 防闪烁**：清屏只清显存、内容变化才整帧刷新（原 0.5 s 全刷会闪）；
  ⑤ **中文界面**：16x16 点阵字库精简为界面所需 54 字（"粤嵌科技/温度/湿度/光照/土壤"+ASCII），纯 ASCII 源文件（中文用 \x 转义字节），适配 **Keil MDK-Lite 32KB 镜像限制**（当前 32632/32768，余量 136 B；再加功能需先腾空间或换正版 Keil）；
  ⑥ **外来帧防护**：告警码仅接受 1..7 且等级非 0；ACK result 仅接受 0/1（同频段其他设备/异协议帧会被丢弃并记日志）。
- 真机经验：仅复位 MCU 不复位 ESP 会残留透传态导致 CWJAP 失败，**整板断电重启可恢复**。

## 3. 板子回来后按此验收

### 3.0 云端准备
巴法云后台创建主题：**`agrialarm`**、**`agristatus`**（agrijson/agricmd/agriack 已有）。

### 3.1 烧录网关（终端保持 M2 v3 不动）
```powershell
& "C:\Keil_v5\ARM\Segger\JLink.exe" -CommanderScript "E:\TEMPLATE\Template\MDK-ARM\flash_GATEWAY_M3.jlink"
```
判定：`Reading 32564 bytes … Verify successful.` 回传输出，独立回读抽查（新固件含 `[CMDLINK] INIT OK`、`[TABLE] INIT OK`、`[LINK] INIT OK`、`[ALARMREG] INIT OK`、`[UI] INIT OK` 启动日志）。

> 状态：本计划所列验收项已于 2026-08-15 全部真机完成，见 `ACCEPTANCE-M3-GATEWAY-20260815.md`。

### 3.2 T02 五执行器远程控制（agricmd 依次发送，value 1..100）
| act | 预期终端 | 预期 agriack |
|---|---|---|
| led | LED3(PB15) 亮/灭 | `"actual":100/0` |
| buzzer | 蜂鸣器响/停 | `"actual":100/0` |
| relay | 继电器吸合/释放 | `"actual":100/0` |
| light | 灯光 LED2(PA8) 亮度 | `"actual":<值>` |
| fan | **用户板无风机硬件**：以 ACK `"actual":<值>` + PB8 PWM 输出为准（万用表可选测），不做目视 | `"actual":<值>` |

### 3.3 非法命令错误 ACK
`agricmd` 发 `{"id":2001,"dev":"node-03","act":"led","value":50,"mode":"manual"}` 或 `value:101` → `agriack` 出现 `{"id":2001,...,"result":"error"}`。

### 3.4 告警转发
遮光/探头拔出等触发终端告警 → 网关日志 `MQTT alarm publish: {"dev":"node-02","code":...}` → BaFa `agrialarm` 卡片更新；恢复时 `"active":false`。

### 3.5 状态主题
等待 60 s → 网关日志 `MQTT status publish: {"gw":"node-01","mqtt":true,"link_rate":…,"terminals":[{"dev":"node-02","online":true}]}` → `agristatus` 卡片。

### 3.6 命令超时重试（T04 部分）
终端断电（或拔 LoRa 天线），`agricmd` 发命令 → 网关日志 4 次 `[CMDLINK] SEND … RETRY=0/1/2/3`（间隔 1 s）→ `[CMDLINK] GIVE UP` → `agriack` 收到 `{"result":"error"}`（超时）。终端恢复供电后，再发同 id 命令 → 正常 ACK。

### 3.7 OLED（T05）
**前置条件**：网关板 I2C 接口（SDA/SCL，PB7/PB6）插上 SSD1306 OLED 模块再上电（网关原理图有该接口）。未插模块时启动会打印 `[UI] OLED INIT FAIL (no display?)`，系统其余功能不受影响。
**按键对应（网关原理图导线追踪）**：SW1 是电源开关（勿按）；页面按键为 **SW2=PB12（短按下一页/长按轮显开关）、SW3=PB13（短按上一页/长按静音）**。
插上 OLED 后：网关板上电 → P0 概览（5 s 轮换 P0-P3）；SW2 短按切页、SW2 长按停/启轮显、SW3 短按回页；有告警时自动切 P4 报警页、SW3 短按浏览；SW3 长按输出 `[UI] BUZZER SILENCE CONFIRMED`；5 min 无操作息屏、按键唤醒（串口有 `[UI] SLEEP/WAKE`）。
> 注意：网关蜂鸣器静音目前只输出日志；若网关板有蜂鸣器，接线确认后再接驱动。

### 3.8 回归
遥测 JSON/温湿度卡片、LED 命令闭环、M2 终端自治全部复测一遍（终端固件未变，不应有回退）。

## 4. 已知边界
- LoRa 噪声（PROTOCOL DROP 等）仍按用户要求不处理（交接 §13）。
- ESP UART 竞态（交接 §12）未复发，暂不改。
- 网关 OLED 显示内容以串口日志 + 屏幕目视为准；字体渲染未经硬件确认。
