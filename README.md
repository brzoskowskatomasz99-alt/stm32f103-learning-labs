# 智慧农业 LoRa-MQTT 系统

STM32F103C8T6 网关 + 终端的温室环境监测、本地联动控制与巴法云（BaFa）MQTT 接入工程。

## 系统架构

```text
终端（node-02）                              网关（node-01）
  DHT22/CO2/光照/土壤 采集（2 s，滤波）         LoRa 汇聚（TYPE=0/2/3）
  本地自治（阈值/滞回/告警，500 ms）      ←→    ESP8266 Wi-Fi/TCP → BaFa MQTT
  执行器（LED3 PB15 / 灯光 LED2 PA8 / 风机 PB8   命令链路（1 s 超时 ×3 重发）
         / 蜂鸣器 PB9 / 继电器 PC14）            OLED P0-P4 + 按键
        │ LoRa 475.5MHz SF9 125k 22dBm │
        └────────── 帧协议（A5 头 + 大端 + CRC16-MODBUS）──────────┘
```

- 遥测：终端 20 s 上报 → 网关 → `agrijson/up`（JSON）+ `agri004/up`（温湿度卡片）
- 命令：`agricmd` → 网关校验 → LoRa TYPE=1 → 终端执行 → LoRa TYPE=2 ACK → `agriack/up`
- 告警：终端 TYPE=3 → 网关 → `agrialarm/up`；状态：`agristatus/up`（60 s）

## 目录

| 路径 | 内容 |
|---|---|
| `Core/Inc/terminal_config.h` | 终端自治全部阈值/延时（任务书第 6 章） |
| `Core/Inc/gateway_config.h` | 网关命令链路/在线表/OLED 参数 |
| `Core/Src/service_control.c` | 联动规则 + P1 安全 > P3 手动 > P2 自动仲裁 |
| `Core/Src/service_alarm.c` | 告警状态机（去重/恢复/升级/限频/蜂鸣器延时） |
| `Core/Src/command_link.c` | LoRa 命令 1 s 超时 ×3 重发 + ACK 匹配 |
| `Core/Src/ui_oled.c` | OLED P0-P4 页面状态机（息屏/唤醒/静音） |
| `Core/Src/bridge_mqtt.c` | JSON 编解码（遥测/命令/ACK/告警/状态） |
| `Core/Inc/protocol_lora.h` | LoRa 帧协议定义（另见 PROTOCOL.md） |
| `tests/` | 12 套主机单元测试（clang，`run_tests.ps1`；`asan_check.ps1` 全量 ASan/UBSan 消毒） |
| `MDK-ARM/flash_*.jlink` | J-Link V4.96d 烧录脚本（loadbin+verifybin） |

## 编译

- 工具链：Keil MDK5（`C:\Keil_v5\UV4\UV4.exe`）
- 角色选择：`Core/Inc/llcc68_p2p_config.h` 中 `LLCC68_P2P_ROLE`（TX=终端 / RX=网关，默认 RX）
- 批处理编译：`UV4.exe -b MDK-ARM\Template.uvprojx -j0 -o build.log`
- BIN 生成：`fromelf.exe --bin --output xxx.bin MDK-ARM\Template\Template.axf`

## 烧录（重要：不要用旧版 J-Link V4.15e）

使用 Keil 自带 V4.96d（`C:\Keil_v5\ARM\Segger\JLink.exe`），脚本含 erase/loadbin/verifybin：

```powershell
# 网关
& "C:\Keil_v5\ARM\Segger\JLink.exe" -CommanderScript "E:\TEMPLATE\Template\MDK-ARM\flash_GATEWAY_M3.jlink"
# 终端
& "C:\Keil_v5\ARM\Segger\JLink.exe" -CommanderScript "E:\TEMPLATE\Template\MDK-ARM\flash_TERMINAL_M2.jlink"
```

判定标准：输出必须出现 `Verify successful.`（仅 `Writing bin data` 不算成功）。
纪律：一次只运行一个 JLink 进程；失败时先结束残留 JLink.exe 并拔插 USB。

## Wi-Fi / MQTT 凭据（已迁出源码）

私有值在 `Core/Inc/secrets.h`（被 `.gitignore` 排除）。新 clone：

1. 复制 `Core/Inc/secrets_template.h` 为 `Core/Inc/secrets.h`
2. 填入 Wi-Fi SSID/密码与巴法云设备私钥（BEMFA_UID）
3. 凭据轮换后同步更新巴法云后台

巴法云需创建主题：`agrijson`、`agricmd`、`agriack`、`agrialarm`、`agristatus`（代码发布用 `/up` 后缀）。

## 执行器引脚（终端，以原理图 SCH_智慧农业终端_2026-03-29 为准）

| 执行器 | 引脚 | 极性 |
|---|---|---|
| LED3（状态灯/手动 LED 命令） | PB15 | 低电平亮 |
| 灯光 LED2（低照度联动） | PA8 = TIM1_CH1 | 低电平亮（反相 PWM） |
| 风机 MOTOR | PB8 = TIM4_CH3 | 高电平转 |
| 蜂鸣器 | PB9 | 高电平响 |
| 继电器（灌溉） | PC14 | 高电平吸合 |
| SW1/SW2 | PB12/PB13 | 输入上拉 |

## 测试

```powershell
.\tests\protocol_lora\run_tests.ps1
.\tests\bridge_mqtt\run_tests.ps1
.\tests\mqtt_subscribe\run_tests.ps1
.\tests\service_control\run_tests.ps1
.\tests\service_alarm\run_tests.ps1
.\tests\command_link\run_tests.ps1
.\tests\terminal_table\run_tests.ps1
.\tests\link_stats\run_tests.ps1
.\tests\alarm_registry\run_tests.ps1
.\tests\ui_oled\run_tests.ps1
.\tests\gateway_data\run_tests.ps1
.\tests\terminal_autonomy\run_tests.ps1
# 全量内存/未定义行为消毒：
.\tests\asan_check.ps1
```

验收记录见 `ACCEPTANCE-CONTROL-LOOP-20260814.md`、`ACCEPTANCE-M2-TERMINAL-AUTONOMY-20260814.md`；项目交接见 `PROJECT-HANDOFF-COMPLETE.md`。
