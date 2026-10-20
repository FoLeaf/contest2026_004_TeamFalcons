#!/usr/bin/env python3
"""RSP-02 PC-side comparator.

Sends the same request shape the board sends, from this computer, and records
the same phases the firmware records.  The point is to separate "the server
or the shared network takes this long" from "the board's own stack takes this
long": if both sides show the same ttfb and body time, the wait is upstream.

The credential is read from the board's provisioning files only for a local
test host; nothing is printed.  When no credential file is present the probe
runs unauthenticated and reports the HTTP status, which is enough for timing
comparison against a 401.

Usage:
  python3 rsp02_pc_probe.py --case short|point|long --repeat 3 [--log FILE]
"""

import argparse
import json
import os
import ssl
import socket
import statistics
import sys
import time

HOST = "token-plan-cn.xiaomimimo.com"
PORT = 443
PATH = "/v1/chat/completions"
MODEL = "mimo-v2.5"

# Same three prompts the board is asked, so the request bodies are comparable.
# The tool schema is not reproduced here: the point of the comparison is the
# server's generation time for a similar output size, not tool selection.
PROMPTS = {
    "short": "你好，请用一句话介绍你自己",
    "point": "告诉我UPS负载的值",
    "long": "按 operations_report Skill 生成今日运营日报",
}

# Output budget matching AGENT_LLM_MAX_TOKENS_OPENAI in the firmware.
MAX_TOKENS = 16384


def read_key(contest_root):
    """Read the credential the board uses, without printing it."""

    key_path = os.path.join(contest_root, "secrets", "agent_llm.key")
    try:
        with open(key_path, "r", encoding="utf-8") as handle:
            return handle.read().strip()
    except OSError:
        return None


def mono():
    return time.monotonic()


def request_once(prompt, api_key):
    """One request; returns a record of phases and sizes."""

    body = {
        "model": MODEL,
        "max_completion_tokens": MAX_TOKENS,
        "messages": [
            {"role": "system",
             "content": "You are VelaGuard, an operations assistant. "
                        "Answer in Chinese. Use tools when the question needs "
                        "live data."},
            {"role": "user", "content": prompt},
        ],
    }
    payload = json.dumps(body, ensure_ascii=False).encode("utf-8")

    rec = {
        "req_bytes": len(payload),
        "t0": mono(),
        "first_byte": None,
        "done": None,
        "resp_bytes": 0,
        "status": 0,
        "gap": None,
    }

    # Full verification: the comparison target is the real endpoint, and a
    # test that disables certificate checks is not measuring the same
    # connection the board makes.
    ctx = ssl.create_default_context()

    head = (
        f"POST {PATH} HTTP/1.1\r\n"
        f"Host: {HOST}\r\n"
        f"Content-Type: application/json\r\n"
        f"Content-Length: {len(payload)}\r\n"
        f"Connection: close\r\n"
    ).encode()
    if api_key:
        head += f"Authorization: Bearer {api_key}\r\n".encode()
    head += b"\r\n"

    raw = b""
    with socket_create(ctx) as sock:
        sock.sendall(head + payload)
        sent = mono()
        rec["send_ms"] = int((sent - rec["t0"]) * 1000)

        # Read in pieces so the arrival pattern, not just the total, is
        # visible: a steady stream and a long silence look nothing alike.
        while True:
            chunk = sock.recv(16384)
            now = mono()
            if not chunk:
                break
            if rec["first_byte"] is None:
                rec["first_byte"] = now - sent
            raw += chunk
            rec["resp_bytes"] += len(chunk)
        rec["done"] = mono()

    rec["ttfb_ms"] = int((rec["first_byte"] or 0) * 1000)
    rec["body_ms"] = int((rec["done"] - sent - (rec["first_byte"] or 0)) * 1000)
    rec["total_ms"] = int((rec["done"] - rec["t0"]) * 1000)

    head_end = raw.find(b"\r\n\r\n")
    if head_end > 0:
        status_line = raw[:head_end].split(b"\r\n", 1)[0].decode("latin-1")
        parts = status_line.split(" ")
        if len(parts) > 1 and parts[1].isdigit():
            rec["status"] = int(parts[1])

    body_bytes = raw[head_end + 4:] if head_end > 0 else b""
    head_text = raw[:head_end].decode("latin-1", "replace") if head_end > 0 else ""
    if "chunked" in head_text.lower():
        body_bytes = decode_chunked(body_bytes)

    try:
        parsed = json.loads(body_bytes)
        usage = parsed.get("usage") or {}
        rec["prompt_tokens"] = usage.get("prompt_tokens")
        rec["completion_tokens"] = usage.get("completion_tokens")
        choice = (parsed.get("choices") or [{}])[0]
        rec["finish"] = choice.get("finish_reason")
        msg = choice.get("message") or {}
        rec["text_bytes"] = len((msg.get("content") or "").encode())
        rec["reasoning_bytes"] = len(
            (msg.get("reasoning_content") or "").encode())
        calls = msg.get("tool_calls") or []
        rec["toolargs_bytes"] = sum(
            len((c.get("function", {}).get("arguments") or "").encode())
            for c in calls)
        rec["calls"] = len(calls)
    except (ValueError, UnicodeDecodeError):
        rec["raw_bytes"] = len(body_bytes)

    return rec


def decode_chunked(data):
    """Decode Transfer-Encoding: chunked in place; returns the payload.

    The endpoint answers chunked.  Without this the sizes and the JSON parse
    are both wrong, which is how an earlier run reported a 22 s body for a
    773-byte payload.
    """

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
        start = nl + 2
        out += data[start:start + size]
        pos = start + size + 2
    return bytes(out)


def socket_create(ctx):
    raw = socket.create_connection((HOST, PORT), timeout=300)
    return ctx.wrap_socket(raw, server_hostname=HOST)


def summarize(label, records):
    def col(name):
        vals = [r.get(name) for r in records if r.get(name) is not None]
        if not vals:
            return "na"
        return f"{statistics.median(vals):.0f}"

    ok = [r for r in records if r.get("status") == 200]
    print(f"\n== {label} (n={len(records)}, ok={len(ok)}) ==")
    print(f"  req_bytes   median {col('req_bytes')}")
    print(f"  ttfb_ms     median {col('ttfb_ms')}")
    print(f"  body_ms     median {col('body_ms')}")
    print(f"  total_ms    median {col('total_ms')}")
    print(f"  resp_bytes  median {col('resp_bytes')}")
    print(f"  completion  median {col('completion_tokens')}")
    print(f"  finish      {[r.get('finish') for r in records]}")
    for r in records:
        print(f"    status={r.get('status')} ttfb={r.get('ttfb_ms')}ms "
              f"body={r.get('body_ms')}ms resp={r.get('resp_bytes')}B "
              f"completion={r.get('completion_tokens')} "
              f"text={r.get('text_bytes')}B reasoning={r.get('reasoning_bytes')}B "
              f"toolargs={r.get('toolargs_bytes')}B calls={r.get('calls')}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--case", default="short",
                    choices=["short", "point", "long"])
    ap.add_argument("--repeat", type=int, default=3)
    ap.add_argument("--contest-root",
                    default="/home/hello19y/openvela/contest2026_004_TeamFalcons")
    ap.add_argument("--log", default="")
    args = ap.parse_args()

    key = read_key(args.contest_root)
    if key is None:
        print("[probe] no credential file; requests will be unauthenticated",
              file=sys.stderr)

    records = []
    for i in range(args.repeat):
        try:
            rec = request_once(PROMPTS[args.case], key)
        except Exception as exc:  # noqa: BLE001 - probe
            rec = {"error": repr(exc)}
        records.append(rec)
        print(f"[probe] {args.case} #{i + 1}: {rec}", file=sys.stderr)
        time.sleep(2)

    summarize(args.case, records)

    if args.log:
        with open(args.log, "w", encoding="utf-8") as handle:
            json.dump(records, handle, ensure_ascii=False, indent=2)
        print(f"\n[saved] {args.log}")


if __name__ == "__main__":
    main()
