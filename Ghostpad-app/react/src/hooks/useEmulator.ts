import React from "react";
// Hooks
import { Context } from "./Provider";
import { useDatabase } from "./useDatabase";
import { useGamePad } from "./useGamePad";
// Configs
// Interfaces
import { ContextProps, SignalProps } from "../interfaces";
import uid from "uniqid";
import rison from "rison";
import {
  DEFAULT_COMMAND_TITLE,
  DEFAULT_GROUP_TITLE,
} from "../configs/defaultNames";
import {
  isValidMacroSignals,
  normalizeSignals,
  takeDueSignals,
} from "../configs/macroSignals";

export const useEmulator = () => {
  const [context, setContext] = React.useContext(Context);
  const [buffer, setBuffer] = React.useState<SignalProps[]>();
  const intervalRef = React.useRef<NodeJS.Timeout | null>();
  const recordingStartedAtRef = React.useRef<number | null>(null);
  const playbackStartedAtRef = React.useRef<number | null>(null);
  const playbackIndexRef = React.useRef(0);
  const playbackModeRef = React.useRef<"playing" | "repeating">("playing");
  const { saveCommand, storeCommand, saveFile } = useDatabase();
  const { onPush, onTilt, neutral } = useGamePad();
  const neutralRef = React.useRef(neutral);

  const bufferRef = React.useRef(buffer);
  React.useEffect(() => {
    bufferRef.current = buffer;
  }, [buffer]);
  React.useEffect(() => {
    neutralRef.current = neutral;
  }, [neutral]);
  React.useEffect(() => {
    return () => {
      if (intervalRef.current) clearInterval(intervalRef.current);
      intervalRef.current = null;
      recordingStartedAtRef.current = null;
      playbackStartedAtRef.current = null;
      if (context.media.recorder?.state === "recording") {
        context.media.recorder.stop();
      }
      neutralRef.current();
      setContext((c: ContextProps) => ({
        ...c,
        emulator: { ...c.emulator, state: "standby", recordingStartedAt: undefined },
      }));
    };
  }, [context.media.recorder, setContext]);

  const recorderStart = React.useCallback((): void => {
    if (!context.media.recorder || context.media.recorder.state === "recording")
      return;
    console.log("Screen Rec...");
    context.media.recorder.ondataavailable = (e: any) => {
      const blob = new Blob([e.data], { type: e.data.type });
      setContext((c: ContextProps) => ({
        ...c,
        emulator: {
          ...c.emulator,
          command: {
            ...c.emulator.command,
            blob: blob,
          },
        },
      }));
    };
    context.media.recorder.start();
  }, [context.media.recorder, setContext]);

  const recorderStop = React.useCallback((): void => {
    if (!context.media.recorder || context.media.recorder.state !== "recording")
      return;
    console.log("Screen Stop...");
    context.media.recorder.stop();
  }, [context.media.recorder]);

  const stopRec = React.useCallback(async (): Promise<void> => {
    console.log("Stop Rec...");
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    recordingStartedAtRef.current = null;
    recorderStop();
    setContext((c: ContextProps) => ({
      ...c,
      emulator: {
        ...c.emulator,
        state: "standby",
        recordingStartedAt: undefined,
        command: {
          ...c.emulator.command,
          signals: c.emulator.command.signals?.concat([
            {
              t: Number(c.emulator.time.toFixed(3)),
              s: [99, 0],
            },
          ]),
        },
      },
    }));
    neutral();
  }, [neutral, recorderStop, setContext]);

  const stopPlay = React.useCallback(
    async (reset?: boolean): Promise<void> => {
      if (intervalRef.current) {
        console.log("Stop Play...");
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
      playbackStartedAtRef.current = null;
      playbackIndexRef.current = 0;
      neutral();
      setContext((c: ContextProps) => ({
        ...c,
        emulator: reset
          ? { ...c.emulator, state: "standby", time: 0 }
          : { ...c.emulator, state: "standby" },
      }));
    },
    [neutral, setContext]
  );

  const recInterval = React.useCallback((): void => {
    if (recordingStartedAtRef.current == null) return;
    const time = (performance.now() - recordingStartedAtRef.current) / 1000;
    setContext((c: ContextProps) => ({
      ...c,
      emulator: {
        ...c.emulator,
        time: Number(time.toFixed(3)),
      },
    }));
  }, [setContext]);

  const playInterval = React.useCallback((): void => {
    const signals = bufferRef.current;
    if (!signals || playbackStartedAtRef.current == null) return;
    const time = (performance.now() - playbackStartedAtRef.current) / 1000;
    const batch = takeDueSignals(signals, playbackIndexRef.current, time);
    playbackIndexRef.current = batch.nextIndex;
    batch.due.forEach((signal) => {
      if (signal.s[0] <= 17) onPush(signal.s[0], signal.s[1]);
      else if (signal.s[0] <= 21) onTilt(signal.s[0], signal.s[1]);
    });
    setContext((c: ContextProps) => ({
      ...c,
      emulator: { ...c.emulator, time: Number(time.toFixed(3)) },
    }));
    if (playbackIndexRef.current < signals.length) return;
    if (playbackModeRef.current === "repeating") {
      neutral();
      playbackIndexRef.current = 0;
      playbackStartedAtRef.current = performance.now();
      setContext((c: ContextProps) => ({
        ...c,
        emulator: { ...c.emulator, state: "repeating", time: 0 },
      }));
    } else {
      stopPlay(true);
    }
  }, [neutral, onPush, onTilt, setContext, stopPlay]);

  const rec = React.useCallback(async (discardExisting = false): Promise<void> => {
    console.log("Rec...");
    if (context.emulator.state === "recording" || intervalRef.current) return;
    if (context.emulator.command.signals.length > 0 && !discardExisting) return;
    const startedAt = performance.now();
    recordingStartedAtRef.current = startedAt;
    recorderStart();
    setContext((c: ContextProps) => ({
      ...c,
      emulator: {
        ...c.emulator,
        state: "recording",
        time: 0,
        recordingStartedAt: startedAt,
        command: {
          ...c.emulator.command,
          signals: [],
        },
      },
    }));
    intervalRef.current = setInterval(recInterval, 1000 / 60);
  }, [
    context.emulator.command.signals.length,
    context.emulator.state,
    recInterval,
    recorderStart,
    setContext,
  ]);

  const play = React.useCallback(
    async (repeat: boolean): Promise<void> => {
      console.log(repeat ? "Repeat..." : "Play...");
      if (intervalRef.current) return;
      setContext((c: ContextProps) => ({
        ...c,
        emulator: {
          ...c.emulator,
          state: repeat ? "repeating" : "playing",
          time: 0,
        },
      }));
      const signals = normalizeSignals(context.emulator.command.signals);
      bufferRef.current = signals;
      setBuffer(signals);
      playbackIndexRef.current = 0;
      playbackStartedAtRef.current = performance.now();
      playbackModeRef.current = repeat ? "repeating" : "playing";
      intervalRef.current = setInterval(playInterval, 1000 / 60);
    },
    [context.emulator.command.signals, playInterval, setContext]
  );

  const save = React.useCallback(async (): Promise<void> => {
    if (!context.emulator.command.signals) return;
    console.log("Saving...", context.emulator.command.signals);
    try {
      // Upload Webm
      const data = context.emulator.command;
      if (data.blob)
        data.videoUrl = await saveFile(`files/${data.id}.webm`, data.blob);
      else delete data.videoUrl;
      // Local storage only
      if (!context.project.id) return;
      if (!context.project.privateData) return;
      const path: string[] = data.path.split("/");
      const newData: any = Array.from(context.project.privateData);
      if (path.length === 1 || !newData) {
        const id1 = uid();
        const id2 = uid();
        newData.splice(newData.length, 0, {
          id: id1,
          index: { title: DEFAULT_GROUP_TITLE, id: uid() },
          items: [
            {
              id: id2,
              path: `0/${id1}/0`,
              title: data.title || DEFAULT_COMMAND_TITLE,
              data: data,
            },
          ],
        });
      } else if (path.length === 3)
        newData[path[0]][path[1]][path[2]].data = data;
      else if (path.length === 5)
        newData[path[0]][path[1]][path[2]][path[3]][path[4]].data = data;
      else window.alert("保存に失敗しました\n Failed to save.");
      await storeCommand(context.project.id, newData);
      if (data.path)
        window.alert(
          `"${data.title}" を上書きしました。\nUpdated "${data.title}."`
        );
      else
        window.alert(
          `Saved as a new command "${data.title || DEFAULT_COMMAND_TITLE}".`
        );
    } catch (error) {
      window.alert("保存に失敗しました\n Failed to save.");
      console.error(error);
    }
  }, [
    context.emulator.command,
    context.project.id,
    context.project.privateData,
    saveFile,
    storeCommand,
  ]);

  const download = React.useCallback(async (): Promise<void> => {
    try {
      if (!context.project.privateData) return;
      const data = context.emulator.command;
      const newData: any = Array.from(context.project.privateData);
      const id1 = uid();
      const id2 = uid();
      newData.splice(newData.length, 0, {
        id: id1,
        index: { title: "Downloaded", id: uid() },
        items: [
          {
            id: id2,
            path: `0/${id1}/0`,
            title: data.title || DEFAULT_COMMAND_TITLE,
            data: data,
          },
        ],
      });
      await storeCommand(context.project.id, newData);
      window.alert(
        `"Local" にコマンドをダウンロードしました。\nDownloaded the command to "Local".`
      );
    } catch (error) {
      window.alert("ダウンロードに失敗しました\nFailed to download.");
      console.error(error);
    }
  }, [
    context.emulator.command,
    context.project.id,
    context.project.privateData,
    storeCommand,
  ]);

  const upload = React.useCallback(async () => {
    if (!context.emulator.command.signals) return;
    console.log("Uploading...", context.emulator.command.signals);
    try {
      const data = context.emulator.command;
      if (!context.user.isAdmin || !context.project.publicData) return;
      const path = `${context.project.id}/${context.project.publicData.length}`;
      await saveCommand(path, {
        id: uid(),
        index: {
          title: "Uploaded",
        },
        items: [
          {
            id: uid(),
            title: data.title || DEFAULT_COMMAND_TITLE,
            data: data,
          },
        ],
      });
      window.alert(
        `"Public"にコマンドをアップロードしました。\nUploaded the command to "Public".`
      );
    } catch (error) {
      window.alert("アップロードに失敗しました\n Failed to upload.");
      console.error(error);
    }
  }, [
    context.emulator.command,
    context.project.id,
    context.project.publicData,
    context.user.isAdmin,
    saveCommand,
  ]);

  const share = React.useCallback(async (): Promise<void> => {
    try {
      const command = rison.encode(context.emulator.command.signals);
      const url = `${window.location.href.split(/[?#]/)[0]}?data=${command}`;
      if (navigator.clipboard?.writeText) {
        await navigator.clipboard.writeText(url);
        window.alert(
          `Share URL copied to clipboard (offline mode).\n\n${url}`
        );
      } else {
        window.prompt("Copy this share URL:", url);
      }
    } catch (error) {
      window.alert("Error copying share URL");
      console.error(error);
    }
  }, [context.emulator.command.signals]);

  const exportJson = React.useCallback(
    (signals: SignalProps[]): void => {
      const blob = new Blob([JSON.stringify(signals, null, 2)], {
        type: "application/json",
      });
      const title = context.emulator.command.title
        .replace(" ", "")
        .match(/^[A-Za-z0-9]*$/)
        ? context.emulator.command.title
        : Date.now();
      const link = document.createElement("a");
      link.href = URL.createObjectURL(blob);
      link.download = `Ghostpad-${title}.json`;
      link.click();
    },
    [context.emulator.command.title]
  );

  const exportArduino = React.useCallback(
    (signals: SignalProps[]): void => {
      const title = context.emulator.command.title
        .replace(" ", "")
        .match(/^[A-Za-z0-9]*$/)
        ? context.emulator.command.title
        : String(Date.now());

      const lines = [
        "# Ghostpad macro replay script (generated)",
        "import json, socket, struct, time",
        "",
        "SIGNALS = " + JSON.stringify(signals, null, 2),
        "",
        "BUTTON_BITS = {0:0x2000,1:0x4000,2:0x1000,3:0x8000,4:0x0400,5:0x0800,6:0x0100,7:0x0200,8:0x0001,9:0x0008,10:0x0002,11:0x0004,12:0x0010,13:0x0040,14:0x0080,15:0x0020,16:0x10000,17:0x100000}",
        "",
        "def send_gpad(sock, buttons=0, lx=128, ly=128, rx=128, ry=128, l2=0, r2=0):",
        "    pkt = struct.pack('!4sIBBBBBB2s', b'GPAD', buttons, lx, ly, rx, ry, l2, r2, b'\\x00\\x00')",
        "    sock.sendall(pkt)",
        "",
        "def main():",
        "    ip = input('PS5 IP: ').strip()",
        "    sock = socket.create_connection((ip, 6967))",
        "    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)",
        "    buttons, lx, ly, rx, ry, l2, r2 = 0, 128, 128, 128, 128, 0, 0",
        "    prev_t = 0",
        "    for sig in SIGNALS:",
        "        time.sleep(max(0, sig['t'] - prev_t))",
        "        prev_t = sig['t']",
        "        signal_id, value = sig['s']",
        "        if signal_id == 99:",
        "            break",
        "        if signal_id <= 17:",
        "            bit = BUTTON_BITS[signal_id]",
        "            buttons = buttons | bit if value else buttons & ~bit",
        "            if signal_id == 6: l2 = value",
        "            if signal_id == 7: r2 = value",
        "        elif signal_id == 18: lx = value",
        "        elif signal_id == 19: ly = value",
        "        elif signal_id == 20: rx = value",
        "        elif signal_id == 21: ry = value",
        "        send_gpad(sock, buttons, lx, ly, rx, ry, l2, r2)",
        "    send_gpad(sock)",
        "    sock.close()",
        "",
        "if __name__ == '__main__':",
        "    main()",
      ];

      const blob = new Blob([lines.join("\n")], { type: "text/plain" });
      const link = document.createElement("a");
      link.href = URL.createObjectURL(blob);
      link.download = `Ghostpad-${title}.py`;
      link.click();
    },
    [context.emulator.command.title]
  );

  const onChangeInputFile = React.useCallback(
    (e: React.ChangeEvent<HTMLInputElement>): void => {
      try {
        if (!e.target.files || !e.target.files[0]) return;
        const file = e.target.files[0];
        console.log(file);
        const reader = new FileReader();
        reader.onload = (e: any) => {
          try {
            const parsed: unknown = JSON.parse(e.target.result);
            if (context.emulator.command.signals.length > 0) {
              if (
                !window.confirm(
                  "Discard the existing command and import the new one?"
                )
              )
                return;
            }
            if (!isValidMacroSignals(parsed))
              return window.alert(
                "JSON format is incorrect."
              );
            const json = normalizeSignals(parsed);
            setContext((c) => ({
              ...c,
              emulator: {
                ...c.emulator,
                command: {
                  ...c.emulator.command,
                  signals: json,
                },
              },
            }));
          } catch (error) {
            window.alert(`Failed to import.\n\n${error}`);
          }
        };
        reader.readAsText(file);
        e.target.value = "";
      } catch (error) {
        console.error(error);
      }
    },
    [context.emulator.command.signals.length, setContext]
  );

  const clear = React.useCallback(() => {
    setContext((c) => ({
      ...c,
      emulator: {
        ...c.emulator,
        command: {
          ...c.emulator.command,
          signals: [],
        },
      },
    }));
  }, [setContext]);

  return {
    rec,
    stopRec,
    play,
    stopPlay,
    save,
    share,
    download,
    upload,
    exportArduino,
    exportJson,
    onChangeInputFile,
    recorderStart,
    recorderStop,
    clear,
  };
};
