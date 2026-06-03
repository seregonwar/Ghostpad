import React from "react";
import {
  DeployResult,
  DeployStatus,
  getGhostpadApi,
  GhostpadSettings,
  GhostpadSettingsInfo,
} from "../configs/ghostpadApi";

export const useGhostpadSettings = () => {
  const api = getGhostpadApi();

  const loadSettings = React.useCallback(async (): Promise<GhostpadSettingsInfo | null> => {
    if (!api) return null;
    return api.getSettings();
  }, [api]);

  const saveSettings = React.useCallback(
    async (patch: Partial<GhostpadSettings>) => {
      if (!api) return null;
      return api.saveSettings(patch);
    },
    [api]
  );

  const pickPayloadFile = React.useCallback(async () => {
    if (!api) return null;
    return api.pickPayloadFile();
  }, [api]);

  const deployPayload = React.useCallback(
    async (ip: string, forceDeploy = false, elfLoaderPort?: number): Promise<DeployResult | null> => {
      if (!api) return null;
      return api.deployPayload({ ip, forceDeploy, elfLoaderPort });
    },
    [api]
  );

  const getDeployStatus = React.useCallback(async (): Promise<DeployStatus | null> => {
    if (!api) return null;
    return api.getDeployStatus();
  }, [api]);

  return {
    loadSettings,
    saveSettings,
    pickPayloadFile,
    deployPayload,
    getDeployStatus,
    isAvailable: Boolean(api),
  };
};

export default useGhostpadSettings;
