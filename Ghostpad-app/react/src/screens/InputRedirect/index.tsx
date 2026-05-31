import React from "react";
import { Link } from "react-router-dom";
import Gamepad from "react-gamepad";
import styled, { keyframes, css } from "styled-components";
import { Context } from "../../hooks/Provider";
import { useGamePad } from "../../hooks/useGamePad";
import { useNetwork } from "../../hooks/useNetwork";
import {
  useInputRedirect,
  DEFAULT_BINDINGS,
  StickKeyBindings,
} from "../../hooks/useInputRedirect";
import { SavedConsole, ScanResult } from "../../configs/ghostpadApi";
import { ProControllerButtonNames, ProControllerConfig } from "../../configs/controller";
import { PadVisualizer } from "../Project/components/PadVisualizer";
import { TheHeader } from "../../ui/systems/Navigation/TheHeader";
import { Button } from "../../ui/parts/Button/Button";
import { Colors } from "../../styles/Colors";

type ListeningFor =
  | { type: "button"; id: number }
  | { type: "stick"; dir: keyof StickKeyBindings }
  | null;

const BUTTON_IDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17] as const;

const GAMEPAD_REMAP_KEY = "ghostpad:gamepad-remap";

function loadGamepadRemap(): Record<string, number> {
  try {
    const raw = localStorage.getItem(GAMEPAD_REMAP_KEY);
    if (raw) return JSON.parse(raw);
  } catch {}
  return {};
}

function saveGamepadRemap(v: Record<string, number>): void {
  localStorage.setItem(GAMEPAD_REMAP_KEY, JSON.stringify(v));
}

const DEFAULT_GPAD_TRIGGER: Record<number, string> = Object.entries(ProControllerConfig)
  .filter(([, id]) => (id as number) < 18)
  .reduce((acc, [name, id]) => ({ ...acc, [id as number]: name }), {} as Record<number, string>);

const STICK_DIRS: { dir: keyof StickKeyBindings; label: string }[] = [
  { dir: "lUp", label: "L↑" },
  { dir: "lDown", label: "L↓" },
  { dir: "lLeft", label: "L←" },
  { dir: "lRight", label: "L→" },
  { dir: "rUp", label: "R↑" },
  { dir: "rDown", label: "R↓" },
  { dir: "rLeft", label: "R←" },
  { dir: "rRight", label: "R→" },
];

function displayKey(key: string): string {
  if (!key) return "—";
  if (key === " ") return "Space";
  if (key === "ArrowUp") return "↑";
  if (key === "ArrowDown") return "↓";
  if (key === "ArrowLeft") return "←";
  if (key === "ArrowRight") return "→";
  if (key === "Backspace") return "⌫";
  if (key === "Enter") return "↵";
  if (key === "Escape") return "Esc";
  if (key === "Tab") return "Tab";
  if (key === "Delete") return "Del";
  return key.length === 1 ? key.toUpperCase() : key;
}

function shortGamepadId(id: string): string {
  const m = id.match(/^([^(]+)/);
  return m ? m[1].trim() : id;
}

export const InputRedirect = () => {
  const [context] = React.useContext(Context);
  const { connect, disconnect, listConsoles, addConsole, updateConsole, deleteConsole, scanNetwork } = useNetwork();
  const {
    connectHandler,
    disconnectHandler,
    buttonChangeHandler,
    axisChangeHandler,
    onPush,
    onRelease,
  } = useGamePad();

  const [consoles, setConsoles] = React.useState<SavedConsole[]>([]);
  const [targetConsoleId, setTargetConsoleId] = React.useState("");
  const [busy, setBusy] = React.useState(false);
  const [showBindings, setShowBindings] = React.useState(false);
  const [listeningFor, setListeningFor] = React.useState<ListeningFor>(null);
  const [listeningForGamepadBtn, setListeningForGamepadBtn] = React.useState<number | null>(null);
  const listeningForGamepadBtnRef = React.useRef(listeningForGamepadBtn);
  React.useEffect(() => { listeningForGamepadBtnRef.current = listeningForGamepadBtn; }, [listeningForGamepadBtn]);
  const [gamepadRemap, setGamepadRemap] = React.useState<Record<string, number>>(loadGamepadRemap);
  const gamepadRemapRef = React.useRef(gamepadRemap);
  React.useEffect(() => { gamepadRemapRef.current = gamepadRemap; }, [gamepadRemap]);

  // XInput gamepad selection
  const [availableGamepads, setAvailableGamepads] = React.useState<globalThis.Gamepad[]>([]);
  const [xinputIndex, setXinputIndex] = React.useState(0);

  // Console manager modal
  const [showConsoleManager, setShowConsoleManager] = React.useState(false);
  const [editingId, setEditingId] = React.useState<string | null>(null);
  const [editName, setEditName] = React.useState("");
  const [editIp, setEditIp] = React.useState("");
  const [editPort, setEditPort] = React.useState(6967);
  const [addName, setAddName] = React.useState("");
  const [addIp, setAddIp] = React.useState("");
  const [scanResults, setScanResults] = React.useState<ScanResult[]>([]);
  const [scanning, setScanning] = React.useState(false);
  const [cmBusy, setCmBusy] = React.useState(false);

  // Auto-clicker
  const [autoClickOn, setAutoClickOn] = React.useState(false);
  const [autoClickButtons, setAutoClickButtons] = React.useState<Set<number>>(new Set([1]));
  const [autoClickHoldMs, setAutoClickHoldMs] = React.useState(50);
  const [autoClickGapMs, setAutoClickGapMs] = React.useState(100);
  const [showAutoClick, setShowAutoClick] = React.useState(false);
  const onPushRef = React.useRef(onPush);
  React.useEffect(() => { onPushRef.current = onPush; }, [onPush]);

  const remappedButtonChangeHandler = React.useCallback(
    (buttonName: string | number, down: boolean): void => {
      if (listeningForGamepadBtnRef.current !== null && down && typeof buttonName === "string") {
        const ps5Id = listeningForGamepadBtnRef.current;
        setGamepadRemap((prev) => {
          const next = { ...prev, [buttonName]: ps5Id };
          saveGamepadRemap(next);
          return next;
        });
        setListeningForGamepadBtn(null);
        return;
      }
      const remap = gamepadRemapRef.current;
      if (typeof buttonName === "string" && buttonName in remap) {
        onPush(remap[buttonName], down);
      } else {
        buttonChangeHandler(buttonName, down);
      }
    },
    [buttonChangeHandler, onPush]
  );

  React.useEffect(() => {
    if (listeningForGamepadBtn === null) return;
    const handler = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        e.preventDefault();
        e.stopPropagation();
        setListeningForGamepadBtn(null);
      }
    };
    document.addEventListener("keydown", handler, { capture: true });
    return () => document.removeEventListener("keydown", handler, { capture: true });
  }, [listeningForGamepadBtn]);

  const { bindings, setBindings } = useInputRedirect(
    (button, state) => onPush(button, state),
    axisChangeHandler,
    listeningFor !== null || listeningForGamepadBtn !== null
  );

  React.useEffect(() => {
    listConsoles().then(setConsoles);
  }, [listConsoles]);

  React.useEffect(() => {
    if (context.network.activeConsoleId) {
      setTargetConsoleId(context.network.activeConsoleId);
    } else if (context.network.ip && !targetConsoleId) {
      const match = consoles.find((c) => c.ip === context.network.ip);
      if (match) setTargetConsoleId(match.id);
    }
  }, [consoles, context.network.activeConsoleId, context.network.ip, targetConsoleId]);

  // Enumerate XInput gamepads.
  // Prefers the Electron IPC path (no button-press activation needed).
  // Falls back to the Web Gamepad API for non-Electron environments.
  const refreshGamepads = React.useCallback(async () => {
    const api = (window as any).ghostpad;
    if (api?.listGamepads) {
      try {
        const pads = await api.listGamepads() as { index: number; name: string }[];
        if (pads.length > 0) {
          setAvailableGamepads(
            pads.map((p) => ({ index: p.index, id: p.name } as unknown as globalThis.Gamepad))
          );
          return;
        }
      } catch {}
    }
    // Web Gamepad API fallback (requires button press to activate in Chromium)
    const pads = Array.from(
      navigator.getGamepads ? navigator.getGamepads() : []
    ).filter((p): p is globalThis.Gamepad => p !== null);
    setAvailableGamepads(pads);
  }, []);

  React.useEffect(() => {
    const onEvent = () => setTimeout(refreshGamepads, 100);
    refreshGamepads();
    window.addEventListener("gamepadconnected", onEvent);
    window.addEventListener("gamepaddisconnected", onEvent);
    const poll = setInterval(refreshGamepads, 2000);
    return () => {
      window.removeEventListener("gamepadconnected", onEvent);
      window.removeEventListener("gamepaddisconnected", onEvent);
      clearInterval(poll);
    };
  }, [refreshGamepads]);

  const selectedConsole =
    consoles.find((c) => c.id === targetConsoleId) ||
    (context.network.ip
      ? { id: "", name: "Connected", ip: context.network.ip, port: 6967 }
      : null);

  const targetIp = selectedConsole?.ip || context.network.ip || "";

  const handleConnect = async () => {
    if (!targetIp) { window.alert("Select a console first."); return; }
    setBusy(true);
    try {
      await connect(targetIp, 6967, targetConsoleId || undefined);
    } finally {
      setBusy(false);
    }
  };

  const handleDisconnect = async () => {
    setBusy(true);
    try { await disconnect(); } finally { setBusy(false); }
  };

  // Console manager handlers
  const refreshConsoles = React.useCallback(
    () => listConsoles().then(setConsoles),
    [listConsoles]
  );

  const cmStartEdit = (c: SavedConsole) => {
    setEditingId(c.id);
    setEditName(c.name);
    setEditIp(c.ip);
    setEditPort(c.port);
  };

  const cmSaveEdit = async () => {
    if (!editingId) return;
    setCmBusy(true);
    try {
      await updateConsole(editingId, { name: editName.trim(), ip: editIp.trim(), port: editPort });
      setEditingId(null);
      await refreshConsoles();
    } finally { setCmBusy(false); }
  };

  const cmDelete = async (id: string) => {
    setCmBusy(true);
    try {
      await deleteConsole(id);
      await refreshConsoles();
    } finally { setCmBusy(false); }
  };

  const cmAdd = async () => {
    if (!addName.trim() || !addIp.trim()) return;
    setCmBusy(true);
    try {
      await addConsole(addName.trim(), addIp.trim(), 6967);
      setAddName("");
      setAddIp("");
      await refreshConsoles();
    } finally { setCmBusy(false); }
  };

  const cmScan = async () => {
    setScanning(true);
    setScanResults([]);
    try {
      const results = await scanNetwork();
      setScanResults(results);
    } finally { setScanning(false); }
  };

  const cmAddFromScan = async (ip: string) => {
    setCmBusy(true);
    try {
      await addConsole(`PS5 (${ip})`, ip, 6967);
      await refreshConsoles();
    } finally { setCmBusy(false); }
  };

  // Close console manager on ESC
  React.useEffect(() => {
    if (!showConsoleManager) return;
    const handler = (e: KeyboardEvent) => { if (e.key === "Escape") setShowConsoleManager(false); };
    document.addEventListener("keydown", handler);
    return () => document.removeEventListener("keydown", handler);
  }, [showConsoleManager]);

  // Lock body scroll when modal open
  React.useEffect(() => {
    document.body.style.overflow = showConsoleManager ? "hidden" : "";
    return () => { document.body.style.overflow = ""; };
  }, [showConsoleManager]);

  // Key capture for rebinding
  React.useEffect(() => {
    if (!listeningFor) return;
    const handler = (e: KeyboardEvent) => {
      e.preventDefault();
      e.stopPropagation();
      if (e.key === "Escape") { setListeningFor(null); return; }
      const key = e.key;
      if (listeningFor.type === "button") {
        const id = listeningFor.id;
        setBindings((prev) => ({ ...prev, buttons: { ...prev.buttons, [id]: key } }));
      } else {
        const dir = listeningFor.dir;
        setBindings((prev) => ({ ...prev, stickKeys: { ...prev.stickKeys, [dir]: key } }));
      }
      setListeningFor(null);
    };
    document.addEventListener("keydown", handler, { capture: true });
    return () => document.removeEventListener("keydown", handler, { capture: true });
  }, [listeningFor, setBindings]);

  // Pointer lock for mouse look
  const padAreaRef = React.useRef<HTMLDivElement>(null);
  const isMouseLock = bindings.mouseLook.enabled;

  const handlePadAreaClick = () => {
    if (isMouseLock && padAreaRef.current && !document.pointerLockElement) {
      padAreaRef.current.requestPointerLock();
    }
  };

  React.useEffect(() => {
    if (!isMouseLock && document.pointerLockElement) document.exitPointerLock();
  }, [isMouseLock]);

  // Auto-clicker loop
  React.useEffect(() => {
    if (!autoClickOn || autoClickButtons.size === 0) return;
    let cancelled = false;
    const buttons = Array.from(autoClickButtons);

    const loop = () => {
      if (cancelled) return;
      buttons.forEach((b) => onPushRef.current(b, true));
      setTimeout(() => {
        if (cancelled) return;
        buttons.forEach((b) => onPushRef.current(b, false));
        setTimeout(() => { if (!cancelled) loop(); }, autoClickGapMs);
      }, autoClickHoldMs);
    };

    loop();

    return () => {
      cancelled = true;
      Array.from(autoClickButtons).forEach((b) => onPushRef.current(b, false));
    };
  }, [autoClickOn, autoClickButtons, autoClickHoldMs, autoClickGapMs]);

  const cps = (1000 / (autoClickHoldMs + autoClickGapMs)).toFixed(1);

  return (
    <>
      <TheHeader title="Ghostpad" buttonUrl="https://github.com/StonedModder" />
      <Page>

        {/* ── Connection Card (2-column) ──────────────────────────────── */}
        <SectionCard>
          <ConnectionColumns>
            {/* Left: Console Selection */}
            <ConnectionColumn>
              <SectionCardLabelRow>
                <SectionCardLabel>Console Selection</SectionCardLabel>
                <GearBtn onClick={() => setShowConsoleManager(true)} title="Manage consoles">⚙</GearBtn>
              </SectionCardLabelRow>
              <ConnectionRow>
                <StyledSelect
                  value={targetConsoleId}
                  onChange={(e) => setTargetConsoleId(e.target.value)}
                >
                  <option value="">Select console…</option>
                  {consoles.map((c) => (
                    <option key={c.id} value={c.id}>
                      {c.name} ({c.ip})
                    </option>
                  ))}
                </StyledSelect>
                <Button
                  color="primary"
                  text={busy ? "…" : "Connect"}
                  icon="none"
                  size="s"
                  onClick={handleConnect}
                  isInactive={busy || !targetIp || context.network.isConnected}
                />
                <Button
                  color="outline"
                  text="Disconnect"
                  icon="none"
                  size="s"
                  onClick={handleDisconnect}
                  isInactive={busy || !context.network.isConnected}
                />
                <StatusChip $connected={context.network.isConnected}>
                  <StatusDot $connected={context.network.isConnected} />
                  {context.network.isConnected ? context.network.ip : "Not connected"}
                </StatusChip>
              </ConnectionRow>
            </ConnectionColumn>

            <ColumnDivider />

            {/* Right: XInput */}
            <ConnectionColumn>
              <SectionCardLabel>Controller (XInput)</SectionCardLabel>
              <ConnectionRow>
                {availableGamepads.length === 0 ? (
                  <NoGamepadNote>No controllers detected</NoGamepadNote>
                ) : (
                  <StyledSelect
                    value={xinputIndex}
                    onChange={(e) => setXinputIndex(Number(e.target.value))}
                  >
                    {availableGamepads.map((gp) => (
                      <option key={gp.index} value={gp.index}>
                        {shortGamepadId(gp.id)} (#{gp.index})
                      </option>
                    ))}
                  </StyledSelect>
                )}
                <RefreshBtn onClick={refreshGamepads} title="Refresh controllers">↻</RefreshBtn>
                <StatusChip $connected={context.gamePad.isConnected}>
                  <StatusDot $connected={context.gamePad.isConnected} />
                  {context.gamePad.isConnected ? "Active" : "Inactive"}
                </StatusChip>
              </ConnectionRow>
            </ConnectionColumn>
          </ConnectionColumns>
        </SectionCard>

        {/* ── Pad + Bindings Card ──────────────────────────────────────── */}
        <PadCard>
          {isMouseLock && !document.pointerLockElement && (
            <MouseHint onClick={handlePadAreaClick}>
              Click to lock mouse for look input
            </MouseHint>
          )}

          <PadWrap
            ref={padAreaRef}
            onClick={handlePadAreaClick}
            $mouseLock={isMouseLock}
          >
            <PadVisualizer
              onPush={onPush}
              onRelease={onRelease}
              onTilt={axisChangeHandler}
              showSmall={false}
            />
            <Gamepad
              gamepadIndex={xinputIndex}
              onConnect={connectHandler}
              onDisconnect={disconnectHandler}
              onButtonChange={remappedButtonChangeHandler}
              onAxisChange={axisChangeHandler}
            >
              <React.Fragment />
            </Gamepad>
          </PadWrap>

          {/* Inline Bindings accordion */}
          <BindingsToggle onClick={() => setShowBindings((v) => !v)}>
            <BindingsToggleLeft>
              <span>Bindings</span>
            </BindingsToggleLeft>
            <AccordionChevron $open={showBindings} />
          </BindingsToggle>

          {showBindings && (
            <BindingsContent>
              {/* Mouse look */}
              <MouseLookRow>
                <ControlLabel>
                  <input
                    type="checkbox"
                    checked={bindings.mouseLook.enabled}
                    onChange={(e) =>
                      setBindings((prev) => ({
                        ...prev,
                        mouseLook: { ...prev.mouseLook, enabled: e.target.checked },
                      }))
                    }
                  />
                  Mouse Look
                </ControlLabel>
                <ControlLabel>
                  Stick:
                  <StyledSelectSm
                    value={bindings.mouseLook.stick}
                    onChange={(e) =>
                      setBindings((prev) => ({
                        ...prev,
                        mouseLook: { ...prev.mouseLook, stick: e.target.value as "left" | "right" },
                      }))
                    }
                  >
                    <option value="right">Right</option>
                    <option value="left">Left</option>
                  </StyledSelectSm>
                </ControlLabel>
                <ControlLabel>
                  Sensitivity: <SensValue>{bindings.mouseLook.sensitivity}</SensValue>
                  <input
                    type="range" min={1} max={20} value={bindings.mouseLook.sensitivity}
                    onChange={(e) =>
                      setBindings((prev) => ({
                        ...prev,
                        mouseLook: { ...prev.mouseLook, sensitivity: Number(e.target.value) },
                      }))
                    }
                  />
                </ControlLabel>
              </MouseLookRow>

              <BindSection>
                <BindSectionTitle>Buttons</BindSectionTitle>
                <BindGrid>
                  {BUTTON_IDS.map((id) => {
                    const isActive = listeningFor?.type === "button" && listeningFor.id === id;
                    return (
                      <BindCell
                        key={id}
                        $listening={isActive}
                        onClick={() => setListeningFor(isActive ? null : { type: "button", id })}
                        title="Click to rebind. ESC to cancel."
                      >
                        <BindLabel>{ProControllerButtonNames[id]}</BindLabel>
                        <BindKey $empty={!bindings.buttons[id]}>
                          {isActive ? "…" : displayKey(bindings.buttons[id] ?? "")}
                        </BindKey>
                      </BindCell>
                    );
                  })}
                </BindGrid>
              </BindSection>

              <BindSection>
                <BindSectionTitle>Stick Keys</BindSectionTitle>
                <BindGrid>
                  {STICK_DIRS.map(({ dir, label }) => {
                    const isActive = listeningFor?.type === "stick" && listeningFor.dir === dir;
                    return (
                      <BindCell
                        key={dir}
                        $listening={isActive}
                        onClick={() => setListeningFor(isActive ? null : { type: "stick", dir })}
                        title="Click to rebind. ESC to cancel."
                      >
                        <BindLabel>{label}</BindLabel>
                        <BindKey $empty={!bindings.stickKeys[dir]}>
                          {isActive ? "…" : displayKey(bindings.stickKeys[dir])}
                        </BindKey>
                      </BindCell>
                    );
                  })}
                </BindGrid>
              </BindSection>

              <BindSection>
                <BindSectionTitle>Controller Button Remap</BindSectionTitle>
                <BindGrid>
                  {(BUTTON_IDS.filter((id) => id < 16) as number[]).map((id) => {
                    const currentTrigger =
                      Object.entries(gamepadRemap).find(([, v]) => v === id)?.[0] ??
                      DEFAULT_GPAD_TRIGGER[id];
                    const isActive = listeningForGamepadBtn === id;
                    return (
                      <BindCell
                        key={id}
                        $listening={isActive}
                        onClick={() => {
                          setListeningFor(null);
                          setListeningForGamepadBtn(isActive ? null : id);
                        }}
                        title="Click then press a controller button. ESC to cancel."
                      >
                        <BindLabel>{ProControllerButtonNames[id]}</BindLabel>
                        <BindKey $empty={!currentTrigger}>
                          {isActive ? "…" : (currentTrigger ?? "—")}
                        </BindKey>
                      </BindCell>
                    );
                  })}
                </BindGrid>
                {Object.keys(gamepadRemap).length > 0 && (
                  <ResetRow style={{ marginTop: 8 }}>
                    <Button
                      color="outline"
                      text="Reset Remap"
                      icon="none"
                      size="s"
                      onClick={() => { setGamepadRemap({}); saveGamepadRemap({}); }}
                    />
                  </ResetRow>
                )}
              </BindSection>

              <ResetRow>
                <Button
                  color="outline"
                  text="Reset to Defaults"
                  icon="none"
                  size="s"
                  onClick={() =>
                    setBindings({
                      buttons: { ...DEFAULT_BINDINGS.buttons },
                      stickKeys: { ...DEFAULT_BINDINGS.stickKeys },
                      mouseLook: { ...DEFAULT_BINDINGS.mouseLook },
                    })
                  }
                />
                {(listeningFor || listeningForGamepadBtn !== null) && (
                  <CancelHint>
                    {listeningFor ? "Press any key" : "Press controller button"} — ESC to cancel
                  </CancelHint>
                )}
              </ResetRow>
            </BindingsContent>
          )}
        </PadCard>

        {/* ── Auto Clicker ─────────────────────────────────────────────── */}
        <AccordionHeader onClick={() => setShowAutoClick((v) => !v)}>
          <AccordionLeft>
            <AccordionTitle>Auto Clicker</AccordionTitle>
            {autoClickOn && <ActiveBadge>ACTIVE</ActiveBadge>}
          </AccordionLeft>
          <AccordionChevron $open={showAutoClick} />
        </AccordionHeader>

        {showAutoClick && (
          <Panel>
            <ACRow>
              <ACLabel>Buttons to press</ACLabel>
              <ACButtonGrid>
                {BUTTON_IDS.map((id) => {
                  const selected = autoClickButtons.has(id);
                  return (
                    <ACButtonChip
                      key={id}
                      $selected={selected}
                      onClick={() => {
                        setAutoClickButtons((prev) => {
                          const next = new Set(prev);
                          if (next.has(id)) next.delete(id); else next.add(id);
                          return next;
                        });
                      }}
                    >
                      {ProControllerButtonNames[id]}
                    </ACButtonChip>
                  );
                })}
              </ACButtonGrid>
            </ACRow>

            <ACTimingRow>
              <ACTimingField>
                <ACLabel>Hold duration</ACLabel>
                <ACInputRow>
                  <ACNumInput
                    type="number"
                    min={16}
                    max={2000}
                    value={autoClickHoldMs}
                    onChange={(e) => setAutoClickHoldMs(Math.max(16, Number(e.target.value)))}
                  />
                  <ACUnit>ms</ACUnit>
                </ACInputRow>
                <input type="range" min={16} max={500} value={autoClickHoldMs}
                  onChange={(e) => setAutoClickHoldMs(Number(e.target.value))} />
              </ACTimingField>
              <ACTimingField>
                <ACLabel>Gap between presses</ACLabel>
                <ACInputRow>
                  <ACNumInput
                    type="number"
                    min={0}
                    max={5000}
                    value={autoClickGapMs}
                    onChange={(e) => setAutoClickGapMs(Math.max(0, Number(e.target.value)))}
                  />
                  <ACUnit>ms</ACUnit>
                </ACInputRow>
                <input type="range" min={0} max={1000} value={autoClickGapMs}
                  onChange={(e) => setAutoClickGapMs(Number(e.target.value))} />
              </ACTimingField>
              <ACCpsCard>
                <ACCpsValue>{cps}</ACCpsValue>
                <ACCpsLabel>CPS</ACCpsLabel>
              </ACCpsCard>
            </ACTimingRow>

            <ACStartRow>
              <Button
                color={autoClickOn ? "primary" : "outlinePrimary"}
                text={autoClickOn ? "Stop Auto Clicker" : "Start Auto Clicker"}
                icon="none"
                size="m"
                onClick={() => setAutoClickOn((v) => !v)}
                isInactive={autoClickButtons.size === 0 || !context.network.isConnected}
              />
              {autoClickButtons.size === 0 && (
                <ACHint>Select at least one button above</ACHint>
              )}
              {!context.network.isConnected && autoClickButtons.size > 0 && (
                <ACHint>Connect to PS5 first</ACHint>
              )}
            </ACStartRow>
          </Panel>
        )}

        <NavLinks>
          <Link to="/">← Home</Link>
          <Link to="/consoles">Consoles</Link>
          <Link to="/settings">Settings</Link>
          <Link to="/projects">Projects</Link>
        </NavLinks>
      </Page>

      {/* ── Console Manager Modal ─────────────────────────────────────── */}
      {showConsoleManager && (
        <ModalOverlay onClick={() => setShowConsoleManager(false)}>
          <ModalPanel onClick={(e) => e.stopPropagation()}>
            <ModalHeader>
              <ModalTitle>Manage Consoles</ModalTitle>
              <ModalClose onClick={() => setShowConsoleManager(false)}>✕</ModalClose>
            </ModalHeader>

            {/* Saved consoles */}
            <ModalSection>
              <ModalSectionTitle>Saved Consoles</ModalSectionTitle>
              {consoles.length === 0 && (
                <CmEmptyNote>No consoles saved yet.</CmEmptyNote>
              )}
              {consoles.map((c) =>
                editingId === c.id ? (
                  <CmEditRow key={c.id}>
                    <CmInput
                      value={editName}
                      onChange={(e) => setEditName(e.target.value)}
                      placeholder="Name"
                    />
                    <CmInput
                      value={editIp}
                      onChange={(e) => setEditIp(e.target.value)}
                      placeholder="IP address"
                    />
                    <CmPortInput
                      type="number"
                      value={editPort}
                      onChange={(e) => setEditPort(Number(e.target.value))}
                    />
                    <CmActionBtn $variant="save" onClick={cmSaveEdit} disabled={cmBusy}>
                      {cmBusy ? "…" : "Save"}
                    </CmActionBtn>
                    <CmActionBtn $variant="cancel" onClick={() => setEditingId(null)}>
                      Cancel
                    </CmActionBtn>
                  </CmEditRow>
                ) : (
                  <CmConsoleRow key={c.id}>
                    <CmConsoleInfo>
                      <CmConsoleName>{c.name}</CmConsoleName>
                      <CmConsoleIp>{c.ip}:{c.port}</CmConsoleIp>
                    </CmConsoleInfo>
                    <CmConsoleActions>
                      <CmActionBtn $variant="edit" onClick={() => cmStartEdit(c)}>Edit</CmActionBtn>
                      <CmActionBtn $variant="remove" onClick={() => cmDelete(c.id)} disabled={cmBusy}>
                        Remove
                      </CmActionBtn>
                    </CmConsoleActions>
                  </CmConsoleRow>
                )
              )}
            </ModalSection>

            {/* Add console */}
            <ModalSection>
              <ModalSectionTitle>Add Console</ModalSectionTitle>
              <CmAddRow>
                <CmInput
                  value={addName}
                  onChange={(e) => setAddName(e.target.value)}
                  placeholder="Name (e.g. My PS5)"
                />
                <CmInput
                  value={addIp}
                  onChange={(e) => setAddIp(e.target.value)}
                  placeholder="IP address"
                  onKeyDown={(e) => { if (e.key === "Enter") cmAdd(); }}
                />
                <CmActionBtn
                  $variant="save"
                  onClick={cmAdd}
                  disabled={cmBusy || !addName.trim() || !addIp.trim()}
                >
                  {cmBusy ? "…" : "Add"}
                </CmActionBtn>
              </CmAddRow>
            </ModalSection>

            {/* Scan */}
            <ModalSection $last>
              <CmSectionRow>
                <ModalSectionTitle>Auto-Scan LAN</ModalSectionTitle>
                <CmScanBtn onClick={cmScan} disabled={scanning}>
                  {scanning ? "Scanning…" : "Scan Network"}
                </CmScanBtn>
              </CmSectionRow>
              {scanning && (
                <CmScanNote>Searching your local network for Ghostpad hosts…</CmScanNote>
              )}
              {!scanning && scanResults.length === 0 && (
                <CmScanNote $muted>No results yet. Press Scan to search.</CmScanNote>
              )}
              {scanResults.map((r) => (
                <CmScanRow key={r.ip}>
                  <CmScanInfo>
                    <CmScanIp>{r.ip}</CmScanIp>
                    {r.hasGhostpad && <CmScanBadge>Ghostpad</CmScanBadge>}
                  </CmScanInfo>
                  <CmActionBtn
                    $variant="save"
                    onClick={() => cmAddFromScan(r.ip)}
                    disabled={cmBusy || consoles.some((c) => c.ip === r.ip)}
                  >
                    {consoles.some((c) => c.ip === r.ip) ? "Saved" : "Add"}
                  </CmActionBtn>
                </CmScanRow>
              ))}
            </ModalSection>
          </ModalPanel>
        </ModalOverlay>
      )}
    </>
  );
};

// ─── Animations ──────────────────────────────────────────────────────────────

const blink = keyframes`
  0%, 100% { opacity: 1; }
  50% { opacity: 0.4; }
`;

// ─── Layout ──────────────────────────────────────────────────────────────────

const Page = styled.main`
  margin: 60px auto 0;
  padding: 24px 32px 64px;
  max-width: 1100px;
  width: 100%;

  @media (max-width: 768px) {
    padding: 20px 20px 48px;
  }

  @media (max-width: 480px) {
    padding: 16px 12px 40px;
  }
`;

// ─── Connection Card ─────────────────────────────────────────────────────────

const SectionCard = styled.div`
  background: ${Colors.bgColorLv1};
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 12px;
  padding: 16px 20px;
  margin-bottom: 16px;
`;

const ConnectionColumns = styled.div`
  display: flex;
  align-items: stretch;
  gap: 0;

  @media (max-width: 720px) {
    flex-direction: column;
    gap: 16px;
  }
`;

const ConnectionColumn = styled.div`
  flex: 1;
  min-width: 0;

  &:first-child { padding-right: 20px; }
  &:last-child  { padding-left: 20px; }

  @media (max-width: 720px) {
    &:first-child { padding-right: 0; }
    &:last-child  { padding-left: 0; }
  }
`;

const ColumnDivider = styled.div`
  width: 1px;
  background: ${Colors.borderColorLv1};
  align-self: stretch;
  margin: 4px 0;
  flex-shrink: 0;

  @media (max-width: 720px) {
    display: none;
  }
`;

const SectionCardLabel = styled.div`
  font-size: 11px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  color: ${Colors.elementColorMute};
  margin-bottom: 10px;
`;

const ConnectionRow = styled.div`
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
`;

const StyledSelect = styled.select`
  height: 32px;
  padding: 0 28px 0 10px;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 6px;
  background: ${Colors.bgColorLv0};
  color: ${Colors.elementColorDefault};
  font-size: 13px;
  min-width: 200px;
  cursor: pointer;
  outline: none;
  appearance: none;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='8' viewBox='0 0 12 8'%3E%3Cpath d='M1 1l5 5 5-5' stroke='%23a0a0a8' stroke-width='1.5' fill='none' stroke-linecap='round'/%3E%3C/svg%3E");
  background-repeat: no-repeat;
  background-position: right 10px center;
  transition: border-color 0.15s;

  &:focus {
    border-color: ${Colors.brandColorPrimary};
    box-shadow: 0 0 0 2px ${Colors.brandColorPrimary}22;
  }

  option { background: ${Colors.bgColorLv2}; }
`;

const StyledSelectSm = styled(StyledSelect)`
  min-width: 90px;
  height: 28px;
  font-size: 12px;
`;

const StatusChip = styled.span<{ $connected: boolean }>`
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 4px 12px;
  border-radius: 20px;
  font-size: 12px;
  font-weight: 600;
  white-space: nowrap;
  background: ${({ $connected }) =>
    $connected ? Colors.statusColorSuccessWeak : Colors.bgColorLv2};
  color: ${({ $connected }) =>
    $connected ? Colors.statusColorSuccess : Colors.elementColorWeak};
  border: 1px solid ${({ $connected }) =>
    $connected ? Colors.statusColorSuccess : Colors.borderColorLv1};
`;

const StatusDot = styled.span<{ $connected: boolean }>`
  width: 6px;
  height: 6px;
  border-radius: 50%;
  flex-shrink: 0;
  background: ${({ $connected }) =>
    $connected ? Colors.statusColorSuccess : Colors.elementColorMute};
  ${({ $connected }) =>
    $connected &&
    css`animation: ${blink} 2s ease-in-out infinite;`}
`;

const NoGamepadNote = styled.span`
  font-size: 12px;
  color: ${Colors.elementColorMute};
  font-style: italic;
`;

// ─── Pad + Bindings Card ──────────────────────────────────────────────────────

const PadCard = styled.div`
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 12px;
  background: ${Colors.bgColorLv1};
  overflow: hidden;
  margin-bottom: 8px;
`;

const MouseHint = styled.div`
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 6px;
  padding: 8px 16px;
  font-size: 12px;
  color: ${Colors.brandColorPrimary};
  cursor: pointer;
  border-bottom: 1px dashed ${Colors.brandColorPrimary}55;
  background: ${Colors.brandColorPrimary}08;
  transition: background 0.15s;

  &:hover { background: ${Colors.brandColorPrimary}14; }
  .material-icon { font-size: 16px; }
`;

const PadWrap = styled.div<{ $mouseLock: boolean }>`
  padding: 16px;
  cursor: ${({ $mouseLock }) => ($mouseLock ? "crosshair" : "default")};
`;

// ─── Bindings inline accordion ────────────────────────────────────────────────

const BindingsToggle = styled.button`
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 16px;
  border: none;
  border-top: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv0};
  cursor: pointer;
  transition: background 0.15s;

  &:hover { background: ${Colors.bgColorLv2}; }
`;

const BindingsToggleLeft = styled.div`
  display: flex;
  align-items: center;
  gap: 8px;

  span {
    font-size: 13px;
    font-weight: 600;
    color: ${Colors.elementColorDefault};
  }
`;

const AccordionChevron = styled.div<{ $open: boolean }>`
  position: relative;
  width: 14px;
  height: 10px;
  flex-shrink: 0;
  transition: transform 0.2s ease;
  transform: rotate(${({ $open }) => ($open ? "0deg" : "-90deg")});

  &::before,
  &::after {
    content: "";
    position: absolute;
    top: 50%;
    height: 1.5px;
    width: 8px;
    background: ${Colors.elementColorMute};
    border-radius: 1px;
    transform-origin: center;
  }
  &::before {
    left: 0;
    transform: translateY(-50%) rotate(40deg);
    transform-origin: right center;
  }
  &::after {
    right: 0;
    transform: translateY(-50%) rotate(-40deg);
    transform-origin: left center;
  }
`;

const BindingsContent = styled.div`
  padding: 20px;
  background: ${Colors.bgColorLv0};
  border-top: 1px solid ${Colors.borderColorLv1};
  display: flex;
  flex-direction: column;
  gap: 20px;
`;

const MouseLookRow = styled.div`
  display: flex;
  align-items: center;
  gap: 20px;
  flex-wrap: wrap;
  padding-bottom: 16px;
  border-bottom: 1px solid ${Colors.borderColorLv1};
`;

const ControlLabel = styled.label`
  display: inline-flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
  color: ${Colors.elementColorDefault};
  font-weight: 500;

  input[type="checkbox"] {
    accent-color: ${Colors.brandColorPrimary};
    width: 14px;
    height: 14px;
    cursor: pointer;
  }
  input[type="range"] {
    width: 100px;
    accent-color: ${Colors.brandColorPrimary};
  }
`;

const SensValue = styled.span`
  font-weight: 700;
  color: ${Colors.brandColorPrimary};
  min-width: 18px;
  text-align: center;
`;

const BindSection = styled.div``;

const BindSectionTitle = styled.div`
  font-size: 11px;
  font-weight: 700;
  color: ${Colors.elementColorMute};
  text-transform: uppercase;
  letter-spacing: 0.08em;
  margin-bottom: 10px;
`;

const BindGrid = styled.div`
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(90px, 1fr));
  gap: 6px;
`;

const BindCell = styled.div<{ $listening: boolean }>`
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  padding: 10px 6px;
  border-radius: 8px;
  cursor: pointer;
  border: 1px solid
    ${({ $listening }) => ($listening ? Colors.brandColorPrimary : Colors.borderColorLv1)};
  background: ${({ $listening }) =>
    $listening ? Colors.brandColorPrimary + "12" : Colors.bgColorLv1};
  transition: border-color 0.15s, background 0.15s;

  &:hover {
    border-color: ${Colors.borderColorLv3};
    background: ${Colors.bgColorLv2};
  }

  ${({ $listening }) =>
    $listening &&
    css`
      &:hover {
        border-color: ${Colors.brandColorPrimary};
        background: ${Colors.brandColorPrimary}12;
      }
    `}
`;

const BindLabel = styled.div`
  font-size: 10px;
  color: ${Colors.elementColorWeak};
  text-align: center;
  line-height: 1.2;
`;

const BindKey = styled.div<{ $empty: boolean }>`
  font-size: 13px;
  font-weight: 700;
  font-family: monospace;
  color: ${({ $empty }) =>
    $empty ? Colors.elementColorMute : Colors.elementColorDefault};
  min-height: 18px;
`;

const ResetRow = styled.div`
  display: flex;
  align-items: center;
  gap: 12px;
`;

const CancelHint = styled.span`
  font-size: 12px;
  color: ${Colors.brandColorPrimary};
`;

// ─── Accordion (Auto Clicker) ─────────────────────────────────────────────────

const AccordionHeader = styled.button`
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 16px;
  margin-top: 8px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv1};
  cursor: pointer;
  transition: border-color 0.15s, background 0.15s;

  &:hover {
    border-color: ${Colors.borderColorLv2};
    background: ${Colors.bgColorLv2};
  }
`;

const AccordionLeft = styled.div`
  display: flex;
  align-items: center;
  gap: 8px;
`;

const AccordionTitle = styled.span`
  font-size: 13px;
  font-weight: 600;
  color: ${Colors.elementColorDefault};
`;

const ActiveBadge = styled.span`
  font-size: 10px;
  font-weight: 700;
  padding: 2px 8px;
  border-radius: 10px;
  background: ${Colors.statusColorSuccessWeak};
  color: ${Colors.statusColorSuccess};
  border: 1px solid ${Colors.statusColorSuccess};
  letter-spacing: 0.08em;
`;

const Panel = styled.div`
  padding: 20px;
  border-radius: 0 0 10px 10px;
  border: 1px solid ${Colors.borderColorLv1};
  border-top: none;
  background: ${Colors.bgColorLv0};
  margin-bottom: 8px;
  display: flex;
  flex-direction: column;
  gap: 20px;
`;

// ─── Auto Clicker ─────────────────────────────────────────────────────────────

const ACRow = styled.div``;

const ACLabel = styled.div`
  font-size: 11px;
  font-weight: 700;
  color: ${Colors.elementColorWeak};
  text-transform: uppercase;
  letter-spacing: 0.06em;
  margin-bottom: 10px;
`;

const ACButtonGrid = styled.div`
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
`;

const ACButtonChip = styled.div<{ $selected: boolean }>`
  padding: 5px 10px;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  user-select: none;
  border: 1px solid
    ${({ $selected }) => ($selected ? Colors.brandColorPrimary : Colors.borderColorLv1)};
  background: ${({ $selected }) =>
    $selected ? Colors.brandColorPrimary + "22" : Colors.bgColorLv1};
  color: ${({ $selected }) =>
    $selected ? Colors.brandColorPrimary : Colors.elementColorWeak};
  transition: border-color 0.12s, background 0.12s, color 0.12s;

  &:hover {
    border-color: ${Colors.brandColorPrimary};
    color: ${Colors.brandColorPrimary};
  }
`;

const ACTimingRow = styled.div`
  display: flex;
  align-items: flex-end;
  gap: 28px;
  flex-wrap: wrap;
`;

const ACTimingField = styled.div`
  display: flex;
  flex-direction: column;
  gap: 6px;
  input[type="range"] {
    width: 150px;
    accent-color: ${Colors.brandColorPrimary};
  }
`;

const ACInputRow = styled.div`
  display: flex;
  align-items: center;
  gap: 6px;
`;

const ACNumInput = styled.input`
  width: 72px;
  height: 32px;
  padding: 0 8px;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 6px;
  background: ${Colors.bgColorLv1};
  color: ${Colors.elementColorDefault};
  font-size: 13px;
  outline: none;
  &:focus { border-color: ${Colors.brandColorPrimary}; }
`;

const ACUnit = styled.span`
  font-size: 11px;
  color: ${Colors.elementColorMute};
  font-weight: 600;
`;

const ACCpsCard = styled.div`
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 12px 20px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv1};
  min-width: 80px;
`;

const ACCpsValue = styled.div`
  font-size: 28px;
  font-weight: 700;
  color: ${Colors.brandColorPrimary};
  line-height: 1;
`;

const ACCpsLabel = styled.div`
  font-size: 10px;
  font-weight: 700;
  color: ${Colors.elementColorMute};
  letter-spacing: 0.08em;
  text-transform: uppercase;
  margin-top: 2px;
`;

const ACStartRow = styled.div`
  display: flex;
  align-items: center;
  gap: 12px;
`;

const ACHint = styled.span`
  font-size: 12px;
  color: ${Colors.elementColorWeak};
`;

// ─── Nav ──────────────────────────────────────────────────────────────────────

const NavLinks = styled.nav`
  display: flex;
  gap: 20px;
  margin-top: 32px;
  flex-wrap: wrap;
  padding-top: 20px;
  border-top: 1px solid ${Colors.borderColorLv1};

  a {
    color: ${Colors.elementColorWeak};
    font-weight: 500;
    text-decoration: none;
    font-size: 13px;
    transition: color 0.15s;
    &:hover { color: ${Colors.brandColorPrimary}; }
  }
`;

// ─── Console Manager ──────────────────────────────────────────────────────────

const SectionCardLabelRow = styled.div`
  display: flex;
  align-items: center;
  gap: 6px;
  margin-bottom: 10px;
`;

const RefreshBtn = styled.button`
  background: none;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 6px;
  width: 32px;
  height: 32px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 16px;
  color: ${Colors.elementColorWeak};
  cursor: pointer;
  flex-shrink: 0;
  transition: border-color 0.15s, color 0.15s, transform 0.3s;

  &:hover {
    border-color: ${Colors.brandColorPrimary};
    color: ${Colors.brandColorPrimary};
    transform: rotate(180deg);
  }
`;

const GearBtn = styled.button`
  background: none;
  border: none;
  padding: 0;
  font-size: 14px;
  color: ${Colors.elementColorMute};
  cursor: pointer;
  line-height: 1;
  transition: color 0.15s, transform 0.25s;

  &:hover {
    color: ${Colors.brandColorPrimary};
    transform: rotate(60deg);
  }
`;

const ModalOverlay = styled.div`
  position: fixed;
  inset: 0;
  z-index: 200;
  background: rgba(0, 0, 0, 0.65);
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px;
`;

const ModalPanel = styled.div`
  background: ${Colors.bgColorLv1};
  border: 1px solid ${Colors.borderColorLv2};
  border-radius: 14px;
  width: 100%;
  max-width: 540px;
  max-height: calc(100vh - 80px);
  overflow-y: auto;
  box-shadow: 0 24px 64px rgba(0, 0, 0, 0.5);
`;

const ModalHeader = styled.div`
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 18px 20px 16px;
  border-bottom: 1px solid ${Colors.borderColorLv1};
  position: sticky;
  top: 0;
  background: ${Colors.bgColorLv1};
  z-index: 1;
  border-radius: 14px 14px 0 0;
`;

const ModalTitle = styled.h2`
  font-size: 15px;
  font-weight: 700;
  color: ${Colors.elementColorDefault};
  margin: 0;
`;

const ModalClose = styled.button`
  background: none;
  border: none;
  color: ${Colors.elementColorWeak};
  font-size: 16px;
  cursor: pointer;
  padding: 4px;
  line-height: 1;
  border-radius: 4px;
  transition: color 0.15s, background 0.15s;

  &:hover {
    color: ${Colors.elementColorDefault};
    background: ${Colors.bgColorLv3};
  }
`;

const ModalSection = styled.div<{ $last?: boolean }>`
  padding: 18px 20px;
  border-bottom: ${({ $last }) => ($last ? "none" : `1px solid ${Colors.borderColorLv1}`)};
`;

const ModalSectionTitle = styled.div`
  font-size: 11px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.1em;
  color: ${Colors.elementColorMute};
  margin-bottom: 12px;
`;

const CmSectionRow = styled.div`
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12px;

  ${ModalSectionTitle} {
    margin-bottom: 0;
  }
`;

const CmEmptyNote = styled.div`
  font-size: 13px;
  color: ${Colors.elementColorMute};
  padding: 8px 0;
`;

const CmConsoleRow = styled.div`
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 10px 12px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv0};
  margin-bottom: 6px;
`;

const CmConsoleInfo = styled.div`
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
`;

const CmConsoleName = styled.div`
  font-size: 13px;
  font-weight: 600;
  color: ${Colors.elementColorDefault};
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
`;

const CmConsoleIp = styled.div`
  font-size: 11px;
  color: ${Colors.elementColorWeak};
  font-family: monospace;
`;

const CmConsoleActions = styled.div`
  display: flex;
  gap: 6px;
  flex-shrink: 0;
`;

const CmEditRow = styled.div`
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  padding: 10px 12px;
  border-radius: 8px;
  border: 1px solid ${Colors.brandColorPrimary}44;
  background: ${Colors.bgColorLv0};
  margin-bottom: 6px;
`;

const CmAddRow = styled.div`
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
`;

const CmInput = styled.input`
  height: 32px;
  padding: 0 10px;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 6px;
  background: ${Colors.bgColorLv2};
  color: ${Colors.elementColorDefault};
  font-size: 13px;
  outline: none;
  flex: 1;
  min-width: 120px;
  transition: border-color 0.15s;

  &:focus { border-color: ${Colors.brandColorPrimary}; }
  &::placeholder { color: ${Colors.elementColorMute}; }
`;

const CmPortInput = styled(CmInput)`
  flex: none;
  width: 72px;
`;

type CmVariant = "save" | "edit" | "remove" | "cancel";

const variantStyles: Record<CmVariant, ReturnType<typeof css>> = {
  save: css`
    background: ${Colors.brandColorPrimary};
    color: #fff;
    border: none;
    &:hover:not(:disabled) { opacity: 0.88; }
  `,
  edit: css`
    background: transparent;
    color: ${Colors.elementColorWeak};
    border: 1px solid ${Colors.borderColorLv1};
    &:hover { border-color: ${Colors.brandColorPrimary}; color: ${Colors.brandColorPrimary}; }
  `,
  remove: css`
    background: transparent;
    color: ${Colors.brandColorDestructive};
    border: 1px solid ${Colors.brandColorDestructive}44;
    &:hover:not(:disabled) { background: ${Colors.brandColorDestructive}18; }
  `,
  cancel: css`
    background: transparent;
    color: ${Colors.elementColorWeak};
    border: 1px solid ${Colors.borderColorLv1};
    &:hover { color: ${Colors.elementColorDefault}; }
  `,
};

const CmActionBtn = styled.button<{ $variant: CmVariant }>`
  height: 30px;
  padding: 0 12px;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  white-space: nowrap;
  transition: opacity 0.12s, background 0.12s, border-color 0.12s, color 0.12s;
  flex-shrink: 0;

  ${({ $variant }) => variantStyles[$variant]}

  &:disabled {
    opacity: 0.4;
    cursor: default;
  }
`;

const CmScanBtn = styled.button`
  height: 30px;
  padding: 0 14px;
  border-radius: 6px;
  border: 1px solid ${Colors.borderColorLv2};
  background: ${Colors.bgColorLv2};
  color: ${Colors.elementColorDefault};
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  transition: border-color 0.15s, background 0.15s;
  flex-shrink: 0;

  &:hover:not(:disabled) {
    border-color: ${Colors.brandColorPrimary};
    color: ${Colors.brandColorPrimary};
  }
  &:disabled { opacity: 0.5; cursor: default; }
`;

const CmScanNote = styled.div<{ $muted?: boolean }>`
  font-size: 12px;
  color: ${({ $muted }) => ($muted ? Colors.elementColorMute : Colors.elementColorWeak)};
  padding: 4px 0;
  font-style: italic;
`;

const CmScanRow = styled.div`
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 8px 12px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv0};
  margin-top: 8px;
`;

const CmScanInfo = styled.div`
  display: flex;
  align-items: center;
  gap: 8px;
`;

const CmScanIp = styled.span`
  font-size: 13px;
  font-family: monospace;
  color: ${Colors.elementColorDefault};
`;

const CmScanBadge = styled.span`
  font-size: 10px;
  font-weight: 700;
  padding: 2px 7px;
  border-radius: 10px;
  background: ${Colors.brandColorPrimary}22;
  color: ${Colors.brandColorPrimary};
  border: 1px solid ${Colors.brandColorPrimary}44;
  letter-spacing: 0.06em;
`;
