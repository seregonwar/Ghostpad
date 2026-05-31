const net = require("net");
const fs = require("fs");

const SSM_PORT = 9112;
const ELFLDR_PORT = 9021;

function sendSsmCommand(ip, cmd, timeoutMs = 5000) {
  return new Promise((resolve) => {
    const socket = new net.Socket();
    let data = "";
    let settled = false;

    const finish = (result) => {
      if (settled) return;
      settled = true;
      try { socket.destroy(); } catch (_) {}
      resolve(result);
    };

    socket.setTimeout(timeoutMs);
    socket.once("timeout", () => finish({ ok: false, response: "ERR timeout" }));
    socket.once("error", (err) => finish({ ok: false, response: `ERR ${err.message}` }));

    socket.on("data", (chunk) => {
      data += chunk.toString();
      if (data.includes("\n")) {
        const line = data.split("\n")[0].trim();
        finish({ ok: line.startsWith("OK"), response: line });
      }
    });

    socket.connect(SSM_PORT, ip, () => {
      socket.write(cmd + "\n");
    });
  });
}

function status(ip) { return sendSsmCommand(ip, "STATUS"); }
function reboot(ip) { return sendSsmCommand(ip, "REBOOT", 8000); }
function shutdown(ip) { return sendSsmCommand(ip, "SHUTDOWN", 8000); }
function restMode(ip) { return sendSsmCommand(ip, "RESTMODE", 8000); }
function ejectDisc(ip) { return sendSsmCommand(ip, "EJECT"); }

function deploySsmElf(ip, elfPath) {
  return new Promise((resolve) => {
    let data;
    try {
      data = fs.readFileSync(elfPath);
    } catch (err) {
      resolve({ ok: false, message: `Cannot read ELF: ${err.message}` });
      return;
    }

    const socket = new net.Socket();
    let settled = false;
    const finish = (result) => {
      if (settled) return;
      settled = true;
      try { socket.destroy(); } catch (_) {}
      resolve(result);
    };

    socket.setTimeout(10000);
    socket.once("timeout", () => finish({ ok: false, message: "Deploy timeout" }));
    socket.once("error", (err) => finish({ ok: false, message: err.message }));

    socket.connect(ELFLDR_PORT, ip, () => {
      socket.write(data, () => {
        socket.end();
        finish({ ok: true, message: `Deployed ${data.length} bytes` });
      });
    });
  });
}

module.exports = {
  SSM_PORT,
  status,
  reboot,
  shutdown,
  restMode,
  ejectDisc,
  deploySsmElf,
};
