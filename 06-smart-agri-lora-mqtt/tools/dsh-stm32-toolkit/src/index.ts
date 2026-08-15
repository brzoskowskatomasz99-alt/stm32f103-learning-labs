// dsh-stm32-toolkit — DSH 插件薄封装（骨架，未安装验证）
//
// 调研结论(2026-08-15): DSH = Cordis 微内核, 插件 = 导出 apply(ctx) 的模块,
// inject 依赖注入, ctx.tools.register() 注册工具。
// 工具注册 API 的精确参数结构请对照官方 tool.md 校准后再 dsh plugin add:
//   https://github.com/deepseek-ai/deepseek-harness/blob/HEAD/docs/user/develop/basic/tool.md
//
// 设计: 插件只做"注册 + 参数校验 + 调用脚本 + 解析退出码"这一层薄壳,
// 重活都在 ../../tools/*.ps1|*.py (今天已全部真机/干跑验证)。
// 脚本路径探测: 1) config.scriptsRoot; 2) 插件所在仓库的 tools/ 目录。

import { spawnSync } from 'node:child_process'
import { existsSync } from 'node:fs'
import { join } from 'node:path'

export const name = 'dsh-stm32-toolkit'
// TODO(校准): 官方 tool.md 的注入名与工具注册字段名
export const inject = ['tools']

const SCRIPTS = [
  'jlink_flash_verify.ps1',
  'serial_probe.py',
  'fw_size_report.py',
  'git_publish.ps1',
]

function run(script: string, args: string[], root: string): string {
  const path = join(root, 'tools', script)
  if (!existsSync(path)) return `[STM32-TOOLKIT] script not found: ${path}`
  const isPs1 = script.endsWith('.ps1')
  const res = spawnSync(
    isPs1 ? 'powershell' : 'python',
    isPs1 ? ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', path, ...args]
          : [path, ...args],
    { encoding: 'utf8', timeout: 10 * 60 * 1000 },
  )
  return (res.stdout || '') + (res.stderr || '') +
         `\n[exit: ${res.status ?? 'killed'}]`
}

export function apply(ctx: any) {
  const root = (ctx.config as { scriptsRoot?: string } | undefined)?.scriptsRoot
    ?? process.cwd()

  // TODO(校准): 以下注册结构为调研结论的预期形态, 以官方 tool.md 为准。
  ctx.tools.register({
    name: 'stm32_flash_verify',
    description: 'J-Link flash a .bin with verifybin + independent mem32 readback SHA256 compare (P1). ReadBackOnly mode never writes flash.',
    parameters: { bin: 'string', readBackOnly: 'boolean', dryRun: 'boolean' },
    execute: (args: any) => {
      const a = [args.bin]
      if (args.readBackOnly) a.push('-ReadBackOnly')
      if (args.dryRun) a.push('-DryRun')
      return run(SCRIPTS[0], a, root)
    },
  })

  ctx.tools.register({
    name: 'stm32_serial_probe',
    description: 'Capture a COM port for N seconds and parse gateway logs: noise count, RECV TYPE=0..4 frame counts, ACK closed-loop, last MQTT status publish (P2). Replay mode parses an existing log.',
    parameters: { port: 'string', duration: 'number', out: 'string', replay: 'string' },
    execute: (args: any) => {
      const a: string[] = []
      if (args.replay) { a.push('--replay', args.replay) }
      else { a.push('--port', args.port, '--duration', String(args.duration ?? 60)) }
      if (args.out) a.push('--out', args.out)
      return run(SCRIPTS[1], a, root)
    },
  })

  ctx.tools.register({
    name: 'stm32_size_budget',
    description: 'Analyze a Keil ARMCC5 .map: flash used, headroom vs MDK-Lite 32KB limit, top symbols/objects, baseline trend (P3).',
    parameters: { map: 'string', top: 'number', baseline: 'string' },
    execute: (args: any) => {
      const a: string[] = []
      if (args.map) a.push('--map', args.map)
      if (args.top) a.push('--top', String(args.top))
      if (args.baseline) a.push('--baseline', args.baseline)
      return run(SCRIPTS[2], a, root)
    },
  })

  ctx.tools.register({
    name: 'git_publish_lab',
    description: 'Import the current repo tree as a new folder into a labs-style remote main (fast-forward only, never force) or update existing files incrementally; blocks secrets.h/bin/hex/axf/map (P4).',
    parameters: { folder: 'string', files: 'string[]', message: 'string', dryRun: 'boolean' },
    execute: (args: any) => {
      const a = ['-Folder', args.folder]
      for (const f of args.files ?? []) a.push('-Files', f)
      if (args.message) a.push('-Message', args.message)
      if (args.dryRun) a.push('-DryRun')
      return run(SCRIPTS[3], a, root)
    },
  })
}
