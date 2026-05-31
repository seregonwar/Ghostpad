import React from "react";
import uid from "uniqid";
import { Link, useHistory } from "react-router-dom";
// Hooks
import { Context } from "../../../hooks/Provider";
import { useEmulator } from "../../../hooks/useEmulator";
import { useNetwork } from "../../../hooks/useNetwork";
import { useMedia } from "../../../hooks/useMedia";
import { SavedConsole, getGhostpadApi } from "../../../configs/ghostpadApi";
// ui
import {
  IconButton,
  IconDropdownButton,
} from "../../../ui/parts/Button/IconButton";
// Styles
import styled from "styled-components";
import * as Layout from "../../../styles/Layout";
import { Colors } from "../../../styles/Colors";

export interface ProjectHeaderProps {
  onToggleSidebar?: () => void;
  isSidebarOpen?: boolean;
}

export const ProjectHeader: React.FC<ProjectHeaderProps> = (props) => {
  const [context] = React.useContext(Context);
  const { connect, disconnect, listConsoles } = useNetwork();
  const { rec, stopRec, play, stopPlay, recorderStart, recorderStop } =
    useEmulator();
  const { getMediaDevices, connectToUserMedia, disconnectUserMedia } =
    useMedia();
  const [consoles, setConsoles] = React.useState<SavedConsole[]>([]);
  const [showHomeConfirm, setShowHomeConfirm] = React.useState(false);
  const [showRecordConfirm, setShowRecordConfirm] = React.useState(false);
  const history = useHistory();

  React.useEffect(() => {
    getMediaDevices();
    listConsoles().then(setConsoles);
  }, [getMediaDevices, listConsoles]);

  const networkLabel = context.network.isConnected
    ? `${context.network.ip}:${context.network.port ?? 6967}`
    : "Not connected";

  return React.useMemo(
    () => (
      <Wrapper>
        {showHomeConfirm && (
          <HomeConfirmOverlay onClick={() => setShowHomeConfirm(false)}>
            <HomeConfirmBox onClick={(e) => e.stopPropagation()}>
              <HomeConfirmText>Leave this project?</HomeConfirmText>
              <HomeConfirmSub>Changes auto-save, but active recording will stop.</HomeConfirmSub>
              <HomeConfirmActions>
                <HomeConfirmBtn $primary onClick={() => history.push("/")}>Go Home</HomeConfirmBtn>
                <HomeConfirmBtn onClick={() => setShowHomeConfirm(false)}>Stay</HomeConfirmBtn>
              </HomeConfirmActions>
            </HomeConfirmBox>
          </HomeConfirmOverlay>
        )}
        {showRecordConfirm && (
          <HomeConfirmOverlay onClick={() => setShowRecordConfirm(false)}>
            <HomeConfirmBox onClick={(e) => e.stopPropagation()}>
              <HomeConfirmText>Discard existing recording?</HomeConfirmText>
              <HomeConfirmSub>
                Starting a new recording permanently replaces the current macro signals.
              </HomeConfirmSub>
              <HomeConfirmActions>
                <HomeConfirmBtn
                  $primary
                  onClick={() => {
                    setShowRecordConfirm(false);
                    rec(true);
                  }}
                >
                  Discard and Record
                </HomeConfirmBtn>
                <HomeConfirmBtn onClick={() => setShowRecordConfirm(false)}>
                  Cancel
                </HomeConfirmBtn>
              </HomeConfirmActions>
            </HomeConfirmBox>
          </HomeConfirmOverlay>
        )}
        <InnerLeft>
          {props.onToggleSidebar && (
            <HamburgerBtn
              onClick={props.onToggleSidebar}
              title={props.isSidebarOpen ? "Close menu" : "Open menu"}
            >
              <span className="material-icon">
                {props.isSidebarOpen ? "close" : "menu"}
              </span>
            </HamburgerBtn>
          )}
          <HomeBtn onClick={() => setShowHomeConfirm(true)} title="Home">
            <span className="material-icon">home</span>
          </HomeBtn>
          <Device
            data-tip={`Physical Controller: ${
              context.gamePad.isConnected ? "ON" : "OFF"
            }`}
          >
            {context.gamePad.isConnected ? (
              <>
                <span className="material-icon fs-xs on">wifi</span>
                <span className="material-icon fs-xl">sports_esports</span>
              </>
            ) : (
              <span className="material-icon fs-xl off">sports_esports</span>
            )}
          </Device>

          <IconDropdownButton
            tooltip={`PS5 Network: ${networkLabel}`}
            id={uid()}
            color={context.network.isConnected ? "outlinePrimary" : "outline"}
            shape="square"
            size="s"
            icon={context.network.isConnected ? "settings_ethernet" : "link_off"}
            positionY="bottom"
            dropdown={
              context.network.isConnected
                ? [
                    {
                      state: "active",
                      leftText: networkLabel,
                      leftIcon: "videogame_asset",
                      rightIcon: "close",
                      onClick: disconnect,
                    },
                    {
                      state: "default" as const,
                      leftText: "Disconnect Virtual",
                      leftIcon: "power_off",
                      onClick: async () => {
                        const api = getGhostpadApi();
                        if (api && context.network.ip) {
                          await api.disconnectVirtual(context.network.ip);
                        }
                        disconnect();
                      },
                    },
                  ]
                : consoles.length > 0
                ? consoles.map((c) => ({
                    state: "default" as const,
                    leftText: `${c.name} (${c.ip})`,
                    leftIcon: "videogame_asset",
                    onClick: () => connect(c.ip, c.port, c.id),
                  }))
                : [
                    {
                      state: "inactive" as const,
                      leftText: "No saved consoles",
                      leftIcon: "devices",
                    },
                  ]
            }
          />

          <ManageLink to="/settings" title="Ghostpad settings">
            <span className="material-icon">settings</span>
          </ManageLink>

          <Spacer />

          <IconDropdownButton
            tooltip={`Capture: ${context.media.isConnected ? "Open" : "Select device"}`}
            id={uid()}
            color={context.media.isConnected ? "outlinePrimary" : "outline"}
            shape="square"
            size="s"
            icon="airplay"
            positionY="bottom"
            dropdown={
              context.media.devices
                ? context.media.isConnected
                  ? [
                      ...context.media.devices.map((d) => ({
                        state: "default" as const,
                        leftText: `${d.label || "Camera"} — Open Window`,
                        leftIcon: "videocam",
                        onClick: () => connectToUserMedia(d.deviceId),
                      })),
                      {
                        state: "active" as const,
                        leftText: "Disconnect",
                        leftIcon: "close",
                        onClick: disconnectUserMedia,
                      },
                    ]
                  : context.media.devices.map((d) => ({
                      state: "default" as const,
                      leftText: `${d.label || "Camera"} — Open Window`,
                      leftIcon: "videocam",
                      onClick: () => connectToUserMedia(d.deviceId),
                    }))
                : [
                    {
                      state: "inactive" as const,
                      leftText: "Not Found",
                      leftIcon: "videocam_off",
                    },
                  ]
            }
          />
        </InnerLeft>

        <InnerRight>
          {context.user.isAdmin && (
            <>
              <IconButton
                icon="center_focus_strong"
                color="outline"
                size="s"
                shape="square"
                tooltip="Start"
                onClick={recorderStart}
                isInactive={
                  !context.media.recorder ||
                  context.emulator.state === "recording" ||
                  context.emulator.state === "repeating" ||
                  context.emulator.command.signals.length === 0
                }
              />
              <IconButton
                icon="cancel_presentation"
                color="outline"
                size="s"
                shape="square"
                tooltip="Stop"
                onClick={recorderStop}
                isInactive={
                  !context.media.recorder ||
                  context.emulator.state === "recording" ||
                  context.emulator.state === "repeating" ||
                  context.emulator.command.signals.length === 0
                }
              />
              <Spacer />
            </>
          )}
          <Time>
            <span className="material-icon">timer</span>
            <span>{context.emulator.time.toFixed(2)}</span>
          </Time>

          {context.emulator.state === "recording" ? (
            <IconButton
              icon="stop"
              color="orange"
              size="s"
              shape="square"
              tooltip="Stop"
              onClick={stopRec}
            />
          ) : (
            <IconButton
              icon="radio_button_checked"
              color="red"
              size="s"
              shape="square"
              tooltip="Rec"
              onClick={() =>
                context.emulator.command.signals.length > 0
                  ? setShowRecordConfirm(true)
                  : rec()
              }
              isInactive={
                context.emulator.state === "playing" ||
                context.emulator.state === "repeating"
              }
            />
          )}

          {context.emulator.state === "playing" ? (
            <IconButton
              icon="stop"
              color="orange"
              size="s"
              shape="square"
              tooltip="Stop"
              isInactive={context.emulator.command.signals.length === 0}
              onClick={() => stopPlay(false)}
            />
          ) : (
            <IconButton
              icon="play_arrow"
              color="blue"
              size="s"
              shape="square"
              tooltip="Play"
              isInactive={
                context.emulator.state === "recording" ||
                context.emulator.state === "repeating" ||
                context.emulator.command.signals.length === 0 ||
                !context.network.isConnected
              }
              onClick={() => play(false)}
            />
          )}

          {context.emulator.state === "repeating" ? (
            <IconButton
              icon="stop"
              color="orange"
              size="s"
              shape="square"
              tooltip="Stop"
              isInactive={context.emulator.command.signals.length === 0}
              onClick={stopPlay}
            />
          ) : (
            <IconButton
              icon="repeat"
              color="green"
              size="s"
              shape="square"
              tooltip="Repeat"
              isInactive={
                context.emulator.state === "recording" ||
                context.emulator.state === "playing" ||
                context.emulator.command.signals.length === 0 ||
                !context.network.isConnected
              }
              onClick={() => play(true)}
            />
          )}
        </InnerRight>
      </Wrapper>
    ),
    [
      context.gamePad.isConnected,
      context.network.isConnected,
      context.network.ip,
      context.media.isConnected,
      context.media.devices,
      context.media.recorder,
      context.user.isAdmin,
      context.emulator.time,
      context.emulator.state,
      context.emulator.command.signals.length,
      consoles,
      connect,
      disconnect,
      disconnectUserMedia,
      recorderStart,
      stopRec,
      rec,
      stopPlay,
      connectToUserMedia,
      recorderStop,
      play,
      networkLabel,
      props.onToggleSidebar,
      props.isSidebarOpen,
      showHomeConfirm,
      showRecordConfirm,
      history,
    ]
  );
};

const HamburgerBtn = styled.button`
  display: none;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: none;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 6px;
  color: ${Colors.elementColorWeak};
  cursor: pointer;
  flex-shrink: 0;

  &:hover {
    background: ${Colors.bgColorLv2};
    color: ${Colors.elementColorDefault};
  }

  @media (max-width: 640px) {
    display: flex;
  }
`;

const Wrapper = styled.header`
  ${Layout.alignElements("flex", "space-between", "center")};
  padding: 0 ${Layout.SpacingX(2)};
  height: 48px;
  border-bottom: 1px solid ${Colors.borderColorLv1};

  @media (max-width: 480px) {
    padding: 0 ${Layout.SpacingX(1)};
  }
`;

const InnerLeft = styled.div`
  ${Layout.alignElements("flex", "space-between", "center")};
  ${Layout.spacingBetweenElements("horizontal", 1)};
  > * {
    ${Layout.alignElements("flex", "space-between", "center")};
  }
`;

const InnerRight = styled.div`
  ${Layout.alignElements("flex", "space-between", "center")};
  ${Layout.spacingBetweenElements("horizontal", 1)};
`;

const Device = styled.div`
  ${Layout.alignElements("flex", "center", "center")};
  ${Layout.spacingBetweenElements("vertical", -2)};
  flex-direction: column;
  padding-right: 8px;
  .on {
    color: ${Colors.brandColorPrimary};
  }
  .off {
    color: ${Colors.brandColorSecondary};
    opacity: 0.6;
  }
`;

const ManageLink = styled(Link)`
  color: ${Colors.elementColorWeak};
  display: flex;
  align-items: center;
  text-decoration: none;
  &:hover {
    color: ${Colors.brandColorPrimary};
  }
`;

const Spacer = styled.div`
  width: 0;
`;

const HomeBtn = styled.button`
  display: flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  background: none;
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 6px;
  color: ${Colors.elementColorWeak};
  cursor: pointer;
  flex-shrink: 0;
  transition: border-color 0.15s, color 0.15s, background 0.15s;

  .material-icon { font-size: 20px; }

  &:hover {
    border-color: ${Colors.brandColorPrimary};
    color: ${Colors.brandColorPrimary};
    background: ${Colors.bgColorLv2};
  }
`;

const HomeConfirmOverlay = styled.div`
  position: fixed;
  inset: 0;
  z-index: 200;
  background: rgba(0, 0, 0, 0.55);
  display: flex;
  align-items: center;
  justify-content: center;
`;

const HomeConfirmBox = styled.div`
  background: ${Colors.bgColorLv1};
  border: 1px solid ${Colors.borderColorLv2};
  border-radius: 12px;
  padding: 24px 28px;
  min-width: 280px;
  max-width: 360px;
  display: flex;
  flex-direction: column;
  gap: 8px;
  box-shadow: 0 8px 32px rgba(0,0,0,0.4);
`;

const HomeConfirmText = styled.div`
  font-size: 16px;
  font-weight: 700;
  color: ${Colors.elementColorDefault};
`;

const HomeConfirmSub = styled.div`
  font-size: 13px;
  color: ${Colors.elementColorWeak};
  margin-bottom: 8px;
`;

const HomeConfirmActions = styled.div`
  display: flex;
  gap: 8px;
`;

const HomeConfirmBtn = styled.button<{ $primary?: boolean }>`
  flex: 1;
  padding: 9px 0;
  border-radius: 8px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  border: 1px solid ${({ $primary }) => $primary ? Colors.brandColorPrimary : Colors.borderColorLv2};
  background: ${({ $primary }) => $primary ? Colors.brandColorPrimary : Colors.bgColorLv0};
  color: ${({ $primary }) => $primary ? "#fff" : Colors.elementColorWeak};
  transition: opacity 0.15s;

  &:hover { opacity: 0.85; }
`;

const Time = styled.div`
  ${Layout.alignElements("flex", "flex-start", "center")};
  ${Layout.spacingBetweenElements("horizontal", 1 / 2)};
  min-width: 64px;
`;
