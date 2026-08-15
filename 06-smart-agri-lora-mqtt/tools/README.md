# tools/ — 项目工具链四件套（DSH 插件核心）

背景：智慧农业 LoRa-MQTT 项目一天真机验收踩过的坑，全部沉淀为可复用工具。
GitHub 调研结论（2026-08-15）：DSH 生态内这四个需求**没有现成插件**；
成熟工具里 pyserial 已采用，pylink/bloaty 因依赖重/吃 .axf 不采用 → 全部自研薄封装。

| # | 工具 | 语言/依赖 | 解决今天哪个坑 | 状态 |
|---|---|---|---|---|
| P1 | `jlink_flash_verify.ps1` | PowerShell + JLink.exe V4.96d | 假烧录、僵尸 JLink 进程、烧录无回读验证 | ✅ 干跑验证，真机待板回连 |
| P2 | `serial_probe.py` | Python3 + pyserial | 每次手写 PowerShell 抓包 + 手数噪声/帧 | ✅ 离线回放验证（真机待板回连） |
| P3 | `fw_size_report.py` | Python3 标准库 | 32KB 溢出三次，手动裁剪无依据 | ✅ 真 map 验证（24216B/余量 8552B） |
| P4 | `git_publish.ps1` | PowerShell + git | 发布被拒、archive 管道损坏、手工 worktree | ✅ 三用例干跑验证 |

## 用法

### P1 J-Link 烧录验证
```powershell
# 烧写 + verifybin + 独立回读 SHA256 比对（一条命令闭环）
.\tools\jlink_flash_verify.ps1 -Bin .\MDK-ARM\Template\Template_GATEWAY_M3.bin
# 只回读比对（不动板上固件，验证脚本本身用）
.\tools\jlink_flash_verify.ps1 -Bin .\MDK-ARM\Template\Template_GATEWAY_M3.bin -ReadBackOnly
# 干跑（只生成 CommanderScript）
.\tools\jlink_flash_verify.ps1 -Bin ... -DryRun
```
纪律已内置：烧前杀残留 JLink.exe（防 Out of sync）、verifybin + mem32 回读双保险、
SHA256 与本地 bin 比对、失败退出码 1。烧录全程约 1~3 分钟（32KB 回读 8132 字）。

### P2 串口抓包解析
```bash
# 实机抓 120 秒并解析（需要 pyserial: pip install pyserial）
python tools/serial_probe.py --port COM12 --duration 120 --out serial_logs/x.log
# 离线回放已有日志（板不在也能分析）
python tools/serial_probe.py --replay serial_logs/x.log
```
输出：行数、噪声（INVALID|DROP）、帧类型计数（RECV TYPE=0..4）、ACK 闭环、
最后一条 `MQTT status publish` JSON、前缀分类（[UI]/[CMDLINK]/[ALARMREG] 等）。

### P3 固件体积预算
```bash
python tools/fw_size_report.py                      # 分析当前 map
python tools/fw_size_report.py --top 20 --baseline .agents/fw_size_baseline.json
python tools/fw_size_report.py --map <其他.map>     # 指定 map（如网关构建后）
```
输出：Flash 用量/限额(MDK-Lite 32768)/余量、Top 符号、Top 对象、余量趋势火花线。
余量 < 0 退出码 1（超限）；< 1KB 退出码 2（预警）。
注意：map 是"最后一次构建"的产物，分析网关前先在 Keil 里构建网关目标。

### P4 项目入库/增量发布
```powershell
# 全量入库：把当前提交整树放进远端 main 的新文件夹并快进推送
.\tools\git_publish.ps1 -Folder 06-smart-agri-lora-mqtt -Message "Add lab 06"
# 增量更新：只同步指定文件
.\tools\git_publish.ps1 -Folder 06-smart-agri-lora-mqtt `
    -Files ACCEPTANCE-M3-GATEWAY-20260815.md -Message "close T06"
# 演练不推送：-DryRun；只提交不推送：-NoPush
```
安全门：导出树含 secrets.h / *.bin / *.hex / *.axf / *.map 一律中止；
绝不 force push，非快进直接失败并提示先 pull。

## DSH 插件封装（待安装）

脚本本身现在就能用（经 DSH 内置 pwsh/python 工具直接调用，今天已实战）。
若要把它们注册成 DSH 一等工具（`ctx.tools.register`），源码在
`tools/dsh-stm32-toolkit/`（Cordis bundle 骨架），安装方式：

```bash
dsh plugin --profile web add github:<你的仓库>#tools/dsh-stm32-toolkit
```
装前请先对照官方文档校准工具注册 API：
`https://github.com/deepseek-ai/deepseek-harness/blob/HEAD/docs/user/develop/basic/tool.md`
（骨架按 2026-08-15 调研的 `apply(ctx)/inject=['tools']` 约定编写，未在真机安装验证）。
