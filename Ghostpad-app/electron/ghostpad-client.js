const net = require("net");

const GPAD_PORT = 6967;
const PROBE_PORTS = [6967, 9021];

let socket = null;
let connectedIp = null;
let connectedPort = null;

function buildGpadBuffer(state) {
  const buf = Buffer.alloc(16);
  buf.write("GPAD", 0);
  buf.writeUInt32BE(state.buttons >>> 0, 4);
  buf[8] = state.lx ?? 128;
  buf[9] = state.ly ?? 128;
  buf[10] = state.rx ?? 128;
  buf[11] = state.ry ?? 128;
  buf[12] = state.l2 ?? 0;
  buf[13] = state.r2 ?? 0;
  return buf;
}

function connect(ip, port = GPAD_PORT) {
  return new Promise((resolve, reject) => {
    if (socket) {
      try {
        socket.destroy();
      } catch (_) {
        /* ignore */
      }
      socket = null;
      connectedIp = null;
      connectedPort = null;
    }

    const client = net.createConnection({ host: ip, port }, () => {
      client.setNoDelay(true);
      socket = client;
      connectedIp = ip;
      connectedPort = port;
      resolve({ ip, port });
    });

    client.on("error", (err) => {
      if (socket === client) {
        socket = null;
        connectedIp = null;
        connectedPort = null;
      }
      reject(err);
    });

    client.on("close", () => {
      if (socket === client) {
        socket = null;
        connectedIp = null;
        connectedPort = null;
      }
    });
  });
}

function disconnect() {
  return new Promise((resolve) => {
    if (!socket) {
      resolve({ ok: true });
      return;
    }
    try {
      socket.write(Buffer.from("DISC"));
    } catch (_) {
      /* ignore */
    }
    socket.end();
    socket = null;
    connectedIp = null;
    connectedPort = null;
    resolve({ ok: true });
  });
}

function sendPadState(state) {
  if (!socket || socket.destroyed) {
    return { ok: false, error: "Not connected" };
  }
  try {
    socket.write(buildGpadBuffer(state));
    return { ok: true };
  } catch (err) {
    return { ok: false, error: err.message };
  }
}

function getStatus() {
  return {
    isConnected: Boolean(socket && !socket.destroyed),
    ip: connectedIp,
    port: connectedPort || GPAD_PORT,
  };
}

function probeHost(ip, port, timeoutMs = 800) {
  return new Promise((resolve) => {
    const client = new net.Socket();
    let settled = false;
    const finish = (result) => {
      if (settled) return;
      settled = true;
      try {
        client.destroy();
      } catch (_) {
        /* ignore */
      }
      resolve(result);
    };

    client.setTimeout(timeoutMs);
    client.once("connect", () => finish({ ip, port, reachable: true }));
    client.once("timeout", () => finish({ ip, port, reachable: false }));
    client.once("error", () => finish({ ip, port, reachable: false }));
    client.connect(port, ip);
  });
}

function getLocalSubnetBase() {
  const os = require("os");
  const ifaces = os.networkInterfaces();
  for (const entries of Object.values(ifaces)) {
    for (const entry of entries || []) {
      if (entry.family === "IPv4" && !entry.internal) {
        const parts = entry.address.split(".");
        if (parts.length === 4) {
          return `${parts[0]}.${parts[1]}.${parts[2]}`;
        }
      }
    }
  }
  return "192.168.1";
}

async function scanNetwork(options = {}) {
  const base = options.subnet || getLocalSubnetBase();
  const ports = options.ports || PROBE_PORTS;
  const timeoutMs = options.timeoutMs || 600;
  const found = [];
  const tasks = [];

  for (let i = 1; i <= 254; i++) {
    const ip = `${base}.${i}`;
    for (const port of ports) {
      tasks.push(
        probeHost(ip, port, timeoutMs).then((result) => {
          if (result.reachable) {
            found.push({
              ip,
              port,
              service: port === 6967 ? "ghostpad" : "elfldr",
            });
          }
        })
      );
    }
  }

  await Promise.all(tasks);

  const byIp = new Map();
  for (const hit of found) {
    const existing = byIp.get(hit.ip) || { ip: hit.ip, ports: [] };
    if (!existing.ports.includes(hit.port)) {
      existing.ports.push(hit.port);
    }
    if (hit.port === 6967) existing.hasGhostpad = true;
    if (hit.port === 9021) existing.hasElfldr = true;
    byIp.set(hit.ip, existing);
  }

  return Array.from(byIp.values()).sort((a, b) => {
    const aScore = (a.hasGhostpad ? 2 : 0) + (a.hasElfldr ? 1 : 0);
    const bScore = (b.hasGhostpad ? 2 : 0) + (b.hasElfldr ? 1 : 0);
    return bScore - aScore;
  });
}

const CTRL_PORT = 6970;

function sendCtrlPacket(ip, buf, timeoutMs = 3000) {
  return new Promise((resolve) => {
    const socket = new net.Socket();
    socket.setTimeout(timeoutMs);
    socket.once("error", (err) => resolve({ ok: false, error: err.message }));
    socket.once("timeout", () => {
      socket.destroy();
      resolve({ ok: false, error: "timeout" });
    });
    socket.connect(CTRL_PORT, ip, () => {
      socket.write(buf, () => {
        socket.end();
        resolve({ ok: true });
      });
    });
  });
}

function sendType(ip, deviceType = 3) {
  const buf = Buffer.alloc(8);
  buf.write("TYPE", 0);
  buf.writeUInt32LE(deviceType >>> 0, 4);
  return sendCtrlPacket(ip, buf);
}

function sendGpadPacket(ip, buf, timeoutMs = 3000) {
  return new Promise((resolve) => {
    const s = new net.Socket();
    s.setTimeout(timeoutMs);
    s.once("error", () => resolve({ ok: false }));
    s.once("timeout", () => { s.destroy(); resolve({ ok: false }); });
    s.connect(GPAD_PORT, ip, () => {
      s.write(buf, () => { s.end(); resolve({ ok: true }); });
    });
  });
}

function disconnectVirtual(ip) {
  const gpadDisc = Buffer.alloc(16);
  gpadDisc.write("DISC", 0);
  const ctrlDisc = Buffer.from("DISC");

  const gpadPromise = (socket && !socket.destroyed)
    ? new Promise((resolve) => {
        try {
          socket.write(gpadDisc);
          resolve({ ok: true });
        } catch {
          resolve({ ok: false });
        }
      })
    : sendGpadPacket(ip, gpadDisc);

  const ctrlPromise = sendCtrlPacket(ip, ctrlDisc).catch(() => ({ ok: false }));
  return Promise.all([gpadPromise, ctrlPromise]).then(([g, c]) => ({
    ok: g.ok || c.ok,
  }));
}

module.exports = {
  GPAD_PORT,
  CTRL_PORT,
  connect,
  disconnect,
  sendPadState,
  getStatus,
  probeHost,
  scanNetwork,
  sendType,
  disconnectVirtual,
};
