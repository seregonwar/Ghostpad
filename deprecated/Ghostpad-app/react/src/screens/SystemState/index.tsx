import React from "react";
import styled from "styled-components";
import { Context } from "../../hooks/Provider";
import { useNetwork } from "../../hooks/useNetwork";
import { SavedConsole, getGhostpadApi } from "../../configs/ghostpadApi";
import { TheHeader } from "../../ui/systems/Navigation/TheHeader";
import { Button } from "../../ui/parts/Button/Button";
import * as Layout from "../../styles/Layout";
import { Colors } from "../../styles/Colors";

type BtnDef = {
  label: string;
  cmd: (ip: string, api: NonNullable<ReturnType<typeof getGhostpadApi>>) => Promise<{ ok: boolean; response: string }>;
  confirm?: string;
  danger?: boolean;
};

const ACTIONS: BtnDef[] = [
  {
    label: "Reboot",
    cmd: (ip, api) => api.ssmReboot(ip),
    confirm: "Reboot the PS5? It will restart immediately.",
    danger: true,
  },
  {
    label: "Shutdown",
    cmd: (ip, api) => api.ssmShutdown(ip),
    confirm: "Shut down the PS5? It will power off immediately.",
    danger: true,
  },
  {
    label: "Rest Mode",
    cmd: (ip, api) => api.ssmRestMode(ip),
    confirm: "Put the PS5 into rest mode? LED will fade to orange.",
    danger: false,
  },
  {
    label: "Eject Disc",
    cmd: (ip, api) => api.ssmEjectDisc(ip),
    danger: false,
  },
];

export const SystemState = () => {
  const [context] = React.useContext(Context);
  const { listConsoles } = useNetwork();

  const [consoles, setConsoles] = React.useState<SavedConsole[]>([]);
  const [targetConsoleId, setTargetConsoleId] = React.useState("");
  const [elfPath, setElfPath] = React.useState("");
  const [status, setStatus] = React.useState("Ready — deploy SystemStateManager.elf to PS5 first, then Ping");
  const [busy, setBusy] = React.useState(false);

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

  const handlePickElf = async () => {
    const api = getGhostpadApi();
    if (!api) return;
    const picked = await api.ssmPickElf();
    if (picked) setElfPath(picked);
  };

  const handleDeploy = async () => {
    if (!requireIp()) return;
    if (!elfPath) {
      window.alert("Browse to SystemStateManager.elf first.");
      return;
    }
    const api = getGhostpadApi();
    if (!api) return;
    setBusy(true);
    setStatus("Deploying SystemStateManager.elf...");
    try {
      const result = await api.ssmDeploy(targetIp, elfPath);
      setStatus(result.message ?? (result.ok ? "Deployed" : "Deploy failed"));
    } finally {
      setBusy(false);
    }
  };

  const handleStatus = async () => {
    if (!requireIp()) return;
    const api = getGhostpadApi();
    if (!api) return;
    setBusy(true);
    try {
      const result = await api.ssmStatus(targetIp);
      setStatus(result.response);
    } finally {
      setBusy(false);
    }
  };

  const handleAction = async (action: BtnDef) => {
    if (!requireIp()) return;
    if (action.confirm) {
      if (!window.confirm(action.confirm)) return;
    }
    const api = getGhostpadApi();
    if (!api) return;
    setBusy(true);
    setStatus(`Sending ${action.label.toUpperCase()}...`);
    try {
      const result = await action.cmd(targetIp, api);
      setStatus(result.response);
    } finally {
      setBusy(false);
    }
  };

  return (
    <>
      <TheHeader title="Ghostpad" buttonUrl="https://github.com/StonedModder" />
      <Page>
        <Section>
          <h2>System State Manager</h2>
          <p>
            Control PS5 system state via <code>SystemStateManager.elf</code> on
            port 9112. Deploy the payload first, then Ping to confirm it is
            running. Reboot and Shutdown will disconnect your session immediately.
          </p>
          {!isAvailable && (
            <Notice>System State Manager requires the Ghostpad desktop app (Electron).</Notice>
          )}
        </Section>

        <Section>
          <h3>Target Console</h3>
          <Field>
            <label htmlFor="ssm-console">Console</label>
            <select
              id="ssm-console"
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
          <h3>Deploy Payload</h3>
          <Field>
            <label htmlFor="ssm-elf">SystemStateManager.elf path</label>
            <PathRow>
              <input
                id="ssm-elf"
                value={elfPath}
                readOnly
                placeholder="Browse to SystemStateManager.elf"
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
            <HelpText>
              Pre-built ELF included in the SystemStateManager folder.
            </HelpText>
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
              text="Ping / Status"
              icon="none"
              size="m"
              onClick={handleStatus}
              isInactive={!isAvailable || busy || !targetIp}
            />
          </ButtonRow>
        </Section>

        <Section>
          <h3>System State</h3>
          <ActionGrid>
            {ACTIONS.map((action) => (
              <ActionCard key={action.label} $danger={action.danger}>
                <ActionLabel>{action.label}</ActionLabel>
                <ActionNote>
                  {action.label === "Reboot" && "sceSystemServiceRequestReboot()"}
                  {action.label === "Shutdown" && "sceShellCoreUtilRequestShutdown(2)"}
                  {action.label === "Rest Mode" && "sceSystemStateMgrEnterStandby(0)"}
                  {action.label === "Eject Disc" && "/dev/cd0 CDIOCALLOW + CDIOCEJECT"}
                </ActionNote>
                <Button
                  color={action.danger ? "primary" : "outlinePrimary"}
                  text={action.label}
                  icon="none"
                  size="m"
                  onClick={() => handleAction(action)}
                  isInactive={!isAvailable || busy || !targetIp}
                />
              </ActionCard>
            ))}
          </ActionGrid>
          <DangerNote>
            Reboot and Shutdown are immediate — no grace period. Rest Mode does not
            show the "entering rest mode" screen but is genuine standby (LED fades
            to orange). Shutdown will require multiple power button presses to boot.
          </DangerNote>
        </Section>

        <StatusBox>{status}</StatusBox>
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

const ActionGrid = styled.div`
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
  margin-bottom: 16px;

  @media (max-width: 560px) {
    grid-template-columns: 1fr;
  }
`;

const ActionCard = styled.div<{ $danger?: boolean }>`
  padding: 16px;
  border-radius: 12px;
  border: 1px solid
    ${({ $danger }) => ($danger ? Colors.statusColorError : Colors.borderColorLv1)};
  background: ${({ $danger }) =>
    $danger ? Colors.statusColorErrorWeak : Colors.bgColorLv1};
  display: flex;
  flex-direction: column;
  gap: 8px;
`;

const ActionLabel = styled.div`
  font-size: 15px;
  font-weight: 700;
  color: ${Colors.elementColorDefault};
`;

const ActionNote = styled.div`
  font-size: 11px;
  font-family: monospace;
  color: ${Colors.elementColorWeak};
  min-height: 16px;
`;

const DangerNote = styled.p`
  font-size: 12px;
  color: ${Colors.elementColorWeak};
  margin: 0;
`;

const StatusBox = styled.div`
  padding: 12px 14px;
  border-radius: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv1};
  color: ${Colors.elementColorDefault};
  font-size: 13px;
  min-height: 44px;
  font-family: monospace;
  margin-top: 16px;
`;
