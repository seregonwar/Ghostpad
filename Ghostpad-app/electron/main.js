const { app, BrowserWindow, session, ipcMain, nativeTheme, dialog } = require("electron");
const path = require("path");
const express = require("express");
const fs = require("fs");
const ghostpad = require("./ghostpad-client");
const beeper = require("./beeper-client");
const ssm = require("./ssm-client");
const consoleStore = require("./console-store");
const captureWindow = require("./capture-window");
const settingsStore = require("./settings-store");
const payloadDeploy = require("./payload-deploy");
const { listXInputControllers } = require("./xinput-detect");

// Candidate paths for the bundled beeper_server.elf, checked in order.
const BEEPER_ELF_CANDIDATES = [
  path.join(__dirname, "beeper_server.elf"),                          // bundled next to main.js
  path.join(__dirname, "..", "BeeperGithub", "payload", "beeper_server.elf"), // built from source
];

function findBeeperElf() {
  return BEEPER_ELF_CANDIDATES.find((p) => fs.existsSync(p)) ?? null;
}

async function deployBeeperIfNeeded(ip, elfLoaderPort) {
  // Already running — nothing to do
  if (await payloadDeploy.portOpen(ip, beeper.BEEPER_PORT, 600)) {
    return { ok: true, skipped: true };
  }

  const elfPath = findBeeperElf();
  if (!elfPath) {
    console.log("[beeper] beeper_server.elf not found — skipping auto-deploy");
    return { ok: false, reason: "elf not found" };
  }

  console.log(`[beeper] deploying ${elfPath} to ${ip}`);
  const result = await beeper.deployBeeperElf(ip, elfPath, elfLoaderPort).catch((e) => ({
    ok: false,
    message: e.message,
  }));

  if (!result.ok) {
    console.warn(`[beeper] deploy failed: ${result.message}`);
    return { ok: false, reason: result.message };
  }

  // Wait up to 5 s for beeper server to open port 9111
  const deadline = Date.now() + 5000;
  while (Date.now() < deadline) {
    if (await payloadDeploy.portOpen(ip, beeper.BEEPER_PORT, 500)) {
      console.log("[beeper] server up on port", beeper.BEEPER_PORT);
      return { ok: true };
    }
    await new Promise((r) => setTimeout(r, 300));
  }

  console.warn("[beeper] deployed but port 9111 never opened");
  return { ok: false, reason: "port never opened" };
}

const PORT = 3847;
const BUILD_DIR = path.join(__dirname, "..", "react", "build");

let mainWindow = null;
let server = null;

function userData() {
  return app.getPath("userData");
}

function pushDeployStatus(status) {
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send("ghostpad:deployStatus", status);
  }
  const settings = settingsStore.readSettings(userData());
  if (
    status.phase === "klog" &&
    status.message.includes("Direct VDI adopted") &&
    status.host &&
    settings.connectBeepEnabled &&
    settings.connectBeepType > 0
  ) {
    beeper.buzz(status.host, settings.connectBeepType).catch(() => {});
  }
}

async function maybeDeployPayload(ip, { forceDeploy = false, deployIfNeeded = false, elfLoaderPort } = {}) {
  const { settings, resolvedPayloadPath } = settingsStore.getSettingsPayloadInfo(
    userData()
  );
  const shouldDeploy = forceDeploy || deployIfNeeded;
  if (!shouldDeploy) {
    return { ok: true, skipped: true };
  }

  return payloadDeploy.ensurePayloadRunning(ip, {
    elfPath: resolvedPayloadPath,
    forceDeploy,
    autoBindViaKlog: settings.autoBindViaKlog,
    elfLoaderPort: elfLoaderPort ?? settings.elfLoaderPort,
    onStatus: pushDeployStatus,
  });
}

function registerIpc() {
  ipcMain.handle(
    "ghostpad:connect",
    async (_e, { ip, port, deployIfNeeded, forceDeploy, elfLoaderPort }) => {
      // Deploy beeper_server.elf first (best-effort — never blocks ghostpad connect)
      deployBeeperIfNeeded(ip, elfLoaderPort).catch(() => {});

      const deploy = await maybeDeployPayload(ip, { forceDeploy, deployIfNeeded, elfLoaderPort });
      if (!deploy.ok) {
        throw new Error(deploy.message || "Payload deploy failed");
      }
      const result = await ghostpad.connect(ip, port || ghostpad.GPAD_PORT);
      return result;
    }
  );

  ipcMain.handle("ghostpad:disconnect", async () => {
    payloadDeploy.stopKlogWatcher();
    return ghostpad.disconnect();
  });

  ipcMain.handle("ghostpad:sendPadState", async (_e, state) =>
    ghostpad.sendPadState(state)
  );

  ipcMain.handle("ghostpad:getStatus", async () => ghostpad.getStatus());

  ipcMain.handle("ghostpad:probeHost", async (_e, { ip, port }) =>
    ghostpad.probeHost(ip, port || 6967)
  );

  ipcMain.handle("ghostpad:scanNetwork", async (_e, options) =>
    ghostpad.scanNetwork(options)
  );

  ipcMain.handle("ghostpad:listConsoles", async () =>
    consoleStore.listConsoles(app.getPath("userData"))
  );

  ipcMain.handle("ghostpad:addConsole", async (_e, payload) =>
    consoleStore.addConsole(app.getPath("userData"), payload)
  );

  ipcMain.handle("ghostpad:updateConsole", async (_e, { id, patch }) =>
    consoleStore.updateConsole(app.getPath("userData"), id, patch)
  );

  ipcMain.handle("ghostpad:deleteConsole", async (_e, id) =>
    consoleStore.deleteConsole(app.getPath("userData"), id)
  );

  ipcMain.handle("ghostpad:openCaptureWindow", async (e, payload) => {
    const parent = BrowserWindow.fromWebContents(e.sender);
    return captureWindow.openCaptureWindow(parent, PORT, payload);
  });

  ipcMain.handle("ghostpad:closeCaptureWindow", async (_e, deviceId) => {
    captureWindow.closeCaptureWindow(deviceId);
    return { ok: true };
  });

  ipcMain.handle("ghostpad:getSettings", async () =>
    settingsStore.getSettingsPayloadInfo(userData())
  );

  ipcMain.handle("ghostpad:saveSettings", async (_e, patch) =>
    settingsStore.writeSettings(userData(), patch)
  );

  ipcMain.handle("ghostpad:pickPayloadFile", async (event) => {
    const win = BrowserWindow.fromWebContents(event.sender);
    const result = await dialog.showOpenDialog(win, {
      title: "Select Ghostpad payload (ghostpad.elf)",
      filters: [
        { name: "ELF Payload", extensions: ["elf"] },
        { name: "All Files", extensions: ["*"] },
      ],
      properties: ["openFile"],
    });
    if (result.canceled || !result.filePaths.length) {
      return null;
    }
    return result.filePaths[0];
  });

  ipcMain.handle("ghostpad:deployPayload", async (_e, { ip, forceDeploy, elfLoaderPort }) => {
    const { settings, resolvedPayloadPath } = settingsStore.getSettingsPayloadInfo(
      userData()
    );
    return payloadDeploy.ensurePayloadRunning(ip, {
      elfPath: resolvedPayloadPath,
      forceDeploy: Boolean(forceDeploy),
      autoBindViaKlog: settings.autoBindViaKlog,
      elfLoaderPort: elfLoaderPort,
      onStatus: pushDeployStatus,
    });
  });

  ipcMain.handle("ghostpad:sendType", async (_e, { ip, deviceType }) =>
    ghostpad.sendType(ip, deviceType ?? 3)
  );

  ipcMain.handle("ghostpad:disconnectVirtual", async (_e, { ip }) =>
    ghostpad.disconnectVirtual(ip)
  );

  ipcMain.handle("ghostpad:getDeployStatus", async () =>
    payloadDeploy.getDeployStatus()
  );

  ipcMain.handle("beeper:buzz", async (_e, { ip, type }) =>
    beeper.buzz(ip, type)
  );
  ipcMain.handle("beeper:vol", async (_e, { ip, level }) =>
    beeper.setVolume(ip, level)
  );
  ipcMain.handle("beeper:mute", async (_e, { ip, mute }) =>
    beeper.setMute(ip, mute)
  );
  ipcMain.handle("beeper:led", async (_e, { ip, level }) =>
    beeper.setLed(ip, level)
  );
  ipcMain.handle("beeper:ping", async (_e, { ip }) =>
    beeper.ping(ip)
  );
  ipcMain.handle("beeper:deploy", async (_e, { ip, elfPath }) =>
    beeper.deployBeeperElf(ip, elfPath)
  );
  ipcMain.handle("ssm:status", async (_e, { ip }) => ssm.status(ip));
  ipcMain.handle("ssm:reboot", async (_e, { ip }) => ssm.reboot(ip));
  ipcMain.handle("ssm:shutdown", async (_e, { ip }) => ssm.shutdown(ip));
  ipcMain.handle("ssm:restMode", async (_e, { ip }) => ssm.restMode(ip));
  ipcMain.handle("ssm:ejectDisc", async (_e, { ip }) => ssm.ejectDisc(ip));
  ipcMain.handle("ssm:deploy", async (_e, { ip, elfPath }) =>
    ssm.deploySsmElf(ip, elfPath)
  );
  ipcMain.handle("ssm:pickElf", async (event) => {
    const win = BrowserWindow.fromWebContents(event.sender);
    const result = await dialog.showOpenDialog(win, {
      title: "Select SystemStateManager payload (SystemStateManager.elf)",
      filters: [
        { name: "ELF Payload", extensions: ["elf"] },
        { name: "All Files", extensions: ["*"] },
      ],
      properties: ["openFile"],
    });
    if (result.canceled || !result.filePaths.length) return null;
    return result.filePaths[0];
  });

  ipcMain.handle("gamepad:list", async () => listXInputControllers());

  ipcMain.handle("beeper:pickElf", async (event) => {
    const win = BrowserWindow.fromWebContents(event.sender);
    const result = await dialog.showOpenDialog(win, {
      title: "Select Beeper Server payload (beeper_server.elf)",
      filters: [
        { name: "ELF Payload", extensions: ["elf"] },
        { name: "All Files", extensions: ["*"] },
      ],
      properties: ["openFile"],
    });
    if (result.canceled || !result.filePaths.length) return null;
    return result.filePaths[0];
  });
}

function startStaticServer() {
  return new Promise((resolve, reject) => {
    if (!fs.existsSync(path.join(BUILD_DIR, "index.html"))) {
      reject(
        new Error(
          "React build not found. Run launch.bat or `npm run build:react` first."
        )
      );
      return;
    }

    const indexPath = path.join(BUILD_DIR, "index.html");
    const webApp = express();
    webApp.use(express.static(BUILD_DIR, { index: false }));
    webApp.get(/^(?!\/static\/).*/, (_req, res, next) => {
      res.sendFile(indexPath, (err) => {
        if (err) next(err);
      });
    });

    server = webApp.listen(PORT, "127.0.0.1", () => {
      console.log(`Ghostpad serving at http://127.0.0.1:${PORT}`);
      resolve();
    });
    server.on("error", reject);
  });
}

function setupPermissions() {
  session.defaultSession.setPermissionRequestHandler(
    (_webContents, permission, callback) => {
      const allowed = [
        "media",
        "display-capture",
        "mediaKeySystem",
        "geolocation",
        "notifications",
        "midi",
        "midiSysex",
        "pointerLock",
        "fullscreen",
        "openExternal",
        "unknown",
      ];
      callback(allowed.includes(permission));
    }
  );

  session.defaultSession.setDevicePermissionHandler(() => true);
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 360,
    minHeight: 500,
    show: false,
    title: "Ghostpad",
    backgroundColor: "#121214",
    icon: path.join(__dirname, "icon.ico"),
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, "preload.js"),
      webSecurity: true,
    },
  });

  mainWindow.loadURL(`http://127.0.0.1:${PORT}/`);
  mainWindow.once("ready-to-show", () => {
    mainWindow.maximize();
    mainWindow.show();
  });
  mainWindow.on("closed", () => {
    mainWindow = null;
  });
}

app.whenReady().then(async () => {
  nativeTheme.themeSource = "dark";
  registerIpc();
  setupPermissions();
  try {
    await startStaticServer();
    createWindow();
  } catch (error) {
    console.error(error.message);
    app.quit();
  }

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", async () => {
  await ghostpad.disconnect();
  payloadDeploy.stopKlogWatcher();
  captureWindow.closeAllCaptureWindows();
  if (server) {
    server.close();
  }
  if (process.platform !== "darwin") {
    app.quit();
  }
});
