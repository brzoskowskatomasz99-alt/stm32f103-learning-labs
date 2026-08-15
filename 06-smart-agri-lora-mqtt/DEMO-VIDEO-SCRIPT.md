# 智慧农业 LoRa-MQTT 系统 — 演示视频脚本（约 4~5 分钟）

## 拍摄前检查（先做，别录进去）
1. 两块板的天线都插好（终端那根是借的弹簧天线，别忘装）。
2. 两块板通电；网关 OLED 亮中文界面；巴法云后台登录并打开五个主题：
   `agrijson / agricmd / agriack / agrialarm / agristatus`，清空历史消息。
3. 先试发一条命令，看到 agriack 回 ok 再开始录：
   `{"id":3000,"dev":"node-02","mode":"manual","act":"led","value":100}`
4. 注意：网关 SW1 是电源开关、SW5 是 ESP 下载开关，拍摄时都别碰。
5. 手机横屏 + 支架；画面能同时拍到板子和电脑屏幕（或双机位）。

## 第 1 段 · 开机自检（30 秒）
- 镜头拍两块板 + 天线。口播："智慧农业 LoRa-MQTT 系统，网关+终端双节点，
  LoRa 475.5MHz，数据上巴法云。"
- 网关断电再上电（拨 SW1）→ OLED 中文页自动轮播（温湿度/光照/土壤/链路统计）。
- 口播："上电自动联网：ESP8266 连 WiFi → MQTT 订阅 agricmd 成功。"

## 第 2 段 · 自动遥测上云（30 秒）
- 电脑屏幕给 agrijson：每 20 秒一条
  `{"dev":"node-02","seq":…,"temp":…,"humi":…,"co2":…,"lux":…,"soil":…,"rssi":-N}`
- 用手遮光敏电阻 → lux 数值立刻下降 → 松开恢复。
- agristatus 每 60 秒：`{"gw":"node-01","mqtt":true,"link_rate":100,…}`
  口播："链路成功率 100%。"

## 第 3 段 · 告警联动（40 秒，必须在手动命令之前拍）
- 持续遮住光照传感器 → 终端自动开灯（LED2 亮）
  + agrialarm 出现 `{"dev":"node-02","code":"LIGHT_LOW","active":true,…}`
- 松开 → active:false、灯自动灭。
- 口播："光照过低自动补光，告警实时上云。"

## 第 4 段 · 云端远程控制闭环（核心，90 秒）
每个命令在 agricmd 发布框粘贴回车，等 1~2 秒看 agriack：

| # | 粘贴内容 | 看什么 |
|---|---|---|
| 1 LED | `{"id":3001,"dev":"node-02","mode":"manual","act":"led","value":100}` | 终端 LED3 亮，agriack 回 `{"id":3001,"dev":"node-02","result":"ok","actual":100}`；再发 `"value":0` 灭 |
| 2 蜂鸣器 | id 3002、`"act":"buzzer"`、value 100 | 响 → 长按网关 SW3 现场静音（展示本地消音）→ 再发 value 0 停 |
| 3 继电器 | id 3003、`"act":"relay"`、value 100 | "咔哒"吸合声（镜头靠近）→ value 0 释放 |
| 4 调光 | id 3004、`"act":"light"`、value 30 → 100 → 0 | LED2 暗→全亮→灭，ACK 带回实际值 |
| 5 风机 | id 3005、`"act":"fan"`、value 80 | 板无风机硬件，agriack 回 `"actual":80`（软证据） |

- 每步口播："云端 → MQTT → 网关 → LoRa → 终端 → 执行 → ACK 原路回云。"

## 第 5 段 · 容错（30 秒）
- 发 `{"id":3006,"dev":"node-03","mode":"manual","act":"led","value":100}`
  （设备号故意写错）→ agriack 回 `{"id":3006,"dev":"node-02","result":"error"}`
- 口播："非法命令被网关拦截并回错误 ACK，不影响系统。"

## 第 6 段 · OLED 交互（20 秒）
- 短按 SW2 翻页；长按 SW2 → 串口 `[UI] ROTATE OFF`（轮播关）。
- 口播："5 分钟无操作自动息屏，按键唤醒。"

## 第 7 段 · 收尾（20 秒）
- 回巴法云看最新 agristatus：`link_rate:100`、node-02 online。
- 口播："M1-M3 全部真机验收通过，T01-T06 两小时稳定性无断线。"

## 三条提醒
1. 命令 id 每次递增不重复，agriack 里好对号。
2. 告警自动灯必须在手动命令前拍（手动模式有 30 分钟优先级，会压住自动）。
3. 想补强证据可另开 COM12 串口助手（115200 8N1），录下 `[CMDLINK] ACK` 日志。
