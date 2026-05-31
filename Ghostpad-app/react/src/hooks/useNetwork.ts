import React from "react";
import { Context } from "./Provider";
import { getGhostpadApi, SavedConsole, ScanResult } from "../configs/ghostpadApi";
import { buildGpadState } from "../configs/ps5Controller";
import { ContextProps } from "../interfaces";

export const useNetwork = () => {
  const [context, setContext] = React.useContext(Context);
  const flushTimer = React.useRef<number | null>(null);
  const pendingState = React.useRef<ReturnType<typeof buildGpadState> | null>(
    null
  );

  const api = getGhostpadApi();

  const flushPadState = React.useCallback(async () => {
    if (!api || !pendingState.current) return;
    const state = pendingState.current;
    pendingState.current = null;
    await api.sendPadState(state);
  }, [api]);

  const scheduleSend = React.useCallback(
    (buttonStates: ContextProps["gamePad"]["buttonStates"], stickStates: ContextProps["gamePad"]["stickStates"]) => {
      if (!context.network.isConnected || !api) return;
      pendingState.current = buildGpadState({ buttonStates, stickStates });
      if (flushTimer.current != null) return;
      flushTimer.current = window.setTimeout(() => {
        flushTimer.current = null;
        flushPadState();
      }, 1000 / 60);
    },
    [api, context.network.isConnected, flushPadState]
  );

  const refreshStatus = React.useCallback(async () => {
    if (!api) return;
    const status = await api.getStatus();
    setContext((c) => ({
      ...c,
      network: {
        ...c.network,
        isConnected: status.isConnected,
        ip: status.ip ?? undefined,
        port: status.port,
      },
    }));
  }, [api, setContext]);

  React.useEffect(() => {
    refreshStatus();
    const interval = window.setInterval(refreshStatus, 2000);
    return () => window.clearInterval(interval);
  }, [refreshStatus]);

  const connect = React.useCallback(
    async (
      ip: string,
      port = 6967,
      consoleId?: string,
      options?: { forceDeploy?: boolean }
    ) => {
      if (!api) {
        window.alert("Ghostpad network API unavailable. Run via Electron.");
        return false;
      }
      try {
        setContext((c) => ({
          ...c,
          network: { ...c.network, isSearching: true },
        }));

        const settingsInfo = await api.getSettings();
        const autoDeploy = settingsInfo?.settings.autoDeployOnConnect ?? true;

        await api.connect({
          ip,
          port,
          deployIfNeeded: options?.forceDeploy ? true : autoDeploy,
          forceDeploy: options?.forceDeploy,
        });

        setContext((c) => ({
          ...c,
          network: {
            isConnected: true,
            isSearching: false,
            ip,
            port,
            activeConsoleId: consoleId,
          },
        }));
        return true;
      } catch (error) {
        console.error(error);
        const message =
          error instanceof Error ? error.message : `Failed to connect to ${ip}:${port}`;
        window.alert(message);
        setContext((c) => ({
          ...c,
          network: {
            ...c.network,
            isConnected: false,
            isSearching: false,
            ip: undefined,
            activeConsoleId: undefined,
          },
        }));
        return false;
      }
    },
    [api, setContext]
  );

  const disconnect = React.useCallback(async () => {
    if (!api) return;
    await api.disconnect();
    setContext((c) => ({
      ...c,
      network: {
        ...c.network,
        isConnected: false,
        ip: undefined,
        activeConsoleId: undefined,
      },
    }));
  }, [api, setContext]);

  const listConsoles = React.useCallback(async (): Promise<SavedConsole[]> => {
    if (!api) return [];
    return api.listConsoles();
  }, [api]);

  const addConsole = React.useCallback(
    async (name: string, ip: string, port = 6967) => {
      if (!api) return null;
      return api.addConsole({ name, ip, port });
    },
    [api]
  );

  const updateConsole = React.useCallback(
    async (
      id: string,
      patch: Partial<Pick<SavedConsole, "name" | "ip" | "port">>
    ) => {
      if (!api) return null;
      return api.updateConsole(id, patch);
    },
    [api]
  );

  const deleteConsole = React.useCallback(
    async (id: string) => {
      if (!api) return;
      await api.deleteConsole(id);
    },
    [api]
  );

  const scanNetwork = React.useCallback(
    async (subnet?: string): Promise<ScanResult[]> => {
      if (!api) return [];
      setContext((c) => ({
        ...c,
        network: { ...c.network, isSearching: true },
      }));
      try {
        return await api.scanNetwork({ subnet });
      } finally {
        setContext((c) => ({
          ...c,
          network: { ...c.network, isSearching: false },
        }));
      }
    },
    [api, setContext]
  );

  const probeHost = React.useCallback(
    async (ip: string, port = 6967) => {
      if (!api) return { reachable: false };
      return api.probeHost(ip, port);
    },
    [api]
  );

  return {
    connect,
    disconnect,
    scheduleSend,
    listConsoles,
    addConsole,
    updateConsole,
    deleteConsole,
    scanNetwork,
    probeHost,
    refreshStatus,
  };
};
