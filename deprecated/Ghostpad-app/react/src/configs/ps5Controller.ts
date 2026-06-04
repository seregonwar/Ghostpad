/** Maps legacy PhantomHand signal IDs to PS5 DualSense GPAD button bits. */
export const PS5_BUTTON_BITS: Record<number, number> = {
  0: 0x00004000, // Cross  (bottom face — A on Xbox)
  1: 0x00002000, // Circle (right face — B on Xbox)
  2: 0x00008000, // Square (left face  — X on Xbox)
  3: 0x00001000, // Triangle (top face — Y on Xbox)
  4: 0x00000400, // L1
  5: 0x00000800, // R1
  6: 0x00000100, // L2 (digital)
  7: 0x00000200, // R2 (digital)
  8: 0x00010000, // Create / Share
  9: 0x00000008, // Options
  10: 0x00000002, // L3
  11: 0x00000004, // R3
  12: 0x00000010, // D-pad Up
  13: 0x00000040, // D-pad Down
  14: 0x00000080, // D-pad Left
  15: 0x00000020, // D-pad Right
  16: 0x00020000, // PS
  17: 0x00100000, // Touchpad click
};

export interface PadStateInput {
  buttonStates: Record<number, boolean | number>;
  stickStates: Record<number, number>;
}

export interface GpadNetworkState {
  buttons: number;
  lx: number;
  ly: number;
  rx: number;
  ry: number;
  l2: number;
  r2: number;
}

export function buildGpadState(input: PadStateInput): GpadNetworkState {
  let buttons = 0;
  const { buttonStates, stickStates } = input;

  for (const [idStr, pressed] of Object.entries(buttonStates)) {
    const id = Number(idStr);
    if (pressed && PS5_BUTTON_BITS[id]) {
      buttons |= PS5_BUTTON_BITS[id];
    }
  }

  const triggerValue = (value: boolean | number | undefined) =>
    typeof value === "number" ? Math.max(0, Math.min(255, value)) : value ? 255 : 0;
  const l2 = triggerValue(buttonStates[6]);
  const r2 = triggerValue(buttonStates[7]);

  if (l2 > 0) buttons |= PS5_BUTTON_BITS[6];
  if (r2 > 0) buttons |= PS5_BUTTON_BITS[7];

  return {
    buttons,
    lx: stickStates[18] ?? 128,
    ly: stickStates[19] ?? 128,
    rx: stickStates[20] ?? 128,
    ry: stickStates[21] ?? 128,
    l2,
    r2,
  };
}
