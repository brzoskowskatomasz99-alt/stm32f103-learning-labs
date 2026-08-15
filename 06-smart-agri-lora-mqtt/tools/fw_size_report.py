#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fw_size_report.py — Keil ARMCC5 .map 固件体积预算分析器（P3 插件核心）

用法:
  python tools/fw_size_report.py [--map PATH] [--top N] [--limit BYTES]
                                 [--baseline FILE] [--no-baseline]

输出:
  1) 总镜像体积 / 余量 / 限额(MDK-Lite 32KB)
  2) Top-N 大符号（Global Symbols, Size>0）
  3) Top-N 大目标文件（按对象聚合）
  4) 可选: 把本次结果追加到基线 JSON 并打印余量趋势(文本火花线)

退出码: 0=正常; 1=超限(余量<0); 2=余量低于预警阈值(<1KB)。

只依赖标准库, 无第三方依赖。
"""

import argparse
import json
import os
import re
import sys
from datetime import datetime

DEFAULT_MAP = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "MDK-ARM", "Template", "Template.map")
MDK_LITE_LIMIT = 32768
WARN_BYTES = 1024

ROM_RE = re.compile(r"Total ROM Size \(Code \+ RO Data \+ RW Data\)\s+(\d+)")
SYM_HDR_RE = re.compile(r"^\s*Global Symbols\s*$")
SYM_LINE_RE = re.compile(r"^\s{4}(\S+)\s+(0x[0-9a-fA-F]+|\d+)\s+\S+\s+(\d+)\s+(.+?)\s*$")


def parse_map(path):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        lines = fh.readlines()
    rom = None
    symbols = []
    in_symbols = False
    for line in lines:
        if rom is None:
            m = ROM_RE.search(line)
            if m:
                rom = int(m.group(1))
        if not in_symbols:
            if SYM_HDR_RE.match(line):
                in_symbols = True
            continue
        m = SYM_LINE_RE.match(line)
        if m:
            name, _value, size, objsec = m.groups()
            size = int(size)
            if size > 0:
                obj = objsec.split("(")[0].strip()
                symbols.append((name, size, obj))
    return rom, symbols


def render_baseline_trend(entries, width=24):
    """文本火花线: 每字节一个字符, 归一化到 [min,max]."""
    heads = [e["headroom"] for e in entries]
    lo, hi = min(heads), max(heads)
    if hi == lo:
        return "=" * len(heads)
    chars = "▁▂▃▄▅▆▇█"
    out = []
    for h in heads:
        idx = int((h - lo) / (hi - lo) * (len(chars) - 1))
        out.append(chars[idx])
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description="Keil ARMCC5 map 体积预算分析")
    ap.add_argument("--map", default=DEFAULT_MAP, help=".map 文件路径")
    ap.add_argument("--top", type=int, default=15, help="Top-N 符号/对象数量")
    ap.add_argument("--limit", type=int, default=MDK_LITE_LIMIT,
                    help="Flash 限额(字节), 默认 MDK-Lite 32768")
    ap.add_argument("--baseline", default=None,
                    help="基线 JSON 文件(追加本次结果并输出趋势)")
    args = ap.parse_args()

    if not os.path.isfile(args.map):
        print(f"[FWSIZE] MAP NOT FOUND: {args.map}")
        return 1

    rom, symbols = parse_map(args.map)
    if rom is None:
        print(f"[FWSIZE] Total ROM Size NOT FOUND in {args.map}")
        return 1

    headroom = args.limit - rom
    print("=" * 56)
    print(f"[FWSIZE] MAP        : {args.map}")
    print(f"[FWSIZE] FLASH USED : {rom} B ({rom/1024.0:.2f} kB)")
    print(f"[FWSIZE] LIMIT      : {args.limit} B ({args.limit/1024.0:.2f} kB)")
    print(f"[FWSIZE] HEADROOM   : {headroom} B ({headroom/1024.0:.2f} kB)")

    # Top 符号
    print("-" * 56)
    print(f"[FWSIZE] TOP {args.top} SYMBOLS:")
    for name, size, obj in sorted(symbols, key=lambda s: -s[1])[:args.top]:
        print(f"  {size:>7}  {name:<40} {obj}")

    # Top 对象
    obj_total = {}
    for _name, size, obj in symbols:
        obj_total[obj] = obj_total.get(obj, 0) + size
    print("-" * 56)
    print(f"[FWSIZE] TOP {args.top} OBJECTS:")
    for obj, size in sorted(obj_total.items(), key=lambda kv: -kv[1])[:args.top]:
        print(f"  {size:>7}  {obj}")

    # 基线
    if args.baseline:
        try:
            with open(args.baseline, "r", encoding="utf-8") as fh:
                data = json.load(fh)
        except (OSError, ValueError):
            data = {"entries": []}
        data.setdefault("entries", []).append({
            "time": datetime.now().strftime("%Y-%m-%d %H:%M"),
            "used": rom,
            "headroom": headroom,
        })
        with open(args.baseline, "w", encoding="utf-8") as fh:
            json.dump(data, fh, ensure_ascii=False, indent=2)
        trend = render_baseline_trend(data["entries"])
        print("-" * 56)
        print(f"[FWSIZE] BASELINE ({len(data['entries'])} builds): {trend}")

    print("=" * 56)
    if headroom < 0:
        print("[FWSIZE] OVER LIMIT! 固件超过 Flash 限额, 必须裁剪!")
        return 1
    if headroom < WARN_BYTES:
        print(f"[FWSIZE] WARNING: 余量不足 {WARN_BYTES} B, 加功能前先腾空间!")
        return 2
    print("[FWSIZE] OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
