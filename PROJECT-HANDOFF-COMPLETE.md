# 智慧农业 LoRa-MQTT 系统完整项目交接

时间戳：2026-08-14 20:53

> 用途：将整个项目交给另一个模型继续接管，不是单次 Bug 摘要。
> 当前结论（2026-08-15 夜更新）：遥测闭环、控制闭环（§18）、M2 终端自治（§19）、M3 网关桥接（§20）**均已真机验收**，验收记录见 `ACCEPTANCE-CONTROL-LOOP-20260814.md`、`ACCEPTANCE-M2-TERMINAL-AUTONOMY-20260814.md`、`ACCEPTANCE-M3-GATEWAY-20260815.md`。网关/终端均用 J-Link V4.96d 烧录并 verify。剩余任务书功能见 §14（T01 统计口径、2 h 长稳、凭据轮换、Git 提交等）。

## 1. 项目位置与资料

- 当前工作区：`E:\TEMPLATE\Template`
- Keil 工程：`E:\TEMPLATE\Template\MDK-ARM\Template.uvprojx`
- CubeMX 工程：`E:\TEMPLATE\Template\Template.ioc`
- 正式任务书：`E:\test\GZFX2602\04-stm32\04-资料\最终项目任务书\智慧农业LoRa-MQTT系统项目任务书.docx`
- 用户历史 GitHub 项目：`https://github.com/brzoskowskatomasz99-alt/stm32f103-learning-labs`
- 当前 README 仍是旧 OLED Demo 说明，不能代表当前 LoRa-MQTT 主流程。

## 2. 总体目标

系统由一块终端板和一块网关板组成：

```text
终端传感器采集
  -> LoRa 遥测帧
  -> 网关解析
  -> ESP8266 Wi-Fi/TCP
  -> MQTT/BaFa 云

云端 MQTT 命令
  -> 网关校验和 LoRa 下发
  -> 指定终端执行
  -> LoRa ACK
  -> 网关发布 MQTT ACK
```

任务书最终还要求本地自动控制、告警、OLED 页面、LED/蜂鸣器/继电器/灯光 PWM/风机 PWM、超时重试、去重和异常恢复。目前尚未全部实现。

## 3. 硬件角色与公共参数

- MCU：STM32F103 系列，J-Link 脚本选择 `STM32F103C8`。
- LoRa：LLCC68，当前公共参数：470.5 MHz、SF9、125 kHz、CR 4/5、22 dBm。
- 终端角色：`LLCC68_P2P_ROLE_TX`，终端 ID `0x0002`。
- 网关角色：`LLCC68_P2P_ROLE_RX`，网关 ID `0x0001`。
- 角色配置：`Core/Inc/llcc68_p2p_config.h`。
- 当前源码默认角色已经恢复为网关 RX。
- 当前真机串口：COM11 为终端、COM12 为网关；端口号以后可能变化。
- 串口参数：115200、8N1。
- ESP8266 使用 2.4 GHz Wi-Fi，配置位于 `Core/Inc/esp.h`。交接文档不复制 SSID、密码或私钥。

## 4. 关键源码入口

### 主循环

- `Core/Src/main.c`
  - 终端：`TerminalSensors_Process()` + `LLCC68_P2P_Process()`。
  - 网关：`LLCC68_P2P_Process()` + `mqtt_task_loop()`。
  - 最新网关启动标识：`[FW] GATEWAY CONTROL-SUB FIX 20260814-1`。

### LoRa 与协议

- `Core/Inc/protocol_lora.h`
- `Core/Src/protocol_lora.c`
- `Core/Inc/llcc68_p2p.h`
- `Core/Src/llcc68_p2p.c`
- `Core/Inc/llcc68_p2p_config.h`

协议采用固定头、显式大端字段和 CRC16-MODBUS，不直接发送 C 结构体。已有帧类型：遥测、命令、ACK、告警、心跳。

### 传感器

- `Core/Inc/terminal_sensors.h`
- `Core/Src/terminal_sensors.c`
- DHT22、CO2、光照、土壤湿度相关驱动分散在 `Core` 与 `MDK-ARM/UserCode`。

### MQTT 与 ESP

- `Core/Inc/esp.h`
- `Core/Src/esp.c`
- `Core/Inc/mqtt.h`
- `Core/Src/mqtt.c`
- `Core/Inc/bridge_mqtt.h`
- `Core/Src/bridge_mqtt.c`

### LED 第一阶段控制

- 终端 LED2：PB15，按当前实现为低电平点亮。
- 第一阶段是二态 LED，不是真正 PWM：命令 `value=0` 表示关闭，`1..100` 表示点亮，ACK 中实际值返回 100。

## 5. 当前 MQTT 主题

当前正确 BaFa 账号中已经创建：

- `agrijson`：完整遥测 JSON。
- `agricmd`：云端控制命令。
- `agriack`：执行 ACK。

代码使用：

- 遥测 JSON 发布：`agrijson/up`
- 命令订阅：`agricmd`
- ACK 发布：`agriack/up`
- 兼容温湿度卡片：`agri004/up`；当前账号页面未确认存在 `agri004`，但不影响已验证的 `agrijson`。

注意：

- `/up` 只更新云端保存值，不推送给订阅者。
- `/set` 用于向订阅者推送控制消息。
- BaFa 网页 `agricmd` 卡片的“发送”应等效向订阅者推送；真机仍需确认。
- 凭据目前硬编码在本地头文件中。不得在聊天、报告或公开仓库复制；公开前应迁出并轮换。

## 6. 已完成并有真机证据的功能

### LoRa 遥测

终端真实日志已经持续出现：

```text
[SENSOR] T=... H=... CO2=... LUX=... SOIL=... STATUS=....
[P2P][TERMINAL] SENT LEN=22
```

网关真实日志已经持续出现：

```text
[P2P][GATEWAY] RECV TYPE=0 SEQ=... SRC=0002 DST=0001 LEN=12
[P2P][GATEWAY] RSSI=... SNR=...
```

### Wi-Fi 与 MQTT

网关真实日志已确认：

```text
ESP Connect WiFi Success
ESP Connect Server Success
MQTT CONNACK OK
MQTT telemetry mode ready
```

### 云端 JSON

网关已经将如下 JSON 发布并在 BaFa `agrijson` 卡片显示：

```json
{"dev":"node-02","seq":169,"temp":26.6,"humi":56.6,"co2":399,"lux":350,"soil":0.0,"rssi":-62}
```

因此“终端传感器 -> LoRa -> 网关 -> MQTT -> BaFa 云卡片”数据闭环已经真机成立。

## 7. 已完成代码但尚未真机验收的控制闭环

> 状态更新（2026-08-14 晚）：本节所述控制闭环已完成真机验收，四层证据齐全，见 §18。

目标命令示例：

```json
{"id":1001,"dev":"node-02","act":"led","value":100,"mode":"manual"}
```

代码路径已经实现：

1. 网关订阅 `agricmd`。
2. `BridgeMqtt_ParseLedCommandJson()` 校验命令并生成 LoRa COMMAND。
3. 网关通过 `LLCC68_P2P_QueueFrame()` 下发。
4. 终端校验目标 ID、执行 PB15 LED，并对重复命令去重。
5. 终端返回 LoRa ACK。
6. 网关格式化并发布：

```json
{"id":1001,"dev":"node-02","result":"ok","actual":100}
```

预期真机日志：

```text
[MQTT][SUB] attempt=1
[MQTT][SUB] waiting topic=agricmd
MQTT SUBACK OK
MQTT control topic ready
MQTT command queued: ...
[P2P][TERMINAL] RECV TYPE=1 ...
[CONTROL][TERMINAL] ACK ID=1001 RESULT=0 ACTUAL=100
[P2P][GATEWAY] RECV TYPE=2 ...
MQTT ACK publish: ...
```

截至本交接时间，这组日志尚未在真机出现，因此不能声称控制闭环完成。

## 8. 自动化测试

测试入口：

```powershell
cd E:\TEMPLATE\Template
.\tests\protocol_lora\run_tests.ps1
.\tests\bridge_mqtt\run_tests.ps1
.\tests\mqtt_subscribe\run_tests.ps1
```

2026-08-14 最近一次实际结果：

```text
PASS protocol_lora
PASS bridge_mqtt
PASS mqtt_subscribe fragmented SUBACK
```

`mqtt_subscribe` 回归测试还断言发出的 SUBSCRIBE 报文字节准确为：

```text
82 0C 00 0A 00 07 61 67 72 69 63 6D 64 00
```

测试只证明纯协议和模拟分片逻辑，不等于真实 UART、真实云端或真机控制已经通过。

## 9. 最新构建产物

### 终端

- HEX：`MDK-ARM/Template/Template_TERMINAL_CONTROL.hex`
- BIN：`MDK-ARM/Template/Template_TERMINAL_CONTROL.bin`
- BIN 大小：20132 字节
- BIN SHA256：`3582A95BBC67180F0EC4E7E8DC40BB63190A25670F83B157514C74C0B75176A7`
- HEX SHA256：`112B611CCF5055A51158DAE12FD425D3BB5A80E510940B72E40103FDA2DE7E15`
- 构建日志：`MDK-ARM/build_terminal_control_bin.log`
- 构建结果：0 errors，0 warnings。
- 注意：该终端产物生成于启动版本横幅加入之前，文件内包含 LED 命令/ACK 代码；但此前终端同样使用了无效的 V4.15e 通用 `loadbin` 流程，因此**不能确认该控制固件已经写入终端板**。当前串口持续发送遥测只证明板上旧遥测程序仍可运行。正式测试控制闭环前，终端也必须用 V4.96d 重新烧录并完成 verify。

### 网关

- HEX：`MDK-ARM/Template/Template_GATEWAY_CONTROL.hex`
- BIN：`MDK-ARM/Template/Template_GATEWAY_CONTROL.bin`
- BIN 大小：22320 字节
- BIN SHA256：`7D680C65BA16941345AD7AA5C3C4B375763DAF0643AA2CE5F5C473871DB7C1EE`
- HEX SHA256：`4BC137DD8B13BCDC47128038483CBFD6F6CA308F800FD5B6A2C96E6C2B11885B`
- 构建日志：`MDK-ARM/build_gateway_flash_verified.log`
- 构建结果：0 errors，0 warnings。
- 包含启动横幅和 MQTT SUBACK 分片/三次重试修复。

## 10. 正确烧录方式

### 重要：不要再使用旧版 J-Link V4.15e

错误路径：

```text
C:\Program Files (x86)\SEGGER\JLinkARM_V415e\JLink.exe
```

V4.15e 不支持 `Device`/`loadfile`/`verifybin`。此前退化为通用 Cortex-M3 `loadbin` 后，虽然输出 `Writing bin data`，但实际没有改写 STM32 Flash。

已经通过只读指纹证明板上仍是旧固件：

- 新 BIN 的 `0x08004EDC` 应以 `[MQTT][SUB] attempt=%u` 开头。
- 板上同地址实际仍是旧字符串 `%d\r\n...agri004/up...`。

正确工具：

```text
C:\Keil_v5\ARM\Segger\JLink.exe
```

版本为 J-Link Commander V4.96d，支持设备选择、Flash 擦除、下载和 verifybin。

### 网关烧录命令

```powershell
& "C:\Keil_v5\ARM\Segger\JLink.exe" -CommanderScript "E:\TEMPLATE\Template\MDK-ARM\flash_GATEWAY.jlink"
```

### 终端烧录命令

```powershell
& "C:\Keil_v5\ARM\Segger\JLink.exe" -CommanderScript "E:\TEMPLATE\Template\MDK-ARM\flash_TERMINAL.jlink"
```

两份脚本现已包含：设备选择、SWD、erase、loadbin、verifybin、reset 和 run。脚本已经静态检查，但尚未由用户使用 V4.96d 完成真机烧录验收，因此不能把 verify 记作已成功。

必须看到下载成功和校验成功；仅看到 `Writing bin data` 不再算成功。

## 11. 当前阻塞问题与已确认根因

> 状态更新（2026-08-14 晚）：该阻塞已解除——已用 V4.96d 完成烧录与 verify，横幅、SUBACK、控制闭环全部通过，见 §18。

用户最新截图表现：

- 遥测仍正常。
- `agricmd` 始终“订阅者：离线”。
- 串口中没有任何新固件应打印的 `[MQTT][SUB]` 日志。

已确认根因：开发板仍运行旧网关固件，V4.15e 的通用 `loadbin` 没有真正刷新 STM32 Flash。

下一步不是继续修改 MQTT，而是先使用 V4.96d 正确烧录并 verify。

正确烧录后，第一行必须出现：

```text
[FW] GATEWAY CONTROL-SUB FIX 20260814-1
```

若出现该横幅并随后：

```text
MQTT SUBACK OK
MQTT control topic ready
```

则刷新 BaFa，`agricmd` 应变为订阅者在线，然后发送 LED 命令完成闭环验收。

若横幅出现但订阅仍失败，根据日志处理：

- `timeout bytes=0`：优先查 ESP 接收回调/发送失败。
- `timeout bytes=2/3`：优先查 UART 分片与同缓冲覆盖。
- `timeout bytes=16`：优先查前置 MQTT 包或固定缓冲上限。
- `SUBACK code:0x80`：服务器明确拒绝订阅，重新核对账号和主题。

## 12. ESP UART 尚存的技术风险

即使新固件烧录成功，`Core/Src/esp.c` 仍有潜在竞态：

1. `ESP_UART_Callback()` 记录长度后立即把同一个 `ESP_buffer` 重新挂给 ReceiveToIdle；下一片可能覆盖上一片。
2. 主线程复制后直接清 `ESP_rx_len`，可能吞掉复制过程中刚到的新事件。
3. `ESP_Send_data_len()` 发送前清 RX 长度和缓冲，可能擦除正在到达的云端命令。

当前只修复了 SUBACK 多片累计、扫描和三次重试。若正确烧录后仍有超时，下一正确方向是 staging buffer + completed chunk 小队列/双缓冲，不要只靠关中断包住 memcpy。

## 13. LoRa 当前已知噪声

真实日志中仍频繁出现：

```text
[P2P][GATEWAY] RX LENGTH INVALID=3/6/8
[P2P][GATEWAY] PROTOCOL DROP=5/6
[P2P][GATEWAY] RADIO DROP IRQ=0020
```

终端偶尔也出现 `PROTOCOL DROP=6`。用户此前明确要求先不处理，因此本阶段没有修改 LoRa 抗干扰逻辑。有效遥测仍能持续通过，但任务书的稳定性指标尚未验收。

## 14. 尚未完成的任务书功能

- ~~MQTT 云端 LED 控制真机闭环和 ACK 验收~~（2026-08-14 晚已完成，见 §18）。
- 蜂鸣器、继电器、灯光 PWM、风机 PWM 的完整远程控制（终端侧 5 执行器手动命令已就绪；网关侧命令桥接解析属 M3）。
- LoRa 命令 1 秒超时、最多 3 次重发与 ACK 匹配。
- 无效云端命令的错误 ACK 发布。
- ~~本地自动控制、阈值配置和网络控制优先级仲裁~~（终端侧 M2 已完成并真机验收，见 §19）。
- 告警帧、告警恢复（终端侧 M2 已完成）；云端告警主题转发属 M3。
- 网关 OLED 数据页、状态页、报警页、按键切换和休眠。
- 断 Wi-Fi、断 LoRa、CRC 错误、非法命令等异常验收（部分观察过，未正式验收）。
- 连续 50 次链路成功率、2 小时稳定性等正式验收。
- 正式 README、架构/接线/部署说明、演示材料和验收报告（验收记录已开始累积）。
- 凭据迁出源码并轮换。
- Git 整理、提交和推送。

## 15. Git 与交付风险

当前工作树非常脏：大量修改文件和大量未跟踪文件。LoRa、MQTT、ESP、协议、测试、构建日志、烧录脚本和本交接文档大多尚未被 Git 跟踪。

因此：

- 当前目录可以运行，但新 clone 无法复现当前系统。
- 不要执行 `git reset --hard`、`git clean` 或覆盖式 checkout。
- 在提交前先排除包含凭据的文件/值，再决定安全的配置方案。
- `MDK-ARM/Template/` 通常被忽略，固件产物不能依赖普通 Git 提交自动保存。
- 当前没有替用户 commit 或 push。

## 16. 接手模型的第一步

严格按以下顺序，不要跳步：

1. 先读本文件、根 `AGENTS.md`、任务书，以及 `Core/Src/main.c`、`mqtt.c`、`esp.c`、`llcc68_p2p.c`。
2. 不再使用 V4.15e。
3. 让用户用 V4.96d 烧录最新网关脚本：

```powershell
& "C:\Keil_v5\ARM\Segger\JLink.exe" -CommanderScript "E:\TEMPLATE\Template\MDK-ARM\flash_GATEWAY.jlink"
```

4. 要求回传 verify 输出与从 `[FW] GATEWAY CONTROL-SUB FIX 20260814-1` 开始的完整串口日志。
5. 只有横幅确认新网关固件正在运行后，才继续判断 SUBACK 或 UART 竞态。
6. 网关订阅成功后，再用同一个 V4.96d 工具执行 `flash_TERMINAL.jlink`。终端当前控制固件在板状态未确认；必须重新烧录后才能验收云端命令执行。
7. 两块板固件均确认后，在 `agricmd` 发送：

```json
{"id":1001,"dev":"node-02","act":"led","value":100,"mode":"manual"}
```

8. 收集网关命令 queued、终端 TYPE=1/ACK、网关 TYPE=2/MQTT ACK 和 `agriack` 卡片更新四层证据。

## 17. 给新模型的简短启动提示

```text
接管 E:\TEMPLATE\Template 的智慧农业 LoRa-MQTT STM32 项目。先完整阅读
PROJECT-HANDOFF-COMPLETE.md 和根 AGENTS.md。遥测闭环已真机跑通，控制闭环代码已实现但未验收。
当前首要问题不是继续改 MQTT，而是此前用 J-Link V4.15e 的 loadbin 并未真正写入 Flash；板上指纹证明仍是旧固件。
必须先用 C:\Keil_v5\ARM\Segger\JLink.exe V4.96d 执行 flash_GATEWAY.jlink，确认 verify 成功和
[FW] GATEWAY CONTROL-SUB FIX 20260814-1 横幅。网关订阅成功后还要用 V4.96d 重新烧录终端；此前
V4.15e 的终端烧录同样不能算成功。之后再验收命令与 ACK，不要改 Wi-Fi、凭据、已验证遥测或 LoRa
格式。用户负责烧录与真机验收，Codex 可负责源码、测试和编译。
```

## 18. 2026-08-14 晚真机验收结果（接管模型记录）

> 本节由接管模型在 2026-08-14 晚控制闭环验收通过后追加，覆盖上面第 7、10、11 节的"尚未验收/阻塞"状态。所有结论均有本会话回传的串口日志与 J-Link 输出为证。

### 18.1 网关烧录（V4.96d 成功）

- 工具：`C:\Keil_v5\ARM\Segger\JLink.exe`（JLinkARM.dll V4.96d）。
- 脚本：`MDK-ARM/flash_GATEWAY.jlink`。
- 输出证据：`Device "STM32F103C8" selected` → `Erasing done.` → `Flash programming performed for 1 range (22528 bytes)` → `O.K.` → `Reading 22320 bytes data from target memory @ 0x08000000. Verify successful.`
- 独立回读比对（`mem32` 只读）：`0x08000000` 向量表、`0x08004EDC`/`0x08004F10` 字符串池与 `Template_GATEWAY_CONTROL.bin` 逐字节一致；halt 时 PC=0x08000172 正在新镜像执行。
- 串口横幅：`[FW] GATEWAY CONTROL-SUB FIX 20260814-1`。
- 订阅成功：`[MQTT][SUB] attempt=1` → `MQTT SUBACK OK` → `MQTT control topic ready`。

### 18.2 终端烧录（V4.96d 成功）

- 脚本：`MDK-ARM/flash_TERMINAL.jlink`。
- 输出证据：`Erasing done.` → `Flash programming performed for 1 range (20480 bytes)` → `O.K.` → `Reading 20132 bytes data from target memory @ 0x08000000. Verify successful.`
- 独立回读比对：`0x08000000` 向量表、`0x08004534`（`[CONTROL][TERMINAL] ACK` 串）与 `Template_TERMINAL_CONTROL.bin` 逐字节一致。
- 终端 BIN 无横幅（生成于横幅加入之前，见 §9），以 `[SENSOR]`/`[P2P][TERMINAL] SENT LEN=` 为运行证据，属预期，勿误判烧录失败。

### 18.3 控制闭环四层证据（全部真机取得）

1. 网关：`MQTT command queued: {"id":1001,"dev":"node-02","act":"led","value":100,"mode":"manual"}`
2. 网关 `[P2P][GATEWAY] SENT LEN=17`；终端 `[P2P][TERMINAL] RECV TYPE=1 SEQ=233 SRC=0001 DST=0002 LEN=7`
3. 终端 `[CONTROL][TERMINAL] ACK ID=1001 RESULT=0 ACTUAL=100` → `[P2P][TERMINAL] SENT LEN=17`；网关 `[P2P][GATEWAY] RECV TYPE=2 SEQ=233 SRC=0002 DST=0001 LEN=7`
4. 网关 `MQTT ACK publish: {"id":1001,"dev":"node-02","result":"ok","actual":100}`

- 用户目视确认：终端板 PB15 LED（CubeMX/任务书命名 LED3，代码宏 `LED2_Pin`，同一引脚）实际点亮；BaFa `agriack` 卡片显示该 ACK。
- `RESULT=0` 即 `PROTOCOL_LORA_ACK_OK`（protocol_lora.h:83）；命令/ACK 帧长 17 = 10 字节头 + 7 字节载荷，收发一致。
- 从 `command queued` 到 `MQTT ACK publish` 在同一日志爆发内完成，远小于任务书 5 s 指标。
- 未覆盖：`value=0` 关灯路径尚未复测（建议随后在 `agricmd` 发 `value:0`，预期 `ACTUAL=0`、灯灭）。

### 18.4 过程排障记录（J-Link USB Out of sync）

- 现象：烧录报 `WARNING: Out of sync` ×N + `Can not connect to J-Link via USB`。
- 根因：系统同时存在 4 个残留 JLink.exe 进程（19:19/20:44/20:47/20:58 启动）争抢同一 USB 设备；J-Link USB 协议不允许并发访问。
- 处置：结束全部残留进程 → 用户拔插网关板 USB → 重跑脚本成功。
- 纪律：一次只允许一个 JLink 进程；跑完确认 Commander 退出；拔插 USB 是 OB 固件（2012 版）争抢后的可靠复位手段。

### 18.5 指纹地址修正

- 本 BIN 中 `[MQTT][SUB] attempt=%u` 实际位于 `0x08004F10`；§10 所写 `0x08004EDC` 处现为 `MQTT connect fail` 串。以后指纹对比以 `0x08004F10` 为准。

## 19. M2 终端自治（2026-08-14 深夜完成，真机验收通过）

> 详细记录见 `ACCEPTANCE-M2-TERMINAL-AUTONOMY-20260814.md`。

### 19.1 新增模块（任务书第 8 章分层）

- `Core/Inc/terminal_config.h`：第 6 章全部阈值/滞回/延时/30 min 手动有效期（编译期配置）。
- `Core/Src/service_hal.c/.h`：执行器硬件抽象（业务与驱动分离）。
- `Core/Src/service_control.c/.h`：4 条规则滞回+1.5 s 确认延时，仲裁 P1 安全 > P3 手动(30 min) > P2 自动 > 关。
- `Core/Src/service_alarm.c/.h`：7 类告警状态机（去重/恢复/CO2 升级/60 s 限频/蜂鸣器 6 s 延时），peek-commit 事件队列。
- `Core/Src/terminal_autonomy.c/.h`：500 ms 调度 + 告警事件组 LoRa ALARM 帧（TYPE=3，载荷 4 字节 code/level/active）。
- 协议扩展：`protocol_lora.h/.c` 告警载荷 + 执行器编码（1 LED/2 蜂鸣器/3 继电器/4 灯光 PWM/5 风机 PWM）。
- 采集增强：`terminal_sensors.c` 滑动均值滤波、连续 3 次失败→故障位（0x20/0x40/0x80/0x100）、CO2 范围检查。
- 集成：`main.c` TIM1 双角色初始化（终端风机 PWM 需要）；`llcc68_p2p.c` 命令改走统一执行器层（保留去重与 LED 二态 ACK）。

### 19.2 测试与构建

- 新增 `tests/service_control`（12 用例）、`tests/service_alarm`（7 用例）；全 5 套测试 PASS。
- 终端角色 0 error 0 warning；网关角色回归 0/0。
- 终端 M2 固件：`MDK-ARM/Template/Template_TERMINAL_M2.bin` 24216 字节，SHA256 `9D65BA6A4853606AB14F77929C69AC72CF8899016787FC2DE76C26658FFD03CB`；烧录脚本 `MDK-ARM/flash_TERMINAL_M2.jlink`。

### 19.3 执行器引脚真实映射（原理图导线连通性追踪，纠正早期文本推断错误）

| 执行器 | 引脚 | 极性 |
|---|---|---|
| LED3 状态灯（手动 LED 命令目标） | PB15 | 低电平亮 |
| 灯光 LED2（低照度联动） | PA8 = TIM1_CH1 | 低电平亮（反相 PWM） |
| 风机 MOTOR | PB8 = TIM4_CH3（Q3 SI2302） | 高电平转 |
| 蜂鸣器 | PB9（Q2 S8050） | 高电平响 |
| 继电器 | PC14（Q4 S8050） | 高电平吸合 |
| SW1/SW2 | PB12/PB13 | 输入上拉 |
| 传感器 | PA0 光敏、PA1 土壤、PC15 DHT22、USART2 CO2 | — |

曾把 PA8/PB8 互换（遮光转风机、CO2 波动 LED2 闪烁），真机现象驱动排查，最终以 PDF 导线矢量 BFS 追踪定案。**今后修改执行器映射必须以此表为准。**

### 19.4 待续（M3）

- ~~网关：告警帧 → `agrialarm` JSON 转发~~（M3 代码完成，待烧录验收，§20）。
- ~~网关：命令桥接扩展到 5 执行器、错误 ACK~~（M3 代码完成）。
- ~~终端在线表、`agristatus` 状态主题~~（M3 代码完成）。
- ~~命令 1 s 超时/3 次重发~~（M3 代码完成）。
- ~~网关 OLED P0-P4 页面与按键~~（M3 代码完成，待屏幕验收）。

## 20. M3 网关桥接（2026-08-15 已真机验收）

> 验收记录见 `ACCEPTANCE-M3-GATEWAY-20260815.md`；验收计划见 `ACCEPTANCE-M3-GATEWAY-PLAN.md`；协议与主题见 `PROTOCOL.md`。

### 20.1 新增模块（网关侧）

- `Core/Src/command_link.c/.h`：命令 1 s 超时 ×3 重发 + cmd_id 匹配 ACK，结果事件队列交给 mqtt 层。
- `Core/Src/terminal_table.c/.h`：终端在线表（30 s 判离线，容量 8）。
- `Core/Src/link_stats.c/.h`：LoRa 成功率滑动统计（50 帧窗口，供 T01）。
- `Core/Src/alarm_registry.c/.h`：活动告警登记表（OLED P4 + 计数）。
- `Core/Src/gateway_data.c/.h`：跨模块数据中枢（遥测/ACK/MQTT 状态 → UI 数据）。
- `Core/Src/ui_oled.c/.h` + `ui_oled_hal.h` + `ui_hal.c`：OLED P0-P4 状态机（轮显/息屏/唤醒/报警优先/静音请求），显示经抽象层可主机单测。
- `Core/Inc/gateway_config.h`：网关参数配置模块。
- `bridge_mqtt` 扩展：FormatAlarmJson / ParseCommandJson（5 执行器）/ FormatErrorAckJson / FormatAckJsonFromAck / FormatStatusJson / AlarmCodeString（公开）。
- `llcc68_p2p` 增加 `LLCC68_P2P_IsTxPending()`；网关 RX 路径接入 LinkStats（有效帧/噪声帧计数）。
- `mqtt.h` 新增主题 `agrialarm/up`、`agristatus/up`；`mqtt.c` 接入全部新模块（ACK 改由命令链路统一发布）。

### 20.2 测试与构建

- 测试套件 12 套全部 PASS（新增 command_link/terminal_table/link_stats/alarm_registry/ui_oled/gateway_data/terminal_autonomy），并全部通过 ASan/UBSan 消毒检查（`tests/asan_check.ps1`）。
- 网关 M3（最新版）：`MDK-ARM/Template/Template_GATEWAY_M3.bin` 32528 B，SHA256 `F9F867D9795D04A3A4ACF2F83EBA2C9C814EC41D85D6493C2C99E5228953311B`；烧录脚本 `MDK-ARM/flash_GATEWAY_M3.jlink`。
- 版本演进（全部 0/0 编译）：1CA55137（中文+防闪烁）→ CDE2F28E（外来帧防护）→ F7870632（本地模块与 ESP 解耦+60 s 自动重试）→ EE9AA29E（UART 接收自愈，验收版）→ **F9F867D9（T01 统计口径修正：20 s 时隙制，噪声不入分母）**。
- Keil 为 MDK-Lite 5.14 试用版（LIC=-），网关镜像 32720/32768（余量 48 B）——**再加功能必须先腾空间或买正版**；中文字库已裁剪 54 字、源文件纯 ASCII（\x 转义）。
- 真机经验（详见验收记录）：仅复位 MCU 不复位 ESP 会残留透传态；UART 接收通道可能停摆需自愈；**终端必须装 LoRa 天线**（换底板后曾因无天线导致终端收不到命令）；网关 SW1=电源开关、SW4=ESP 复位、SW5=ESP 下载模式开关（勿误拨）。
- 终端 M3 构建产物与已验收 M2 v3 **字节级一致**（SHA256 `9D65BA6A…`）→ 终端无需重烧。
- 双角色 Keil 编译 0 error 0 warning（build_gateway_m3.log / build_terminal_m3.log）。

### 20.3 凭据迁出

- `Core/Inc/secrets.h`（SSID/密码/BEMFA_UID，已被 `.gitignore` 排除）+ `Core/Inc/secrets_template.h`（占位模板）。
- `esp.h`/`mqtt.h` 改为 `#include "secrets.h"`；迁移后网关产物哈希不变（字节透明验证）。
- **轮换凭据待用户执行**（巴法云后台 + secrets.h 同步更新）。

### 20.4 文档

- `README.md` 重写（架构/编译/烧录/引脚/测试/凭据说明）。
- `PROTOCOL.md` 新建（帧格式/执行器与告警码/device_status/MQTT 主题/可靠性规则）。
