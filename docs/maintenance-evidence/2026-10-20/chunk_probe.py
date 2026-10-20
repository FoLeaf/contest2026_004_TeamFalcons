#!/usr/bin/env python3
"""Reproduce the board's observed arrival pattern against a local server.

The board reports gaps of exactly 5000 ms, each adding exactly 6 bytes, then
one burst.  This server sends the same shape so the cause can be checked
directly rather than inferred:

  mode=heartbeat   one 1-byte chunk every 5 s, then the payload
  mode=slow        the payload dribbled out steadily
  mode=burst       the payload all at once after a delay

Comparing the client's gap log across these three says whether the board is
seeing a keep-alive heartbeat or genuinely slow generation.
"""

import argparse
import socket
import ssl
import sys
import threading
import time

HOST = "127.0.0.1"
PORT = 9445

PAYLOAD = b'{"shaped":"heartbeat","padding":"' + b"x" * 3400 + b'"}'


def send_chunk(conn, data):
    conn.sendall(b"%x\r\n" % len(data) + data + b"\r\n")


def serve(conn, mode):
    try:
        req = b""
        while b"\r\n\r\n" not in req:
            chunk = conn.recv(4096)
            if not chunk:
                return
            req += chunk

        head = (b"HTTP/1.1 200 OK\r\n"
                b"Content-Type: application/json\r\n"
                b"Transfer-Encoding: chunked\r\n"
                b"Connection: close\r\n\r\n")
        conn.sendall(head)

        if mode == "heartbeat":
            # One byte every 5 s: the shape the board logs as +6 bytes and
            # 5000 ms idle, repeated.
            for _ in range(4):
                send_chunk(conn, b"X")
                time.sleep(5.0)
            send_chunk(conn, PAYLOAD)
        elif mode == "slow":
            step = len(PAYLOAD) // 20
            for i in range(0, len(PAYLOAD), step):
                send_chunk(conn, PAYLOAD[i:i + step])
                time.sleep(0.3)
        else:
            time.sleep(3.0)
            send_chunk(conn, PAYLOAD)

        conn.sendall(b"0\r\n\r\n")
    except Exception as exc:  # noqa: BLE001
        print(f"[chunk] error: {exc}", flush=True)
    finally:
        try:
            conn.close()
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", default="heartbeat",
                    choices=["heartbeat", "slow", "burst"])
    ap.add_argument("--seconds", type=float, default=90)
    args = ap.parse_args()

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain("/tmp/rsp01-tls/cert.pem", "/tmp/rsp01-tls/key.pem")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((HOST, PORT))
    sock.listen(4)
    print(f"[chunk] {args.mode} on {HOST}:{PORT}", flush=True)

    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        try:
            sock.settimeout(1.0)
            conn, _ = sock.accept()
        except (socket.timeout, TimeoutError):
            continue
        try:
            tls_conn = ctx.wrap_socket(conn, server_side=True)
        except Exception as exc:  # noqa: BLE001
            print(f"[chunk] handshake failed: {exc}", flush=True)
            conn.close()
            continue
        threading.Thread(target=serve, args=(tls_conn, args.mode),
                         daemon=True).start()

    sock.close()


if __name__ == "__main__":
    main()
