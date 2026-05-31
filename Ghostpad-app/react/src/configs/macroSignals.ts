import { SignalProps } from "../interfaces";

type SerializedSignalData =
  | number[]
  | Uint8Array
  | { [key: string]: number };

const VALID_SIGNAL_IDS = new Set([...Array.from({ length: 22 }, (_, id) => id), 99]);

function readSignalValue(data: SerializedSignalData, index: 0 | 1): number {
  return Number((data as any)?.[index]);
}

function normalizeSignalData(data: SerializedSignalData): [number, number] {
  let pair: [number, number];
  if (Array.isArray(data) || data instanceof Uint8Array) {
    pair = [readSignalValue(data, 0) || 0, readSignalValue(data, 1) || 0];
  } else {
    pair = [readSignalValue(data, 0) || 0, readSignalValue(data, 1) || 0];
  }
  return [pair[0], Math.max(0, Math.min(255, pair[1]))];
}

export function isValidMacroSignals(signals: unknown): boolean {
  return (
    Array.isArray(signals) &&
    signals.every((signal) => {
      if (!signal || typeof signal !== "object") return false;
      const value = signal as { t?: unknown; s?: SerializedSignalData };
      const data = value.s;
      const id = data == null ? NaN : readSignalValue(data, 0);
      const signalValue = data == null ? NaN : readSignalValue(data, 1);
      return (
        Number.isFinite(Number(value.t)) &&
        VALID_SIGNAL_IDS.has(id) &&
        Number.isFinite(signalValue) &&
        signalValue >= 0 &&
        signalValue <= 255
      );
    })
  );
}

export function normalizeSignals(signals: unknown): SignalProps[] {
  if (!Array.isArray(signals)) return [];
  return signals
    .filter(
      (signal) =>
        signal &&
        Number.isFinite(Number(signal.t)) &&
        signal.s != null &&
        VALID_SIGNAL_IDS.has(Number(signal.s[0]))
    )
    .map((signal) => ({
      t: Math.max(0, Number(signal.t)),
      s: normalizeSignalData(signal.s),
    }))
    .sort((a, b) => a.t - b.t);
}

export function takeDueSignals(
  signals: SignalProps[],
  startIndex: number,
  time: number
): { due: SignalProps[]; nextIndex: number } {
  let nextIndex = startIndex;
  while (nextIndex < signals.length && signals[nextIndex].t <= time) {
    nextIndex += 1;
  }
  return { due: signals.slice(startIndex, nextIndex), nextIndex };
}
