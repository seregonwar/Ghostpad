import React from "react";
// Hooks
import { Context } from "../../../hooks/Provider";
import { useGamePad } from "../../../hooks/useGamePad";
// Styles
import styled from "styled-components";
import * as Layout from "../../../styles/Layout";
import { Colors } from "../../../styles/Colors";
// Components
import { PadVisualizer } from "./PadVisualizer";
// Configs
import { ProControllerButtonNames } from "../../../configs/controller";

type Axis = "LeftStickX" | "LeftStickY" | "RightStickX" | "RightStickY" | "LeftTrigger" | "RightTrigger";

export const GamePad: React.FC<{ showSmall: boolean }> = (props) => {
  const [context] = React.useContext(Context);
  const {
    connectHandler,
    disconnectHandler,
    buttonChangeHandler,
    axisChangeHandler,
    onPush,
    onRelease,
  } = useGamePad();

  const cbRef = React.useRef({ connectHandler, disconnectHandler, buttonChangeHandler, axisChangeHandler });
  cbRef.current = { connectHandler, disconnectHandler, buttonChangeHandler, axisChangeHandler };

  React.useEffect(() => {
    let rafId: number;
    let prevConnected = false;
    let prevAxes: number[] = [];
    let prevButtons: { pressed: boolean; value: number }[] = [];

    const AXIS_NAMES: Record<number, Axis> = {
      0: "LeftStickX",
      1: "LeftStickY",
      2: "RightStickX",
      3: "RightStickY",
    };

    const BTN_NAMES: Record<number, string> = {
      0: "A", 1: "B", 2: "X", 3: "Y",
      4: "LB", 5: "RB", 6: "LT", 7: "RT",
      8: "Back", 9: "Start",
      10: "LS", 11: "RS",
      12: "DPadUp", 13: "DPadDown", 14: "DPadLeft", 15: "DPadRight",
      16: "PS", 17: "Touchpad",
    };

    const poll = () => {
      try {
        const gamepads = navigator.getGamepads();
        const gp = gamepads[0];

        if (gp) {
          if (!prevConnected) {
            prevConnected = true;
            cbRef.current.connectHandler(gp.index);
          }

          for (let i = 0; i < Math.min(gp.axes.length, 4); i++) {
            const v = gp.axes[i];
            if (i >= prevAxes.length || v !== prevAxes[i]) {
              cbRef.current.axisChangeHandler(AXIS_NAMES[i], v);
            }
          }
          prevAxes = Array.from(gp.axes).slice(0, 4);

          for (let i = 0; i < Math.min(gp.buttons.length, 18); i++) {
            const btn = gp.buttons[i];
            const name = BTN_NAMES[i];
            if (!name) continue;

            const prev = prevButtons[i] ?? { pressed: false, value: 0 };

            if (i === 6 || i === 7) {
              if (btn.value !== prev.value) {
                cbRef.current.axisChangeHandler(i === 6 ? "LeftTrigger" : "RightTrigger", btn.value);
              }
            }

            if (btn.pressed !== prev.pressed) {
              cbRef.current.buttonChangeHandler(name, btn.pressed);
            }
          }
          prevButtons = Array.from(gp.buttons).slice(0, 18).map(b => ({ pressed: b.pressed, value: b.value }));
        } else {
          if (prevConnected) {
            prevConnected = false;
            cbRef.current.disconnectHandler(0);
          }
          prevAxes = [];
          prevButtons = [];
        }
      } catch (e) {
        console.error("Gamepad poll error:", e);
      }

      rafId = requestAnimationFrame(poll);
    };

    rafId = requestAnimationFrame(poll);

    return () => cancelAnimationFrame(rafId);
  }, []);

  return (
    <GamePadPreview>
      <PadVisualizer
        onPush={onPush}
        onRelease={onRelease}
        onTilt={axisChangeHandler}
        showSmall={props.showSmall}
      />
      <Buttons>
        {Object.keys(context.gamePad.buttonStates).map(
          (button: any) =>
            button < 18 &&
            context.gamePad.buttonStates[button] && (
              <li className="fs-xl fw-bold" key={button}>
                {ProControllerButtonNames[button]}
              </li>
            )
        )}
      </Buttons>
      <Sticks>
        <div>
          <span>X: {context.gamePad.stickStates[18]}</span>
          <span>Y: {context.gamePad.stickStates[19]}</span>
        </div>
        <div>
          <span>X: {context.gamePad.stickStates[20]}</span>
          <span>Y: {context.gamePad.stickStates[21]}</span>
        </div>
      </Sticks>
      <StyledID>
        {context.user.isAdmin && "★ "}
        {context.user.isSignedIn && context.user.uid}
      </StyledID>
    </GamePadPreview>
  );
};

const GamePadPreview = styled.div`
  display: flex;
  flex-direction: column;
  justify-content: center;
  position: relative;
  width: 100%;
  min-width: 0;
  height: 100%;
  padding: 8px 12px;
`;

const Buttons = styled.ul`
  ${Layout.alignElements("inline-flex", "center", "center")};
  ${Layout.spacingBetweenElements("horizontal", 1)};
  height: 32px;
  user-select: none;
  position: absolute;
  bottom: 20px;
  > li {
    ${Layout.centralizeInnerElement};
    ${Layout.roundX(1)};
    height: 100%;
    padding: ${Layout.spacingVH(0, 2)};
    border: 1px solid ${Colors.borderColorLv1};
    font-size: 12px;
    background-color: ${Colors.bgColorLv0};
    /* color: ${Colors.elementColorWeak}; */
  }
`;

const Sticks = styled.div`
  ${Layout.spacingBetweenElements("horizontal", 2)};
  color: ${Colors.elementColorWeak};
  top: 16px;
  position: absolute;
  user-select: none;
  > div {
    ${Layout.spacingBetweenElements("vertical", 1 / 2)};
    ${Layout.alignElements("inline-flex", "flex-start", "center")};
    flex-direction: column;
    > span {
      width: 38px;
      display: inline-block;
      font-size: 12px;
    }
  }
`;

const StyledID = styled.span`
  position: absolute;
  bottom: 4px;
  color: ${Colors.elementColorMute};
  font-size: 9px;
  &.admin {
    color: ${Colors.brandColorPrimary};
  }
`;
