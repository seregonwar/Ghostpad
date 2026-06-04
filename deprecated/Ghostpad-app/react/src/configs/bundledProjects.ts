/**
 * Default projects bundled with Ghostpad.
 * Signals use plain number arrays [id, value] — compatible with Ghostpad's
 * playback engine after JSON round-trip (Uint8Array would serialize as object).
 *
 * Button IDs:  0=○  1=✕  2=△  3=□  4=L1  5=R1  6=L2  7=R2
 *              8=Create  9=Options  10=L3  11=R3
 *              12=D↑  13=D↓  14=D←  15=D→  16=PS  17=Touchpad
 * Axis IDs:    18=LeftStickX (0=full-left 128=center 255=full-right)
 *              19=LeftStickY (0=full-up   128=center 255=full-down)
 *              20=RightStickX  21=RightStickY
 * Special:     99=END sentinel
 *
 * All t values are multiples of 0.02 (one frame at 60fps) so they align
 * exactly with Ghostpad's toFixed(2) timer accumulator.
 */

// ─── Oblivion Remastered ───────────────────────────────────────────────────

const speechcraftSignals = [
  // Step 1 — Start dialogue with NPC
  { t: 0.02, s: [1, 1] }, { t: 0.32, s: [1, 0] },
  // Step 2 — Select persuasion option
  { t: 0.32, s: [1, 1] }, { t: 0.62, s: [1, 0] },
  // Step 3 — Rotate wheel to Admire
  { t: 0.62, s: [18, 255] },
  // Step 4 — Return stick to neutral
  { t: 0.82, s: [18, 128] }, { t: 0.82, s: [19, 128] },
  // Step 5 — Select Admire
  { t: 0.88, s: [1, 1] }, { t: 1.18, s: [1, 0] },
  // Step 6 — Rotate wheel to Boast
  { t: 1.18, s: [18, 255] },
  // Step 7 — Return stick to neutral
  { t: 1.38, s: [18, 128] }, { t: 1.38, s: [19, 128] },
  // Step 8 — Select Boast
  { t: 1.44, s: [1, 1] }, { t: 1.74, s: [1, 0] },
  // Step 9 — Rotate wheel to Joke
  { t: 1.74, s: [18, 255] },
  // Step 10 — Return stick to neutral
  { t: 1.94, s: [18, 128] }, { t: 1.94, s: [19, 128] },
  // Step 11 — Select Joke
  { t: 2.00, s: [1, 1] }, { t: 2.30, s: [1, 0] },
  // Step 12 — Rotate wheel to Coerce
  { t: 2.30, s: [18, 255] },
  // Step 13 — Return stick to neutral
  { t: 2.50, s: [18, 128] }, { t: 2.50, s: [19, 128] },
  // Step 14 — Select Coerce
  { t: 2.56, s: [1, 1] }, { t: 2.86, s: [1, 0] },
  // Step 15 — Exit dialogue
  { t: 2.86, s: [0, 1] }, { t: 3.16, s: [0, 0] },
  { t: 3.16, s: [99, 0] },
];

const itemDuplicationSignals = [
  // Step 1 — Interact with container
  { t: 0.02, s: [1, 1] }, { t: 0.32, s: [1, 0] },
  // Step 2 — Exit container
  { t: 0.32, s: [0, 1] }, { t: 0.62, s: [0, 0] },
  // Step 3 — Open player inventory
  { t: 0.62, s: [9, 1] }, { t: 0.92, s: [9, 0] },
  // Step 4 — Navigate down to stack of arrows
  { t: 0.92, s: [19, 255] },
  // Step 5 — Return stick to neutral
  { t: 1.12, s: [19, 128] }, { t: 1.12, s: [18, 128] },
  // Step 6 — R1 + ✕ simultaneously (quantity menu glitch)
  { t: 1.18, s: [5, 1] }, { t: 1.18, s: [1, 1] },
  { t: 1.48, s: [5, 0] }, { t: 1.48, s: [1, 0] },
  // Step 7 — Withdraw duplicated item from container
  { t: 1.48, s: [1, 1] }, { t: 1.78, s: [1, 0] },
  // Step 8 — Exit inventory
  { t: 1.78, s: [0, 1] }, { t: 2.08, s: [0, 0] },
  { t: 2.08, s: [99, 0] },
];

export const BUNDLED_PROJECT_ID = "bundled-oblivion-remastered";

export const OBLIVION_PROJECT = {
  id: BUNDLED_PROJECT_ID,
  name: "Oblivion Remastered",
  imageUrl: "/projects/oblivion.jpg",
  description:
    "The Elder Scrolls IV: Oblivion Remastered (PS5 — PPSA21203). " +
    "Macros ported from PSAuto.py. Requires payload connected before running.",
  createdAt: "2025-04-22T00:00:00.000Z",
  updatedAt: "2025-04-22T00:00:00.000Z",
};

export const OBLIVION_PRIVATE_DATA = [
  {
    id: "bundled-oblivion-grp-1",
    index: { title: "Oblivion Macros", id: "bundled-oblivion-grp-1" },
    items: [
      {
        id: "bundled-oblivion-speechcraft",
        path: "0/bundled-oblivion-grp-1/0",
        title: "Speechcraft Farming",
        data: {
          id: "bundled-oblivion-speechcraft",
          title: "Speechcraft Farming",
          path: "0/bundled-oblivion-grp-1/0",
          signals: speechcraftSignals,
        },
      },
      {
        id: "bundled-oblivion-duplication",
        path: "0/bundled-oblivion-grp-1/1",
        title: "Item Duplication",
        data: {
          id: "bundled-oblivion-duplication",
          title: "Item Duplication",
          path: "0/bundled-oblivion-grp-1/1",
          signals: itemDuplicationSignals,
        },
      },
    ],
  },
];
