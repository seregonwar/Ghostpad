const fs = require("fs");
const path = require("path");
const { randomUUID } = require("crypto");

function storePath(userDataPath) {
  return path.join(userDataPath, "ghostpad-consoles.json");
}

function readConsoles(userDataPath) {
  const file = storePath(userDataPath);
  if (!fs.existsSync(file)) return [];
  try {
    const data = JSON.parse(fs.readFileSync(file, "utf8"));
    return Array.isArray(data) ? data : [];
  } catch {
    return [];
  }
}

function writeConsoles(userDataPath, consoles) {
  fs.writeFileSync(storePath(userDataPath), JSON.stringify(consoles, null, 2));
}

function listConsoles(userDataPath) {
  return readConsoles(userDataPath);
}

function addConsole(userDataPath, { name, ip, port = 6967, elfLoaderPort }) {
  const consoles = readConsoles(userDataPath);
  const entry = {
    id: randomUUID(),
    name: name || `PS5 (${ip})`,
    ip,
    port,
    elfLoaderPort: elfLoaderPort || undefined,
    createdAt: new Date().toISOString(),
    updatedAt: new Date().toISOString(),
  };
  consoles.push(entry);
  writeConsoles(userDataPath, consoles);
  return entry;
}

function updateConsole(userDataPath, id, patch) {
  const consoles = readConsoles(userDataPath);
  const idx = consoles.findIndex((c) => c.id === id);
  if (idx < 0) return null;
  consoles[idx] = {
    ...consoles[idx],
    ...patch,
    updatedAt: new Date().toISOString(),
  };
  writeConsoles(userDataPath, consoles);
  return consoles[idx];
}

function deleteConsole(userDataPath, id) {
  const consoles = readConsoles(userDataPath).filter((c) => c.id !== id);
  writeConsoles(userDataPath, consoles);
  return { ok: true };
}

module.exports = {
  listConsoles,
  addConsole,
  updateConsole,
  deleteConsole,
};
