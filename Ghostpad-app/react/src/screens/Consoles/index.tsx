import React from "react";
import { Link } from "react-router-dom";
import styled from "styled-components";
import { useNetwork } from "../../hooks/useNetwork";
import { SavedConsole, ScanResult } from "../../configs/ghostpadApi";
import { TheHeader } from "../../ui/systems/Navigation/TheHeader";
import { Button } from "../../ui/parts/Button/Button";
import * as Layout from "../../styles/Layout";
import { Colors } from "../../styles/Colors";

export const Consoles = () => {
  const {
    connect,
    disconnect,
    listConsoles,
    addConsole,
    updateConsole,
    deleteConsole,
    scanNetwork,
    probeHost,
  } = useNetwork();

  const [consoles, setConsoles] = React.useState<SavedConsole[]>([]);
  const [scanResults, setScanResults] = React.useState<ScanResult[]>([]);
  const [isScanning, setIsScanning] = React.useState(false);
  const [name, setName] = React.useState("");
  const [ip, setIp] = React.useState("");
  const [subnet, setSubnet] = React.useState("192.168.1");
  const [editingId, setEditingId] = React.useState<string | null>(null);
  const [editName, setEditName] = React.useState("");
  const [editIp, setEditIp] = React.useState("");

  const reload = React.useCallback(async () => {
    setConsoles(await listConsoles());
  }, [listConsoles]);

  React.useEffect(() => {
    reload();
  }, [reload]);

  const handleAdd = async () => {
    if (!ip.trim()) return;
    const probe = await probeHost(ip.trim(), 6967);
    if (!probe.reachable) {
      const elf = await probeHost(ip.trim(), 9021);
      if (!elf.reachable) {
        const ok = window.confirm(
          `Could not reach Ghostpad on ${ip} (ports 6967/9021).\nSave anyway?`
        );
        if (!ok) return;
      }
    }
    await addConsole(name.trim() || `PS5 (${ip.trim()})`, ip.trim());
    setName("");
    setIp("");
    reload();
  };

  const handleScan = async () => {
    setIsScanning(true);
    try {
      const results = await scanNetwork(subnet.trim() || undefined);
      setScanResults(results);
    } finally {
      setIsScanning(false);
    }
  };

  const saveEdit = async (id: string) => {
    await updateConsole(id, {
      name: editName.trim(),
      ip: editIp.trim(),
    });
    setEditingId(null);
    reload();
  };

  return (
    <>
      <TheHeader
        title="Ghostpad"
        buttonUrl="https://github.com/StonedModder"
      />
      <Page>
        <Section>
          <h2>Saved Consoles</h2>
          <p>
            Add your PS5 by IP, scan the LAN for Ghostpad payloads, then connect
            from here or inside a project. Configure your payload ELF in{" "}
            <Link to="/settings">Ghostpad Settings</Link>.
          </p>
          <ConsoleGrid>
            {consoles.map((c) => (
              <ConsoleCard key={c.id}>
                {editingId === c.id ? (
                  <>
                    <input
                      value={editName}
                      onChange={(e) => setEditName(e.target.value)}
                      placeholder="Name"
                    />
                    <input
                      value={editIp}
                      onChange={(e) => setEditIp(e.target.value)}
                      placeholder="IP address"
                    />
                    <Row>
                      <Button
                        color="primary"
                        text="Save"
                        icon="none"
                        size="s"
                        onClick={() => saveEdit(c.id)}
                      />
                      <Button
                        color="outline"
                        text="Cancel"
                        icon="none"
                        size="s"
                        onClick={() => setEditingId(null)}
                      />
                    </Row>
                  </>
                ) : (
                  <>
                    <strong>{c.name}</strong>
                    <span>{c.ip}:{c.port}</span>
                    <Row>
                      <Button
                        color="primary"
                        text="Connect"
                        icon="none"
                        size="s"
                        onClick={() => connect(c.ip, c.port, c.id)}
                      />
                      <Button
                        color="outline"
                        text="Rename"
                        icon="none"
                        size="s"
                        onClick={() => {
                          setEditingId(c.id);
                          setEditName(c.name);
                          setEditIp(c.ip);
                        }}
                      />
                      <Button
                        color="outline"
                        text="Remove"
                        icon="none"
                        size="s"
                        onClick={async () => {
                          await deleteConsole(c.id);
                          reload();
                        }}
                      />
                    </Row>
                  </>
                )}
              </ConsoleCard>
            ))}
          </ConsoleGrid>
        </Section>

        <Section>
          <h2>Add Console</h2>
          <FormRow>
            <input
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="Name (optional)"
            />
            <input
              value={ip}
              onChange={(e) => setIp(e.target.value)}
              placeholder="PS5 IP address"
            />
            <Button
              color="primary"
              text="Add"
              icon="none"
              size="m"
              onClick={handleAdd}
            />
          </FormRow>
        </Section>

        <Section>
          <h2>Scan Network</h2>
          <FormRow>
            <input
              value={subnet}
              onChange={(e) => setSubnet(e.target.value)}
              placeholder="Subnet e.g. 192.168.1"
            />
            <Button
              color="primary"
              text={isScanning ? "Scanning…" : "Scan LAN"}
              icon="none"
              size="m"
              isInactive={isScanning}
              onClick={handleScan}
            />
            <Button
              color="outline"
              text="Disconnect"
              icon="none"
              size="m"
              onClick={() => disconnect()}
            />
          </FormRow>
          {scanResults.length > 0 && (
            <ScanList>
              {scanResults.map((r) => (
                <ScanItem key={r.ip}>
                  <div>
                    <strong>{r.ip}</strong>
                    <small>
                      {r.hasGhostpad ? " Ghostpad" : ""}
                      {r.hasElfldr ? " elfldr" : ""}
                      {r.ports?.length ? ` [${r.ports.join(", ")}]` : ""}
                    </small>
                  </div>
                  <Button
                    color="outline"
                    text="Save"
                    icon="none"
                    size="s"
                    onClick={async () => {
                      await addConsole(`PS5 (${r.ip})`, r.ip);
                      reload();
                    }}
                  />
                </ScanItem>
              ))}
            </ScanList>
          )}
        </Section>

        <NavLinks>
          <Link to="/settings">Ghostpad Settings →</Link>
          <Link to="/projects">Go to Projects →</Link>
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
  h2 {
    margin-bottom: 8px;
  }
  p {
    color: ${Colors.elementColorWeak};
    margin-bottom: 16px;
    a {
      color: ${Colors.brandColorPrimary};
      font-weight: 600;
    }
  }
`;

const ConsoleGrid = styled.div`
  display: grid;
  gap: 12px;
`;

const ConsoleCard = styled.div`
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 8px;
  padding: 16px;
  display: grid;
  gap: 8px;
  input {
    padding: 8px;
    border: 1px solid ${Colors.borderColorLv1};
    border-radius: 4px;
  }
`;

const Row = styled.div`
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
`;

const FormRow = styled(Row)`
  input {
    flex: 1;
    min-width: 160px;
    padding: 8px;
    border: 1px solid ${Colors.borderColorLv1};
    border-radius: 4px;
  }
`;

const ScanList = styled.div`
  margin-top: 16px;
  display: grid;
  gap: 8px;
`;

const ScanItem = styled.div`
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 8px;
  small {
    display: block;
    color: ${Colors.elementColorWeak};
  }
`;

const NavLinks = styled.div`
  margin-top: 24px;
  a {
    color: ${Colors.brandColorPrimary};
    font-weight: 600;
  }
`;
