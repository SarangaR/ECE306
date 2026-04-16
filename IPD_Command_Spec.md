# ESP32 TCP Command Frame Specification
**Project:** ECE 306 — Car IoT (Project 09)**Target:** MSP430FR2355 via ESP32-WROOM AT firmware  
**Transport:** TCP, port `3107`  
**Max frame size:** 31 bytes (hardware buffer limit)

---

## Connection Details

| Parameter | Value |
|-----------|-------|
| IP address | Displayed on ESP Cmds menu page (DHCP assigned) |
| Port | `3107` |
| Protocol | TCP (raw socket, no HTTP) |
| Connection type | Multi-connection (`AT+CIPMUX=1`); up to 5 simultaneous clients |
| Line ending | `\r\n` — **required**, terminates every command |

Connect with any raw TCP client (e.g. `nc`, a Python socket, or a custom app). Send one command per line. No handshake or login required; the PIN embedded in the payload is the only auth.

---

## Command Payload Format

Every command sent to the car over TCP must follow this exact format:

```
^<PIN><DIR><VALUE>\r\n
```

| Field | Width | Description |
|-------|-------|-------------|
| `^` | 1 byte | Start sentinel — ASCII `0x5E`. Must be the first recognizable character |
| `<PIN>` | 4 bytes | Authentication PIN — currently `1234`. Any command with a wrong PIN is silently dropped |
| `<DIR>` | 1 byte | Direction character — see table below |
| `<VALUE>` | 1–4 digits | Numeric argument — meaning depends on direction |
| `\r\n` | 2 bytes | Line terminator — required by the ESP32 AT framing |

**Total minimum payload:** `^1234F1\r\n` = 9 bytes  
**Total maximum useful payload:** `^1234B999\r\n` = 12 bytes (3-digit value)

---

## Direction Characters

| Char | Command | `<VALUE>` meaning | Example payload | Robot action |
|------|---------|-------------------|-----------------|--------------|
| `F` | Forward | Duration in **seconds** | `^1234F2\r\n` | Drive forward (IMU-guided) for 2 s |
| `B` | Reverse | Duration in **seconds** | `^1234B3\r\n` | Drive in reverse for 3 s |
| `R` | Turn right | Target angle in **degrees** | `^1234R90\r\n` | Turn clockwise to +90° from current heading |
| `L` | Turn left | Target angle in **degrees** | `^1234L45\r\n` | Turn counter-clockwise to −45° from current heading |

Any other direction character causes the command to be silently dropped.

---

## Behavior Notes

**Forward uses IMU guidance.** `F` schedules a `DriveStraight` command, not a raw open-loop forward. The car actively corrects heading drift using the OTOS IMU.

**R/L are absolute relative turns.** The value is a delta from the heading at the moment the command starts, not a compass bearing. `R90` means "rotate 90° clockwise from wherever you are now."

**Commands are ignored while the robot is busy.** If the car is already executing a motion command, any new incoming TCP command is dropped. Send a new command only after the previous one has had time to complete (or build a confirmation layer in your UI).

**Value is parsed as an unsigned integer** with no upper bound in the parser itself. Practical safe ranges:

| Direction | Recommended range | Notes |
|-----------|------------------|-------|
| F / B | 1 – 99 (seconds) | Values above ~30 s are rarely useful |
| R / L | 1 – 359 (degrees) | 180 is a U-turn; 360 would return to start |

A value of `0` is accepted by the parser but results in a zero-duration command (instant stop).

---

## Wire Example

Sending a 2-second forward drive from a Unix shell:

```bash
echo -e "^1234F2\r" | nc 192.168.X.X 3107
```

From Python:
```python
import socket, time

HOST = "192.168.X.X"   # IP shown on car display
PORT = 3107

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    s.sendall(b"^1234F2\r\n")  # forward 2 seconds
    time.sleep(3)
    s.sendall(b"^1234R90\r\n") # turn right 90 degrees
```

---

## What the ESP32 Wraps Around It

The ESP32 AT firmware wraps each TCP payload in an `+IPD` envelope before forwarding it to the MSP430 over UART. The MSP430 parser scans forward for `^` and ignores everything before it, so your UI only needs to care about the payload — the firmware wrapping is transparent.

Internally, the UART frame the MSP430 sees looks like:

```
+IPD,0,7:^1234F2
```

(`+IPD,<conn_id>,<byte_count>:<payload>` — the `^` and everything after is what your UI controls.)

---

## Changing the PIN

The PIN is defined in `Car/include/esp.h`:

```c
#define ESP_COMMAND_PIN  "1234"
```

It must always be exactly 4 ASCII characters. Change it before demo day.
