# LoRa 帧协议与 MQTT 数据模型（v1）

## 1. LoRa 帧格式（任务书第 5 章）

无线电：475.5 MHz（2026-08-15 由 470.5 改频，避开粤嵌课程默认频点的 50 组同频干扰）、SF9、BW125 kHz、CR 4/5、前导码 8、22 dBm、显式头、CRC 开。

```text
┌──────┬──────────────┬──────┬──────┬──────┬──────┬──────────┬────────┐
│ SOF  │ 版本/类型     │ 源ID │ 目的ID│ 序号 │ 载荷长│ 载荷      │ CRC16  │
│ 1B   │ 1B（各4bit）  │ 2B   │ 2B    │ 1B   │ 1B    │ 0..240B  │ 2B     │
└──────┴──────────────┴──────┴──────┴──────┴──────┴──────────┴────────┘
```

- SOF = `0xA5`；版本 = 1；CRC16-MODBUS（对"版本/类型"至载荷计算，大端存放）。
- 所有多字节字段**大端**；禁止直接发送 C 结构体内存。
- 设备 ID：网关 `0x0001`，终端 `0x0002` 起；序号 0-255 循环，用于去重与应答匹配。
- 载荷上限 240 B，整帧上限 250 B。

### 帧类型（低 4 bit）

| 类型 | 值 | 载荷 | 说明 |
|---|---|---|---|
| 遥测 | 0 | 12 B | temp_x10:int16、humi_x10:u16、co2_ppm:u16、lux:u16、soil_x10:u16、device_status:u16 |
| 命令 | 1 | 7 B | cmd_id:u16、actuator:u8、action:u8（0 关/1 设）、value:u16、mode:u8（1 手动） |
| 应答 | 2 | 7 B | cmd_id:u16、result:u8（0 OK/1 无效）、actual_value:u16、device_status:u16 |
| 告警 | 3 | 4 B | alarm_code:u16、alarm_level:u8、active:u8（1 发生/0 恢复） |
| 心跳 | 4 | 0 B | — |

### 执行器编码（actuator）

`1`=LED（LED3/PB15，二态，ACK 实际值 0/100）、`2`=蜂鸣器、`3`=继电器、`4`=灯光 PWM、`5`=风机 PWM。

### 告警码（alarm_code）

`1`=LIGHT_LOW、`2`=SOIL_WET、`3`=SOIL_DRY、`4`=CO2_HIGH、`5`=TEMP_ALARM、`6`=HUMI_ALARM、`7`=SENSOR_FAULT。
CO2 告警等级：>1500 ppm level 1，>2000 ppm level 2。

### device_status 位定义

| 位 | 含义 |
|---|---|
| 0x0001/0x0002/0x0004/0x0008 | DHT22/CO2/光照/土壤 数据无效 |
| 0x0010 | ADC 故障 |
| 0x0020/0x0040/0x0080/0x0100 | DHT22/CO2/光照/土壤 连续 3 周期失败（SENSOR_FAULT） |

## 2. 可靠性（任务书第 9 章）

- 命令链路（网关）：1 s 超时、最多重发 3 次、按 cmd_id 匹配 ACK；超时发布错误 ACK。
- 重复命令（同 cmd_id）终端不重复执行，仅回 ACK。
- 远端手动命令 30 min 有效期，超时恢复本地自动（P3）。
- 入站帧全部 CRC 校验；无效传感器值不覆盖最后有效值。

## 3. MQTT 数据模型（巴法云）

| 主题 | 方向 | 载荷示例 |
|---|---|---|
| `agrijson/up` | 网关→云 | `{"dev":"node-02","seq":18,"temp":25.6,"humi":62.1,"co2":860,"lux":420,"soil":48.5,"rssi":-78}` |
| `agri004/up` | 网关→云 | `#25.6#62.1`（温湿度卡片兼容） |
| `agricmd` | 云→网关 | `{"id":1001,"dev":"node-02","act":"led","value":100,"mode":"manual"}` |
| `agriack/up` | 网关→云 | `{"id":1001,"dev":"node-02","result":"ok","actual":100}` |
| `agrialarm/up` | 网关→云 | `{"dev":"node-02","code":"CO2_HIGH","active":true,"level":2}` |
| `agristatus/up` | 网关→云 | `{"gw":"node-01","mqtt":true,"link_rate":98,"terminals":[{"dev":"node-02","online":true}]}` |

命令 `act` 取值：`led`/`buzzer`/`relay`/`light`/`fan`；`value` 0-100（LED 二态：0 关/1..100 亮）。
非法命令（缺 id、错误 dev、未知 act、value>100、非 manual）→ 网关直接发布
`{"id":1001,"dev":"node-02","result":"error"}`。
`/up` 只更新云端保存值；`agricmd` 卡片"发送"向订阅者推送。

## 4. 实现位置

- 编解码：`Core/Src/protocol_lora.c`（`ProtocolLora_Set/Get*Payload`）
- JSON：`Core/Src/bridge_mqtt.c`
- 命令链路：`Core/Src/command_link.c`；终端去重：`Core/Src/llcc68_p2p.c`（`llcc68_p2p_handle_terminal_command`）
