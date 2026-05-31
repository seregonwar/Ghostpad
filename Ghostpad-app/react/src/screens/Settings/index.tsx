import React from "react";
import { Link } from "react-router-dom";
import styled from "styled-components";
import { Context } from "../../hooks/Provider";
import { useNetwork } from "../../hooks/useNetwork";
import { useGhostpadSettings } from "../../hooks/useGhostpadSettings";
import { SavedConsole, getGhostpadApi } from "../../configs/ghostpadApi";
import { TheHeader } from "../../ui/systems/Navigation/TheHeader";
import { Button } from "../../ui/parts/Button/Button";
import * as Layout from "../../styles/Layout";
import { Colors } from "../../styles/Colors";

const CONTROLLER_TYPES = [
  { label: "DualSense / PS5", value: 3 },
  { label: "DS4 compatible", value: 0 },
  { label: "Alt virtual pad", value: 1 },
];

const BEEP_TYPES = [
  { label: "Silent", value: 0 },
  { label: "Single Beep", value: 1 },
  { label: "Error Pattern", value: 2 },
  { label: "Long Beep", value: 3 },
];

const CONNECT_BEEP_KEY = "ghostpad:connect-beep";

function loadConnectBeep(): { enabled: boolean; type: number } {
  try {
    const raw = localStorage.getItem(CONNECT_BEEP_KEY);
    if (raw) return JSON.parse(raw);
  } catch (_) {}
  return { enabled: false, type: 1 };
}

function saveConnectBeep(v: { enabled: boolean; type: number }) {
  localStorage.setItem(CONNECT_BEEP_KEY, JSON.stringify(v));
}

export const Settings = () => {
  const [context] = React.useContext(Context);
  const { connect, disconnect, listConsoles } = useNetwork();
  const {
    loadSettings,
    saveSettings,
    pickPayloadFile,
    deployPayload,
    getDeployStatus,
    isAvailable,
  } = useGhostpadSettings();

  const [payloadPath, setPayloadPath] = React.useState("");
  const [resolvedPath, setResolvedPath] = React.useState("");
  const [payloadExists, setPayloadExists] = React.useState(false);
  const [autoDeployOnConnect, setAutoDeployOnConnect] = React.useState(true);
  const [autoBindViaKlog, setAutoBindViaKlog] = React.useState(true);
  const [consoles, setConsoles] = React.useState<SavedConsole[]>([]);
  const [targetConsoleId, setTargetConsoleId] = React.useState("");
  const [statusMessage, setStatusMessage] = React.useState("Ready");
  const [statusLog, setStatusLog] = React.useState<string[]>([]);
  const [busy, setBusy] = React.useState(false);
  const [controllerType, setControllerType] = React.useState(3);

  const [connectBeep, setConnectBeep] = React.useState(loadConnectBeep);
  const [previewBusy, setPreviewBusy] = React.useState(false);
  const [beepStatus, setBeepStatus] = React.useState("");
  const updateConnectBeep = (patch: Partial<{ enabled: boolean; type: number }>) => {
    setConnectBeep((prev) => {
      const next = { ...prev, ...patch };
      saveConnectBeep(next);
      saveSettings({
        connectBeepEnabled: next.enabled,
        connectBeepType: next.type,
      });
      return next;
    });
  };

  React.useEffect(() => {
    const api = getGhostpadApi();
    if (!api?.onDeployStatus) return;
    const unsub = api.onDeployStatus((s) => {
      setStatusMessage(s.message);
      setStatusLog((prev) => [`[${s.phase}] ${s.message}`, ...prev].slice(0, 20));
    });
    return unsub;
  }, []);

  const refresh = React.useCallback(async () => {
    const info = await loadSettings();
    if (info) {
      setPayloadPath(info.settings.payloadElfPath);
      setResolvedPath(info.resolvedPayloadPath);
      setPayloadExists(info.payloadExists);
      setAutoDeployOnConnect(info.settings.autoDeployOnConnect);
      setAutoBindViaKlog(info.settings.autoBindViaKlog);
      setConnectBeep({
        enabled: info.settings.connectBeepEnabled,
        type: info.settings.connectBeepType,
      });
    }
    const saved = await listConsoles();
    setConsoles(saved);
    const deployStatus = await getDeployStatus();
    if (deployStatus?.message) {
      setStatusMessage(deployStatus.message);
    }
  }, [getDeployStatus, listConsoles, loadSettings]);

  React.useEffect(() => {
    refresh();
  }, [refresh]);

  React.useEffect(() => {
    if (context.network.activeConsoleId) {
      setTargetConsoleId(context.network.activeConsoleId);
    } else if (context.network.ip && !targetConsoleId) {
      const match = consoles.find((c) => c.ip === context.network.ip);
      if (match) setTargetConsoleId(match.id);
    } else if (!targetConsoleId && consoles.length > 0) {
      setTargetConsoleId(consoles[0].id);
    }
  }, [
    consoles,
    context.network.activeConsoleId,
    context.network.ip,
    targetConsoleId,
  ]);

  const selectedConsole =
    consoles.find((c) => c.id === targetConsoleId) ||
    (context.network.ip
      ? { id: "", name: "Connected console", ip: context.network.ip, port: 6967 }
      : null);

  const targetIp = selectedConsole?.ip || context.network.ip || "";

  const persistSettings = async (patch: {
    payloadElfPath?: string;
    autoDeployOnConnect?: boolean;
    autoBindViaKlog?: boolean;
    connectBeepEnabled?: boolean;
    connectBeepType?: number;
  }) => {
    await saveSettings(patch);
    await refresh();
  };

  const handleBrowsePayload = async () => {
    const picked = await pickPayloadFile();
    if (!picked) return;
    setPayloadPath(picked);
    await persistSettings({ payloadElfPath: picked });
  };

  const handleDeploy = async (force = false) => {
    if (!targetIp) {
      window.alert("Select a console or connect to a PS5 first.");
      return;
    }
    setBusy(true);
    setStatusLog([]);
    setStatusMessage(force ? "Force redeploying payload..." : "Deploying payload...");
    try {
      const result = await deployPayload(targetIp, force);
      if (result?.ok) {
        setStatusMessage(result.message || "Payload deployed.");
      } else {
        setStatusMessage(result?.message || "Deploy failed.");
        window.alert(result?.message || "Deploy failed.");
      }
    } finally {
      setBusy(false);
    }
  };

  const handleConnectWithDeploy = async () => {
    if (!targetIp) {
      window.alert("Select a console first.");
      return;
    }
    setBusy(true);
    setStatusMessage(`Connecting to ${targetIp}...`);
    setStatusLog([]);
    try {
      const ok = await connect(targetIp, 6967, targetConsoleId || undefined, {
        forceDeploy: false,
      });
      setStatusMessage(
        ok ? `Connected to ${targetIp}` : `Failed to connect to ${targetIp}`
      );
    } finally {
      setBusy(false);
    }
  };

  const handlePreviewBeep = async () => {
    if (!targetIp) { window.alert("Select a console or connect first."); return; }
    const api = getGhostpadApi();
    if (!api) return;
    setPreviewBusy(true);
    setBeepStatus("");
    try {
      const r1 = await api.beeperBuzz(targetIp, connectBeep.type);
      if (!r1.ok) {
        setBeepStatus(`Beep failed: ${r1.response}. Deploy beeper server first.`);
        return;
      }
      await new Promise((resolve) => setTimeout(resolve, 700));
      await api.beeperBuzz(targetIp, connectBeep.type);
    } finally {
      setPreviewBusy(false);
    }
  };

  const handleSendType = async () => {
    if (!targetIp) {
      window.alert("Select a console or connect first.");
      return;
    }
    const api = getGhostpadApi();
    if (!api) return;
    const result = await api.sendType(targetIp, controllerType);
    setStatusMessage(
      result?.ok
        ? `Controller type ${controllerType} sent to ${targetIp}`
        : `Failed to send type`
    );
  };

  const handleDisconnectVirtual = async () => {
    const ip = targetIp || context.network.ip;
    if (!ip) {
      window.alert("Not connected to any console.");
      return;
    }
    const api = getGhostpadApi();
    if (!api) return;
    await api.disconnectVirtual(ip);
    await disconnect();
    setStatusMessage("Virtual controller disconnected");
  };

  return (
    <>
      <TheHeader title="Ghostpad" buttonUrl="https://github.com/StonedModder" />
      <Page>
        <Section>
          <h2>Ghostpad Settings</h2>
          <p>
            Choose your Ghostpad payload ELF and deploy it to the PS5 you are
            using. Deploy sends <code>ghostpad.elf</code> to the ELF loader on
            port 9021, waits for GPAD on 6967, and auto-binds the virtual
            controller when klog is available.
          </p>
          {!isAvailable && (
            <Notice>Settings require the Ghostpad desktop app (Electron).</Notice>
          )}
        </Section>

        <Section>
          <h3>Payload</h3>
          <Field>
            <label htmlFor="payload-path">Payload ELF path</label>
            <PathRow>
              <input
                id="payload-path"
                value={payloadPath}
                readOnly
                placeholder="Browse to ghostpad.elf"
              />
              <Button
                color="outline"
                text="Browse"
                icon="none"
                size="s"
                onClick={handleBrowsePayload}
                isInactive={!isAvailable || busy}
              />
            </PathRow>
            <HelpText>
              Resolved path: {resolvedPath || "None"}
              {payloadExists ? " (found)" : " (missing — build payload or browse to ghostpad.elf)"}
            </HelpText>
          </Field>

          <CheckboxRow>
            <label>
              <input
                type="checkbox"
                checked={autoDeployOnConnect}
                onChange={async (e) => {
                  setAutoDeployOnConnect(e.target.checked);
                  await persistSettings({ autoDeployOnConnect: e.target.checked });
                }}
              />
              Auto-deploy payload when connecting
            </label>
          </CheckboxRow>

          <CheckboxRow>
            <label>
              <input
                type="checkbox"
                checked={autoBindViaKlog}
                onChange={async (e) => {
                  setAutoBindViaKlog(e.target.checked);
                  await persistSettings({ autoBindViaKlog: e.target.checked });
                }}
              />
              Auto-bind virtual controller via klog (recommended)
            </label>
          </CheckboxRow>

          <ConnectBeepRow>
            <FirmwareWarning>⚠ Firmware 8.0+ Only</FirmwareWarning>
            <CheckboxRow style={{ margin: 0 }}>
              <label>
                <input
                  type="checkbox"
                  checked={connectBeep.enabled}
                  onChange={(e) => updateConnectBeep({ enabled: e.target.checked })}
                />
                Beep when virtual controller connects
              </label>
            </CheckboxRow>
            <BeepTypeRow>
              {BEEP_TYPES.map((t) => (
                <BeepTypeBtn
                  key={t.value}
                  $active={connectBeep.type === t.value}
                  onClick={() => updateConnectBeep({ type: t.value })}
                >
                  {t.label}
                </BeepTypeBtn>
              ))}
              <Button
                color="outline"
                text={previewBusy ? "Playing…" : "Preview Beep"}
                icon="none"
                size="s"
                onClick={handlePreviewBeep}
                isInactive={!isAvailable || previewBusy || !targetIp}
              />
            </BeepTypeRow>
            {!targetIp && isAvailable && (
              <HelpText>Select a console below to use Preview Beep</HelpText>
            )}
            {beepStatus && <HelpText style={{ color: beepStatus.includes("failed") || beepStatus.includes("Deploy") ? Colors.statusColorError : undefined }}>{beepStatus}</HelpText>}
          </ConnectBeepRow>
        </Section>

        <Section>
          <h3>Deploy to Console</h3>
          <Field>
            <label htmlFor="target-console">Target console</label>
            <select
              id="target-console"
              value={targetConsoleId}
              onChange={(e) => setTargetConsoleId(e.target.value)}
            >
              <option value="">Select a saved console</option>
              {consoles.map((c) => (
                <option key={c.id} value={c.id}>
                  {c.name} ({c.ip})
                </option>
              ))}
            </select>
            <HelpText>
              {context.network.isConnected
                ? `Currently connected: ${context.network.ip}:${context.network.port ?? 6967}`
                : "Not connected"}
            </HelpText>
          </Field>

          <ButtonRow>
            <Button
              color="primary"
              text={busy ? "Working…" : "Deploy Payload"}
              icon="none"
              size="m"
              onClick={() => handleDeploy(false)}
              isInactive={!isAvailable || busy || !targetIp}
            />
            <Button
              color="outlinePrimary"
              text="Force Redeploy"
              icon="none"
              size="m"
              onClick={() => handleDeploy(true)}
              isInactive={!isAvailable || busy || !targetIp}
            />
            <Button
              color="primary"
              text="Connect"
              icon="none"
              size="m"
              onClick={handleConnectWithDeploy}
              isInactive={!isAvailable || busy || !targetIp}
            />
            <Button
              color="outline"
              text="Disconnect"
              icon="none"
              size="m"
              onClick={() => disconnect()}
              isInactive={!context.network.isConnected || busy}
            />
            <Button
              color="outline"
              text="Disconnect Virtual"
              icon="none"
              size="m"
              onClick={handleDisconnectVirtual}
              isInactive={!isAvailable || busy}
            />
          </ButtonRow>

          <StatusBox>{statusMessage}</StatusBox>
          {statusLog.length > 0 && (
            <StatusLog>
              {statusLog.map((line, i) => (
                <div key={i}>{line}</div>
              ))}
            </StatusLog>
          )}
        </Section>

        <Section>
          <h3>Controller Type</h3>
          <p>
            Send a TYPE command to the payload to switch the virtual controller
            identity. Default is DualSense / PS5 (type 3).
          </p>
          <ButtonRow>
            {CONTROLLER_TYPES.map((t) => (
              <Button
                key={t.value}
                color={controllerType === t.value ? "primary" : "outline"}
                text={t.label}
                icon="none"
                size="s"
                onClick={() => setControllerType(t.value)}
              />
            ))}
          </ButtonRow>
          <ButtonRow>
            <Button
              color="outlinePrimary"
              text="Send Type to PS5"
              icon="none"
              size="m"
              onClick={handleSendType}
              isInactive={!isAvailable || busy || !targetIp}
            />
          </ButtonRow>
        </Section>

        <NavLinks>
          <Link to="/consoles">Manage Consoles →</Link>
          <Link to="/projects">Projects →</Link>
          <Link to="/beeper">Beeper & LED →</Link>
          <Link to="/systemstate">System State →</Link>
        </NavLinks>
      </Page>
    </>
  );
};

const Page = styled.main`
  margin-top: 60px;
  padding: ${Layout.SpacingX(2)};
  max-width: 960px;
  width: 100%;

  @media (max-width: 480px) {
    padding: ${Layout.SpacingX(1)};
  }
`;

const Section = styled.section`
  margin-bottom: ${Layout.SpacingX(3)};
  h2,
  h3 {
    margin-bottom: 8px;
  }
  p {
    color: ${Colors.elementColorWeak};
    margin-bottom: 16px;
  }
  code {
    color: ${Colors.brandColorPrimary};
  }
`;

const Field = styled.div`
  display: grid;
  gap: 8px;
  margin-bottom: 16px;
  label {
    font-size: 13px;
    font-weight: 600;
  }
  select,
  input {
    width: 100%;
    padding: 10px;
    border: 1px solid ${Colors.borderColorLv1};
    border-radius: 8px;
    background: ${Colors.bgColorLv0};
    color: ${Colors.elementColorDefault};
  }
`;

const PathRow = styled.div`
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 8px;
`;

const CheckboxRow = styled.div`
  margin-bottom: 10px;
  label {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    color: ${Colors.elementColorDefault};
  }
`;

const HelpText = styled.p`
  margin: 0;
  font-size: 12px;
`;

const Notice = styled.div`
  padding: 12px;
  border-radius: 8px;
  border: 1px solid ${Colors.statusColorError};
  color: ${Colors.statusColorError};
  background: ${Colors.statusColorErrorWeak};
`;

const ButtonRow = styled.div`
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 12px;
`;

const StatusBox = styled.div`
  padding: 12px 14px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv1};
  color: ${Colors.elementColorDefault};
  font-size: 13px;
  min-height: 44px;
`;

const StatusLog = styled.div`
  margin-top: 8px;
  padding: 10px 12px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv0};
  font-size: 11px;
  font-family: monospace;
  color: ${Colors.elementColorWeak};
  max-height: 140px;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 2px;
`;

const FirmwareWarning = styled.div`
  padding: 7px 12px;
  border-radius: 6px;
  border: 1px solid #f59e0b;
  background: #f59e0b1a;
  color: #f59e0b;
  font-size: 12px;
  font-weight: 700;
`;

const ConnectBeepRow = styled.div`
  margin-top: 12px;
  padding: 12px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv1};
  display: flex;
  flex-direction: column;
  gap: 10px;
`;

const BeepTypeRow = styled.div`
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
  align-items: center;
`;

const BeepTypeBtn = styled.button<{ $active: boolean }>`
  padding: 5px 12px;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
  border: 1px solid
    ${({ $active }) => ($active ? Colors.brandColorPrimary : Colors.borderColorLv1)};
  background: ${({ $active }) =>
    $active ? Colors.brandColorPrimary + "33" : Colors.bgColorLv0};
  color: ${({ $active }) =>
    $active ? Colors.brandColorPrimary : Colors.elementColorWeak};
  transition: border-color 0.12s, background 0.12s, color 0.12s;

  &:hover {
    border-color: ${Colors.brandColorPrimary};
    color: ${Colors.brandColorPrimary};
  }
`;

const NavLinks = styled.div`
  display: flex;
  gap: 16px;
  margin-top: 24px;
  a {
    color: ${Colors.brandColorPrimary};
    font-weight: 600;
  }
`;
