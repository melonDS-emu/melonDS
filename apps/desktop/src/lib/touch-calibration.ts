/**
 * DS Touch Input Calibration — persists coordinate offset/scale corrections
 * for mouse-to-touchscreen mapping.
 *
 * melonDS maps the bottom screen to the bottom half of the display window.
 * In borderless/windowed mode the exact pixel region depends on window size
 * and layout. These calibration values let users fine-tune the mapping so
 * that mouse clicks land in the correct DS screen coordinates.
 *
 * Calibration values:
 *   - offsetX / offsetY  — pixel offset applied before mapping (signed)
 *   - scaleX  / scaleY   — multiplicative scale factor (1.0 = no change)
 *
 * Applied transformation (for a raw mouse point [mx, my]):
 *   dsX = (mx + offsetX) * scaleX
 *   dsY = (my + offsetY) * scaleY
 */

const STORAGE_KEY = 'retro-oasis-ds-touch-calibration';

export interface DsTouchCalibration {
  offsetX: number;
  offsetY: number;
  scaleX: number;
  scaleY: number;
}

export const DEFAULT_CALIBRATION: DsTouchCalibration = {
  offsetX: 0,
  offsetY: 0,
  scaleX: 1.0,
  scaleY: 1.0,
};

export function loadTouchCalibration(): DsTouchCalibration {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return { ...DEFAULT_CALIBRATION };
    const parsed = JSON.parse(raw) as Record<string, unknown>;
    const merged: DsTouchCalibration = { ...DEFAULT_CALIBRATION };
    for (const field of ['offsetX', 'offsetY', 'scaleX', 'scaleY'] as const) {
      const v = parsed[field];
      if (typeof v === 'number' && Number.isFinite(v)) {
        merged[field] = v;
      }
    }
    return merged;
  } catch {
    return { ...DEFAULT_CALIBRATION };
  }
}

export function saveTouchCalibration(cal: DsTouchCalibration): void {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(cal));
}

export function resetTouchCalibration(): void {
  localStorage.removeItem(STORAGE_KEY);
}

/**
 * Apply calibration to a raw mouse coordinate pair.
 * Returns the adjusted (dsX, dsY) coordinates.
 */
export function applyCalibration(
  mx: number,
  my: number,
  cal: DsTouchCalibration,
): [number, number] {
  return [(mx + cal.offsetX) * cal.scaleX, (my + cal.offsetY) * cal.scaleY];
}
