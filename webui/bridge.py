#!/usr/bin/env python3
"""
Minimal WebSocket → raw TCP bridge for Car Controller.
No pip installs needed — pure Python 3.6+ stdlib.

Usage:
    python3 bridge.py [car_ip] [car_port] [ws_port]

Defaults:
    car_ip   = read from stdin on first WS connection (see note below)
    car_port = 3107
    ws_port  = 8080

The bridge accepts WebSocket connections on ws://0.0.0.0:<ws_port>.
Every text frame received from the browser is forwarded byte-for-byte
to the car's raw TCP socket. The car IP is sent as the first text frame
in the form:  CONNECT <ip>
After that, all frames are forwarded as-is.

In the WebUI, select "Bridge" mode and enter:  ws://<this-machine-ip>:8080
"""

import asyncio, socket, sys, struct, hashlib, base64, os

CAR_PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 3107
WS_PORT  = int(sys.argv[3]) if len(sys.argv) > 3 else 8080
DEFAULT_CAR_IP = sys.argv[1] if len(sys.argv) > 1 else None

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

def ws_handshake(raw: bytes) -> bytes:
    lines = raw.decode(errors='ignore').split('\r\n')
    key = next((l.split(': ',1)[1] for l in lines if l.lower().startswith('sec-websocket-key')), None)
    if not key: return b''
    accept = base64.b64encode(hashlib.sha1((key.strip()+GUID).encode()).digest()).decode()
    return (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
    ).encode()

def ws_decode(data: bytes):
    if len(data) < 2: return None, 0
    fin = (data[0] >> 7) & 1
    opcode = data[0] & 0x0F
    masked = (data[1] >> 7) & 1
    plen = data[1] & 0x7F
    idx = 2
    if plen == 126:
        if len(data) < 4: return None, 0
        plen = struct.unpack('>H', data[2:4])[0]; idx = 4
    elif plen == 127:
        if len(data) < 10: return None, 0
        plen = struct.unpack('>Q', data[2:10])[0]; idx = 10
    mask = data[idx:idx+4] if masked else b'\x00\x00\x00\x00'
    idx += 4 if masked else 0
    if len(data) < idx + plen: return None, 0
    payload = bytes(b ^ mask[i%4] for i,b in enumerate(data[idx:idx+plen]))
    if opcode == 8: return None, -1  # close
    return payload.decode(errors='ignore') if opcode == 1 else None, idx+plen

async def handle(reader, writer):
    buf = b''
    handshaked = False
    car_ip = DEFAULT_CAR_IP
    tcp_writer = None

    try:
        while True:
            chunk = await reader.read(4096)
            if not chunk: break
            buf += chunk

            if not handshaked:
                if b'\r\n\r\n' not in buf: continue
                hs = ws_handshake(buf)
                if not hs: break
                writer.write(hs); await writer.drain()
                handshaked = True
                buf = b''
                print(f"[WS] client connected from {writer.get_extra_info('peername')}")
                continue

            while buf:
                text, consumed = ws_decode(buf)
                if consumed == -1: return
                if consumed == 0: break
                buf = buf[consumed:]
                if text is None: continue

                if text.startswith('CONNECT '):
                    car_ip = text[8:].strip()
                    print(f"[WS] connecting to car at {car_ip}:{CAR_PORT}")
                    try:
                        tr, tw = await asyncio.open_connection(car_ip, CAR_PORT)
                        tcp_writer = tw
                        print(f"[TCP] connected to {car_ip}:{CAR_PORT}")
                    except Exception as e:
                        print(f"[TCP] failed: {e}")
                    continue

                if tcp_writer:
                    try:
                        tcp_writer.write(text.encode())
                        await tcp_writer.drain()
                    except Exception:
                        tcp_writer = None
    except Exception as e:
        print(f"[WS] error: {e}")
    finally:
        if tcp_writer:
            try: tcp_writer.close()
            except: pass
        writer.close()
        print("[WS] client disconnected")

async def main():
    srv = await asyncio.start_server(handle, '0.0.0.0', WS_PORT)
    host = socket.gethostbyname(socket.gethostname())
    print(f"Bridge running on ws://0.0.0.0:{WS_PORT}")
    print(f"  On this machine: ws://{host}:{WS_PORT}")
    if DEFAULT_CAR_IP:
        print(f"  Default car IP: {DEFAULT_CAR_IP}:{CAR_PORT}")
    print("In WebUI: select Bridge mode, enter ws://<this-machine-ip>:8080")
    async with srv: await srv.serve_forever()

asyncio.run(main())
