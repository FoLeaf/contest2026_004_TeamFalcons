#!/usr/bin/env python3
"""Local TLS server that answers immediately, to separate network from server.

If the board reaches a local server without the 8 s first-byte wait, that
wait is the remote service's own scheduling, not this network path.
"""
import socket, ssl, threading, time, json

def handle(conn):
    try:
        req=b""
        while b"\r\n\r\n" not in req:
            c=conn.recv(4096)
            if not c: return
            req+=c
        body=json.dumps({"ok":True}).encode()
        conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                     b"Content-Length: "+str(len(body)).encode()+b"\r\n"
                     b"Connection: close\r\n\r\n"+body)
    finally:
        conn.close()

ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain("/tmp/rsp01-tls/cert.pem","/tmp/rsp01-tls/key.pem")
s=socket.socket(); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
s.bind(("127.0.0.1",9446)); s.listen(8)
print("[local8s] listening", flush=True)
end=time.monotonic()+40
while time.monotonic()<end:
    s.settimeout(1.0)
    try: c,_=s.accept()
    except (socket.timeout,TimeoutError): continue
    try: tc=ctx.wrap_socket(c,server_side=True)
    except Exception: c.close(); continue
    threading.Thread(target=handle,args=(tc,),daemon=True).start()
s.close()
