'use strict';

const http = require('http');
const fs   = require('fs');
const path = require('path');
const net  = require('net');
const { WebSocketServer } = require('ws');

// ── Configuration ─────────────────────────────────────────────────────────────

const HTTP_PORT    = 8080;
const CAR_TCP_PORT = 3107;
const PIN          = '1234';

// ── HTTP server (serves index.html for every GET) ─────────────────────────────

const httpServer = http.createServer((req, res) => {
  const file = path.join(__dirname, 'index.html');
  fs.readFile(file, (err, data) => {
    if (err) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('index.html not found');
      return;
    }
    // ngrok-skip-browser-warning bypasses ngrok's interstitial click-through.
    res.writeHead(200, {
      'Content-Type': 'text/html; charset=utf-8',
      'ngrok-skip-browser-warning': 'true',
    });
    res.end(data);
  });
});

// ── WebSocket server ──────────────────────────────────────────────────────────
//
// Browser  ──WS──►  server.js  ──TCP──►  ESP32 / car
//
// Messages from browser → server:
//   { type: "connect",  ip: "x.x.x.x" }
//   { type: "speed",    fwd: float, turn: float }   // -100..100 each
//   { type: "disconnect" }
//
// Messages from server → browser:
//   { type: "status",   ok: true,  ip: "x.x.x.x" }
//   { type: "status",   ok: false, error?: "msg"  }
//
// The "speed" message is translated directly into a C command frame:
//   ^<PIN>C<fwd>,<turn>\r\n
//
// The car firmware expects these to arrive at ≤ 50 ms intervals; if no
// command arrives for ~300 ms its watchdog stops the motors automatically.

const wss = new WebSocketServer({ server: httpServer });

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

wss.on('connection', (ws) => {
  let tcp      = null;   // net.Socket to the car
  let tcpReady = false;

  // ── helpers ────────────────────────────────────────────────────────────────

  const toWS  = (msg) => { try { ws.send(JSON.stringify(msg)); } catch (_) {} };
  const toCar = (raw) => { if (tcp && tcpReady) tcp.write(raw); };

  function closeTCP() {
    if (tcp) { try { tcp.destroy(); } catch (_) {} tcp = null; }
    tcpReady = false;
  }

  // ── message handler ────────────────────────────────────────────────────────

  ws.on('message', (raw) => {
    let msg;
    try { msg = JSON.parse(raw); } catch (_) { return; }

    switch (msg.type) {

      case 'connect': {
        closeTCP();
        if (!msg.ip || typeof msg.ip !== 'string') {
          toWS({ type: 'status', ok: false, error: 'No IP provided' });
          return;
        }
        tcp = new net.Socket();
        tcp.setTimeout(5000);
        tcp.setKeepAlive(true, 3000);

        tcp.connect(CAR_TCP_PORT, msg.ip.trim(), () => {
          tcpReady = true;
          tcp.setTimeout(0);
          toWS({ type: 'status', ok: true, ip: msg.ip.trim() });
          console.log(`[TCP] Connected to ${msg.ip.trim()}:${CAR_TCP_PORT}`);
        });

        tcp.on('timeout', () => {
          console.log('[TCP] Connection timed out');
          closeTCP();
          toWS({ type: 'status', ok: false, error: 'Connection timed out' });
        });

        tcp.on('error', (err) => {
          console.log(`[TCP] Error: ${err.message}`);
          closeTCP();
          toWS({ type: 'status', ok: false, error: err.message });
        });

        tcp.on('close', () => {
          if (tcpReady) {
            console.log('[TCP] Connection closed by car');
            closeTCP();
            toWS({ type: 'status', ok: false, error: 'Connection closed by car' });
          }
        });
        break;
      }

      case 'speed': {
        // Forward velocity (+ = forward, - = reverse) and turn rate
        // (+ = right, - = left), both in -100..100 percent.
        const fwd  = clamp(+(msg.fwd  ?? 0), -100, 100).toFixed(1);
        const turn = clamp(+(msg.turn ?? 0), -100, 100).toFixed(1);
        toCar(`^${PIN}C${fwd},${turn}\r\n`);
        break;
      }

      case 'disconnect':
        // Send a final stop before dropping the TCP connection.
        toCar(`^${PIN}C0.0,0.0\r\n`);
        closeTCP();
        toWS({ type: 'status', ok: false });
        break;
    }
  });

  ws.on('close', () => {
    // Browser closed — send stop if still connected so the car doesn't
    // coast until the firmware watchdog fires.
    toCar(`^${PIN}C0.0,0.0\r\n`);
    closeTCP();
    console.log('[WS] Browser disconnected');
  });

  ws.on('error', (err) => {
    console.log(`[WS] Error: ${err.message}`);
    closeTCP();
  });

  console.log('[WS] Browser connected');
});

// ── Start ─────────────────────────────────────────────────────────────────────

httpServer.listen(HTTP_PORT, () => {
  console.log('');
  console.log('  ╔══════════════════════════════════════╗');
  console.log(`  ║  Car controller  →  http://localhost:${HTTP_PORT}  ║`);
  console.log('  ║  ngrok expose:                       ║');
  console.log(`  ║    ngrok http ${HTTP_PORT}                  ║`);
  console.log('  ╚══════════════════════════════════════╝');
  console.log('');
});
