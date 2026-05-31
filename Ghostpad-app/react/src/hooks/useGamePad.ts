import React from "react";
// Hooks
import { Context } from "./Provider";
import { useNetwork } from "./useNetwork";
// Interfaces
import { ContextProps } from "../interfaces";
// Configs
import { ProControllerConfig } from "../configs/controller";
import { Axis } from "react-gamepad";

export const useGamePad = () => {
  const [context, setContext] = React.useContext(Context);
  const { scheduleSend } = useNetwork();

  const sendToDevice = React.useCallback(
    async (data: Uint8Array): Promise<void> => {
      try {
        setContext((c: ContextProps) => {
          if (c.emulator.state !== "recording") return c;
          const startedAt = c.emulator.recordingStartedAt ?? performance.now();
          return {
            ...c,
            emulator: {
              ...c.emulator,
              command: {
                ...c.emulator.command,
                signals: c.emulator.command.signals?.concat([
                  {
                    t: Number(((performance.now() - startedAt) / 1000).toFixed(3)),
                    s: [data[0], data[1]],
                  },
                ]),
              },
            },
          };
        });
        if (data[0] === 99) return;
      } catch (error) {
        console.error(error);
      }
    },
    [setContext]
  );

  const pushPadState = React.useCallback(
    (
      buttonStates: ContextProps["gamePad"]["buttonStates"],
      stickStates: ContextProps["gamePad"]["stickStates"]
    ) => {
      if (context.network.isConnected) {
        scheduleSend(buttonStates, stickStates);
      }
    },
    [context.network.isConnected, scheduleSend]
  );

  const connectHandler = React.useCallback(
    (gamepadIndex: number): void => {
      console.log(`Gamepad connected: ${gamepadIndex}`);
      setContext((c: ContextProps) => ({
        ...c,
        gamePad: { ...c.gamePad, isConnected: true },
      }));
    },
    [setContext]
  );

  const disconnectHandler = React.useCallback(
    (gamepadIndex: number): void => {
      console.log(`Gamepad disconnected: ${gamepadIndex}`);
      setContext((c: ContextProps) => ({
        ...c,
        gamePad: { ...c.gamePad, isConnected: false },
      }));
    },
    [setContext]
  );

  const onPush = React.useCallback(
    async (button: number, state: boolean | number): Promise<void> => {
      try {
        const data = new Uint8Array(2);
        data[0] = button;
        data[1] =
          typeof state === "number"
            ? Math.max(0, Math.min(255, Math.round(state)))
            : state
            ? 1
            : 0;
        await sendToDevice(data);
        setContext((c: ContextProps) => {
          const buttonStates = {
            ...c.gamePad.buttonStates,
            [button]: typeof state === "number" ? data[1] : state,
          };
          pushPadState(buttonStates, c.gamePad.stickStates);
          return {
            ...c,
            gamePad: {
              ...c.gamePad,
              buttonStates,
            },
          };
        });
      } catch (error) {
        console.error(error);
      }
    },
    [pushPadState, sendToDevice, setContext]
  );

  const onRelease = React.useCallback(
    async (button: number): Promise<void> => {
      try {
        if (context.gamePad.buttonStates[button]) {
          const data = new Uint8Array(2);
          data[0] = button;
          data[1] = 0;
          await sendToDevice(data);
          setContext((c: ContextProps) => {
            const buttonStates = {
              ...c.gamePad.buttonStates,
              [button]: false,
            };
            pushPadState(buttonStates, c.gamePad.stickStates);
            return {
              ...c,
              gamePad: {
                ...c.gamePad,
                buttonStates,
              },
            };
          });
        }
      } catch (error) {
        console.error(error);
      }
    },
    [context.gamePad.buttonStates, pushPadState, sendToDevice, setContext]
  );

  const onTilt = React.useCallback(
    async (stick: number, val: number): Promise<void> => {
      try {
        if (stick !== 18 && stick !== 19 && stick !== 20 && stick !== 21)
          return;
        const data = new Uint8Array(2);
        data[0] = stick;
        data[1] = val;
        setContext((c: ContextProps) => {
          if (c.gamePad.stickStates[stick] !== val) {
            sendToDevice(data);
            const stickStates = {
              ...c.gamePad.stickStates,
              [stick]: val,
            };
            pushPadState(c.gamePad.buttonStates, stickStates);
            return {
              ...c,
              gamePad: {
                ...c.gamePad,
                stickStates,
              },
            };
          }
          return c;
        });
      } catch (error) {
        console.error(error);
      }
    },
    [pushPadState, sendToDevice, setContext]
  );

  const buttonChangeHandler = React.useCallback(
    (buttonName: string | number, down: boolean): void => {
      try {
        if (buttonName !== "LeftTrigger" && buttonName !== "RightTrigger") {
          const buttonNumber = ProControllerConfig[buttonName];
          onPush(buttonNumber, down);
        }
      } catch (error) {
        console.error(error);
      }
    },
    [onPush]
  );

  const convert = React.useCallback((x: number): number => {
    const val = x > 0.08 || x < -0.08 ? (256 * (x + 1)) / 2 : 128;
    if (val !== 69 && val !== 83) return val;
    else return val + 1;
  }, []);

  const axisChangeHandler = React.useCallback(
    (axisName: Axis, value: number): void => {
      try {
        const stickNumber = ProControllerConfig[axisName];
        if (axisName === "LeftTrigger" || axisName === "RightTrigger") {
          onPush(stickNumber, value * 255);
          return;
        }
        const convertedValue =
          stickNumber === 19 || stickNumber === 21
            ? convert(value * -1)
            : convert(value);
        const clamped = Math.max(0, Math.min(255, Math.round(convertedValue)));
        onTilt(stickNumber, clamped);
      } catch (error) {
        console.error(error);
      }
    },
    [convert, onPush, onTilt]
  );

  const neutral = React.useCallback(() => {
    setContext((c: ContextProps) => {
      const buttonStates = Object.fromEntries(
        Array.from({ length: 18 }, (_, id) => [id, false])
      );
      const stickStates = { 18: 128, 19: 128, 20: 128, 21: 128 };
      pushPadState(buttonStates, stickStates);
      return {
        ...c,
        gamePad: { ...c.gamePad, buttonStates, stickStates },
      };
    });
  }, [pushPadState, setContext]);

  return {
    connectHandler,
    disconnectHandler,
    buttonChangeHandler,
    axisChangeHandler,
    onPush,
    onTilt,
    onRelease,
    neutral,
  };
};
