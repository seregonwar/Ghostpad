import React from "react";
import { Axis } from "react-gamepad";

export type ButtonBindings = Record<number, string>;

export type StickKeyBindings = {
  lUp: string; lDown: string; lLeft: string; lRight: string;
  rUp: string; rDown: string; rLeft: string; rRight: string;
};

export type MouseLookConfig = {
  enabled: boolean;
  stick: "left" | "right";
  sensitivity: number;
};

export type KeyBindings = {
  buttons: ButtonBindings;
  stickKeys: StickKeyBindings;
  mouseLook: MouseLookConfig;
};

const STORAGE_KEY = "ghostpad:keybindings";

// Matches Python ghostpad_gui.py KEY_BINDINGS defaults
export const DEFAULT_BINDINGS: KeyBindings = {
  buttons: {
    0: "l",          // Circle
    1: "k",          // Cross
    2: "i",          // Triangle
    3: "j",          // Square
    4: "u",          // L1
    5: "o",          // R1
    6: "q",          // L2
    7: "e",          // R2
    8: "Backspace",  // Create / Share
    9: "Enter",      // Options
    10: "",          // L3
    11: "",          // R3
    12: "w",         // D-Up
    13: "s",         // D-Down
    14: "a",         // D-Left
    15: "d",         // D-Right
    16: " ",         // PS
    17: "t",         // Touchpad
  },
  stickKeys: {
    lUp: "", lDown: "", lLeft: "", lRight: "",
    rUp: "", rDown: "", rLeft: "", rRight: "",
  },
  mouseLook: { enabled: false, stick: "right", sensitivity: 5 },
};

function load(): KeyBindings {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) {
      const p = JSON.parse(raw);
      return {
        buttons: { ...DEFAULT_BINDINGS.buttons, ...p.buttons },
        stickKeys: { ...DEFAULT_BINDINGS.stickKeys, ...p.stickKeys },
        mouseLook: { ...DEFAULT_BINDINGS.mouseLook, ...p.mouseLook },
      };
    }
  } catch (_) {}
  return { ...DEFAULT_BINDINGS, buttons: { ...DEFAULT_BINDINGS.buttons }, stickKeys: { ...DEFAULT_BINDINGS.stickKeys }, mouseLook: { ...DEFAULT_BINDINGS.mouseLook } };
}

function persist(b: KeyBindings) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(b));
}

export function useInputRedirect(
  onPush: (button: number, state: boolean) => void,
  axisChangeHandler: (axis: Axis, value: number) => void,
  isListening: boolean // true while rebinding a key — pause pad input
) {
  const [bindings, _setBindings] = React.useState<KeyBindings>(load);

  // Refs so event listeners never go stale
  const bindingsRef = React.useRef(bindings);
  const onPushRef = React.useRef(onPush);
  const axisRef = React.useRef(axisChangeHandler);
  const listeningRef = React.useRef(isListening);
  const heldRef = React.useRef(new Set<string>());
  const decayRef = React.useRef<ReturnType<typeof setTimeout> | null>(null);

  React.useEffect(() => { bindingsRef.current = bindings; }, [bindings]);
  React.useEffect(() => { onPushRef.current = onPush; }, [onPush]);
  React.useEffect(() => { axisRef.current = axisChangeHandler; }, [axisChangeHandler]);
  React.useEffect(() => { listeningRef.current = isListening; }, [isListening]);

  const setBindings = React.useCallback(
    (updater: KeyBindings | ((prev: KeyBindings) => KeyBindings)) => {
      _setBindings((prev) => {
        const next = typeof updater === "function" ? updater(prev) : updater;
        persist(next);
        return next;
      });
    },
    []
  );

  // Recompute and send both stick axes from held-key state
  const sendSticks = React.useCallback(() => {
    const sk = bindingsRef.current.stickKeys;
    const held = heldRef.current;
    const get = (k: string) => (k !== "" && held.has(k) ? 1 : 0);

    const lx = get(sk.lRight) - get(sk.lLeft);
    const ly = get(sk.lUp) - get(sk.lDown);
    const rx = get(sk.rRight) - get(sk.rLeft);
    const ry = get(sk.rUp) - get(sk.rDown);

    axisRef.current("LeftStickX", lx);
    axisRef.current("LeftStickY", ly);   // inverted inside axisChangeHandler for axis 19
    axisRef.current("RightStickX", rx);
    axisRef.current("RightStickY", ry);  // inverted inside axisChangeHandler for axis 21
  }, []);

  // Keyboard input
  React.useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      if (listeningRef.current) return;
      const tgt = e.target as HTMLElement;
      if (tgt.tagName === "INPUT" || tgt.tagName === "TEXTAREA" || tgt.tagName === "SELECT") return;
      if (e.repeat) return;

      const key = e.key;
      const b = bindingsRef.current;
      let consumed = false;

      // Button bindings
      for (const [idStr, bk] of Object.entries(b.buttons)) {
        if (bk && key === bk) {
          onPushRef.current(Number(idStr), true);
          consumed = true;
        }
      }

      // Stick direction bindings
      const sk = b.stickKeys;
      const stickKeySet = new Set([sk.lUp, sk.lDown, sk.lLeft, sk.lRight, sk.rUp, sk.rDown, sk.rLeft, sk.rRight].filter(Boolean));
      if (stickKeySet.has(key)) consumed = true;

      if (consumed) {
        heldRef.current.add(key);
        sendSticks();
        e.preventDefault();
      }
    };

    const onKeyUp = (e: KeyboardEvent) => {
      const key = e.key;
      const b = bindingsRef.current;

      for (const [idStr, bk] of Object.entries(b.buttons)) {
        if (bk && key === bk) {
          onPushRef.current(Number(idStr), false);
        }
      }

      heldRef.current.delete(key);
      sendSticks();
    };

    document.addEventListener("keydown", onKeyDown);
    document.addEventListener("keyup", onKeyUp);
    return () => {
      document.removeEventListener("keydown", onKeyDown);
      document.removeEventListener("keyup", onKeyUp);
    };
  }, [sendSticks]);

  // Mouse look
  React.useEffect(() => {
    if (!bindings.mouseLook.enabled) return;

    const axisX: Axis = bindings.mouseLook.stick === "right" ? "RightStickX" : "LeftStickX";
    const axisY: Axis = bindings.mouseLook.stick === "right" ? "RightStickY" : "LeftStickY";
    // pixels of mouse movement that equals full stick deflection
    const fullPx = 60 / bindings.mouseLook.sensitivity;

    const onMove = (e: MouseEvent) => {
      if (listeningRef.current) return;
      const nx = Math.max(-1, Math.min(1, e.movementX / fullPx));
      const ny = Math.max(-1, Math.min(1, e.movementY / fullPx));
      axisRef.current(axisX, nx);
      axisRef.current(axisY, -ny); // negate: mouse down → positive movementY → stick push down → need value < 0 before convert inversion

      if (decayRef.current) clearTimeout(decayRef.current);
      decayRef.current = setTimeout(() => {
        axisRef.current(axisX, 0);
        axisRef.current(axisY, 0);
      }, 80);
    };

    document.addEventListener("mousemove", onMove);
    return () => {
      document.removeEventListener("mousemove", onMove);
      if (decayRef.current) clearTimeout(decayRef.current);
      axisRef.current(axisX, 0);
      axisRef.current(axisY, 0);
    };
  }, [bindings.mouseLook.enabled, bindings.mouseLook.stick, bindings.mouseLook.sensitivity]);

  return { bindings, setBindings };
}
