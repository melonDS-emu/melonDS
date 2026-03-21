/**
 * Phase 7 — Input and couch-friendliness tests.
 *
 * Covers:
 *  - New input profiles for Wii, Wii U, Genesis, Dreamcast, PSX, PS2, PSP
 *  - Controller detection (detectGamepadType, getConnectedGamepads)
 *  - Slot assignment (getSlotAssignments, setSlotAssignment, autoAssignSlots, clearSlotAssignments)
 *  - Preflight input validation (validateInputConfig)
 */
import { describe, it, expect, beforeEach, vi, afterEach } from 'vitest';
import { InputProfileManager } from '@retro-oasis/multiplayer-profiles';
import { detectGamepadType, getConnectedGamepads } from '../controller-detection';
import type { GamepadInfo } from '../controller-detection';
import {
  getSlotAssignments,
  setSlotAssignment,
  autoAssignSlots,
  clearSlotAssignments,
} from '../slot-assignment';
import { validateInputConfig } from '../preflight-validator';

// ---------------------------------------------------------------------------
// localStorage mock
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function makeGamepad(index: number, id: string, connected = true): GamepadInfo {
  return { index, id, controllerType: detectGamepadType(id), axisCount: 6, buttonCount: 17, connected };
}

// ---------------------------------------------------------------------------
// 1. New input profiles — Wii, Wii U, Genesis, Dreamcast, PSX, PS2, PSP
// ---------------------------------------------------------------------------

describe('Phase 7 — new input profiles', () => {
  const manager = new InputProfileManager();

  const systems = ['wii', 'wiiu', 'genesis', 'dreamcast', 'psx', 'ps2', 'psp'] as const;
  const controllerTypes = ['xbox', 'playstation', 'keyboard'] as const;

  for (const system of systems) {
    describe(system, () => {
      it('has profiles for all three controller types', () => {
        for (const ct of controllerTypes) {
          const profile = manager.getDefault(system, ct);
          expect(profile, `Missing ${system} ${ct} profile`).not.toBeNull();
        }
      });

      it('all profiles have at least one binding', () => {
        const profiles = manager.listForSystem(system);
        expect(profiles.length).toBeGreaterThan(0);
        for (const p of profiles) {
          expect(p.bindings.length, `${p.id} has no bindings`).toBeGreaterThan(0);
        }
      });

      it('keyboard profile has only key bindings', () => {
        const profile = manager.getDefault(system, 'keyboard');
        expect(profile).not.toBeNull();
        for (const b of profile!.bindings) {
          // Keyboard profiles may use mouse() for touchscreen-style inputs but no axis/hat
          const hasKey = !!b.key || (!!b.button && b.button.startsWith('mouse('));
          expect(hasKey, `Non-key binding in keyboard profile: ${JSON.stringify(b)}`).toBe(true);
        }
      });

      it('xbox profile has only button/axis bindings (no key)', () => {
        const profile = manager.getDefault(system, 'xbox');
        expect(profile).not.toBeNull();
        for (const b of profile!.bindings) {
          expect(b.key, `Key binding in xbox profile: ${JSON.stringify(b)}`).toBeUndefined();
        }
      });

      it('profile IDs follow the <system>-<type>-default naming convention', () => {
        for (const ct of controllerTypes) {
          const profile = manager.getDefault(system, ct);
          expect(profile).not.toBeNull();
          expect(profile!.id).toBe(`${system}-${ct}-default`);
          expect(profile!.isDefault).toBe(true);
        }
      });
    });
  }

  it('total profile count covers all 16 systems × 3 types = 48 default profiles', () => {
    const all = manager.listAll();
    const defaults = all.filter((p) => p.isDefault);
    // 16 systems * 3 controller types = 48
    expect(defaults.length).toBe(48);
  });
});

// ---------------------------------------------------------------------------
// 2. Controller detection
// ---------------------------------------------------------------------------

describe('detectGamepadType', () => {
  it('identifies Xbox controllers', () => {
    expect(detectGamepadType('Xbox 360 Controller (XInput STANDARD GAMEPAD)')).toBe('xbox');
    expect(detectGamepadType('Xbox One Controller (XInput STANDARD GAMEPAD)')).toBe('xbox');
    expect(detectGamepadType('Vendor: 045e Product: 028e')).toBe('xbox');
  });

  it('identifies PlayStation controllers', () => {
    expect(detectGamepadType('Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)')).toBe('playstation');
    expect(detectGamepadType('DualShock 4 Controller')).toBe('playstation');
    expect(detectGamepadType('DualSense Wireless Controller')).toBe('playstation');
  });

  it('identifies generic gamepads', () => {
    expect(detectGamepadType('USB Gamepad (Vendor: 0079 Product: 0011)')).toBe('generic');
  });

  it('returns unknown for unrecognized IDs', () => {
    expect(detectGamepadType('Some Random HID Device')).toBe('unknown');
  });
});

describe('getConnectedGamepads', () => {
  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('returns empty array when navigator.getGamepads is not available', () => {
    const origNavigator = globalThis.navigator;
    Object.defineProperty(globalThis, 'navigator', { value: undefined, writable: true, configurable: true });
    expect(getConnectedGamepads()).toEqual([]);
    Object.defineProperty(globalThis, 'navigator', { value: origNavigator, writable: true, configurable: true });
  });

  it('returns only connected gamepads', () => {
    const mockGamepads = [
      { index: 0, id: 'Xbox Controller', axes: [0, 0, 0, 0, 0, 0], buttons: new Array(17).fill({ pressed: false, value: 0 }), connected: true },
      null,
      { index: 2, id: 'Xbox Controller', axes: [0, 0, 0, 0, 0, 0], buttons: new Array(17).fill({ pressed: false, value: 0 }), connected: false },
    ];

    // Define getGamepads on navigator if missing (test environment)
    const navDescriptor = Object.getOwnPropertyDescriptor(globalThis, 'navigator') ?? {};
    const origGetGamepads = (globalThis.navigator as Navigator & { getGamepads?: () => unknown[] }).getGamepads;
    Object.defineProperty(globalThis.navigator, 'getGamepads', {
      value: () => mockGamepads,
      writable: true,
      configurable: true,
    });

    const result = getConnectedGamepads();
    expect(result).toHaveLength(1);
    expect(result[0].index).toBe(0);
    expect(result[0].controllerType).toBe('xbox');

    // Restore
    if (origGetGamepads !== undefined) {
      Object.defineProperty(globalThis.navigator, 'getGamepads', {
        value: origGetGamepads,
        writable: true,
        configurable: true,
      });
    } else {
      // @ts-expect-error remove test stub
      delete (globalThis.navigator as Record<string, unknown>).getGamepads;
    }
    void navDescriptor;
  });
});

// ---------------------------------------------------------------------------
// 3. Slot assignment
// ---------------------------------------------------------------------------

describe('slot-assignment', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  describe('getSlotAssignments', () => {
    it('returns null devices for all slots when nothing is stored', () => {
      const assignments = getSlotAssignments(4);
      expect(assignments).toHaveLength(4);
      for (const a of assignments) {
        expect(a.device).toBeNull();
      }
    });

    it('returns correct slot numbers (1-based)', () => {
      const assignments = getSlotAssignments(3);
      expect(assignments.map((a) => a.slot)).toEqual([1, 2, 3]);
    });
  });

  describe('setSlotAssignment', () => {
    it('assigns a gamepad index to a slot', () => {
      setSlotAssignment(1, 0);
      const [a] = getSlotAssignments(1);
      expect(a.device).toBe(0);
    });

    it('assigns keyboard to a slot', () => {
      setSlotAssignment(2, 'keyboard');
      const assignments = getSlotAssignments(2);
      expect(assignments[1].device).toBe('keyboard');
    });

    it('clears a slot assignment when null is passed', () => {
      setSlotAssignment(1, 0);
      setSlotAssignment(1, null);
      const [a] = getSlotAssignments(1);
      expect(a.device).toBeNull();
    });
  });

  describe('clearSlotAssignments', () => {
    it('removes all stored assignments', () => {
      setSlotAssignment(1, 0);
      setSlotAssignment(2, 'keyboard');
      clearSlotAssignments();
      const assignments = getSlotAssignments(2);
      expect(assignments.every((a) => a.device === null)).toBe(true);
    });
  });

  describe('autoAssignSlots', () => {
    it('assigns gamepads in index order to unassigned slots', () => {
      const gamepads = [makeGamepad(0, 'Xbox Controller'), makeGamepad(1, 'DualShock 4')];
      const result = autoAssignSlots(2, gamepads);
      expect(result[0].device).toBe(0);
      expect(result[1].device).toBe(1);
    });

    it('falls back to keyboard when gamepads are exhausted', () => {
      const gamepads = [makeGamepad(0, 'Xbox Controller')];
      const result = autoAssignSlots(3, gamepads);
      expect(result[0].device).toBe(0);
      expect(result[1].device).toBe('keyboard');
      expect(result[2].device).toBe('keyboard');
    });

    it('does not overwrite already-assigned slots', () => {
      setSlotAssignment(1, 0);
      const gamepads = [makeGamepad(0, 'Xbox Controller'), makeGamepad(1, 'DualShock 4')];
      const result = autoAssignSlots(2, gamepads);
      // Slot 1 already has gamepad 0 → slot 2 should get gamepad 1
      expect(result[0].device).toBe(0);
      expect(result[1].device).toBe(1);
    });

    it('uses keyboard only when no gamepads are connected', () => {
      const result = autoAssignSlots(2, []);
      expect(result[0].device).toBe('keyboard');
      expect(result[1].device).toBe('keyboard');
    });

    it('returns correct slot numbers', () => {
      const result = autoAssignSlots(4, []);
      expect(result.map((r) => r.slot)).toEqual([1, 2, 3, 4]);
    });
  });
});

// ---------------------------------------------------------------------------
// 4. Preflight input validation
// ---------------------------------------------------------------------------

describe('validateInputConfig', () => {
  beforeEach(() => {
    mockLocalStorage.clear();
  });

  it('returns valid with no issues when all slots are covered by connected gamepads', () => {
    const gamepads = [makeGamepad(0, 'Xbox Controller'), makeGamepad(1, 'Xbox Controller')];
    const assignments = [
      { slot: 1, device: 0 as number | 'keyboard' | null },
      { slot: 2, device: 1 as number | 'keyboard' | null },
    ];
    const result = validateInputConfig([1, 2], assignments, gamepads);
    expect(result.valid).toBe(true);
    expect(result.issues).toHaveLength(0);
  });

  it('returns valid when a slot uses keyboard', () => {
    const result = validateInputConfig(
      [1],
      [{ slot: 1, device: 'keyboard' }],
      [],
    );
    expect(result.valid).toBe(true);
    expect(result.issues).toHaveLength(0);
  });

  it('reports error when a slot has no assignment', () => {
    const result = validateInputConfig(
      [1, 2],
      [{ slot: 1, device: 0 }, { slot: 2, device: null }],
      [makeGamepad(0, 'Xbox Controller')],
    );
    expect(result.valid).toBe(false);
    const error = result.issues.find((i) => i.slot === 2);
    expect(error?.severity).toBe('error');
  });

  it('reports error when an assigned gamepad is disconnected', () => {
    const result = validateInputConfig(
      [1],
      [{ slot: 1, device: 2 }],
      [makeGamepad(0, 'Xbox Controller')],  // index 2 not present
    );
    expect(result.valid).toBe(false);
    expect(result.issues[0].severity).toBe('error');
    expect(result.issues[0].slot).toBe(1);
  });

  it('warns when multiple slots share the keyboard', () => {
    const result = validateInputConfig(
      [1, 2],
      [
        { slot: 1, device: 'keyboard' },
        { slot: 2, device: 'keyboard' },
      ],
      [],
    );
    expect(result.valid).toBe(true);  // warning, not error
    const warning = result.issues.find((i) => i.severity === 'warning');
    expect(warning).toBeDefined();
    expect(warning?.message).toMatch(/keyboard/i);
  });

  it('returns warning (not error) for empty player slots', () => {
    const result = validateInputConfig([], [], []);
    expect(result.valid).toBe(true);
    expect(result.issues[0].severity).toBe('warning');
  });

  it('handles a mix of valid and invalid slots correctly', () => {
    const gamepads = [makeGamepad(0, 'Xbox Controller')];
    const assignments = [
      { slot: 1, device: 0 as number | 'keyboard' | null },
      { slot: 2, device: null as number | 'keyboard' | null },
      { slot: 3, device: 'keyboard' as number | 'keyboard' | null },
    ];
    const result = validateInputConfig([1, 2, 3], assignments, gamepads);
    expect(result.valid).toBe(false);
    const errors = result.issues.filter((i) => i.severity === 'error');
    expect(errors).toHaveLength(1);
    expect(errors[0].slot).toBe(2);
  });
});
