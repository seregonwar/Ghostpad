import { FirebaseProjectProps } from "../interfaces";
import { MenuGroupProps } from "../ui/systems/Navigation/MenuGroup";
import { resolveProjectImageUrl } from "../utils/resolveProjectImageUrl";
import {
  BUNDLED_PROJECT_ID,
  OBLIVION_PROJECT,
  OBLIVION_PRIVATE_DATA,
} from "./bundledProjects";

export const OFFLINE_USER_ID = "offline-local-user";

const PROJECTS_KEY = "Ghostpad-user-projects";

const PUBLIC_KEY = (projectId: string) => `Ghostpad-public-${projectId}`;
const PRIVATE_KEY = (projectId: string) => `Ghostpad-${projectId}`;
const USER_KEY = "Ghostpad-offline-user";

const listeners = new Map<string, Set<() => void>>();

function notify(projectId: string) {
  listeners.get(projectId)?.forEach((fn) => fn());
}

export function subscribePublicCommands(
  projectId: string,
  callback: () => void
): () => void {
  if (!listeners.has(projectId)) {
    listeners.set(projectId, new Set());
  }
  listeners.get(projectId)!.add(callback);
  return () => listeners.get(projectId)?.delete(callback);
}

export function readJson<T>(key: string, fallback: T): T {
  try {
    const raw = localStorage.getItem(key);
    if (!raw) return fallback;
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

export function writeJson(key: string, data: unknown): void {
  localStorage.setItem(key, JSON.stringify(data));
}

const RTDB_KEY = "Ghostpad-rtdb";

function getRtdb(): Record<string, unknown> {
  return readJson<Record<string, unknown>>(RTDB_KEY, {});
}

function saveRtdb(db: Record<string, unknown>): void {
  writeJson(RTDB_KEY, db);
}

function setByPath(root: Record<string, unknown>, path: string, value: unknown): void {
  const parts = path.split("/").filter(Boolean);
  if (parts.length === 0) return;
  let current: Record<string, unknown> | unknown[] = root;
  for (let i = 0; i < parts.length - 1; i++) {
    const key = parts[i];
    const nextKey = parts[i + 1];
    const nextIsIndex = /^\d+$/.test(nextKey);
    if (Array.isArray(current)) {
      const idx = Number(key);
      if (!current[idx]) current[idx] = nextIsIndex ? [] : {};
      current = current[idx] as Record<string, unknown> | unknown[];
    } else {
      const record = current as Record<string, unknown>;
      if (!(key in record) || record[key] == null) {
        record[key] = nextIsIndex ? [] : {};
      }
      current = record[key] as Record<string, unknown> | unknown[];
    }
  }
  const last = parts[parts.length - 1];
  if (Array.isArray(current)) {
    current[Number(last)] = value;
  } else {
    (current as Record<string, unknown>)[last] = value;
  }
}

function getByPath(root: Record<string, unknown>, path: string): unknown {
  const parts = path.split("/").filter(Boolean);
  let current: unknown = root;
  for (const part of parts) {
    if (current == null || typeof current !== "object") return null;
    if (Array.isArray(current)) {
      current = current[Number(part)];
    } else {
      current = (current as Record<string, unknown>)[part];
    }
  }
  return current ?? null;
}

export function saveRtdbPath(path: string, data: unknown): void {
  const db = getRtdb();
  setByPath(db, path, data);
  saveRtdb(db);
  const projectId = path.split("/")[0];
  if (projectId) notify(projectId);
}

export function readRtdbPath(path: string): unknown {
  return getByPath(getRtdb(), path);
}

export function readRtdbProject(projectId: string): MenuGroupProps[] {
  const value = readRtdbPath(projectId);
  if (Array.isArray(value)) return value as MenuGroupProps[];
  return getPublicCommands(projectId);
}

export function getPublicCommands(projectId: string): MenuGroupProps[] {
  const rtdbValue = readRtdbPath(projectId);
  if (Array.isArray(rtdbValue)) return rtdbValue as MenuGroupProps[];
  return readJson<MenuGroupProps[]>(PUBLIC_KEY(projectId), []);
}

export function setPublicCommands(
  projectId: string,
  data: MenuGroupProps[]
): void {
  writeJson(PUBLIC_KEY(projectId), data);
  saveRtdbPath(projectId, data);
}

export function getPrivateCommands(projectId: string): MenuGroupProps[] {
  return readJson<MenuGroupProps[]>(PRIVATE_KEY(projectId), []);
}

export function setPrivateCommands(
  projectId: string,
  data: MenuGroupProps[]
): void {
  writeJson(PRIVATE_KEY(projectId), data);
}

export function getOfflineUser() {
  const existing = readJson<{ uid: string; isAdmin: boolean } | null>(
    USER_KEY,
    null
  );
  if (existing) return existing;
  const user = { uid: OFFLINE_USER_ID, isAdmin: true };
  writeJson(USER_KEY, user);
  return user;
}

export function getProjectById(id: string): FirebaseProjectProps | undefined {
  return getAllProjects().find((p) => p.id === id);
}

const SEEDED_KEY = "Ghostpad-seeded-v1";

function ensureBundledProjects(): void {
  if (localStorage.getItem(SEEDED_KEY)) return;
  const projects = readJson<FirebaseProjectProps[]>(PROJECTS_KEY, []);
  if (!projects.find((p) => p.id === BUNDLED_PROJECT_ID)) {
    projects.push(OBLIVION_PROJECT);
    saveAllProjects(projects);
    setPrivateCommands(BUNDLED_PROJECT_ID, OBLIVION_PRIVATE_DATA as any);
  }
  localStorage.setItem(SEEDED_KEY, "1");
}

export function getAllProjects(): FirebaseProjectProps[] {
  ensureBundledProjects();
  return readJson<FirebaseProjectProps[]>(PROJECTS_KEY, [])
    .map((p) => ({
      ...p,
      imageUrl: p.imageUrl ? resolveProjectImageUrl(p.imageUrl) : undefined,
    }))
    .sort(
      (a, b) =>
        new Date(b.updatedAt || b.createdAt || 0).getTime() -
        new Date(a.updatedAt || a.createdAt || 0).getTime()
    );
}

export function saveAllProjects(projects: FirebaseProjectProps[]): void {
  writeJson(PROJECTS_KEY, projects);
}

export function addUserProject(input: {
  name: string;
  imageUrl?: string;
  description?: string;
}): FirebaseProjectProps {
  const projects = readJson<FirebaseProjectProps[]>(PROJECTS_KEY, []);
  const now = new Date().toISOString();
  const project: FirebaseProjectProps = {
    id: `proj-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
    name: input.name.trim() || "Untitled Project",
    imageUrl: input.imageUrl,
    description: input.description,
    createdAt: now,
    updatedAt: now,
  };
  projects.push(project);
  saveAllProjects(projects);
  return project;
}

export function updateUserProject(
  id: string,
  patch: Partial<Pick<FirebaseProjectProps, "name" | "imageUrl" | "description">>
): FirebaseProjectProps | null {
  const projects = readJson<FirebaseProjectProps[]>(PROJECTS_KEY, []);
  const idx = projects.findIndex((p) => p.id === id);
  if (idx < 0) return null;
  projects[idx] = {
    ...projects[idx],
    ...patch,
    updatedAt: new Date().toISOString(),
  };
  saveAllProjects(projects);
  return projects[idx];
}

export function deleteUserProject(id: string): void {
  const projects = readJson<FirebaseProjectProps[]>(PROJECTS_KEY, []).filter(
    (p) => p.id !== id
  );
  saveAllProjects(projects);
  localStorage.removeItem(PRIVATE_KEY(id));
  localStorage.removeItem(PUBLIC_KEY(id));
  const db = getRtdb();
  delete db[id];
  saveRtdb(db);
}

export function importUserProject(payload: {
  name: string;
  imageUrl?: string;
  description?: string;
  privateData?: unknown;
  publicData?: unknown;
}): FirebaseProjectProps {
  const project = addUserProject({
    name: payload.name,
    imageUrl: payload.imageUrl,
    description: payload.description,
  });
  if (payload.privateData) {
    setPrivateCommands(project.id, payload.privateData as MenuGroupProps[]);
  }
  if (payload.publicData) {
    setPublicCommands(project.id, payload.publicData as MenuGroupProps[]);
  }
  return project;
}

export function saveOfflineFile(_filePath: string, _data: Blob): Promise<string> {
  return Promise.resolve("offline://local");
}
