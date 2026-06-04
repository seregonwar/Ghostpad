// Offline mode: Firebase is not used. This stub prevents accidental imports from crashing.
const noop = () => undefined;

export default {
  firestore: { FieldValue: { serverTimestamp: noop } },
};

export const auth = { signInAnonymously: async () => ({ user: { uid: "offline" } }) };
export const firestore = {
  collection: () => ({
    orderBy: () => ({ get: async () => ({ forEach: noop }) }),
    doc: () => ({
      get: async () => ({ data: () => undefined }),
      withConverter: () => ({ get: async () => ({ data: () => undefined }), set: async () => undefined }),
    }),
  }),
};
export const database = {
  ref: () => ({
    orderByKey: () => ({
      once: async () => undefined,
      on: () => undefined,
    }),
    set: async () => undefined,
    push: async () => undefined,
    remove: async () => undefined,
  }),
};
export const analytics = { logEvent: noop };
export const storage = {
  ref: () => ({
    child: () => ({
      put: async () => ({ ref: { getDownloadURL: async () => "offline://local" } }),
    }),
  }),
};
