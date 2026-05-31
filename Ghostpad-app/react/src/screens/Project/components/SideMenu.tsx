import React from "react";
// Components
import { LogoNav } from "../../../ui/systems/SideMenu/LogoNav";
import { Menu } from "../../../ui/systems/SideMenu/Menu";
// Hooks
import { Context } from "../../../hooks/Provider";
// Styles
import styled from "styled-components";
import * as Layout from "../../../styles/Layout";
import { Colors } from "../../../styles/Colors";
import { ProjectDataProps } from "../../../interfaces";

export interface SideMenuProps {
  index: {
    id: string;
    title: string;
    imageUrl?: string;
  };
  publicData: ProjectDataProps[];
  privateData: ProjectDataProps[];
  width: number;
  onWidthChange: (width: number) => void;
}

export const SideMenu = (props: SideMenuProps) => {
  const { width, onWidthChange } = props;
  const [context, setContext] = React.useContext(Context);
  const [isResizing, setIsResizing] = React.useState<boolean>(false);
  const [isMobile, setIsMobile] = React.useState<boolean>(() => window.innerWidth < 640);

  React.useEffect(() => {
    const handler = () => {
      const mobile = window.innerWidth < 640;
      setIsMobile(mobile);
    };
    window.addEventListener("resize", handler);
    return () => window.removeEventListener("resize", handler);
  }, []);

  React.useEffect(() => {
    setContext((c) => ({
      ...c,
      emulator: {
        ...c.emulator,
        saveTo: "storage",
      },
    }));
  }, [setContext]);

  const onMouseMove = React.useCallback(
    (e: MouseEvent) => {
      onWidthChange(Math.max(0, Math.min(e.clientX, 600)));
    },
    [onWidthChange]
  );

  const onMouseUp = React.useCallback(() => {
    document.removeEventListener("mousemove", onMouseMove);
    document.removeEventListener("mouseup", onMouseUp);
    setIsResizing(false);
  }, [onMouseMove]);

  const onMouseDown = React.useCallback(() => {
    document.addEventListener("mouseup", onMouseUp);
    document.addEventListener("mousemove", onMouseMove);
    setIsResizing(true);
  }, [onMouseMove, onMouseUp]);

  const isOverlay = isMobile && width > 0;

  return React.useMemo(() => {
    return (
      <>
        {isOverlay && (
          <Backdrop onClick={() => onWidthChange(0)} />
        )}
        <StyledWrapper width={width} $isOverlay={isOverlay}>
          {(context.emulator.state === "playing" ||
            context.emulator.state === "recording") && <StyledOverlay />}
          <StyledBorder
            onMouseDown={onMouseDown}
            onMouseUp={onMouseUp}
            isResizing={isResizing}
          />
          <LogoNav
            id={props.index.id}
            title={props.index.title}
            imageUrl={props.index.imageUrl}
          />
          <StyledContent>
            <Menu
              index={props.index}
              data={props.privateData}
              isEditable={true}
              saveTo="storage"
              width={width}
            />
          </StyledContent>
        </StyledWrapper>
      </>
    );
  }, [
    context.emulator.state,
    isResizing,
    isOverlay,
    onMouseDown,
    onMouseUp,
    onWidthChange,
    props.index,
    props.privateData,
    width,
  ]);
};

const Backdrop = styled.div`
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.55);
  z-index: 199;
`;

const StyledWrapper = styled.nav.attrs<{ width: number; $isOverlay: boolean }>(
  (props) => ({
    style: {
      width: `${props.width}px`,
      position: props.$isOverlay ? "fixed" : undefined,
      top: props.$isOverlay ? 0 : undefined,
      left: props.$isOverlay ? 0 : undefined,
      height: props.$isOverlay ? "100vh" : undefined,
      zIndex: props.$isOverlay ? 200 : undefined,
      boxShadow: props.$isOverlay ? "4px 0 24px rgba(0,0,0,0.6)" : undefined,
    },
  })
)<{ width: number; $isOverlay: boolean }>`
  ${Layout.alignElements("flex", "space-between", "center")};
  flex-direction: column;
  transition: width 0.25s ease;
  overflow: hidden;
  background-color: ${Colors.bgColorLv1};
  position: relative;
  > * {
    width: 100%;
  }
`;

const StyledContent = styled.div`
  height: calc(100vh - 48px);
  flex-grow: 1;
  overflow-y: scroll;
`;

const StyledBorder = styled.div.attrs<{ isResizing: boolean }>((props) => ({
  style: {
    backgroundColor: `${
      props.isResizing ? Colors.borderColorLv2 : "transparent"
    }`,
  },
}))<{ isResizing: boolean }>`
  position: absolute;
  right: 0;
  width: 4px;
  height: 100%;
  user-select: none;
  z-index: 10;
  &:hover {
    background-color: ${Colors.borderColorLv2} !important;
    cursor: col-resize;
  }
`;

const StyledOverlay = styled.div`
  position: absolute;
  width: 100%;
  height: calc(100% - 48px);
  bottom: 0;
  background-color: ${Colors.bgColorLv0};
  opacity: 0.4;
  z-index: 1000;
`;
