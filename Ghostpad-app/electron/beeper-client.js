const net = require("net");
const fs  = require("fs");

const BEEPER_PORT = 9111;
const ELFLDR_PORT = 9021;

/**
 * Open a fresh TCP connection, send one newline-terminated command, read the
 * single-line response, then close.  Mirrors the Python beeper_gui.py
 * send_command() pattern exactly:
 *   - Separate connect timeout (setTimeout on a pre-connect socket only
 *     counts idle time, not connection establishment time)
 *   - TCP_NODELAY after connect so the small command packet isn't held by
 *     Nagle's algorithm
 *   - Response idle timeout set only after the connection is up
 */
function sendBeeperCommand(ip, cmd, timeoutMs = 3000) {
  return new Promise((resolve) => {
    let done = false;
    let buf = "";

    const finish = (result) => {
      if (done) return;
      done = true;
      try { sock.destroy(); } catch (_) {}
      resolve(result);
    };

    // --- connection timeout (independent of socket idle timer) ---
    const connectTimer = setTimeout(
      () => finish({ ok: false, response: "ERR connect timeout" }),
      timeoutMs
    );

    const sock = net.createConnection({ host: ip, port: BEEPER_PORT });

    sock.once("connect", () => {
      clearTimeout(connectTimer);
      sock.setNoDelay(true);
      // Now set an idle timeout for the response phase only
      sock.setTimeout(timeoutMs, () =>
        finish({ ok: false, response: "ERR response timeout" })
      );
      sock.write(cmd + "\n", "utf8");
    });

    sock.once("error", (err) => {
      clearTimeout(connectTimer);
      finish({ ok: false, response: `ERR ${err.code || err.message}` });
    });

    sock.on("data", (chunk) => {
      buf += chunk.toString("utf8");
      const nl = buf.indexOf("\n");
      if (nl !== -1) {
        // Strip any trailing CR before the newline (defensive)
        const line = buf.slice(0, nl).replace(/\r$/, "").trim();
        finish({ ok: line.startsWith("OK"), response: line });
      }
    });
  });
}

function buzz(ip, type = 1) {
  return sendBeeperCommand(ip, `BUZZ ${type}`);
}

function setVolume(ip, level) {
  return sendBeeperCommand(ip, `VOL ${level}`);
}

function setMute(ip, mute) {
  return sendBeeperCommand(ip, `MUTE ${mute}`);
}

function setLed(ip, level) {
  return sendBeeperCommand(ip, `LED ${level}`);
}

function ping(ip) {
  return sendBeeperCommand(ip, "BUZZ 1");
}

function deployBeeperElf(ip, elfPath) {
  return new Promise((resolve) => {
    let data;
    try {
      data = fs.readFileSync(elfPath);
    } catch (err) {
      resolve({ ok: false, message: `Cannot read ELF: ${err.message}` });
      return;
    }

    const sock = net.createConnection({ host: ip, port: ELFLDR_PORT });
    let done = false;

    const finish = (result) => {
      if (done) return;
      done = true;
      try { sock.destroy(); } catch (_) {}
      resolve(result);
    };

    const timer = setTimeout(
      () => finish({ ok: false, message: "Deploy timeout" }),
      10000
    );

    sock.once("connect", () => {
      sock.setNoDelay(true);
      sock.write(data, () => {
        clearTimeout(timer);
        sock.end();
        finish({ ok: true, message: `Deployed ${data.length} bytes` });
      });
    });

    sock.once("error", (err) => {
      clearTimeout(timer);
      finish({ ok: false, message: err.message });
    });
  });
}

module.exports = {
  BEEPER_PORT,
  buzz,
  setVolume,
  setMute,
  setLed,
  ping,
  deployBeeperElf,
};
