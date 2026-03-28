/**
 * Controller slot-assignment service.
 *
 * Manages which physical input device (gamepad by index, or the keyboard) is
 * assigned to each multiplayer player slot.  Assignments are persisted to
 * localStorage so they survive page reloads.
 *
 * Slot numbers are 1-based (slot 1 = player 1, matching lobby conventions).
 */

import type { GamepadInfo } from './controller-detection';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

/** An assignment of a physical input device to a player slot. */
export interface SlotAssignment {
  /** 1-based player slot number. */
  slot: number;
  /**
   * The assigned input device:
   *  - A non-negative integer → gamepad index from the Gamepad API
   *  - `'keyboard'` → keyboard / mouse input
   *  - `null` → no device assigned yet
   */
  device: number | 'keyboard' | null;
}

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------

const STORAGE_KEY = 'retro-oasis-slot-assignments';

function loadAll(): Record<number, number | 'keyboard' | null> {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return raw ? (JSON.parse(raw) as Record<number, number | 'keyboard' | null>) : {};
  } catch {
    return {};
  }
}

function saveAll(assignments: Record<number, number | 'keyboard' | null>): void {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(assignments));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Return all stored slot assignments.
 * Slots without an explicit assignment will have `device: null`.
 */
export function getSlotAssignments(playerCount: number): SlotAssignment[] {
  const stored = loadAll();
  return Array.from({ length: playerCount }, (_, i) => {
    const slot = i + 1;
    return {
      slot,
      device: stored[slot] ?? null,
    };
  });
}

/**
 * Assign a device to a player slot.  Pass `null` to clear an assignment.
 */
export function setSlotAssignment(slot: number, device: number | 'keyboard' | null): void {
  const all = loadAll();
  if (device === null) {
    delete all[slot];
  } else {
    all[slot] = device;
  }
  saveAll(all);
}

/**
 * Remove all stored slot assignments.
 */
export function clearSlotAssignments(): void {
  localStorage.removeItem(STORAGE_KEY);
}

/**
 * Automatically assign available gamepads (and keyboard as fallback) to player
 * slots.  Already-assigned slots are left unchanged.  New gamepad assignments
 * are distributed in gamepad-index order.  If there are more slots than
 * connected gamepads, remaining slots fall back to `'keyboard'`.
 *
 * Returns the full list of resulting `SlotAssignment` objects.
 */
export function autoAssignSlots(
  playerCount: number,
  gamepads: GamepadInfo[],
): SlotAssignment[] {
  const stored = loadAll();

  // Determine which gamepad indices are already assigned
  const assignedGamepadIndices = new Set<number>(
    Object.values(stored).filter((v): v is number => typeof v === 'number'),
  );

  // Available gamepads: connected, not yet assigned
  const available = gamepads
    .filter((g) => g.connected && !assignedGamepadIndices.has(g.index))
    .map((g) => g.index);

  let availableIdx = 0;

  for (let s = 1; s <= playerCount; s++) {
    if (stored[s] !== undefined && stored[s] !== null) continue; // already assigned

    if (availableIdx < available.length) {
      stored[s] = available[availableIdx++];
    } else {
      // Fall back to keyboard for remaining slots
      stored[s] = 'keyboard';
    }
  }

  saveAll(stored);

  return Array.from({ length: playerCount }, (_, i) => ({
    slot: i + 1,
    device: stored[i + 1] ?? null,
  }));
}
