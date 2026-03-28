/**
 * Tests for DS touch input calibration service.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import {
  loadTouchCalibration,
  saveTouchCalibration,
  resetTouchCalibration,
  applyCalibration,
  DEFAULT_CALIBRATION,
} from '../touch-calibration';

// Minimal mock of localStorage for the test environment
const store: Record<string, string> = {};
const mockLocalStorage = {
  getItem: (key: string) => store[key] ?? null,
  setItem: (key: string, value: string) => { store[key] = value; },
  removeItem: (key: string) => { delete store[key]; },
  clear: () => { Object.keys(store).forEach((k) => delete store[k]); },
  get length() { return Object.keys(store).length; },
  key: (i: number) => Object.keys(store)[i] ?? null,
};
Object.defineProperty(globalThis, 'localStorage', { value: mockLocalStorage, writable: false });

describe('touch-calibration', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  it('returns default calibration when no data is stored', () => {
    const cal = loadTouchCalibration();
    expect(cal).toEqual(DEFAULT_CALIBRATION);
  });

  it('saves and reloads calibration', () => {
    saveTouchCalibration({ offsetX: 10, offsetY: -5, scaleX: 1.2, scaleY: 0.9 });
    const cal = loadTouchCalibration();
    expect(cal.offsetX).toBe(10);
    expect(cal.offsetY).toBe(-5);
    expect(cal.scaleX).toBeCloseTo(1.2);
    expect(cal.scaleY).toBeCloseTo(0.9);
  });

  it('resets to defaults', () => {
    saveTouchCalibration({ offsetX: 10, offsetY: -5, scaleX: 1.2, scaleY: 0.9 });
    resetTouchCalibration();
    const cal = loadTouchCalibration();
    expect(cal).toEqual(DEFAULT_CALIBRATION);
  });

  it('applies no transformation when using default calibration', () => {
    const [x, y] = applyCalibration(100, 200, DEFAULT_CALIBRATION);
    expect(x).toBe(100);
    expect(y).toBe(200);
  });

  it('applies offset correctly', () => {
    const [x, y] = applyCalibration(100, 200, { ...DEFAULT_CALIBRATION, offsetX: 10, offsetY: -20 });
    expect(x).toBe(110);   // (100 + 10) * 1.0
    expect(y).toBe(180);   // (200 - 20) * 1.0
  });

  it('applies scale correctly', () => {
    const [x, y] = applyCalibration(100, 200, { ...DEFAULT_CALIBRATION, scaleX: 2.0, scaleY: 0.5 });
    expect(x).toBe(200);   // (100 + 0) * 2.0
    expect(y).toBe(100);   // (200 + 0) * 0.5
  });

  it('applies both offset and scale', () => {
    const [x, y] = applyCalibration(100, 200, { offsetX: 50, offsetY: 0, scaleX: 1.0, scaleY: 0.5 });
    expect(x).toBeCloseTo(150);  // (100 + 50) * 1.0
    expect(y).toBeCloseTo(100);  // (200 + 0) * 0.5
  });

  it('handles partial stored values by merging with defaults', () => {
    // Store an object that's missing scaleY (simulates a partial save from an older version)
    store['retro-oasis-ds-touch-calibration'] = JSON.stringify({ offsetX: 5 });
    const cal = loadTouchCalibration();
    expect(cal.offsetX).toBe(5);
    expect(cal.scaleX).toBe(DEFAULT_CALIBRATION.scaleX);  // filled from default
    expect(cal.scaleY).toBe(DEFAULT_CALIBRATION.scaleY);
  });
});
