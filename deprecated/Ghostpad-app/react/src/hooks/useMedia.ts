import React from "react";
// Hooks
import { Context } from "./Provider";
import { getGhostpadApi } from "../configs/ghostpadApi";

export const useMedia = () => {
  const [context, setContext] = React.useContext(Context);
  const activeDeviceId = React.useRef<string | null>(null);
  const streamRef = React.useRef<MediaStream>();
  const recorderRef = React.useRef<MediaRecorder>();

  const getMediaDevices = React.useCallback(async (): Promise<void> => {
    console.log("Getting Media Devices...");
    try {
      try {
        const temp = await navigator.mediaDevices.getUserMedia({ video: true });
        temp.getTracks().forEach((t) => t.stop());
      } catch {
        /* permission may be denied until user picks a device */
      }
      const devices = await navigator.mediaDevices.enumerateDevices();
      setContext((c) => ({
        ...c,
        media: {
          ...c.media,
          devices: devices.filter((d) => d.kind === "videoinput"),
        },
      }));
    } catch (error) {
      throw error;
    }
  }, [setContext]);

  const connectToUserMedia = React.useCallback(
    async (deviceId: string): Promise<void> => {
      console.log("Connecting to Media Device...");
      try {
        if (!navigator.mediaDevices) return;
        const device = context.media.devices?.find((d) => d.deviceId === deviceId);
        const label = device?.label || "Capture Device";
        const api = getGhostpadApi();

        if (api?.openCaptureWindow) {
          await api.openCaptureWindow({ deviceId, label });
        } else {
          window.alert("Capture preview requires the Ghostpad desktop app.");
          return;
        }

        activeDeviceId.current = deviceId;

        let stream: MediaStream | undefined;
        let recorder: MediaRecorder | undefined;
        try {
          stream = await navigator.mediaDevices.getUserMedia({
            video: {
              deviceId: { exact: deviceId },
              width: { ideal: 1920, max: 3840 },
              height: { ideal: 1080, max: 2160 },
              frameRate: { ideal: 60 },
            },
          });
          recorder = new MediaRecorder(stream);
        } catch {
          console.warn("Recorder stream unavailable (device may be in use by preview).");
        }

        setContext((c) => ({
          ...c,
          media: {
            ...c.media,
            isConnected: true,
            activeDeviceId: deviceId,
            stream,
            recorder,
          },
        }));
        streamRef.current = stream;
        recorderRef.current = recorder;
      } catch (error) {
        console.error(error);
        window.alert("Could not open capture device. Check permissions and try again.");
        throw error;
      }
    },
    [context.media.devices, setContext]
  );

  const disconnectUserMedia = React.useCallback(async (): Promise<void> => {
    const deviceId = activeDeviceId.current;
    const api = getGhostpadApi();
    if (deviceId && api?.closeCaptureWindow) {
      await api.closeCaptureWindow(deviceId);
    }

    if (recorderRef.current?.state === "recording") {
      recorderRef.current.stop();
    }
    if (streamRef.current) {
      streamRef.current.getTracks().forEach((t) => t.stop());
    }

    activeDeviceId.current = null;
    streamRef.current = undefined;
    recorderRef.current = undefined;
    setContext((c) => ({
      ...c,
      media: {
        ...c.media,
        isConnected: false,
        activeDeviceId: undefined,
        stream: undefined,
        recorder: undefined,
      },
    }));
  }, [setContext]);

  React.useEffect(() => () => {
    const deviceId = activeDeviceId.current;
    const api = getGhostpadApi();
    if (deviceId && api?.closeCaptureWindow) {
      api.closeCaptureWindow(deviceId).catch(() => {});
    }
    if (recorderRef.current?.state === "recording") recorderRef.current.stop();
    streamRef.current?.getTracks().forEach((t) => t.stop());
  }, []);

  return { getMediaDevices, connectToUserMedia, disconnectUserMedia };
};
