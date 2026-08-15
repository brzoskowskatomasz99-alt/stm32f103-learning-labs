# M2 终端自治验收记录

- 日期：2026-08-14 晚
- 对应范围：任务书 M2（F01 采集滤波 / F05 本地联动报警 / 第 6 章阈值与仲裁 / 第 8 章分层架构）+ 交接文档 §14 相关项
- 结论：**通过（真机）**。终端侧 4 条联动规则、7 类告警、P1-P4 仲裁、LoRa 告警帧上报全部真机验证；控制闭环（LED）回归无回退。

## 1. 交付物

| 类别 | 内容 |
|---|---|
| 配置模块 | `Core/Inc/terminal_config.h`（任务书第 6 章全部阈值/滞回/延时/30 min 手动有效期） |
| 硬件抽象 | `Core/Inc/service_hal.h` + `Core/Src/service_hal.c` |
| 控制服务 | `Core/Inc/service_control.h` + `Core/Src/service_control.c` |
| 告警服务 | `Core/Inc/service_alarm.h` + `Core/Src/service_alarm.c` |
| 胶水层 | `Core/Inc/terminal_autonomy.h` + `Core/Src/terminal_autonomy.c` |
| 协议扩展 | `protocol_lora.h/.c`：ALARM 帧载荷（code u16/level u8/active u8，4 字节）+ 执行器编码 1-5 |
| 采集增强 | `terminal_sensors.h/.c`：滑动均值滤波（窗口 3）、连续 3 次失败→故障位（0x20/0x40/0x80/0x100）、CO2 范围检查 |
| 集成 | `main.c`（TIM1 双角色初始化、自治调度）、`llcc68_p2p.c`（命令改走统一执行器层，保留去重与 LED 二态 ACK 语义）、`Template.uvprojx`（注册新源文件） |
| 测试 | `tests/service_control`（12 用例）、`tests/service_alarm`（7 用例）、`tests/protocol_lora`（+告警载荷用例） |
| 固件 | `MDK-ARM/Template/Template_TERMINAL_M2.bin` 24216 字节，SHA256 `9D65BA6A4853606AB14F77929C69AC72CF8899016787FC2DE76C26658FFD03CB` |
| 烧录脚本 | `MDK-ARM/flash_TERMINAL_M2.jlink`（V4.96d，loadbin+verifybin） |

## 2. 自动化验证

```text
PASS protocol_lora
PASS bridge_mqtt
PASS mqtt_subscribe fragmented SUBACK
PASS service_control   （滞回/确认延时/手动覆盖/30min 过期/安全优先/非法命令）
PASS service_alarm     （去重/恢复/CO2 升级降级/60s 限频/6s 蜂鸣器延时/peek-commit）
```

终端角色 Keil 编译 0 error 0 warning；网关角色回归编译 0 error 0 warning；网关板上运行固件不受影响。

## 3. 真机证据（用户目视 + 串口日志，本会话回传）

| 规则 | 串口证据（摘录） | 硬件确认 |
|---|---|---|
| SOIL_DRY | `RULE=SOIL_DRY ACTIVE` → `ACT=2 SRC=AUTO VALUE=100`；恢复 `CLEAR` | 继电器吸合/释放 ✓ |
| SOIL_WET | `RULE=SOIL_WET ACTIVE` → `ACT=2 SRC=AUTO VALUE=0` + `SEND CODE=2`（入水 84.2%） | 继电器释放 ✓ |
| LIGHT_LOW | 遮光 `ACTIVE` → `ACT=3 SRC=AUTO VALUE=100`；见光 `CLEAR`（LUX=350 边界可恢复） | LED2（PA8）亮/灭 ✓ |
| CO2_HIGH | `ACTIVE` → `ACT=4 SRC=AUTO VALUE=80`；<1300 恢复 | 风机（PB8）80% 转 ✓ |
| SENSOR_FAULT | `RAISE CODE=7 LEVEL=2`（STATUS=0021=DHT 无效+故障）→ 插回 `CLEAR CODE=7` | 故障期间安全输出 ✓ |
| 告警帧上链 | 网关 `RECV TYPE=3 SEQ=… LEN=4` 持续收到 | 网关侧日志 ✓ |
| 蜂鸣器 6 s | 温湿度越限持续 6 s 后响（用户确认），恢复即停 | 蜂鸣器 ✓ |
| LED 命令回归 | `ACK ID=… RESULT=0 ACTUAL=100`，`value=0` 关灯路径此前已验收 | LED3（PB15）✓ |

## 4. 重要修正：执行器引脚真实映射（原理图导线追踪）

最初实现把灯光/风机 PWM 引脚互换（PA8↔PB8），真机现象（遮光转风机、哈气 LED2 闪烁、见光不灭）暴露后，用 `SCH_智慧农业终端_2026-03-29.pdf` 的导线矢量做连通性追踪纠正。最终映射：

| 执行器 | 引脚 | 极性 |
|---|---|---|
| LED3 状态灯（手动 LED 命令目标） | PB15 | 低电平亮 |
| 灯光 LED2（低照度联动） | PA8 = TIM1_CH1 | 低电平亮（反相 PWM：compare=(100-v)*999/100） |
| 风机 MOTOR | PB8 = TIM4_CH3（Q3 SI2302） | 高电平转（compare=v*99/100） |
| 蜂鸣器 | PB9（Q2 S8050） | 高电平响 |
| 继电器（灌溉） | PC14（Q4 S8050） | 高电平吸合 |
| 按键/传感器 | PB12=SW1、PB13=SW2；PA0 光敏、PA1 土壤、PC15 DHT22、USART2 CO2 | — |

## 5. 已知事项

- 网关收到 TYPE=3 告警帧后当前打印 `MQTT telemetry drop: json=2`（网关侧告警转发属 M3）。
- 温湿度 90% 阈值真机用蒸汽触发（哈气难达标）；阈值全部在 `terminal_config.h`，可改。
- 上电瞬态蜂鸣器可能短促响一声（PB9 驱动管基极浮空，硬件特性，非固件逻辑）。
- LoRa 噪声（PROTOCOL DROP 等）仍按用户要求不处理（交接 §13）。
