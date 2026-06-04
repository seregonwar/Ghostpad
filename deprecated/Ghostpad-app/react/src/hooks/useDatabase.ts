import React from "react";
import {
  getAllProjects,
  getProjectById,
  getPublicCommands,
  getPrivateCommands,
  setPublicCommands,
  setPrivateCommands,
  subscribePublicCommands,
  saveOfflineFile,
  getOfflineUser,
  saveRtdbPath,
  readRtdbPath,
  addUserProject,
  updateUserProject,
  deleteUserProject,
  importUserProject,
} from "../configs/offlineStorage";
import {
  FirebaseProjectProps,
  FirebaseUserProps,
  LoadedProjectProps,
  ContextProps,
} from "../interfaces";
import { MenuGroupProps } from "../ui/systems/Navigation/MenuGroup";
import { Context } from "./Provider";

export const useDatabase = () => {
  const [context, setContext] = React.useContext(Context);
  const unsubscribePublicRef = React.useRef<(() => void) | null>(null);

  React.useEffect(() => () => unsubscribePublicRef.current?.(), []);

  const fetchCommands = React.useCallback(
    async (projectId: string): Promise<MenuGroupProps[] | null> => {
      console.log("Fetching Commands (offline)...");
      return getPublicCommands(projectId);
    },
    []
  );

  const watchCommands = React.useCallback(
    async (projectId: string): Promise<void> => {
      console.log("Watch Commands (offline)...");
      const apply = () => {
        setContext((c): ContextProps => {
          if (!c.project.isLoaded || c.project.id !== projectId) return c;
          const loaded = c.project;
          return {
            ...c,
            project: {
              ...loaded,
              publicData: getPublicCommands(projectId),
            },
          };
        });
      };
      apply();
      unsubscribePublicRef.current?.();
      unsubscribePublicRef.current = subscribePublicCommands(projectId, apply);
    },
    [setContext]
  );

  const saveCommand = React.useCallback(
    async (path: string, data: unknown): Promise<void> => {
      saveRtdbPath(path, data);
      const projectId = path.split("/")[0];
      if (projectId && Array.isArray(data) && !path.includes("/")) {
        setPublicCommands(projectId, data as MenuGroupProps[]);
      }
    },
    []
  );

  const pushCommand = React.useCallback(
    async (path: string, data: unknown): Promise<void> => {
      const existing = readRtdbPath(path);
      if (Array.isArray(existing)) {
        saveRtdbPath(path, [...existing, data]);
      } else {
        saveRtdbPath(path, [data]);
      }
    },
    []
  );

  const removeCommand = React.useCallback(
    async (_path: string): Promise<void> => {
      console.warn("removeCommand is not used in offline mode");
    },
    []
  );

  const fetchCommand = React.useCallback(async (path: string): Promise<unknown> => {
    return readRtdbPath(path);
  }, []);

  const fetchProjects = React.useCallback(async (): Promise<
    FirebaseProjectProps[]
  > => {
    console.log("Fetching Projects (offline)...");
    return getAllProjects();
  }, []);

  const fetchProject = React.useCallback(
    async (id: string, _isAdmin?: boolean): Promise<void> => {
      console.log("Fetching Project (offline)...");
      const project = getProjectById(id);
      if (!project) {
        throw new Error(`Unknown project: ${id}`);
      }
      await watchCommands(id);
      const privateData = getPrivateCommands(id) as LoadedProjectProps["privateData"];
      setContext((c): ContextProps => ({
        ...c,
        project: {
          isLoaded: true,
          id: project.id,
          name: project.name,
          imageUrl: project.imageUrl,
          privateData,
          publicData: c.project.isLoaded && c.project.id === id
            ? c.project.publicData
            : getPublicCommands(id),
        },
      }));
    },
    [setContext, watchCommands]
  );

  const fetchUser = React.useCallback(
    async (_id: string): Promise<FirebaseUserProps | null> => {
      return getOfflineUser();
    },
    []
  );

  const saveUser = React.useCallback(
    async (user: FirebaseUserProps): Promise<void> => {
      console.log("Saving User (offline)...", user.uid);
    },
    []
  );

  const saveFile = React.useCallback(
    async (_filePath: string, data: Blob): Promise<string> => {
      const fileUrl = await saveOfflineFile(_filePath, data);
      await setContext((c) => {
        const next = { ...c };
        delete next.emulator.command.blob;
        next.emulator = {
          ...next.emulator,
          command: {
            ...next.emulator.command,
            videoUrl: fileUrl,
          },
        };
        return next;
      });
      return fileUrl;
    },
    [setContext]
  );

  const storeCommand = React.useCallback(
    (projectId: string, data: unknown, notUpload?: boolean): void => {
      console.log("Store Commands (offline)...");
      setPrivateCommands(projectId, data as MenuGroupProps[]);
      if (notUpload !== true && context.user.uid) {
        const blob = new Blob([JSON.stringify(data)], {
          type: "application/json",
        });
        saveFile(`users/${context.user.uid}/${projectId}.json`, blob);
      }
      setContext((c): ContextProps => {
        if (!c.project.isLoaded) return c;
        const loaded = c.project;
        return {
          ...c,
          project: {
            ...loaded,
            privateData: data as LoadedProjectProps["privateData"],
          },
        };
      });
    },
    [context.user.uid, saveFile, setContext]
  );

  return {
    fetchProjects,
    fetchProject,
    watchCommands,
    fetchCommands,
    saveCommand,
    storeCommand,
    pushCommand,
    fetchCommand,
    removeCommand,
    fetchUser,
    saveUser,
    saveFile,
    addProject: addUserProject,
    updateProject: updateUserProject,
    deleteProject: deleteUserProject,
    importProject: importUserProject,
  };
};
