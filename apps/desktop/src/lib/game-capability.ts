/**
 * Centralized game capability model for RetroOasis.
 *
 * Answers system-level and game-level questions about what is supported so
 * that UI components consume resolved capability state rather than computing
 * it ad hoc.
 */

import { getAchievementCapability, systemSupportsAchievements } from './achievement-capability';

// ─────────────────────────────────────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────────────────────────────────────

/** Indicates the level of online / room-play support for a system. */
export type OnlineSupportLevel = 'supported' | 'partial' | 'experimental' | 'local-only';

export interface OnlineSupportDescriptor {
  level: OnlineSupportLevel;
  label: string;
  note: string;
}

/** Actions that can be surfaced for a game in the UI. */
export type GameAction =
  | 'play'                     // Play locally
  | 'host-room'                // Host an online room
  | 'join-room'                // Join an existing room
  | 'configure-emulator'       // Go to Settings → Emulator Paths
  | 'configure-achievements';  // Go to Settings → RetroAchievements

/** Resolved capability state for a game / system combination. */
export interface GameCapability {
  /** Whether local single-player launch is possible given current setup. */
  canPlayLocally: boolean;
  /** Reason local play is blocked, if applicable. */
  localPlayBlockReason?: string;

  /** System-level online support classification. */
  onlineSupportLevel: OnlineSupportLevel;
  /** Short label for the online support level (e.g. "Online Ready"). */
  onlineSupportLabel: string;
  /** One-line note explaining the online support state. */
  onlineSupportNote: string;

  /** Whether room actions (host/join) should be surfaced. */
  canHostRoom: boolean;
  /** Whether joining existing rooms should be surfaced. */
  canJoinRoom: boolean;

  /** RA achievement support level for the system. */
  achievementSupport: 'full' | 'partial' | 'none';
  /** Whether achievements are potentially available for this game. */
  achievementsAvailable: boolean;

  /** Whether the configured emulator path is present. */
  emulatorConfigured: boolean;
  /** Whether a ROM file is associated. */
  romAvailable: boolean;

  /** Ordered list of actions that should be enabled / offered in the UI. */
  availableActions: GameAction[];
}

// ─────────────────────────────────────────────────────────────────────────────
// System online-support matrix
// ─────────────────────────────────────────────────────────────────────────────

/**
 * System-level online play support classification.
 *
 * - `'supported'`    — works reliably via RetroArch / standalone netplay
 * - `'experimental'` — functional but may be unstable; user should be aware
 * - `'local-only'`   — no true online netplay; room creation is coordination only
 */
export const SYSTEM_ONLINE_SUPPORT: Record<string, OnlineSupportLevel> = {
  nes:        'supported',
  snes:       'supported',
  gb:         'experimental',
  gbc:        'experimental',
  gba:        'experimental',
  n64:        'partial',
  nds:        'experimental',  // WFC via Wiimmfi relay; local wireless via download play
  gc:         'experimental',
  wii:        'experimental',
  wiiu:       'local-only',    // Cemu has no online netplay support
  '3ds':      'local-only',    // Citra local play only
  genesis:    'supported',
  sms:        'supported',     // RetroArch Genesis Plus GX relay
  dreamcast:  'experimental',  // Flycast — growing support
  psx:        'supported',     // PCSX-ReARMed netplay
  ps2:        'experimental',  // PCSX2 — limited support
  psp:        'experimental',  // PPSSPP ad-hoc / infrastructure
};

const ONLINE_LABELS: Record<OnlineSupportLevel, string> = {
  supported:    'Online Ready',
  partial:      'Partial Online',
  experimental: 'Experimental',
  'local-only': 'Local Only',
};

const ONLINE_NOTES: Record<string, string> = {
  nes:        'Reliable online play via RetroArch rollback netplay.',
  snes:       'Reliable online play via RetroArch rollback netplay.',
  gb:         'Game Boy Link Cable emulation — experimental relay.',
  gbc:        'Game Boy Color Link Cable emulation — experimental relay.',
  gba:        'GBA Link Cable emulation is experimental via mGBA.',
  n64:        'N64 online via Mupen64Plus — works well for party titles like MK64, Smash, and Bomberman. Expansion Pak games (Perfect Dark, Star Fox VS) require the plugin enabled.',
  nds:        'WFC games use Wiimmfi; local wireless uses the Download Play relay.',
  gc:         'GameCube online via Dolphin is experimental.',
  wii:        'Wii online via Dolphin is experimental — Wiimmfi required for some titles.',
  wiiu:       'Cemu does not support online netplay. Local play only.',
  '3ds':      'Citra local play only — no internet netplay available.',
  genesis:    'Reliable online play via RetroArch Genesis Plus GX.',
  sms:        'Online play via RetroArch Genesis Plus GX relay.',
  dreamcast:  'Flycast online is experimental — growing game coverage.',
  psx:        'Reliable online play via RetroArch PCSX-ReARMed.',
  ps2:        'PCSX2 online is experimental — limited game support.',
  psp:        'PPSSPP infrastructure mode is experimental.',
};

const BACKEND_ONLINE_OVERRIDES: Record<string, OnlineSupportDescriptor> = {
  'psx:duckstation': {
    level: 'experimental',
    label: ONLINE_LABELS.experimental,
    note:
      'DuckStation is excellent for local PSX play, but RetroOasis cannot auto-wire true online netplay for it. Use RetroArch for synced online sessions; DuckStation rooms are best for matchmaking or co-viewing.',
  },
  'psx:retroarch': {
    level: 'supported',
    label: ONLINE_LABELS.supported,
    note: 'RetroArch with a compatible PSX core provides the most reliable fully synced PlayStation netplay flow.',
  },
  'wii:dolphin': {
    level: 'experimental',
    label: ONLINE_LABELS.experimental,
    note:
      'Dolphin supports Wii multiplayer, but most online sessions still need Dolphin Netplay or Wiimmfi setup. RetroOasis rooms coordinate players and launch data; expect per-game setup work.',
  },
};

/**
 * Resolve online-support messaging for a system, optionally refined by the
 * currently selected emulator backend.
 */
export function resolveOnlineSupport(systemId: string, backendId?: string | null): OnlineSupportDescriptor {
  const key = systemId.toLowerCase();
  const backendKey = backendId?.toLowerCase();
  const override = backendKey ? BACKEND_ONLINE_OVERRIDES[`${key}:${backendKey}`] : undefined;
  if (override) return override;

  const level = SYSTEM_ONLINE_SUPPORT[key] ?? 'supported';
  return {
    level,
    label: ONLINE_LABELS[level],
    note: ONLINE_NOTES[key] ?? 'Online play availability depends on the emulator backend.',
  };
}

// ─────────────────────────────────────────────────────────────────────────────
// Capability resolver
// ─────────────────────────────────────────────────────────────────────────────

export interface ResolveCapabilityOptions {
  /** Emulator executable path (from settings), or null/undefined if not configured. */
  emulatorPath?: string | null;
  /** Associated ROM file path, or null/undefined if not set. */
  romPath?: string | null;
  /** Whether RetroAchievements credentials are configured. */
  raConnected?: boolean;
  /** Whether this game has known achievement definitions. */
  hasAchievementDefs?: boolean;
  /** Active emulator backend used for launch/runtime messaging. */
  backendId?: string | null;
}

/**
 * Resolve the full capability state for a game / system combination.
 *
 * Pass runtime state (emulator path, ROM path, RA credentials) to get an
 * accurate picture of what is currently possible for this game.
 */
export function resolveGameCapability(
  systemId: string,
  opts: ResolveCapabilityOptions = {},
): GameCapability {
  const key = systemId.toLowerCase();

  // Online support ────────────────────────────────────────────────────────────
  const onlineSupport = resolveOnlineSupport(key, opts.backendId);
  const onlineSupportLevel = onlineSupport.level;
  const onlineSupportLabel = onlineSupport.label;
  const onlineSupportNote  = onlineSupport.note;

  // Config state ──────────────────────────────────────────────────────────────
  const emulatorConfigured = Boolean(opts.emulatorPath);
  const romAvailable       = Boolean(opts.romPath);

  // Local launch readiness ────────────────────────────────────────────────────
  const canPlayLocally = emulatorConfigured && romAvailable;
  let localPlayBlockReason: string | undefined;
  if (!emulatorConfigured && !romAvailable) {
    localPlayBlockReason = 'No emulator configured and no ROM file set.';
  } else if (!emulatorConfigured) {
    localPlayBlockReason = 'No emulator configured. Set the path in Settings → Emulator Paths.';
  } else if (!romAvailable) {
    localPlayBlockReason = 'No ROM file associated. Use "Set ROM File" or scan your ROM directory.';
  }

  // Room actions ──────────────────────────────────────────────────────────────
  // Room creation is always surfaced — the lobby layer is a coordination mechanism
  // even for systems without true online netplay.
  const canHostRoom = true;
  const canJoinRoom = true;

  // Achievements ──────────────────────────────────────────────────────────────
  const achCap = getAchievementCapability(key);
  const achievementSupport  = achCap.level;
  const achievementsAvailable =
    systemSupportsAchievements(key) && Boolean(opts.hasAchievementDefs);

  // Available actions ─────────────────────────────────────────────────────────
  const availableActions: GameAction[] = [];

  if (canPlayLocally) {
    availableActions.push('play');
  }
  if (!emulatorConfigured) {
    availableActions.push('configure-emulator');
  }
  availableActions.push('host-room');
  availableActions.push('join-room');
  if (achievementSupport !== 'none' && !opts.raConnected) {
    availableActions.push('configure-achievements');
  }

  return {
    canPlayLocally,
    localPlayBlockReason,
    onlineSupportLevel,
    onlineSupportLabel,
    onlineSupportNote,
    canHostRoom,
    canJoinRoom,
    achievementSupport,
    achievementsAvailable,
    emulatorConfigured,
    romAvailable,
    availableActions,
  };
}

/**
 * Returns the icon emoji for a given online support level.
 */
export function onlineSupportIcon(level: OnlineSupportLevel): string {
  switch (level) {
    case 'supported':    return '🌐';
    case 'partial':      return '🌍';
    case 'experimental': return '⚡';
    case 'local-only':   return '🔌';
  }
}

/**
 * Returns a short descriptive phrase for a given online support level
 * suitable for inline display (e.g. in HostRoomModal).
 */
export function onlineSupportSummary(level: OnlineSupportLevel): string {
  switch (level) {
    case 'supported':    return 'Online Ready — reliable netplay for this system.';
    case 'partial':      return 'Partial Online — works well for select titles; check the Netplay Whitelist.';
    case 'experimental': return 'Experimental — online play may be unstable for this system.';
    case 'local-only':   return 'Local Only — no true online netplay. Room used for coordination.';
  }
}

/**
 * Returns badge colours suitable for rendering the online support level.
 */
export function onlineSupportBadgeColor(level: OnlineSupportLevel): { bg: string; text: string } {
  switch (level) {
    case 'supported':    return { bg: 'rgba(22,163,74,0.15)',  text: '#4ade80' };
    case 'partial':      return { bg: 'rgba(34,197,94,0.12)',  text: '#86efac' };
    case 'experimental': return { bg: 'rgba(245,158,11,0.15)', text: '#fbbf24' };
    case 'local-only':   return { bg: 'rgba(99,102,241,0.15)', text: '#a5b4fc' };
  }
}
