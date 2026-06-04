import React from "react";
import { Link } from "react-router-dom";
import { Context } from "../../hooks/Provider";
import { useDatabase } from "../../hooks/useDatabase";
import styled from "styled-components";
import * as Layout from "../../styles/Layout";
import { TheHeader } from "../../ui/systems/Navigation/TheHeader";
import { TextWithIcon } from "../../ui/parts/Text/TextWithIcon";
import { Button } from "../../ui/parts/Button/Button";
import { ProjectThumbnail } from "../../ui/parts/ProjectThumbnail";
import { FirebaseProjectProps } from "../../interfaces";
import { Colors } from "../../styles/Colors";
import { getPublicCommands } from "../../configs/offlineStorage";
import { isValidMacroSignals } from "../../configs/macroSignals";

function readThumbnail(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result));
    reader.onerror = reject;
    reader.readAsDataURL(file);
  });
}

// Fetch any remote/bundled URL and re-encode as a self-contained data URL so
// exported project files work offline on any machine.
async function embedImageAsDataUrl(imageUrl: string | undefined): Promise<string | undefined> {
  if (!imageUrl) return undefined;
  if (imageUrl.startsWith("data:")) return imageUrl; // already embedded
  try {
    const res = await fetch(imageUrl);
    const blob = await res.blob();
    return await new Promise<string>((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(reader.result as string);
      reader.onerror = reject;
      reader.readAsDataURL(blob);
    });
  } catch {
    return imageUrl; // fallback: URL as-is if fetch fails
  }
}

function isValidProjectData(data: unknown): boolean {
  if (!Array.isArray(data)) return false;
  const validSignals = (signals: unknown) =>
    signals === undefined || isValidMacroSignals(signals);
  const validItems = (items: unknown) =>
    items === undefined ||
    (Array.isArray(items) &&
      items.every(
        (item) =>
          item &&
          typeof item === "object" &&
          validSignals((item as any).data?.signals)
      ));
  return data.every(
    (group) =>
      group &&
      typeof group === "object" &&
      (group as any).index &&
      typeof (group as any).index === "object" &&
      validItems((group as any).items) &&
      ((group as any).folders === undefined ||
        (Array.isArray((group as any).folders) &&
          (group as any).folders.every((folder: any) => validItems(folder?.items))))
  );
}

export const Projects = () => {
  const [projects, setProjects] = React.useState<FirebaseProjectProps[]>([]);
  const [, setContext] = React.useContext(Context);
  const { fetchProjects, addProject, deleteProject, importProject, updateProject } =
    useDatabase();

  const [name, setName] = React.useState("");
  const [thumbnail, setThumbnail] = React.useState<string>();
  const [description, setDescription] = React.useState("");
  const [editingId, setEditingId] = React.useState<string | null>(null);
  const [editName, setEditName] = React.useState("");
  const [editThumbnail, setEditThumbnail] = React.useState<string>();
  const [editDescription, setEditDescription] = React.useState("");
  const importRef = React.useRef<HTMLInputElement>(null);

  const reload = React.useCallback(async () => {
    setContext((c) => ({ ...c, project: { isLoaded: false } }));
    setProjects(await fetchProjects());
  }, [fetchProjects, setContext]);

  React.useEffect(() => {
    reload();
  }, [reload]);

  const handleCreate = async () => {
    if (!name.trim()) return;
    await addProject({ name: name.trim(), imageUrl: thumbnail, description: description.trim() || undefined });
    setName("");
    setThumbnail(undefined);
    setDescription("");
    reload();
  };

  const handleThumbnailPick = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setThumbnail(await readThumbnail(file));
    e.target.value = "";
  };

  const handleImport = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    try {
      const text = await file.text();
      const data = JSON.parse(text);
      if (
        !data ||
        typeof data !== "object" ||
        (data.privateData !== undefined && !isValidProjectData(data.privateData)) ||
        (data.publicData !== undefined && !isValidProjectData(data.publicData))
      ) {
        throw new Error("Invalid project schema");
      }
      await importProject({
        name: data.name || file.name.replace(/\.json$/i, ""),
        imageUrl: data.imageUrl,
        description: data.description,
        privateData: data.privateData,
        publicData: data.publicData,
      });
      reload();
    } catch {
      window.alert("Invalid project file.");
    }
    e.target.value = "";
  };

  const handleExport = async (project: FirebaseProjectProps) => {
    const privateKey = `Ghostpad-${project.id}`;
    const imageUrl = await embedImageAsDataUrl(project.imageUrl);
    const payload = {
      name: project.name,
      imageUrl,
      description: project.description,
      privateData: JSON.parse(localStorage.getItem(privateKey) || "[]"),
      publicData: getPublicCommands(project.id),
    };
    const blob = new Blob([JSON.stringify(payload, null, 2)], {
      type: "application/json",
    });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = `Ghostpad-${project.name.replace(/\s+/g, "-")}.json`;
    link.click();
    URL.revokeObjectURL(link.href);
  };

  const saveEdit = async (id: string) => {
    await updateProject(id, {
      name: editName.trim(),
      description: editDescription.trim() || undefined,
      ...(editThumbnail !== undefined ? { imageUrl: editThumbnail || undefined } : {}),
    });
    setEditingId(null);
    setEditThumbnail(undefined);
    setEditDescription("");
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
          <h2>Your Projects</h2>
          <p>Saved and imported projects only — add a name and optional thumbnail.</p>

          <CreateRow>
            <input
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="Project name"
              onKeyDown={(e) => { if (e.key === "Enter") handleCreate(); }}
            />
            <DescriptionTextarea
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              placeholder="Description (optional)"
              rows={2}
            />
            <ThumbPick>
              {thumbnail ? (
                <img src={thumbnail} alt="Thumbnail preview" />
              ) : (
                <Placeholder>Thumbnail</Placeholder>
              )}
              <input type="file" accept="image/*" onChange={handleThumbnailPick} />
            </ThumbPick>
            <Button
              color="primary"
              text="Create"
              icon="none"
              size="m"
              onClick={handleCreate}
            />
            <Button
              color="outline"
              text="Import"
              icon="none"
              size="m"
              onClick={() => importRef.current?.click()}
            />
            <HiddenInput
              ref={importRef}
              type="file"
              accept="application/json,.json"
              onChange={handleImport}
            />
          </CreateRow>
        </Section>

        {projects.length === 0 ? (
          <Empty>
            <span className="material-icon">folder_open</span>
            <p>No projects yet. Create one above or import a saved project file.</p>
          </Empty>
        ) : (
          <StyledProjects>
            {projects.map((p) => (
              <Card key={p.id}>
                {editingId === p.id ? (
                  <EditBlock>
                    <input
                      value={editName}
                      onChange={(e) => setEditName(e.target.value)}
                      placeholder="Project name"
                    />
                    <DescriptionTextarea
                      value={editDescription}
                      onChange={(e) => setEditDescription(e.target.value)}
                      placeholder="Description (optional)"
                      rows={2}
                    />
                    <ThumbPick>
                      {editThumbnail !== undefined && editThumbnail ? (
                        <img src={editThumbnail} alt="Thumbnail" />
                      ) : (
                        <Placeholder>Change thumbnail</Placeholder>
                      )}
                      <input
                        type="file"
                        accept="image/*"
                        onChange={async (e) => {
                          const file = e.target.files?.[0];
                          if (file) setEditThumbnail(await readThumbnail(file));
                          else setEditThumbnail("");
                          e.target.value = "";
                        }}
                      />
                    </ThumbPick>
                    <Row>
                      <Button
                        color="primary"
                        text="Save"
                        icon="none"
                        size="s"
                        onClick={() => saveEdit(p.id)}
                      />
                      <Button
                        color="outline"
                        text="Cancel"
                        icon="none"
                        size="s"
                        onClick={() => setEditingId(null)}
                      />
                    </Row>
                  </EditBlock>
                ) : (
                  <>
                    <StyledProject to={`/projects/${p.id}`}>
                      <StyledProjectImage>
                        <ProjectThumbnail
                          imageUrl={p.imageUrl}
                          alt={p.name}
                          fill
                        />
                      </StyledProjectImage>
                      <TextWithIcon text={p.name} fontSize="m" icon="sports_esports" />
                    </StyledProject>
                    {p.description && (
                      <ProjectDescription>{p.description}</ProjectDescription>
                    )}
                    <Row>
                      <Button
                        color="outline"
                        text="Edit"
                        icon="none"
                        size="s"
                        onClick={() => {
                          setEditingId(p.id);
                          setEditName(p.name);
                          setEditDescription(p.description || "");
                          setEditThumbnail(undefined);
                        }}
                      />
                      <Button
                        color="outline"
                        text="Export"
                        icon="none"
                        size="s"
                        onClick={() => handleExport(p)}
                      />
                      <Button
                        color="outline"
                        text="Delete"
                        icon="none"
                        size="s"
                        onClick={async () => {
                          if (
                            window.confirm(`Delete "${p.name}" and all its macros?`)
                          ) {
                            await deleteProject(p.id);
                            reload();
                          }
                        }}
                      />
                    </Row>
                  </>
                )}
              </Card>
            ))}
          </StyledProjects>
        )}
      </Page>
    </>
  );
};

const Page = styled.main`
  margin-top: 60px;
  padding: ${Layout.SpacingX(2)};
  width: 100%;

  @media (max-width: 480px) {
    padding: ${Layout.SpacingX(1)};
  }
`;

const Section = styled.section`
  margin-bottom: ${Layout.SpacingX(2)};
  h2 {
    margin-bottom: 8px;
  }
  p {
    color: ${Colors.elementColorWeak};
    margin-bottom: 16px;
  }
`;

const CreateRow = styled.div`
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  align-items: center;
  input[type="text"],
  input:not([type]) {
    flex: 1;
    min-width: 180px;
    padding: 8px;
    border: 1px solid ${Colors.borderColorLv1};
    border-radius: 4px;
  }
`;

const ThumbPick = styled.label`
  position: relative;
  width: 72px;
  height: 72px;
  border: 1px dashed ${Colors.borderColorLv1};
  border-radius: 8px;
  overflow: hidden;
  cursor: pointer;
  display: grid;
  place-items: center;
  img {
    width: 100%;
    height: 100%;
    object-fit: cover;
  }
  input {
    position: absolute;
    inset: 0;
    opacity: 0;
    cursor: pointer;
  }
`;

const Placeholder = styled.span`
  font-size: 11px;
  color: ${Colors.elementColorWeak};
  text-align: center;
  padding: 4px;
`;

const HiddenInput = styled.input`
  display: none;
`;

const Empty = styled.div`
  text-align: center;
  padding: 48px 16px;
  color: ${Colors.elementColorWeak};
  .material-icon {
    font-size: 48px;
    opacity: 0.5;
  }
`;

const StyledProjects = styled.div`
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 16px;
`;

const Card = styled.div`
  min-width: 0;
`;

const StyledProject = styled(Link)`
  ${Layout.alignElements("flex", "flex-start", "flex-start")};
  flex-direction: column;
  text-decoration: none;
  color: inherit;
`;

const StyledProjectImage = styled.div`
  ${Layout.fixRatio(9 / 16)};
  ${Layout.roundX(1 / 2)};
  display: block;
  width: 100%;
  background: ${Colors.bgColorLv1};
  position: relative;
  overflow: hidden;
`;

const Row = styled.div`
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  margin-top: 8px;
`;

const EditBlock = styled.div`
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 8px;
  padding: 12px;
  input {
    width: 100%;
    padding: 8px;
    margin-bottom: 8px;
    border: 1px solid ${Colors.borderColorLv1};
    border-radius: 4px;
  }
`;

const DescriptionTextarea = styled.textarea`
  flex: 2;
  min-width: 220px;
  padding: 8px;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 4px;
  background: ${Colors.bgColorLv1};
  color: ${Colors.elementColorDefault};
  font-size: 13px;
  font-family: inherit;
  resize: vertical;
  outline: none;
  transition: border-color 0.15s;

  &:focus { border-color: ${Colors.brandColorPrimary}; }
  &::placeholder { color: ${Colors.elementColorMute}; }

  ${EditBlock} & {
    width: 100%;
    min-width: 0;
    margin-bottom: 8px;
  }
`;

const ProjectDescription = styled.p`
  font-size: 12px;
  color: ${Colors.elementColorWeak};
  margin-top: 6px;
  margin-bottom: 4px;
  line-height: 1.5;
  white-space: pre-wrap;
  word-break: break-word;
`;
