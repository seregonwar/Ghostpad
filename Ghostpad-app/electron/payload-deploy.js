const net = require("net");
const fs = require("fs");

const GPAD_PORT = 6967;
const ELF_LOAD_PORT = 9021;
const CTRL_PORT = 6970;
const KLOG_PORT = 3232;

const RE_DEV_PHYS =
  /DEVICE_ADDED.*?deviceId=0x([0-9a-f]+).*?capabilityBattery:1/i;
const RE_DEV_VIRT =
  /DEVICE_ADDED.*?deviceId=0x([0-9a-f]+).*?capabilityBattery:0/i;
const RE_DEV_VIRT2 =
  /DeviceAdded.*?deviceId=0x([0-9a-f]+).*?userId=0xffffffff/i;
const RE_OWNER =
  /DEVICE_OWNER_CHANGED.*?deviceId=0x([0-9a-f]+).*?userId=0x([0-9a-f]+)/i;
const RE_ADOPTED = /adopted deviceId for direct/i;

let activeWatcher = null;
let lastDeployStatus = {
  phase: "idle",
  message: "Ready",
  host: null,
  at: null,
};

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function setDeployStatus(phase, message, host) {
  lastDeployStatus = {
    phase,
    message,
    host: host ?? lastDeployStatus.host,
    at: new Date().toISOString(),
  };
  return lastDeployStatus;
}

function portOpen(host, port, timeoutMs = 1000) {
  return new Promise((resolve) => {
    const socket = new net.Socket();
    let settled = false;
    const finish = (result) => {
      if (settled) return;
      settled = true;
      try {
        socket.destroy();
      } catch (_) {
        /* ignore */
      }
      resolve(result);
    };
    socket.setTimeout(timeoutMs);
    socket.once("connect", () => finish(true));
    socket.once("timeout", () => finish(false));
    socket.once("error", () => finish(false));
    socket.connect(port, host);
  });
}

async function waitPort(host, port, deadlineMs) {
  const end = Date.now() + deadlineMs;
  while (Date.now() < end) {
    if (await portOpen(host, port, 1000)) {
      return true;
    }
    await sleep(500);
  }
  return false;
}

async function deployElf(host, elfPath) {
  if (!elfPath || !fs.existsSync(elfPath)) {
    return { ok: false, message: `ELF missing at ${elfPath || "(not set)"}` };
  }

  let data;
  try {
    data = fs.readFileSync(elfPath);
  } catch (err) {
    return { ok: false, message: `Read ELF: ${err.message}` };
  }

  return new Promise((resolve) => {
    const socket = new net.Socket();
    socket.setTimeout(30000);
    socket.once("error", (err) =>
      resolve({
        ok: false,
        message: `Send to ${host}:${ELF_LOAD_PORT}: ${err.message}`,
      })
    );
    socket.connect(ELF_LOAD_PORT, host, () => {
      socket.write(data, (err) => {
        socket.end();
        if (err) {
          resolve({ ok: false, message: err.message });
        } else {
          resolve({ ok: true, message: "Payload sent" });
        }
      });
    });
  });
}

function sendGbnd(host, virt, phys, user = 0) {
  // 4 (magic) + 4 (userId u32) + 8 (virt u64) + 8 (phys u64) = 24 bytes
  const buf = Buffer.alloc(24);
  buf.write("GBND", 0);
  buf.writeUInt32LE(user >>> 0, 4);
  buf.writeBigUInt64LE(BigInt(virt >>> 0), 8);
  buf.writeBigUInt64LE(BigInt(phys >>> 0), 16);

  return new Promise((resolve) => {
    const socket = new net.Socket();
    socket.setTimeout(3000);
    socket.once("error", (err) => resolve({ ok: false, message: err.message }));
    socket.connect(CTRL_PORT, host, () => {
      socket.write(buf, () => {
        socket.end();
        resolve({ ok: true });
      });
    });
  });
}

class KlogWatcher {
  constructor(host, onStatus) {
    this.host = host;
    this.onStatus = onStatus || (() => {});
    this.autoPhys = 0;
    this.autoVirt = 0;
    this.autoBound = false;
    this.autoAdopted = false;
    this.autoSentGbnd = false;
    this.socket = null;
    this.buffer = "";
  }

  start() {
    return new Promise((resolve) => {
      const socket = new net.Socket();
      this.socket = socket;
      socket.setTimeout(500);
      socket.once("error", () => resolve(false));
      socket.connect(KLOG_PORT, this.host, () => resolve(true));
      socket.on("data", (chunk) => {
        this.processChunk(chunk.toString("utf8", "replace"));
      });
      socket.on("timeout", () => {
        /* keep listening */
      });
    });
  }

  processChunk(text) {
    this.buffer += text;
    let idx = this.buffer.indexOf("\n");
    while (idx >= 0) {
      const line = this.buffer.slice(0, idx);
      this.buffer = this.buffer.slice(idx + 1);
      this.processLine(line);
      idx = this.buffer.indexOf("\n");
    }
  }

  processLine(line) {
    let match = RE_DEV_PHYS.exec(line);
    if (match) {
      this.autoPhys = parseInt(match[1], 16);
    }

    match = RE_DEV_VIRT.exec(line) || RE_DEV_VIRT2.exec(line);
    if (match && !this.autoSentGbnd) {
      this.autoVirt = parseInt(match[1], 16);
      this.autoSentGbnd = true;
      this.onStatus(
        `Virtual device 0x${this.autoVirt.toString(16)} detected — sending GBND`
      );
      setTimeout(() => {
        try {
          sendGbnd(this.host, this.autoVirt, this.autoPhys, 0).catch(() => {});
        } catch (e) {
          console.error("sendGbnd threw:", e);
        }
      }, 400);
    }

    match = RE_OWNER.exec(line);
    if (match) {
      this.autoBound = true;
      this.onStatus(
        `Controller bound: dev=0x${parseInt(match[1], 16).toString(16)}`
      );
    }

    if (RE_ADOPTED.test(line)) {
      this.autoAdopted = true;
      this.onStatus("Direct VDI adopted — GPAD input should work");
    }
  }

  stop() {
    if (this.socket) {
      try {
        this.socket.destroy();
      } catch (_) {
        /* ignore */
      }
      this.socket = null;
    }
  }
}

function stopKlogWatcher() {
  if (activeWatcher) {
    activeWatcher.stop();
    activeWatcher = null;
  }
}

async function ensurePayloadRunning(host, options = {}) {
  const {
    elfPath,
    forceDeploy = false,
    autoBindViaKlog = true,
    onStatus,
  } = options;

  const emit = (phase, message) => {
    const status = setDeployStatus(phase, message, host);
    if (onStatus) onStatus(status);
    return status;
  };

  stopKlogWatcher();

  let watcher = null;
  if (autoBindViaKlog) {
    watcher = new KlogWatcher(host, (message) => emit("klog", message));
    const klogOk = await watcher.start();
    if (!klogOk) {
      emit("klog", "Klog port unavailable — continuing without auto-bind");
      watcher.stop();
      watcher = null;
    } else {
      activeWatcher = watcher;
    }
  }

  const gpAlready = await portOpen(host, GPAD_PORT, 1000);

  if (forceDeploy || !gpAlready) {
    if (!(await portOpen(host, ELF_LOAD_PORT, 1000))) {
      if (watcher) watcher.stop();
      activeWatcher = null;
      emit(
        "error",
        `ELF loader port ${ELF_LOAD_PORT} is closed — launch the ELF loader on your PS5 first`
      );
      return { ok: false, message: lastDeployStatus.message };
    }

    if (!elfPath) {
      if (watcher) watcher.stop();
      activeWatcher = null;
      emit(
        "error",
        "No payload ELF configured. Choose ghostpad.elf in Settings."
      );
      return { ok: false, message: lastDeployStatus.message };
    }

    emit("deploying", `Deploying payload to ${host}:${ELF_LOAD_PORT}...`);
    const deployResult = await deployElf(host, elfPath);
    if (!deployResult.ok) {
      if (watcher) watcher.stop();
      activeWatcher = null;
      emit("error", deployResult.message);
      return { ok: false, message: deployResult.message };
    }

    emit("waiting", "Payload sent — waiting for GPAD port...");
    if (!(await waitPort(host, GPAD_PORT, 30000))) {
      if (watcher) watcher.stop();
      activeWatcher = null;
      emit("error", "Payload deployed but GPAD port 6967 never opened");
      return { ok: false, message: lastDeployStatus.message };
    }

    if (watcher) {
      emit("waiting", "Waiting for direct-VDI adopt (auto-bind)...");
      const end = Date.now() + 12000;
      while (Date.now() < end && !watcher.autoAdopted) {
        await sleep(150);
      }
    }
  } else {
    emit("ready", `Payload already running on ${host} — reusing existing session`);
  }

  emit("ready", `Ghostpad ready on ${host}:${GPAD_PORT}`);
  return {
    ok: true,
    message: lastDeployStatus.message,
    adopted: watcher?.autoAdopted ?? false,
    bound: watcher?.autoBound ?? false,
  };
}

function getDeployStatus() {
  return lastDeployStatus;
}

module.exports = {
  GPAD_PORT,
  ELF_LOAD_PORT,
  ensurePayloadRunning,
  deployElf,
  getDeployStatus,
  stopKlogWatcher,
  portOpen,
};
