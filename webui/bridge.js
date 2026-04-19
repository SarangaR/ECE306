'use strict';

/**
 * bridge.js — WebSocket ↔ TCP bridge for the car controller.
 *
 * Run this locally while the car is on the same network:
 *   node bridge.js
 *
 * The HTML is served statically (e.g. GitHub Pages) and connects to this
 * bridge at ws://localhost:8080.  Browsers allow ws://localhost from HTTPS
 * pages because localhost is a trusted origin per the Secure Contexts spec.
 *
 * Message protocol (browser → bridge):
 *   { type: "connect",    ip: "x.x.x.x" }
 *   { type: "speed",      fwd: float, turn: float }   // -100..100
 *   { type: "disconnect" }
 *
 * Message protocol (bridge → browser):
 *   { type: "status", ok: true,  ip: "x.x.x.x" }
 *   { type: "status", ok: false, error?: "msg"  }
 *
 * Car command frame sent over TCP:
 *   ^<PIN>C<fwd>,<turn>\r\n
 */

const net = require('net');
const { WebSocketServer } = require('ws');

// ── Configuration ─────────────────────────────────────────────────────────────
const WS_PORT      = 8080;
const CAR_TCP_PORT = 3107;
const PIN          = '1234';

// ── WebSocket server ──────────────────────────────────────────────────────────
const wss = new WebSocketServer({ port: WS_PORT });

function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)); }

wss.on('connection', (ws, req) => {
  const origin = req.headers.origin || '(no origin)';
  console.log(`[WS]  browser connected  origin=${origin}`);

  let tcp      = null;
  let tcpReady = false;

  const toWS  = (msg) => { try { ws.send(JSON.stringify(msg)); } catch (_) {} };
  const toCar = (raw) => { if (tcp && tcpReady) tcp.write(raw); };

  function closeTCP() {
    if (tcp) { try { tcp.destroy(); } catch (_) {} tcp = null; }
    tcpReady = false;
  }

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
          console.log(`[TCP] connected  ${msg.ip.trim()}:${CAR_TCP_PORT}`);
        });

        tcp.on('timeout', () => {
          console.log('[TCP] timed out');
          closeTCP();
          toWS({ type: 'status', ok: false, error: 'Connection timed out' });
        });

        tcp.on('error', (err) => {
          console.log(`[TCP] error: ${err.message}`);
          closeTCP();
          toWS({ type: 'status', ok: false, error: err.message });
        });

        tcp.on('close', () => {
          if (tcpReady) {
            console.log('[TCP] closed by car');
            closeTCP();
            toWS({ type: 'status', ok: false, error: 'Connection closed by car' });
          }
        });
        break;
      }

      case 'speed': {
        const fwd  = clamp(+(msg.fwd  ?? 0), -100, 100).toFixed(1);
        const turn = clamp(+(msg.turn ?? 0), -100, 100).toFixed(1);
        toCar(`^${PIN}C${fwd},${turn}\r\n`);
        break;
      }

      case 'disconnect':
        toCar(`^${PIN}C0.0,0.0\r\n`);
        closeTCP();
        toWS({ type: 'status', ok: false });
        break;
    }
  });

  ws.on('close', () => {
    toCar(`^${PIN}C0.0,0.0\r\n`);
    closeTCP();
    console.log('[WS]  browser disconnected');
  });

  ws.on('error', (err) => {
    console.log(`[WS]  error: ${err.message}`);
    closeTCP();
  });
});

wss.on('listening', () => {
  console.log('');
  console.log('  ┌─────────────────────────────────────────────┐');
  console.log(`  │  Car bridge  →  ws://localhost:${WS_PORT}         │`);
  console.log('  │                                             │');
  console.log('  │  Open the GitHub Pages URL in your browser │');
  console.log('  │  then enter the car IP and tap Connect.    │');
  console.log('  └─────────────────────────────────────────────┘');
  console.log('');
});
