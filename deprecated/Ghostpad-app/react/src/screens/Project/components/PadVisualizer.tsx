import React from "react";
import styled from "styled-components";
import { Axis } from "react-gamepad";
import { Context } from "../../../hooks/Provider";
import { Colors } from "../../../styles/Colors";
import { ProControllerButtonNames } from "../../../configs/controller";

export interface PadVisualizerProps {
  showSmall: boolean;
  onPush: (button: number, state: boolean) => void;
  onRelease: (button: number) => void;
  onTilt: (axis: Axis, value: number) => void;
}

type StickSide = "left" | "right";

const FACE_BUTTONS = [
  { id: 3, label: "△", row: 1, col: 2 },   // Triangle (top)
  { id: 2, label: "□", row: 2, col: 1 },   // Square (left)
  { id: 1, label: "○", row: 2, col: 3 },   // Circle (right)
  { id: 0, label: "✕", row: 3, col: 2 },   // Cross (bottom)
] as const;

const DPAD_BUTTONS = [
  { id: 12, label: "↑", row: 1, col: 2 },
  { id: 14, label: "←", row: 2, col: 1 },
  { id: 15, label: "→", row: 2, col: 3 },
  { id: 13, label: "↓", row: 3, col: 2 },
] as const;

const SHOULDER_BUTTONS = [
  { id: 4, label: "L1" },
  { id: 6, label: "L2" },
  { id: 5, label: "R1" },
  { id: 7, label: "R2" },
] as const;

const CENTER_BUTTONS = [
  { id: 8, label: "Create" },
  { id: 17, label: "Touch" },
  { id: 16, label: "PS" },
  { id: 9, label: "Opt" },
] as const;

export const PadVisualizer: React.FC<PadVisualizerProps> = ({
  showSmall,
  onPush,
  onRelease,
  onTilt,
}) => {
  const [context] = React.useContext(Context);
  const [origin, setOrigin] = React.useState({ x: 0, y: 0 });
  const originRef = React.useRef(origin);
  const activeStick = React.useRef<StickSide | null>(null);

  React.useEffect(() => {
    originRef.current = origin;
  }, [origin]);

  const calc = React.useCallback((value: number) => {
    const maxRadius = 40;
    return Math.max(-1, Math.min(1, value / maxRadius));
  }, []);

  const onStickMove = React.useCallback(
    (event: MouseEvent) => {
      const stick = activeStick.current;
      if (!stick) return;

      const x = calc(event.clientX - originRef.current.x);
      const y = calc(event.clientY - originRef.current.y) * -1;

      if (stick === "left") {
        onTilt("LeftStickX", x);
        onTilt("LeftStickY", y);
      } else {
        onTilt("RightStickX", x);
        onTilt("RightStickY", y);
      }
    },
    [calc, onTilt]
  );

  const onStickUp = React.useCallback(() => {
    document.removeEventListener("mousemove", onStickMove);
    document.removeEventListener("mouseup", onStickUp);
    activeStick.current = null;
    onTilt("LeftStickX", 0);
    onTilt("LeftStickY", 0);
    onTilt("RightStickX", 0);
    onTilt("RightStickY", 0);
  }, [onStickMove, onTilt]);

  const onStickDown = React.useCallback(
    (event: React.MouseEvent<HTMLDivElement>, stick: StickSide) => {
      const rect = event.currentTarget.getBoundingClientRect();
      activeStick.current = stick;
      setOrigin({
        x: rect.left + rect.width / 2,
        y: rect.top + rect.height / 2,
      });
      document.addEventListener("mousemove", onStickMove);
      document.addEventListener("mouseup", onStickUp);
    },
    [onStickMove, onStickUp]
  );

  const bindButton = (id: number) => ({
    onMouseDown: () => onPush(id, true),
    onMouseUp: () => onPush(id, false),
    onMouseLeave: () => onRelease(id),
    $active: Boolean(context.gamePad.buttonStates[id]),
    title: ProControllerButtonNames[id],
  });

  const stickActive = (xId: number, yId: number, clickId: number): boolean => {
    const { stickStates, buttonStates } = context.gamePad;
    return (
      stickStates[xId] <= 95 ||
      stickStates[xId] >= 159 ||
      stickStates[yId] <= 95 ||
      stickStates[yId] >= 159 ||
      Boolean(buttonStates[clickId])
    );
  };

  // Derive real-time dot positions from stickStates (0–255, center=128)
  const maxOff = showSmall ? 10 : 13;
  const norm = (v: number) => Math.max(-1, Math.min(1, (v - 128) / 128));

  const lox = norm(context.gamePad.stickStates[18]) * maxOff;
  const loy = norm(context.gamePad.stickStates[19]) * maxOff;
  const rox = norm(context.gamePad.stickStates[20]) * maxOff;
  const roy = norm(context.gamePad.stickStates[21]) * maxOff;

  return (
    <Shell $compact={showSmall}>
      <HeaderRow>
        <Title>Input Map</Title>
        <ShoulderRow>
          <ShoulderGroup>
            {SHOULDER_BUTTONS.slice(0, 2).map(({ id, label }) => (
              <PadButton key={id} {...bindButton(id)}>
                {label}
              </PadButton>
            ))}
          </ShoulderGroup>
          <ShoulderGroup>
            {SHOULDER_BUTTONS.slice(2).map(({ id, label }) => (
              <PadButton key={id} {...bindButton(id)}>
                {label}
              </PadButton>
            ))}
          </ShoulderGroup>
        </ShoulderRow>
      </HeaderRow>

      <MainRow $compact={showSmall}>
        <Panel $compact={showSmall}>
          <PanelLabel>D-Pad</PanelLabel>
          <MiniGrid $compact={showSmall}>
            {DPAD_BUTTONS.map(({ id, label, row, col }) => (
              <PadButton
                key={id}
                {...bindButton(id)}
                style={{ gridRow: row, gridColumn: col }}
              >
                {label}
              </PadButton>
            ))}
          </MiniGrid>
        </Panel>

        <Panel $compact={showSmall}>
          <PanelLabel>L Stick</PanelLabel>
          <Stick
            $active={stickActive(18, 19, 10)}
            $compact={showSmall}
            onMouseDown={(event) => onStickDown(event, "left")}
          >
            <StickDot $compact={showSmall} $ox={lox} $oy={loy} />
          </Stick>
        </Panel>

        <Panel $compact={showSmall}>
          <PanelLabel>Center</PanelLabel>
          <CenterRow>
            {CENTER_BUTTONS.map(({ id, label }) => (
              <PadButton key={id} {...bindButton(id)} $small>
                {label}
              </PadButton>
            ))}
          </CenterRow>
        </Panel>

        <Panel $compact={showSmall}>
          <PanelLabel>Face</PanelLabel>
          <MiniGrid $compact={showSmall}>
            {FACE_BUTTONS.map(({ id, label, row, col }) => (
              <PadButton
                key={id}
                {...bindButton(id)}
                style={{ gridRow: row, gridColumn: col }}
              >
                {label}
              </PadButton>
            ))}
          </MiniGrid>
        </Panel>

        <Panel $compact={showSmall}>
          <PanelLabel>R Stick</PanelLabel>
          <Stick
            $active={stickActive(20, 21, 11)}
            $compact={showSmall}
            onMouseDown={(event) => onStickDown(event, "right")}
          >
            <StickDot $compact={showSmall} $ox={rox} $oy={roy} />
          </Stick>
        </Panel>
      </MainRow>
    </Shell>
  );
};

const Shell = styled.div<{ $compact?: boolean }>`
  width: 100%;
  max-width: 100%;
  padding: ${(props) => (props.$compact ? "10px 12px" : "12px 16px")};
  border: 1px solid ${Colors.borderColorLv1};
  border-radius: 16px;
  background: ${Colors.bgColorLv1};
  user-select: none;
`;

const HeaderRow = styled.div`
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 10px;
`;

const Title = styled.div`
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: ${Colors.elementColorWeak};
  flex-shrink: 0;
`;

const ShoulderRow = styled.div`
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  flex: 1;
  min-width: 0;
`;

const ShoulderGroup = styled.div`
  display: flex;
  gap: 6px;
`;

const MainRow = styled.div<{ $compact?: boolean }>`
  display: grid;
  grid-template-columns: repeat(5, minmax(0, 1fr));
  gap: ${(props) => (props.$compact ? "6px" : "8px")};
  width: 100%;
`;

const Panel = styled.div<{ $compact?: boolean }>`
  padding: ${(props) => (props.$compact ? "8px 6px" : "10px 8px")};
  border-radius: 12px;
  border: 1px solid ${Colors.borderColorLv1};
  background: ${Colors.bgColorLv0};
  min-height: ${(props) => (props.$compact ? "96px" : "108px")};
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
`;

const PanelLabel = styled.span`
  font-size: 10px;
  color: ${Colors.elementColorWeak};
  text-transform: uppercase;
  letter-spacing: 0.06em;
  white-space: nowrap;
`;

const MiniGrid = styled.div<{ $compact?: boolean }>`
  display: grid;
  grid-template-columns: repeat(3, ${(props) => (props.$compact ? "28px" : "32px")});
  grid-template-rows: repeat(3, ${(props) => (props.$compact ? "28px" : "32px")});
  gap: 4px;
`;

const CenterRow = styled.div`
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 4px;
  width: 100%;
`;

const PadButton = styled.button<{ $active?: boolean; $small?: boolean }>`
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-height: ${(props) => (props.$small ? "26px" : "32px")};
  padding: 0 6px;
  border-radius: 8px;
  border: 1px solid
    ${(props) =>
      props.$active ? Colors.brandColorPrimary : Colors.borderColorLv2};
  background: ${(props) =>
    props.$active ? Colors.brandColorPrimary : Colors.bgColorLv2};
  color: ${(props) =>
    props.$active ? Colors.elementColorInverse : Colors.elementColorDefault};
  font-size: ${(props) => (props.$small ? "9px" : "12px")};
  font-weight: 600;
  cursor: pointer;
  transition: background 80ms ease, border-color 80ms ease, color 80ms ease;

  &:hover {
    border-color: ${Colors.brandColorPrimary};
  }
`;

const Stick = styled.div<{ $active?: boolean; $compact?: boolean }>`
  width: ${(props) => (props.$compact ? "56px" : "64px")};
  height: ${(props) => (props.$compact ? "56px" : "64px")};
  border-radius: 50%;
  border: 1px solid ${Colors.borderColorLv2};
  background: ${Colors.bgColorLv2};
  position: relative;
  cursor: grab;
  flex-shrink: 0;

  ${(props) =>
    props.$active &&
    `
    border-color: ${Colors.brandColorPrimary};
    box-shadow: 0 0 0 2px rgba(155, 81, 224, 0.25);
  `}
`;

const StickDot = styled.div<{ $compact?: boolean; $ox: number; $oy: number }>`
  width: ${(props) => (props.$compact ? "22px" : "26px")};
  height: ${(props) => (props.$compact ? "22px" : "26px")};
  border-radius: 50%;
  background: ${Colors.brandColorPrimary};
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(
    calc(-50% + ${({ $ox }) => $ox}px),
    calc(-50% + ${({ $oy }) => $oy}px)
  );
  transition: transform 40ms linear;
  pointer-events: none;
  will-change: transform;
`;

export default PadVisualizer;
