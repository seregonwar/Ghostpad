import { css } from "styled-components";

export const Colors = {
  // brand
  brandColorPrimary: "#9B51E0",
  brandColorSecondary: "#a8abb0",
  brandColorTertiary: "#2e2e32",
  brandColorDestructive: "#fc4c80",
  brandColorArduino: "#17A1A5",
  // status
  statusColorSuccess: "#5cc689",
  statusColorSuccessWeak: "#1a2e22",
  statusColorInfo: "#b8a8ff",
  statusColorError: "#ec5c5c",
  statusColorErrorWeak: "#2e1a1f",
  statusColorNotification: "#ec5c5c",
  // element
  elementColorDefault: "#e8e8ea",
  elementColorWeak: "#a0a0a8",
  elementColorMute: "#4a4a52",
  elementColorInverse: "#ffffff",
  elementColorLink: "#6ec4ff",
  elementColorHighlight: "#3d3520",
  // border
  borderColorLv1: "#3a3a40",
  borderColorLv2: "#45454c",
  borderColorLv3: "#5c5c64",
  // background
  bgColorLv0: "#121214",
  bgColorLv1: "#1a1a1e",
  bgColorLv2: "#25252a",
  bgColorLv3: "#35353c",
  bgGrad:
    "linear-gradient(180deg, rgba(155, 81, 224, 0.18) 0%, rgba(18, 18, 20, 0) 100%)",
  // sns
  twitter: "#00acee",
  facebook: "#1877f2",
  google: "#DB4437",
};

export const setBgColors = () => css`
  &.primary {
    color: ${Colors.elementColorInverse};
    background: ${Colors.brandColorPrimary};
  }
  &.secondary {
    color: ${Colors.elementColorInverse};
    background: ${Colors.brandColorSecondary};
  }
  &.tertiary {
    color: ${Colors.elementColorDefault};
    background: ${Colors.brandColorTertiary};
  }
  &.destructive {
    color: ${Colors.elementColorInverse};
    background: ${Colors.brandColorDestructive};
  }
  &.arduino {
    /* color: ${Colors.elementColorInverse};
    background: ${Colors.brandColorArduino}; */
    color: ${Colors.brandColorArduino};
    border: 1px solid ${Colors.brandColorArduino};
  }
  &.outline {
    color: ${Colors.elementColorWeak};
    border: 1px solid ${Colors.borderColorLv1};
  }
  &.outlinePrimary {
    color: ${Colors.brandColorPrimary};
    border: 1px solid ${Colors.brandColorPrimary};
  }
  &.ghost {
    color: ${Colors.elementColorWeak};
    padding: 0;
  }
  &.red {
    color: ${Colors.elementColorInverse};
    background-color: #ed1a1a;
  }
  &.orange {
    color: ${Colors.elementColorInverse};
    background-color: #ffaa2c;
  }
  &.green {
    color: ${Colors.elementColorInverse};
    background-color: #05a128;
  }
  &.blue {
    color: ${Colors.elementColorInverse};
    background-color: #296fd7;
  }
`;

export const seElementColors = () => css`
  &.primary {
    color: ${Colors.brandColorPrimary};
  }
  &.secondary {
    color: ${Colors.brandColorSecondary};
  }
  &.tertiary {
    color: ${Colors.brandColorTertiary};
  }
  &.destructive {
    color: ${Colors.brandColorDestructive};
  }
`;

export const setHoverStyle = (alpha?: number) => {
  return css`
    &:hover {
      opacity: ${alpha || 0.8};
      cursor: pointer;
    }
  `;
};

export default Colors;
