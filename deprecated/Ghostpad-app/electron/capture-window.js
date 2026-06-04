const { BrowserWindow } = require("electron");

const captureWindows = new Map();

function openCaptureWindow(parent, port, { deviceId, label }) {
  const existing = captureWindows.get(deviceId);
  if (existing && !existing.isDestroyed()) {
    existing.focus();
    return { reused: true };
  }

  const win = new BrowserWindow({
    width: 1280,
    height: 720,
    minWidth: 480,
    minHeight: 270,
    title: label ? `Capture — ${label}` : "Capture Preview",
    backgroundColor: "#121214",
    resizable: true,
    autoHideMenuBar: true,
    parent: parent && !parent.isDestroyed() ? parent : undefined,
    modal: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: require("path").join(__dirname, "preload.js"),
      webSecurity: true,
    },
  });

  const url = `http://127.0.0.1:${port}/capture?deviceId=${encodeURIComponent(deviceId)}&label=${encodeURIComponent(label || "")}`;
  win.loadURL(url);

  win.on("closed", () => {
    captureWindows.delete(deviceId);
  });

  captureWindows.set(deviceId, win);
  return { reused: false };
}

function closeCaptureWindow(deviceId) {
  const win = captureWindows.get(deviceId);
  if (win && !win.isDestroyed()) {
    win.close();
  }
  captureWindows.delete(deviceId);
}

function closeAllCaptureWindows() {
  for (const [id] of captureWindows) {
    closeCaptureWindow(id);
  }
}

module.exports = {
  openCaptureWindow,
  closeCaptureWindow,
  closeAllCaptureWindows,
};
