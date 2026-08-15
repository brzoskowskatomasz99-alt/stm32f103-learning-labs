#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
serial_probe.py — 串口抓包 + 网关日志协议解析器（P2 插件核心）

用法:
  python tools/serial_probe.py --port COM12 --baud 115200 --duration 120
                              [--out LOG] [--json] [--match REGEX] [--quiet]

行为:
  1) 打开串口抓取 duration 秒(缺省 60), 全量落盘 UTF-8 日志;
  2) 解析网关日志: [UI]/[CMDLINK]/[ALARMREG]/[FW] 前缀行分类计数、
     LoRa 帧类型计数(RECV TYPE=0..4)、噪声计数(INVALID|DROP)、
     最后一条 MQTT status publish JSON、ACK 闭环判定;
  3) 输出一行摘要报表; --json 时另输出机器可读 JSON(供插件消费)。

依赖: pyserial (pip install pyserial)。其余标准库。
退出码: 0=正常捕获; 1=串口打不开; 2=捕获期间无任何数据。
"""

import argparse
import json
import os
import re
import sys
import time

try:
    import serial
except ImportError:
    print("[PROBE] pyserial not installed: pip install pyserial")
    sys.exit(3)

PREFIXES = ("[UI]", "[CMDLINK]", "[ALARMREG]", "[FW]", "[TABLE]",
            "[LINK]", "[GWDATA]", "[CTRL]")
FRAME_RE = re.compile(r"RECV TYPE=(\d)")
NOISE_RE = re.compile(r"INVALID|DROP")
STATUS_RE = re.compile(r'MQTT status publish:\s*(\{.*\})')
CMD_ID_RE = re.compile(r"\bID=(\d+)")
ACK_OK_RE = re.compile(r"RESULT=0\s+ACTUAL=(\d+)")
ACK_ERR_RE = re.compile(r"RESULT=1")


def capture(port, baud, duration, logfile):
    ser = serial.Serial(port, baud, timeout=0.2)
    ser.reset_input_buffer()
    start = time.monotonic()
    lines = []
    buf = ""
    try:
        while time.monotonic() - start < duration:
            data = ser.read(2048)
            if data:
                try:
                    buf += data.decode("utf-8", errors="replace")
                except Exception:
                    pass
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.rstrip("\r")
                    if line:
                        lines.append(line)
    finally:
        ser.close()
    if logfile:
        os.makedirs(os.path.dirname(os.path.abspath(logfile)), exist_ok=True)
        with open(logfile, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines))
    return lines


def analyze(lines):
    prefix_count = {}
    for p in PREFIXES:
        n = sum(1 for l in lines if l.startswith(p))
        if n:
            prefix_count[p] = n
    frame_count = {}
    for l in lines:
        m = FRAME_RE.search(l)
        if m:
            t = int(m.group(1))
            frame_count[t] = frame_count.get(t, 0) + 1
    noise = sum(1 for l in lines if NOISE_RE.search(l))
    last_status = None
    for l in lines:
        m = STATUS_RE.search(l)
        if m:
            last_status = m.group(1)
    # ACK 闭环: 每个 ID 发命令后是否在 5 行内收到 RESULT=0/1
    pending = {}
    closed_ok = 0
    closed_err = 0
    open_ids = []
    for l in lines:
        m = CMD_ID_RE.search(l)
        if m and ("SEND" in l or "RETRY=0" in l):
            pending[int(m.group(1))] = 5
        for cid in list(pending):
            if pending[cid] <= 0:
                open_ids.append(cid)
                del pending[cid]
                continue
            if ACK_OK_RE.search(l) or ACK_ERR_RE.search(l):
                if ACK_OK_RE.search(l):
                    closed_ok += 1
                else:
                    closed_err += 1
                del pending[cid]
            else:
                pending[cid] -= 1
    for cid in list(pending):
        open_ids.append(cid)
    return {
        "lines": len(lines),
        "prefix_count": prefix_count,
        "frame_count": frame_count,
        "noise": noise,
        "last_status": last_status,
        "ack_closed_ok": closed_ok,
        "ack_closed_err": closed_err,
        "ack_open": open_ids,
    }


def main():
    ap = argparse.ArgumentParser(description="串口抓包 + 网关协议解析")
    ap.add_argument("--port", default=None, help="COM 口, 如 COM12 (--replay 时不需要)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--duration", type=float, default=60.0, help="抓取秒数")
    ap.add_argument("--out", default=None, help="日志落盘路径")
    ap.add_argument("--json", action="store_true", help="额外输出 JSON 摘要")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--replay", default=None,
                    help="离线模式: 解析已有日志文件而不开串口")
    args = ap.parse_args()

    if args.replay:
        with open(args.replay, "r", encoding="utf-8",
                  errors="replace") as fh:
            lines = [l.rstrip("\r\n") for l in fh if l.strip()]
        if not lines:
            print(f"[PROBE] REPLAY EMPTY: {args.replay}")
            return 2
        r = analyze(lines)
        print(f"[PROBE] REPLAY {args.replay}: LINES={r['lines']} "
              f"NOISE={r['noise']} FRAMES={json.dumps(r['frame_count'])} "
              f"ACK_OK={r['ack_closed_ok']} ACK_ERR={r['ack_closed_err']} "
              f"ACK_OPEN={r['ack_open']}")
        if r["last_status"]:
            print(f"[PROBE] LAST_STATUS: {r['last_status']}")
        if r["prefix_count"]:
            print(f"[PROBE] PREFIX: {json.dumps(r['prefix_count'])}")
        if args.json:
            print(json.dumps(r, ensure_ascii=False, indent=2))
        return 0

    if not args.port:
        print("[PROBE] --port required without --replay")
        return 1

    try:
        lines = capture(args.port, args.baud, args.duration, args.out)
    except serial.SerialException as exc:
        print(f"[PROBE] OPEN FAIL {args.port}: {exc}")
        return 1

    if not lines:
        print(f"[PROBE] NO DATA on {args.port} in {args.duration:.0f}s")
        return 2

    r = analyze(lines)
    if not args.quiet:
        print(f"[PROBE] {args.port}@{args.baud} {args.duration:.0f}s: "
              f"LINES={r['lines']} NOISE={r['noise']} "
              f"FRAMES={json.dumps(r['frame_count'])} "
              f"ACK_OK={r['ack_closed_ok']} ACK_ERR={r['ack_closed_err']} "
              f"ACK_OPEN={r['ack_open']}")
        if r["last_status"]:
            print(f"[PROBE] LAST_STATUS: {r['last_status']}")
        if r["prefix_count"]:
            print(f"[PROBE] PREFIX: {json.dumps(r['prefix_count'])}")
    if args.json:
        print(json.dumps(r, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
