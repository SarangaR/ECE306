/**
 * Car Controller Relay  —  deploy to Deno Deploy (deno.com/deploy)
 *
 * HOW TO DEPLOY (no CLI needed):
 *   1. Go to https://dash.deno.com  →  New Playground
 *   2. Paste this entire file, click Save & Deploy
 *   3. Copy your URL  (e.g. https://abc123.deno.dev)
 *   4. Set an environment variable:  RELAY_SECRET = some-long-random-string
 *      (Playgrounds: Settings → Environment Variables)
 *
 * Two WebSocket endpoints:
 *   /browser?secret=<RELAY_SECRET>   ← GitHub Pages HTML connects here
 *   /car?secret=<RELAY_SECRET>       ← ESP32 connects here on boot
 *
 * Messages are forwarded verbatim between browser and car.
 * The relay also synthesises status messages so the browser knows
 * whether the car is currently online.
 */

const SECRET     = Deno.env.get("RELAY_SECRET") ?? "change-me";
const ISOLATE_ID = crypto.randomUUID();              // stable for this process

// Per-isolate socket sets — kept in sync via BroadcastChannel below.
const browsers = new Set<WebSocket>();
const cars     = new Set<WebSocket>();

// Cross-isolate bus (Deno Deploy may spawn multiple isolates per region).
const bc = new BroadcastChannel("car-relay");

// ── helpers ───────────────────────────────────────────────────────────────────

function send(ws: WebSocket, data: string) {
  try { if (ws.readyState === WebSocket.OPEN) ws.send(data); } catch (_) {}
}

function fanout(sockets: Set<WebSocket>, data: string) {
  for (const ws of sockets) send(ws, data);
}

function relay(from: "browser" | "car", data: string) {
  const targets = from === "browser" ? cars : browsers;
  fanout(targets, data);
  // Also push to sibling isolates.
  bc.postMessage({ iid: ISOLATE_ID, to: from === "browser" ? "car" : "browser", data });
}

function carCount() {
  return [...cars].filter(c => c.readyState === WebSocket.OPEN).length;
}

// ── BroadcastChannel: messages from sibling isolates ─────────────────────────

bc.onmessage = ({ data }: MessageEvent) => {
  if (data.iid === ISOLATE_ID) return;          // skip own echoes
  const targets = data.to === "car" ? cars : browsers;
  fanout(targets, data.data);
};

// ── HTTP / WebSocket handler ──────────────────────────────────────────────────

Deno.serve((req: Request): Response => {
  const url = new URL(req.url);

  // Simple health check.
  if (req.headers.get("upgrade")?.toLowerCase() !== "websocket") {
    return new Response(
      `Car relay online  •  cars=${carCount()}  •  browsers=${browsers.size}`,
      { status: 200, headers: { "content-type": "text/plain" } }
    );
  }

  // Authenticate.
  if (url.searchParams.get("secret") !== SECRET) {
    return new Response("Unauthorized — wrong or missing secret", { status: 401 });
  }

  const type: "browser" | "car" =
    url.pathname.startsWith("/car") ? "car" : "browser";

  const { socket, response } = Deno.upgradeWebSocket(req);

  // ── Socket lifecycle ────────────────────────────────────────────────────────

  socket.onopen = () => {
    if (type === "car") {
      cars.add(socket);
      // Tell all browsers the car just came online.
      const msg = JSON.stringify({ type: "status", ok: true });
      fanout(browsers, msg);
      bc.postMessage({ iid: ISOLATE_ID, to: "browser", data: msg });
    } else {
      browsers.add(socket);
      // Tell this browser whether a car is already online.
      send(socket, JSON.stringify({ type: "status", ok: carCount() > 0 }));
    }
  };

  socket.onmessage = ({ data }) => relay(type, data as string);

  socket.onclose = () => {
    if (type === "car") {
      cars.delete(socket);
      if (carCount() === 0) {
        const msg = JSON.stringify({ type: "status", ok: false, error: "Car went offline" });
        fanout(browsers, msg);
        bc.postMessage({ iid: ISOLATE_ID, to: "browser", data: msg });
      }
    } else {
      browsers.delete(socket);
    }
  };

  socket.onerror = () => {
    cars.delete(socket);
    browsers.delete(socket);
  };

  return response;
});
