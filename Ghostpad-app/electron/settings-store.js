const fs = require("fs");
const path = require("path");

const DEFAULT_SETTINGS = {
  payloadElfPath: "",
  autoDeployOnConnect: true,
  autoBindViaKlog: true,
  connectBeepEnabled: false,
  connectBeepType: 1,
};

function settingsPath(userDataPath) {
  return path.join(userDataPath, "ghostpad-settings.json");
}

function readSettings(userDataPath) {
  const file = settingsPath(userDataPath);
  if (!fs.existsSync(file)) {
    return { ...DEFAULT_SETTINGS };
  }
  try {
    return { ...DEFAULT_SETTINGS, ...JSON.parse(fs.readFileSync(file, "utf8")) };
  } catch {
    return { ...DEFAULT_SETTINGS };
  }
}

function writeSettings(userDataPath, settings) {
  const next = { ...readSettings(userDataPath), ...settings };
  fs.writeFileSync(settingsPath(userDataPath), JSON.stringify(next, null, 2));
  return next;
}

function defaultElfCandidates() {
  const appRoot = path.join(__dirname, "..");
  return [
    path.join(appRoot, "Ghostpad", "payload", "ghostpad.elf"),
    path.join(
      appRoot,
      "Ghostpad",
      "payloadExamples",
      "ghostpadOGpartial",
      "payload",
      "ghostpad.elf"
    ),
  ];
}

function resolvePayloadPath(userDataPath, settings) {
  const configured = settings?.payloadElfPath?.trim();
  if (configured && fs.existsSync(configured)) {
    return configured;
  }
  for (const candidate of defaultElfCandidates()) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }
  return configured || "";
}

function getSettingsPayloadInfo(userDataPath) {
  const settings = readSettings(userDataPath);
  const resolvedPath = resolvePayloadPath(userDataPath, settings);
  return {
    settings,
    resolvedPayloadPath: resolvedPath,
    payloadExists: Boolean(resolvedPath && fs.existsSync(resolvedPath)),
    defaultCandidates: defaultElfCandidates(),
  };
}

module.exports = {
  DEFAULT_SETTINGS,
  readSettings,
  writeSettings,
  resolvePayloadPath,
  getSettingsPayloadInfo,
  defaultElfCandidates,
};
