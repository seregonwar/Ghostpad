import { GpadNetworkState } from "./ps5Controller";

export interface SavedConsole {
  id: string;
  name: string;
  ip: string;
  port: number;
  elfLoaderPort?: number;
  createdAt: string;
  updatedAt: string;
}

export interface ScanResult {
  ip: string;
  ports: number[];
  hasGhostpad?: boolean;
  hasElfldr?: boolean;
}

export interface GhostpadSettings {
  payloadElfPath: string;
  autoDeployOnConnect: boolean;
  autoBindViaKlog: boolean;
  connectBeepEnabled: boolean;
  connectBeepType: number;
}

export interface GhostpadSettingsInfo {
  settings: GhostpadSettings;
  resolvedPayloadPath: string;
  payloadExists: boolean;
  defaultCandidates: string[];
}

export interface DeployStatus {
  phase: string;
  message: string;
  host: string | null;
  at: string | null;
}

export interface DeployResult {
  ok: boolean;
  message?: string;
  adopted?: boolean;
  bound?: boolean;
  skipped?: boolean;
}

export interface ConnectOptions {
  ip: string;
  port?: number;
  deployIfNeeded?: boolean;
  forceDeploy?: boolean;
  elfLoaderPort?: number;
}

export interface GhostpadApi {
  connect: (options: ConnectOptions) => Promise<{ ip: string; port: number }>;
  disconnect: () => Promise<{ ok: boolean }>;
  sendPadState: (state: GpadNetworkState) => Promise<{ ok: boolean; error?: string }>;
  getStatus: () => Promise<{ isConnected: boolean; ip: string | null; port: number }>;
  probeHost: (
    ip: string,
    port?: number
  ) => Promise<{ ip: string; port: number; reachable: boolean }>;
  scanNetwork: (options?: {
    subnet?: string;
    ports?: number[];
    timeoutMs?: number;
  }) => Promise<ScanResult[]>;
  listConsoles: () => Promise<SavedConsole[]>;
  addConsole: (payload: {
    name: string;
    ip: string;
    port?: number;
    elfLoaderPort?: number;
  }) => Promise<SavedConsole>;
  updateConsole: (
    id: string,
    patch: Partial<Pick<SavedConsole, "name" | "ip" | "port" | "elfLoaderPort">>
  ) => Promise<SavedConsole | null>;
  deleteConsole: (id: string) => Promise<{ ok: boolean }>;
  openCaptureWindow: (payload: {
    deviceId: string;
    label?: string;
  }) => Promise<{ reused: boolean }>;
  closeCaptureWindow: (deviceId: string) => Promise<{ ok: boolean }>;
  getSettings: () => Promise<GhostpadSettingsInfo>;
  saveSettings: (patch: Partial<GhostpadSettings>) => Promise<GhostpadSettings>;
  pickPayloadFile: () => Promise<string | null>;
  deployPayload: (payload: {
    ip: string;
    forceDeploy?: boolean;
    elfLoaderPort?: number;
  }) => Promise<DeployResult>;
  getDeployStatus: () => Promise<DeployStatus>;
  sendType: (ip: string, deviceType?: number) => Promise<{ ok: boolean }>;
  disconnectVirtual: (ip: string) => Promise<{ ok: boolean }>;
  onDeployStatus: (callback: (status: DeployStatus) => void) => () => void;
  ssmStatus: (ip: string) => Promise<{ ok: boolean; response: string }>;
  ssmReboot: (ip: string) => Promise<{ ok: boolean; response: string }>;
  ssmShutdown: (ip: string) => Promise<{ ok: boolean; response: string }>;
  ssmRestMode: (ip: string) => Promise<{ ok: boolean; response: string }>;
  ssmEjectDisc: (ip: string) => Promise<{ ok: boolean; response: string }>;
  ssmDeploy: (ip: string, elfPath: string) => Promise<{ ok: boolean; message: string }>;
  ssmPickElf: () => Promise<string | null>;
  beeperBuzz: (ip: string, type: number) => Promise<{ ok: boolean; response: string }>;
  beeperSetVol: (ip: string, level: number) => Promise<{ ok: boolean; response: string }>;
  beeperSetMute: (ip: string, mute: number) => Promise<{ ok: boolean; response: string }>;
  beeperSetLed: (ip: string, level: number) => Promise<{ ok: boolean; response: string }>;
  beeperPing: (ip: string) => Promise<{ ok: boolean; response: string }>;
  beeperDeploy: (ip: string, elfPath: string) => Promise<{ ok: boolean; message: string }>;
  beeperPickElf: () => Promise<string | null>;
  listGamepads: () => Promise<{ index: number; name: string }[]>;
}

declare global {
  interface Window {
    ghostpad?: GhostpadApi;
  }
}

export function getGhostpadApi(): GhostpadApi | null {
  return window.ghostpad ?? null;
}
