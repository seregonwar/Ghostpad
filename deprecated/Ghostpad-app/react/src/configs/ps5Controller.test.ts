import { buildGpadState, PS5_BUTTON_BITS } from "./ps5Controller";

test("preserves analog trigger pressure in GPAD state", () => {
  const state = buildGpadState({
    buttonStates: { 6: 73, 7: 191 },
    stickStates: {},
  });

  expect(state.l2).toBe(73);
  expect(state.r2).toBe(191);
  expect(state.buttons & PS5_BUTTON_BITS[6]).toBeTruthy();
  expect(state.buttons & PS5_BUTTON_BITS[7]).toBeTruthy();
});
