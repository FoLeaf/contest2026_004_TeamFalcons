#!/usr/bin/env python3
"""RSP-03 part 1: reasoning on/off paired comparison.

Fixed: model, prompts, tool schema, output budget, message history.
Varied: reasoning_effort only, alternating within the same time window so
network drift affects both arms equally.

Correctness is judged on tool selection, not on prose: a run is correct when
the model picks the expected read-only tool and passes a query that names the
expected point.  That is what the product depends on.

Reasoning parameters are taken from what the endpoint actually accepts, not
from guesses: reasoning_effort=none was verified to return
reasoning_content of zero length, while enable_thinking and temperature had
no effect.

Usage:
  python3 rsp03_reasoning_ab.py --pairs 10 --log out.json
"""

import argparse
import json
import socket
import ssl
import statistics
import sys
import time

HOST = "token-plan-cn.xiaomimimo.com"
PORT = 443
PATH = "/v1/chat/completions"
MODEL = "mimo-v2.5"
MAX_TOKENS = 4096

# Same schemas the board sends (app/velaguard/vg_agent_tools.c).
POINT_TOOL = {
    "name": "vg_point_read",
    "description": "读取本机已确认点表中一个点位的实时值、单位、采样新鲜度和阈值。"
                   "query 可以写点位 id（如 ups_load）或中文点名（如 UPS负载）；"
                   "留空则列出全部点位。只读，不发起总线通信。",
    "input_schema": {
        "type": "object",
        "properties": {
            "query": {"type": "string",
                      "description": "点位 id 或中文点名，例如 ups_load 或 UPS负载；"
                                     "留空列出全部点位"}
        },
        "required": [],
    },
}

REPORT_TOOL = {
    "name": "vg_run_report",
    "description": "读取本机自本次上电以来的运行报告，含通信质量、点位在线和异常时间线三节。"
                   "数字由板上统计产生，不需要参数。只读。",
    "input_schema": {"type": "object", "properties": {}, "required": []},
}

TOOLS = [{"type": "function", "function": POINT_TOOL},
         {"type": "function", "function": REPORT_TOOL}]

SYSTEM = (
    "你是 VelaGuard，运行在 openvela 设备上的运营助手。"
    "回答使用中文。需要实时数据时调用只读工具，不要编造数值。"
    "设备点表包含 ups_load（UPS负载）、temp（温度）、humid（湿度）。"
)

# Each case names the tool and what the arguments must mention.
CASES = [
    {"prompt": "告诉我UPS负载的值", "tool": "vg_point_read", "need": "load"},
    {"prompt": "现在温度多少度", "tool": "vg_point_read", "need": "temp"},
    {"prompt": "湿度是多少", "tool": "vg_point_read", "need": "humid"},
    {"prompt": "给我截止目前的运行报告", "tool": "vg_run_report", "need": None},
    {"prompt": "总线通信质量怎么样", "tool": "vg_run_report", "need": None},
]


def mono():
    return time.monotonic()


def decode_chunked(data):
    out = bytearray()
    pos = 0
    while pos < len(data):
        nl = data.find(b"\r\n", pos)
        if nl < 0:
            break
        try:
            size = int(data[pos:nl].split(b";")[0], 16)
        except ValueError:
            break
        if size == 0:
            break
        out += data[nl + 2:nl + 2 + size]
        pos = nl + 2 + size + 2
    return bytes(out)


def call(api_key, prompt, reasoning_mode):
    body = {
        "model": MODEL,
        "max_completion_tokens": MAX_TOKENS,
        "messages": [{"role": "system", "content": SYSTEM},
                     {"role": "user", "content": prompt}],
        "tools": TOOLS,
    }
    if reasoning_mode == "none":
        body["reasoning_effort"] = "none"

    payload = json.dumps(body, ensure_ascii=False).encode("utf-8")

    ctx = ssl.create_default_context()
    raw = socket.create_connection((HOST, PORT), timeout=300)
    sock = ctx.wrap_socket(raw, server_hostname=HOST)

    head = (f"POST {PATH} HTTP/1.1\r\nHost: {HOST}\r\n"
            f"Content-Type: application/json\r\n"
            f"Authorization: Bearer {api_key}\r\n"
            f"Content-Length: {len(payload)}\r\n"
            f"Connection: close\r\n\r\n").encode()

    t0 = mono()
    sock.sendall(head + payload)
    sent = mono()
    first = None
    data = b""
    while True:
        chunk = sock.recv(16384)
        if not chunk:
            break
        if first is None:
            first = mono() - sent
        data += chunk
    done = mono()
    sock.close()

    rec = {
        "reasoning_mode": reasoning_mode,
        "prompt": prompt,
        "ttfb_ms": round((first or 0) * 1000),
        "total_ms": round((done - t0) * 1000),
        "resp_bytes": len(data),
        "req_bytes": len(payload),
    }

    head_end = data.find(b"\r\n\r\n")
    body_bytes = data[head_end + 4:]
    if b"chunked" in data[:head_end].lower():
        body_bytes = decode_chunked(body_bytes)

    try:
        parsed = json.loads(body_bytes)
    except ValueError:
        rec["error"] = "parse"
        return rec

    usage = parsed.get("usage") or {}
    choice = (parsed.get("choices") or [{}])[0]
    msg = choice.get("message") or {}
    calls = msg.get("tool_calls") or []

    rec["completion_tokens"] = usage.get("completion_tokens")
    rec["prompt_tokens"] = usage.get("prompt_tokens")
    rec["reasoning_bytes"] = len((msg.get("reasoning_content") or "").encode())
    rec["finish"] = choice.get("finish_reason")

    # MiMo returns tool calls as XML inside `content` with tool_calls null.
    # The firmware has parse_xml_tool_calls() for this; the harness needs the
    # same reading or every tool call looks like a plain-text answer.
    text = msg.get("content")

    rec["text_bytes"] = len((text or "").encode())

    if calls:
        rec["tool_choice"] = calls[0]["function"]["name"]
        rec["tool_args"] = (calls[0]["function"].get("arguments") or "")[:200]
    else:
        name, args = parse_xml_tool_call(text or "")
        rec["tool_choice"] = name
        rec["tool_args"] = args[:200]

    return rec


def parse_xml_tool_call(text):
    """Read <tool_call><function=NAME><parameter=K>V</parameter>...</function>."""

    import re

    m = re.search(r"<function\s*=\s*([A-Za-z0-9_]+)\s*>", text)
    if not m:
        return None, ""
    name = m.group(1)
    params = re.findall(
        r"<parameter\s*=\s*([A-Za-z0-9_]+)\s*>(.*?)</parameter>", text,
        re.S)
    return name, json.dumps({k: v.strip() for k, v in params},
                            ensure_ascii=False)


def judge(rec, case):
    """Correct when the expected tool was picked and the query names the point."""

    if rec.get("tool_choice") != case["tool"]:
        return False
    if case["need"] is None:
        return True
    args = (rec.get("tool_args") or "").lower()
    return case["need"] in args


def stats(values):
    if not values:
        return {"n": 0}
    return {
        "n": len(values),
        "median": statistics.median(values),
        "max": max(values),
        "min": min(values),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs", type=int, default=10)
    ap.add_argument("--log", default="")
    ap.add_argument("--key-file",
                    default="/home/hello19y/openvela/contest2026_004_TeamFalcons"
                            "/secrets/agent_llm.key")
    args = ap.parse_args()

    api_key = open(args.key_file, encoding="utf-8").read().strip()

    rows = []
    for i in range(args.pairs):
        case = CASES[i % len(CASES)]
        # Alternate which arm goes first so ordering does not favour one.
        order = ["on", "none"] if i % 2 == 0 else ["none", "on"]
        for mode in order:
            try:
                rec = call(api_key, case["prompt"], mode)
            except Exception as exc:  # noqa: BLE001
                rec = {"reasoning_mode": mode, "prompt": case["prompt"],
                       "error": repr(exc)}
            rec["correct"] = judge(rec, case) if "error" not in rec else False
            rec["pair"] = i + 1
            rows.append(rec)
            print(f"pair {i + 1:2d} {mode:4s} {case['prompt'][:16]:18s} "
                  f"total={rec.get('total_ms')}ms tok={rec.get('completion_tokens')} "
                  f"tool={rec.get('tool_choice')} correct={rec['correct']}",
                  file=sys.stderr)

    print("\n== RSP-03 reasoning A/B ==")
    for mode in ("on", "none"):
        arm = [r for r in rows if r.get("reasoning_mode") == mode]
        ok = [r for r in arm if "total_ms" in r]
        s = stats([r["total_ms"] for r in ok])
        t = stats([r["ttfb_ms"] for r in ok])
        tok = stats([r["completion_tokens"] for r in ok
                     if r.get("completion_tokens") is not None])
        correct = sum(1 for r in arm if r.get("correct"))
        print(f"\n{mode:5s} n={len(arm)} ok={len(ok)} correct={correct}/{len(arm)}")
        print(f"  total_ms  median={s.get('median')} max={s.get('max')}")
        print(f"  ttfb_ms   median={t.get('median')}")
        print(f"  tokens    median={tok.get('median')}")

    if args.log:
        with open(args.log, "w", encoding="utf-8") as handle:
            json.dump(rows, handle, ensure_ascii=False, indent=2)
        print(f"\n[saved] {args.log}")


if __name__ == "__main__":
    main()
