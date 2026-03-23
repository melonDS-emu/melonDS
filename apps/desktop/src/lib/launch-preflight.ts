/**
 * Launch preflight checks.
 *
 * Validates the preconditions for a local or room launch and returns a
 * structured result with per-check status and user-facing messages.
 *
 * Three severity levels:
 *   ok      — check passed
 *   warning — non-blocking issue the user should know about
 *   blocked — launch cannot proceed until resolved
 */

import { resolveOnlineSupport, type OnlineSupportLevel } from './game-capability';
import { systemSupportsAchievements } from './achievement-capability';
import { systemRequiresBios } from './bios-validator';

// ─────────────────────────────────────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────────────────────────────────────

export type PreflightStatus = 'ok' | 'warning' | 'blocked';

/** A single atomic check in the preflight sequence. */
export interface LaunchPreflightCheck {
  /** Stable identifier for this check (useful for test assertions). */
  id: string;
  /** Result of this check. */
  status: PreflightStatus;
  /** User-visible explanation shown in the UI. */
  message: string;
}

/** Aggregated result returned by `runLaunchPreflight`. */
export interface LaunchPreflightResult {
  /**
   * Whether the launch should proceed.
   * `false` when any check has status `'blocked'`.
   */
  canLaunch: boolean;
  /** Worst-case status across all checks. */
  status: PreflightStatus;
  /** All checks that produced a warning or blocked status (passes are omitted). */
  issues: LaunchPreflightCheck[];
  /** All checks including passes — useful for diagnostic display. */
  allChecks: LaunchPreflightCheck[];
}

/** Options for `runLaunchPreflight`. */
export interface LaunchPreflightOptions {
  /** The system identifier (lowercase, e.g. `'nes'`, `'sms'`, `'gba'`). */
  system: string;
  /** Whether this is a local or online / room launch. */
  mode: 'local' | 'online';
  /** Associated ROM file path, or null if not set. */
  romPath?: string | null;
  /** Emulator executable path, or null if not configured. */
  emulatorPath?: string | null;
  /** Active backend used for launch/runtime messaging. */
  backendId?: string | null;
  /**
   * Path to the required BIOS file for this system, or null when not provided.
   * Only checked when `systemRequiresBios(system)` returns true.
   */
  biosPath?: string | null;
  /** Whether RetroAchievements credentials are configured. */
  raConnected?: boolean;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper
// ─────────────────────────────────────────────────────────────────────────────

function buildOnlineWarning(level: OnlineSupportLevel, system: string): string {
  const label = system.toUpperCase();
  switch (level) {
    case 'local-only':
      return `Online play is not supported for ${label}. Rooms can be used for local coordination only.`;
    case 'experimental':
      return `Online play for ${label} is experimental — expect possible instability or desync.`;
    default:
      return '';
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Preflight runner
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Run all preflight checks for a local or online launch.
 *
 * Returns a `LaunchPreflightResult` describing the readiness state.
 * Components should surface `issues` to the user and gate the launch
 * button on `canLaunch`.
 */
export function runLaunchPreflight(opts: LaunchPreflightOptions): LaunchPreflightResult {
  const allChecks: LaunchPreflightCheck[] = [];

  function pass(id: string): void {
    allChecks.push({ id, status: 'ok', message: '' });
  }
  function warn(id: string, message: string): void {
    allChecks.push({ id, status: 'warning', message });
  }
  function block(id: string, message: string): void {
    allChecks.push({ id, status: 'blocked', message });
  }

  const systemKey = opts.system.toLowerCase();

  // ── 1. ROM availability ────────────────────────────────────────────────────
  if (opts.romPath) {
    pass('rom-available');
  } else {
    block(
      'rom-missing',
      'No ROM file associated with this game. Use "Set ROM File" in Advanced Settings, or run a ROM scan from the Library.',
    );
  }

  // ── 2. Emulator configured ─────────────────────────────────────────────────
  if (opts.emulatorPath) {
    pass('emulator-configured');
  } else {
    block(
      'emulator-not-configured',
      `Emulator not configured for ${systemKey.toUpperCase()}. Go to Settings → Emulator Paths to set the executable location.`,
    );
  }

  // ── 3. BIOS file (systems that require it) ─────────────────────────────────
  if (systemRequiresBios(systemKey)) {
    if (opts.biosPath) {
      pass('bios-configured');
    } else {
      block(
        'bios-missing',
        `A BIOS file is required for ${systemKey.toUpperCase()}. Go to Settings → Emulator Paths to configure the BIOS directory.`,
      );
    }
  }

  // ── 4. Online support (online mode only) ───────────────────────────────────
  if (opts.mode === 'online') {
    const { level, note } = resolveOnlineSupport(systemKey, opts.backendId);
    if (level === 'local-only' || level === 'experimental') {
      warn(`online-${level}`, note || buildOnlineWarning(level, systemKey));
    } else {
      pass('online-supported');
    }
  }

  // ── 5. RetroAchievements (local mode, optional warning) ───────────────────
  if (opts.mode === 'local' && systemSupportsAchievements(systemKey) && !opts.raConnected) {
    warn(
      'achievements-not-configured',
      'RetroAchievements not connected — achievement progress will not be tracked. Configure in Settings → RetroAchievements.',
    );
  }

  const issues = allChecks.filter((c) => c.status !== 'ok');
  const hasBlocked = issues.some((c) => c.status === 'blocked');
  const hasWarning = issues.some((c) => c.status === 'warning');
  const status: PreflightStatus = hasBlocked ? 'blocked' : hasWarning ? 'warning' : 'ok';

  return {
    canLaunch: !hasBlocked,
    status,
    issues,
    allChecks,
  };
}
