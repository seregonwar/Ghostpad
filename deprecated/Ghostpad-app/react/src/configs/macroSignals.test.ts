import {
  isValidMacroSignals,
  normalizeSignals,
  takeDueSignals,
} from "./macroSignals";

test("normalizes legacy JSON-serialized typed arrays", () => {
  const signals = normalizeSignals([
    { t: 0.125, s: { 0: 18, 1: 173 } },
  ] as any);

  expect(signals).toEqual([{ t: 0.125, s: [18, 173] }]);
});

test("rejects malformed imported signals", () => {
  expect(isValidMacroSignals([{ t: 0.1, s: [18, 300] }])).toBe(false);
  expect(isValidMacroSignals([{ t: 0.1, s: [88, 1] }])).toBe(false);
});

test("drains analog events between playback timer callbacks", () => {
  const signals = normalizeSignals([
    { t: 0.001, s: [18, 140] },
    { t: 0.009, s: [18, 175] },
    { t: 0.015, s: [19, 90] },
    { t: 0.031, s: [99, 0] },
  ]);

  expect(takeDueSignals(signals, 0, 0.017)).toEqual({
    due: signals.slice(0, 3),
    nextIndex: 3,
  });
});

test("drains analog events after more than five minutes", () => {
  const signals = normalizeSignals([
    { t: 300.001, s: [20, 201] },
    { t: 315.275, s: [21, 67] },
    { t: 315.3, s: [99, 0] },
  ]);

  expect(takeDueSignals(signals, 0, 315.28)).toEqual({
    due: signals.slice(0, 2),
    nextIndex: 2,
  });
});
