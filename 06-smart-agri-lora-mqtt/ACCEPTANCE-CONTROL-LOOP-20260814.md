# 控制闭环真机验收记录

- 日期：2026-08-14 晚
- 对应范围：任务书 F04/T02 的 LED 项 + 交接文档 §16 接管验收目标
- 结论：**通过**。MQTT 命令 → 网关校验/下发 → LoRa → 终端 LED 执行 → LoRa ACK → 网关 MQTT ACK → BaFa 卡片，四层证据齐全。

## 1. 环境

| 项 | 值 |
|---|---|
| MCU | STM32F103C8 ×2（网关/终端） |
| LoRa | 470.5 MHz、SF9、125 kHz、CR 4/5、22 dBm |
| 串口 | COM12 网关 / COM11 终端，115200 8N1 |
| 烧录工具 | `C:\Keil_v5\ARM\Segger\JLink.exe`（JLinkARM.dll **V4.96d**） |
| 网关固件 | `MDK-ARM/Template/Template_GATEWAY_CONTROL.bin`（22320 B，SHA256 `7D680C65BA16941345AD7AA5C3C4B375763DAF0643AA2CE5F5C473871DB7C1EE`） |
| 终端固件 | `MDK-ARM/Template/Template_TERMINAL_CONTROL.bin`（20132 B，SHA256 `3582A95BBC67180F0EC4E7E8DC40BB63190A25670F83B157514C74C0B75176A7`） |

## 2. 烧录证据（J-Link Commander 输出要点）

### 网关

```text
Info: Device "STM32F103C8" selected.
Erasing done.
Flash programming performed for 1 range (22528 bytes)
O.K.
Reading 22320 bytes data from target memory @ 0x08000000.
Verify successful.
```

### 终端

```text
Info: Device "STM32F103C8" selected.
Erasing done.
Flash programming performed for 1 range (20480 bytes)
O.K.
Reading 20132 bytes data from target memory @ 0x08000000.
Verify successful.
```

（写入范围按 256 B 对齐：22320→22528、20132→20480；读回校验为精确字节数。）

## 3. 独立回读比对（防"假写"，mem32 只读）

| 板 | 回读地址 | 板上值（摘录） | 与 BIN 比对 |
|---|---|---|---|
| 网关 | `0x08000000` | `20000D90 08000101 080033AB 08002CAD` | ✅ 一致 |
| 网关 | `0x08004F10` | `[MQTT][SUB] attempt=%u\r\n` | ✅ 一致 |
| 终端 | `0x08000000` | `200009F0 08000101 0800309F 08002A41` | ✅ 一致 |
| 终端 | `0x08004534` | `[CONTROL][TERMINAL] ACK ID=%u RESULT=%u ACTUAL=%...` | ✅ 一致 |

## 4. 四层闭环日志（真机）

网关（COM12）：

```text
MQTT command queued: {"id":1001,"dev":"node-02","act":"led","value":100,"mode":"manual"}
[P2P][GATEWAY] SENT LEN=17
[P2P][GATEWAY] RECV TYPE=2 SEQ=233 SRC=0002 DST=0001 LEN=7
[P2P][GATEWAY] RSSI=-60 dBm SNR=11 dB
MQTT ACK publish: {"id":1001,"dev":"node-02","result":"ok","actual":100}
```

终端（COM11）：

```text
[P2P][TERMINAL] RECV TYPE=1 SEQ=233 SRC=0001 DST=0002 LEN=7
[P2P][TERMINAL] RSSI=-61 dBm SNR=12 dB
[CONTROL][TERMINAL] ACK ID=1001 RESULT=0 ACTUAL=100
[P2P][TERMINAL] SENT LEN=17
```

命令/ACK 帧长 17 = 10 字节协议头 + 7 字节载荷；`RESULT=0` = `PROTOCOL_LORA_ACK_OK`（`Core/Inc/protocol_lora.h:83`）；从命令入队到 ACK 发布在同一日志爆发内完成，远小于任务书 5 s 指标。

## 5. 用户目视确认

- 终端板 **PB15 LED**（CubeMX/任务书命名 LED3，代码宏 `LED2_Pin`，同一引脚）实际点亮（`value=100`）。
- BaFa **`agriack` 卡片**显示 `{"id":1001,"dev":"node-02","result":"ok","actual":100}`。
- 网关横幅 `[FW] GATEWAY CONTROL-SUB FIX 20260814-1`、`MQTT SUBACK OK`、`MQTT control topic ready` 均已确认。

## 6. 过程排障记录（J-Link USB Out of sync）

- 现象：烧录报 `WARNING: Out of sync` ×N + `Can not connect to J-Link via USB`。
- 根因：4 个残留 JLink.exe 进程（19:19/20:44/20:47/20:58 启动）并发争抢同一 USB 设备。
- 处置：结束全部残留进程 → 拔插网关板 USB → 重跑脚本成功。
- 纪律：一次只允许一个 JLink 进程；跑完确认 Commander 退出。

## 7. 未覆盖项（后续补测/实现）

- `value=0` 关灯路径（预期 `ACTUAL=0`、灯灭）——建议随手复测。
- 蜂鸣器、继电器、灯光 PWM、风机 PWM 远程控制（任务书 T02 其余项）。
- LoRa 命令 1 s 超时、最多 3 次重发、无效命令错误 ACK。
- 本地自动控制/阈值联动/告警、网关 OLED 页面、异常与长稳验收（交接 §14 全清单）。
