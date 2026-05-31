const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("ghostpad", {
  connect: (payload) => ipcRenderer.invoke("ghostpad:connect", payload),
  disconnect: () => ipcRenderer.invoke("ghostpad:disconnect"),
  sendPadState: (state) => ipcRenderer.invoke("ghostpad:sendPadState", state),
  getStatus: () => ipcRenderer.invoke("ghostpad:getStatus"),
  probeHost: (ip, port) => ipcRenderer.invoke("ghostpad:probeHost", { ip, port }),
  scanNetwork: (options) => ipcRenderer.invoke("ghostpad:scanNetwork", options || {}),
  listConsoles: () => ipcRenderer.invoke("ghostpad:listConsoles"),
  addConsole: (payload) => ipcRenderer.invoke("ghostpad:addConsole", payload),
  updateConsole: (id, patch) =>
    ipcRenderer.invoke("ghostpad:updateConsole", { id, patch }),
  deleteConsole: (id) => ipcRenderer.invoke("ghostpad:deleteConsole", id),
  openCaptureWindow: (payload) =>
    ipcRenderer.invoke("ghostpad:openCaptureWindow", payload),
  closeCaptureWindow: (deviceId) =>
    ipcRenderer.invoke("ghostpad:closeCaptureWindow", deviceId),
  getSettings: () => ipcRenderer.invoke("ghostpad:getSettings"),
  saveSettings: (patch) => ipcRenderer.invoke("ghostpad:saveSettings", patch),
  pickPayloadFile: () => ipcRenderer.invoke("ghostpad:pickPayloadFile"),
  deployPayload: (payload) => ipcRenderer.invoke("ghostpad:deployPayload", payload),
  getDeployStatus: () => ipcRenderer.invoke("ghostpad:getDeployStatus"),
  sendType: (ip, deviceType) =>
    ipcRenderer.invoke("ghostpad:sendType", { ip, deviceType }),
  disconnectVirtual: (ip) =>
    ipcRenderer.invoke("ghostpad:disconnectVirtual", { ip }),
  onDeployStatus: (callback) => {
    const handler = (_e, data) => callback(data);
    ipcRenderer.on("ghostpad:deployStatus", handler);
    return () => ipcRenderer.removeListener("ghostpad:deployStatus", handler);
  },
  ssmStatus: (ip) => ipcRenderer.invoke("ssm:status", { ip }),
  ssmReboot: (ip) => ipcRenderer.invoke("ssm:reboot", { ip }),
  ssmShutdown: (ip) => ipcRenderer.invoke("ssm:shutdown", { ip }),
  ssmRestMode: (ip) => ipcRenderer.invoke("ssm:restMode", { ip }),
  ssmEjectDisc: (ip) => ipcRenderer.invoke("ssm:ejectDisc", { ip }),
  ssmDeploy: (ip, elfPath) => ipcRenderer.invoke("ssm:deploy", { ip, elfPath }),
  ssmPickElf: () => ipcRenderer.invoke("ssm:pickElf"),
  beeperBuzz: (ip, type) => ipcRenderer.invoke("beeper:buzz", { ip, type }),
  beeperSetVol: (ip, level) => ipcRenderer.invoke("beeper:vol", { ip, level }),
  beeperSetMute: (ip, mute) => ipcRenderer.invoke("beeper:mute", { ip, mute }),
  beeperSetLed: (ip, level) => ipcRenderer.invoke("beeper:led", { ip, level }),
  beeperPing: (ip) => ipcRenderer.invoke("beeper:ping", { ip }),
  beeperDeploy: (ip, elfPath) => ipcRenderer.invoke("beeper:deploy", { ip, elfPath }),
  beeperPickElf: () => ipcRenderer.invoke("beeper:pickElf"),
  listGamepads: () => ipcRenderer.invoke("gamepad:list"),
});
