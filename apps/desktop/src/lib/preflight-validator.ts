/**
 * Preflight input validator.
 *
 * Checks that every required player slot has a valid, connected input device
 * before a session starts.  Call `validateInputConfig` from the
 * "Start Game" flow and surface any warnings / errors to the host.
 */

import type { GamepadInfo } from './controller-detection';
import type { SlotAssignment } from './slot-assignment';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type PreflightSeverity = 'error' | 'warning';

/** A single preflight finding. */
export interface PreflightIssue {
  /** Severity level. */
  severity: PreflightSeverity;
  /** Human-readable message suitable for display in the UI. */
  message: string;
  /** Player slot this issue relates to, or undefined for global issues. */
  slot?: number;
}

/** Result returned by `validateInputConfig`. */
export interface PreflightResult {
  /**
   * Whether the configuration is valid enough to start.
   * `false` if any `error`-level issue was found.
   */
  valid: boolean;
  /** All issues found (errors + warnings). */
  issues: PreflightIssue[];
}

// ---------------------------------------------------------------------------
// Validation logic
// ---------------------------------------------------------------------------

/**
 * Validate that every player slot has an assigned and (where applicable)
 * connected input device.
 *
 * @param playerSlots  1-based slot numbers that must be covered.
 * @param assignments  Current slot ↔ device assignments.
 * @param gamepads     Snapshot of currently-connected gamepads.
 */
export function validateInputConfig(
  playerSlots: number[],
  assignments: SlotAssignment[],
  gamepads: GamepadInfo[],
): PreflightResult {
  const issues: PreflightIssue[] = [];

  const assignmentMap = new Map<number, SlotAssignment>();
  for (const a of assignments) {
    assignmentMap.set(a.slot, a);
  }

  const connectedIndices = new Set(gamepads.filter((g) => g.connected).map((g) => g.index));

  for (const slot of playerSlots) {
    const assignment = assignmentMap.get(slot);

    // No assignment at all
    if (!assignment || assignment.device === null) {
      issues.push({
        severity: 'error',
        message: `Player ${slot} has no controller assigned. Assign a gamepad or keyboard before starting.`,
        slot,
      });
      continue;
    }

    const { device } = assignment;

    if (device === 'keyboard') {
      // Keyboard is always valid — just a warning if multiple slots share it
      const keyboardSlots = [...assignmentMap.values()].filter(
        (a) => a.device === 'keyboard' && playerSlots.includes(a.slot),
      );
      if (keyboardSlots.length > 1 && slot === keyboardSlots[0].slot) {
        issues.push({
          severity: 'warning',
          message: `Multiple players are assigned to the keyboard. Only one player at a time can use it effectively.`,
        });
      }
      continue;
    }

    // Gamepad index: check that it is still connected
    if (!connectedIndices.has(device)) {
      issues.push({
        severity: 'error',
        message: `Gamepad ${device} assigned to Player ${slot} is no longer connected. Reconnect it or reassign the slot.`,
        slot,
      });
    }
  }

  // Warn if no players at all
  if (playerSlots.length === 0) {
    issues.push({
      severity: 'warning',
      message: 'No player slots defined. Nothing to validate.',
    });
  }

  const valid = !issues.some((i) => i.severity === 'error');
  return { valid, issues };
}
