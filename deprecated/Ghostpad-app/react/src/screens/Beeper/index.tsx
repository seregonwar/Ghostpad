import React from "react";
import styled from "styled-components";
import { Context } from "../../hooks/Provider";
import { useNetwork } from "../../hooks/useNetwork";
import { SavedConsole, getGhostpadApi } from "../../configs/ghostpadApi";
import { TheHeader } from "../../ui/systems/Navigation/TheHeader";
import { Button } from "../../ui/parts/Button/Button";
import * as Layout from "../../styles/Layout";
import { Colors } from "../../styles/Colors";

const BEEP_TYPES = [
  { label: "Silent (0)", value: 0 },
  { label: "Single Beep (1)", value: 1 },
  { label: "Error Pattern (2)", value: 2 },
  { label: "Long Beep (3)", value: 3 },
];

const VOLUMES = [
  { label: "High", value: 0 },
  { label: "Medium", value: 1 },
  { label: "Low", value: 2 },
];

const LED_LEVELS = [
  { label: "Bright", value: 0 },
  { label: "Medium", value: 1 },
  { label: "Dim", value: 2 },
];

export const Beeper = () => {
  const [context] = React.useContext(Context);
  const { listConsoles } = useNetwork();

  const [consoles, setConsoles] = React.useState<SavedConsole[]>([]);
  const [targetConsoleId, setTargetConsoleId] = React.useState("");
  const [elfPath, setElfPath] = React.useState("");
  const [status, setStatus] = React.useState("Ready — deploy beeper_server.elf to PS5 first, then Ping");
  const [busy, setBusy] = React.useState(false);
  const [volume, setVolume] = React.useState<number | null>(null);
  const [led, setLed] = React.useState<number | null>(null);
  const [muted, setMuted] = React.useState<boolean | null>(null);
  const [spamType, setSpamType] = React.useState(1);
  const [spamCount, setSpamCount] = React.useState(5);
  const [spamDelay, setSpamDelay] = React.useState(400);
  const [spamming, setSpamming] = React.useState(false);
  const spamRef = React.useRef(false);

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

  const selectedConsole =
    consoles.find((c) => c.id === targetConsoleId) ||
    (context.network.ip
      ? { id: "", name: "Connected console", ip: context.network.ip, port: 6967 }
      : null);

  const targetIp = selectedConsole?.ip || context.network.ip || "";

  const isAvailable = Boolean(getGhostpadApi());

  const requireIp = () => {
    if (!targetIp) {
      window.alert("Select a console or connect first.");
      return false;
    }
    return true;
  };

  const run = async (fn: () => Promise<{ ok: boolean; response?: string; message?: string }>) => {
    setBusy(true);
    try {
      const result = await fn();
      setStatus(result.response ?? result.message ?? (result.ok ? "OK" : "Failed"));
    } finally {
      setBusy(false);
    }
  };

  const handlePickElf = async () => {
    const api = getGhostpadApi();
    if (!api) return;
    const picked = await api.beeperPickElf();
    if (picked) setElfPath(picked);
  };

  const handleDeploy = async () => {
    if (!requireIp()) return;
    if (!elfPath) {
      window.alert("Browse to beeper_server.elf first.");
      return;
    }
    const api = getGhostpadApi();
    if (!api) return;
    setBusy(true);
    setStatus("Deploying beeper_server.elf...");
    try {
      const result = await api.beeperDeploy(targetIp, elfPath);
      setStatus(result.message ?? (result.ok ? "Deployed" : "Deploy failed"));
    } finally {
      setBusy(false);
    }
  };

  const handlePing = async () => {
    if (!requireIp()) return;
    const api = getGhostpadApi();
    if (!api) return;
    await run(() => api.beeperPing(targetIp));
  };

  const handleBuzz = async (type: number) => {
    if (!requireIp()) return;
    const api = getGhostpadApi();
    if (!api) return;
    await run(() => api.beeperBuzz(targetIp, type));
  };

  const handleVol = async (level: number) => {
    if (!requireIp()) return;
    const api = getGhostpadApi();
    if (!api) return;
    setVolume(level);
    await run(() => api.beeperSetVol(targetIp, level));
  };

  const handleMute = async (mute: number) => {
    if (!requireIp()) return;
    const api = getGhostpadApi();
    if (!api) return;
    setMuted(Boolean(mute));
    await run(() => api.beeperSetMute(targetIp, mute));
  };

  const handleLed = async (level: number) => {
    if (!requireIp()) return;
    const api = getGhostpadApi();
    if (!api) return;
    setLed(level);
    await run(() => api.beeperSetLed(targetIp, level));
  };

  const handleSpam = async () => {
    if (spamming) {
      spamRef.current = false;
      setSpamming(false);
      return;
    }
    if (!requireIp()) return;
    const api = getGhostpadApi();
    if (!api) return;

    spamRef.current = true;
    setSpamming(true);
    let remaining = spamCount;

    const tick = async () => {
      if (!spamRef.current || remaining <= 0) {
        spamRef.current = false;
        setSpamming(false);
        return;
      }
      const result = await api.beeperBuzz(targetIp, spamType);
      setStatus(`Spam ${spamCount - remaining + 1}/${spamCount}: ${result.response}`);
      remaining--;
      if (spamRef.current && remaining > 0) {
        setTimeout(tick, spamDelay);
      } else {
        spamRef.current = false;
        setSpamming(false);
      }
    };
    tick();
  };

  return (
    <>
      <TheHeader title="Ghostpad" buttonUrl="https://github.com/StonedModder" />
      <Page>
        <Section>
          <h2>Beeper & LED Control</h2>
          <p>
            Control the PS5 hardware beeper and LED indicator via{" "}
            <code>beeper_server.elf</code> running on port 9111. Deploy the
            payload first, then Ping to confirm the server is running.
          </p>
          <Warning>
            ⚠ Beeper control for firmwares 8.0+ only
          </Warning>
          {!isAvailable && (
            <Notice>Beeper control requires the Ghostpad desktop app (Electron).</Notice>
          )}
        </Section>

        <Section>
          <h3>Target Console</h3>
          <Field>
            <label htmlFor="beeper-console">Console</label>
            <select
              id="beeper-console"
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
                ? `Currently connected: ${context.network.ip}`
                : "Not connected — select a console above"}
            </HelpText>
          </Field>
        </Section>

        <Section>
          <h3>Deploy Beeper Server</h3>
          <Field>
            <label htmlFor="beeper-elf">beeper_server.elf path</label>
            <PathRow>
              <input
                id="beeper-elf"
                value={elfPath}
                readOnly
                placeholder="Browse to beeper_server.elf"
              />
              <Button
                color="outline"
                text="Browse"
                icon="none"
                size="s"
                onClick={handlePickElf}
                isInactive={!isAvailable || busy}
              />
            </PathRow>
          </Field>
          <ButtonRow>
            <Button
              color="primary"
              text={busy ? "Working…" : "Deploy Payload"}
              icon="none"
              size="m"
              onClick={handleDeploy}
              isInactive={!isAvailable || busy || !targetIp || !elfPath}
            />
            <Button
              color="outlinePrimary"
              text="Ping Server"
              icon="none"
              size="m"
              onClick={handlePing}
              isInactive={!isAvailable || busy || !targetIp}
            />
          </ButtonRow>
        </Section>

        <ControlGrid>
          <Section>
            <h3>Beep Type</h3>
            <ButtonColumn>
              {BEEP_TYPES.map((t) => (
                <Button
                  key={t.value}
                  color="outline"
                  text={t.label}
                  icon="none"
                  size="m"
                  onClick={() => handleBuzz(t.value)}
                  isInactive={!isAvailable || busy || !targetIp}
                />
              ))}
            </ButtonColumn>
          </Section>

          <Section>
            <h3>Volume</h3>
            <RadioGroup>
              {VOLUMES.map((v) => (
                <RadioLabel key={v.value}>
                  <input
                    type="radio"
                    name="volume"
                    checked={volume === v.value}
                    onChange={() => handleVol(v.value)}
                    disabled={!isAvailable || busy || !targetIp}
                  />
                  {v.label}
                </RadioLabel>
              ))}
            </RadioGroup>

            <h3 style={{ marginTop: 20 }}>Mute</h3>
            <ButtonRow>
              <Button
                color={muted === true ? "primary" : "outline"}
                text="Mute ON"
                icon="none"
                size="s"
                onClick={() => handleMute(1)}
                isInactive={!isAvailable || busy || !targetIp}
              />
              <Button
                color={muted === false ? "primary" : "outline"}
                text="Mute OFF"
                icon="none"
                size="s"
                onClick={() => handleMute(0)}
                isInactive={!isAvailable || busy || !targetIp}
              />
            </ButtonRow>
          </Section>

          <Section>
            <h3>LED Brightness</h3>
            <RadioGroup>
              {LED_LEVELS.map((l) => (
                <RadioLabel key={l.value}>
                  <input
                    type="radio"
                    name="led"
                    checked={led === l.value}
                    onChange={() => handleLed(l.value)}
                    disabled={!isAvailable || busy || !targetIp}
                  />
                  {l.label}
                </RadioLabel>
              ))}
            </RadioGroup>
          </Section>
        </ControlGrid>

        <Section>
          <h3>Spam Beep</h3>
          <SpamRow>
            <SpamField>
              <label>Type (0–3)</label>
              <input
                type="number"
                min={0}
                max={3}
                value={spamType}
                onChange={(e) => setSpamType(Number(e.target.value))}
              />
            </SpamField>
            <SpamField>
              <label>Count</label>
              <input
                type="number"
                min={1}
                max={100}
                value={spamCount}
                onChange={(e) => setSpamCount(Number(e.target.value))}
              />
            </SpamField>
            <SpamField>
              <label>Delay (ms)</label>
              <input
                type="number"
                min={50}
                max={5000}
                step={50}
                value={spamDelay}
                onChange={(e) => setSpamDelay(Number(e.target.value))}
              />
            </SpamField>
            <Button
              color={spamming ? "outlinePrimary" : "primary"}
              text={spamming ? "STOP" : "SPAM"}
              icon="none"
              size="m"
              onClick={handleSpam}
              isInactive={!isAvailable || (!spamming && !targetIp)}
            />
          </SpamRow>
        </Section>

        <StatusBox ok={status.startsWith("OK")}>{status}</StatusBox>
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
  input[type="text"],
  input[readonly] {
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

const HelpText = styled.p`
  margin: 0;
  font-size: 12px;
`;

const Warning = styled.div`
  padding: 12px 16px;
  border-radius: 8px;
  border: 1px solid #f59e0b;
  color: #f59e0b;
  background: rgba(245, 158, 11, 0.1);
  font-size: 13px;
  font-weight: 600;
  margin-bottom: 8px;
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

const ButtonColumn = styled.div`
  display: flex;
  flex-direction: column;
  gap: 8px;
`;

const ControlGrid = styled.div`
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: ${Layout.SpacingX(2)};
  margin-bottom: ${Layout.SpacingX(3)};

  @media (max-width: 680px) {
    grid-template-columns: 1fr;
  }
`;

const RadioGroup = styled.div`
  display: flex;
  flex-direction: column;
  gap: 10px;
`;

const RadioLabel = styled.label`
  display: inline-flex;
  align-items: center;
  gap: 8px;
  color: ${Colors.elementColorDefault};
  cursor: pointer;
`;

const SpamRow = styled.div`
  display: flex;
  align-items: flex-end;
  gap: 12px;
  flex-wrap: wrap;
`;

const SpamField = styled.div`
  display: flex;
  flex-direction: column;
  gap: 4px;
  label {
    font-size: 12px;
    font-weight: 600;
    color: ${Colors.elementColorWeak};
  }
  input[type="number"] {
    width: 80px;
    padding: 8px;
    border: 1px solid ${Colors.borderColorLv1};
    border-radius: 8px;
    background: ${Colors.bgColorLv0};
    color: ${Colors.elementColorDefault};
  }
`;

const StatusBox = styled.div<{ ok?: boolean }>`
  padding: 12px 14px;
  border-radius: 8px;
  border: 1px solid ${({ ok }) => (ok ? Colors.statusColorSuccess ?? Colors.borderColorLv1 : Colors.borderColorLv1)};
  background: ${Colors.bgColorLv1};
  color: ${Colors.elementColorDefault};
  font-size: 13px;
  min-height: 44px;
  font-family: monospace;
`;
