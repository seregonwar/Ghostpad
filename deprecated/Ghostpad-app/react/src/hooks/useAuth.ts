import React from "react";
import { useDatabase } from "./useDatabase";
import { getOfflineUser } from "../configs/offlineStorage";
import { FirebaseUserProps } from "../interfaces";

export const useAuth = () => {
  const { saveUser } = useDatabase();

  const signInAnonymously =
    React.useCallback(async (): Promise<FirebaseUserProps | null> => {
      try {
        console.log("Signing in (offline mode)...");
        const user = getOfflineUser();
        await saveUser(user);
        return user;
      } catch (error) {
        throw error;
      }
    }, [saveUser]);

  return { signInAnonymously };
};
