/**
 * Netplay Whitelist
 *
 * Curated list of games rated for online netplay safety.
 * Each entry carries a safety rating and a human-readable reason,
 * so the UI can surface appropriate warnings before a room is hosted.
 *
 * Ratings:
 *   approved    — deterministic, well-tested online; first-class experience
 *   caution     — works online but may have occasional desync or quirks
 *   incompatible — known-broken for online play; block or strongly discourage
 */

// ─────────────────────────────────────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────────────────────────────────────

export type NetplayRating = 'approved' | 'caution' | 'incompatible';

export interface NetplayEntry {
  /** Game ID matching `mock-games.ts` / lobby catalog keys. */
  gameId: string;
  /** Lowercase system identifier. */
  system: string;
  /** Online netplay safety rating. */
  rating: NetplayRating;
  /**
   * Reason surfaced to the user.
   * Keep these factual — no hype, just honest status.
   */
  reason: string;
  /**
   * Recommended maximum player count for stable online sessions.
   * Omit when the default max is fine.
   */
  recommendedMaxPlayers?: number;
}

// ─────────────────────────────────────────────────────────────────────────────
// Catalog
// ─────────────────────────────────────────────────────────────────────────────

export const NETPLAY_WHITELIST: NetplayEntry[] = [
  // ── PSX ────────────────────────────────────────────────────────────────────
  {
    gameId: 'psx-crash-team-racing',
    system: 'psx',
    rating: 'approved',
    reason: 'Fully deterministic race logic; excellent record with SwanStation netplay.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'psx-tekken-3',
    system: 'psx',
    rating: 'approved',
    reason: 'Input-driven fighter with no random physics; very low desync rate.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-street-fighter-alpha-3',
    system: 'psx',
    rating: 'approved',
    reason: 'Deterministic 2D fighter; recommended at ≤ 100 ms one-way latency.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-spyro-2',
    system: 'psx',
    rating: 'caution',
    reason: 'No multiplayer mode — online session is for synchronized co-viewing only.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-resident-evil-2',
    system: 'psx',
    rating: 'caution',
    reason: 'Single-player game; occasional FMV desync reported at > 80 ms latency.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-final-fantasy-vii',
    system: 'psx',
    rating: 'caution',
    reason: 'Turn-based game tolerates latency well, but random-encounter RNG can desync under some emulator configs.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-castlevania-sotn',
    system: 'psx',
    rating: 'caution',
    reason: 'Item-drop RNG is non-deterministic with some BIOS revisions; use scph5501 for best results.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-metal-gear-solid',
    system: 'psx',
    rating: 'caution',
    reason: 'FMV-heavy; streaming decompression can drift at high latency. Keep sessions under 4 hours.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-crash-bandicoot',
    system: 'psx',
    rating: 'approved',
    reason: 'Single-player platformer; fully deterministic movement. Excellent for synchronized co-viewing and race challenges.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-tony-hawks-pro-skater-2',
    system: 'psx',
    rating: 'approved',
    reason: 'Deterministic score-attack sessions; very low desync rate at ≤ 100 ms.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-crash-bash',
    system: 'psx',
    rating: 'approved',
    reason: 'Minigame-based party game; short bursts are tolerant of moderate latency. Multitap required for 4P.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'psx-worms-armageddon',
    system: 'psx',
    rating: 'approved',
    reason: 'Turn-based strategy; extremely latency-tolerant. Reliable at up to 200 ms one-way.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'psx-twisted-metal-2',
    system: 'psx',
    rating: 'caution',
    reason: 'Physics determinism holds in 1v1; 4-player sessions may desync at > 120 ms due to collision resolution.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-gran-turismo-2',
    system: 'psx',
    rating: 'caution',
    reason: 'Physics simulation is partially non-deterministic; desyncs reported at > 80 ms. Analog controller required.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'psx-diablo',
    system: 'psx',
    rating: 'caution',
    reason: 'No multiplayer mode on PS1 — online session is spectate / co-viewing only.',
    recommendedMaxPlayers: 2,
  },

  // ── NES ────────────────────────────────────────────────────────────────────
  {
    gameId: 'nes-super-mario-bros',
    system: 'nes',
    rating: 'approved',
    reason: 'Fully deterministic; the gold standard for netplay testing.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'nes-double-dragon',
    system: 'nes',
    rating: 'approved',
    reason: 'Deterministic co-op brawler; well-tested with Nestopia UE core.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'nes-contra',
    system: 'nes',
    rating: 'approved',
    reason: 'Pure input-driven action; no floating-point or timer drift.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'nes-punch-out',
    system: 'nes',
    rating: 'approved',
    reason: 'Deterministic AI and combat; 2P "cheering" mode works reliably.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'nes-excitebike',
    system: 'nes',
    rating: 'caution',
    reason: 'Parallelism between AI and player racers can diverge slightly at > 60 ms.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'nes-battletoads',
    system: 'nes',
    rating: 'caution',
    reason: 'Notorious for co-op hitbox quirks; mild desync possible in speed-run stages.',
    recommendedMaxPlayers: 2,
  },

  // ── SNES ───────────────────────────────────────────────────────────────────
  {
    gameId: 'snes-super-mario-kart',
    system: 'snes',
    rating: 'approved',
    reason: 'Deterministic racing; reliable with Snes9x netplay at ≤ 80 ms.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'snes-street-fighter-ii-turbo',
    system: 'snes',
    rating: 'approved',
    reason: 'Classic 2D fighter; deterministic frame logic and excellent community testing.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'snes-super-bomberman-2',
    system: 'snes',
    rating: 'approved',
    reason: 'Up to 4-player; deterministic grid-based logic.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'snes-secret-of-mana',
    system: 'snes',
    rating: 'caution',
    reason: 'Action-RPG with real-time AI; enemy AI timer can desync at > 100 ms.',
    recommendedMaxPlayers: 3,
  },
  {
    gameId: 'snes-earthbound',
    system: 'snes',
    rating: 'caution',
    reason: 'Single-player; sessions are co-viewing only; emulator anti-piracy patch required.',
    recommendedMaxPlayers: 2,
  },

  // ── GBA ────────────────────────────────────────────────────────────────────
  {
    gameId: 'gba-mario-kart-super-circuit',
    system: 'gba',
    rating: 'approved',
    reason: 'Deterministic racing; tested extensively with mGBA link-cable emulation.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'gba-f-zero-maximum-velocity',
    system: 'gba',
    rating: 'approved',
    reason: 'Pure deterministic physics; no random elements.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'gba-pokemon-firered',
    system: 'gba',
    rating: 'caution',
    reason: 'Battle RNG is seeded from frame counter — timing drift can cause desyncs in long sessions.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'gba-golden-sun',
    system: 'gba',
    rating: 'caution',
    reason: 'Single-player RPG; co-viewing only, djinn RNG may diverge.',
    recommendedMaxPlayers: 2,
  },

  // ── N64 ────────────────────────────────────────────────────────────────────
  {
    gameId: 'n64-mario-kart-64',
    system: 'n64',
    rating: 'approved',
    reason: 'Best-tested N64 netplay title; item RNG is deterministic.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-super-smash-bros',
    system: 'n64',
    rating: 'approved',
    reason: 'Deterministic physics; large community with documented netplay guides.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-goldeneye-007',
    system: 'n64',
    rating: 'caution',
    reason: '4P split-screen degrades emulator performance; keep to 2P for stable sessions.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'n64-banjo-kazooie',
    system: 'n64',
    rating: 'caution',
    reason: 'Single-player; note RNG divergence can occur after long play at high latency.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'n64-f-zero-x',
    system: 'n64',
    rating: 'approved',
    reason: 'Deterministic race physics; works well with Mupen64Plus netplay at ≤ 120 ms.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-diddy-kong-racing',
    system: 'n64',
    rating: 'approved',
    reason: 'Deterministic kart physics similar to MK64; up to 4 players supported.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-mario-tennis',
    system: 'n64',
    rating: 'approved',
    reason: 'Turn-based rally mechanics are deterministic; stable at ≤ 80 ms.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-mario-golf',
    system: 'n64',
    rating: 'approved',
    reason: 'Turn-based golf tolerates high latency gracefully; great online experience.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-pokemon-stadium',
    system: 'n64',
    rating: 'caution',
    reason: 'Battle animations can cause timing drift above 80 ms; set speed to Fast.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-pokemon-stadium-2',
    system: 'n64',
    rating: 'caution',
    reason: 'Same timing-drift issue as Stadium 1; set battle speed to Fastest.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-wave-race-64',
    system: 'n64',
    rating: 'approved',
    reason: 'Deterministic wave physics; 2-player mode works well at ≤ 80 ms.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'n64-star-fox-64',
    system: 'n64',
    rating: 'approved',
    reason: 'VS battle mode is fully deterministic; requires Expansion Pak for 4P.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-perfect-dark',
    system: 'n64',
    rating: 'caution',
    reason: 'Requires Expansion Pak; CPU-heavy in 4P — keep to 2–3P for stable sessions.',
    recommendedMaxPlayers: 3,
  },
  {
    gameId: 'n64-mario-party',
    system: 'n64',
    rating: 'approved',
    reason: 'Turn-based board mini-games tolerate up to 120 ms; fill all 4 slots.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'n64-bomberman-64',
    system: 'n64',
    rating: 'approved',
    reason: 'Battle mode is fully deterministic; excellent 4-player netplay title.',
    recommendedMaxPlayers: 4,
  },

  // ── Genesis ────────────────────────────────────────────────────────────────
  {
    gameId: 'genesis-sonic-the-hedgehog-2',
    system: 'genesis',
    rating: 'approved',
    reason: '2P co-op/competition is fully deterministic with BlastEm core.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'genesis-streets-of-rage-2',
    system: 'genesis',
    rating: 'approved',
    reason: 'Deterministic beat-em-up; recommended as a first Genesis netplay test.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'genesis-toe-jam-and-earl',
    system: 'genesis',
    rating: 'caution',
    reason: 'Procedural map generation uses non-deterministic RNG after reset.',
    recommendedMaxPlayers: 2,
  },

  // ── SMS ────────────────────────────────────────────────────────────────────
  {
    gameId: 'sms-alex-kidd-miracle-world',
    system: 'sms',
    rating: 'approved',
    reason: 'Deterministic platformer; no random elements.',
    recommendedMaxPlayers: 1,
  },
  {
    gameId: 'sms-sonic-the-hedgehog',
    system: 'sms',
    rating: 'approved',
    reason: 'Deterministic; tested with Genesis Plus GX core.',
    recommendedMaxPlayers: 1,
  },

  // ── NDS ────────────────────────────────────────────────────────────────────
  {
    gameId: 'nds-mario-kart-ds',
    system: 'nds',
    rating: 'approved',
    reason: 'Wiimmfi-enabled; deterministic racing with melonDS link-cable emulation.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'nds-pokemon-diamond',
    system: 'nds',
    rating: 'caution',
    reason: 'Battle RNG seeded from RTC — time drift can cause desync in long online sessions.',
    recommendedMaxPlayers: 2,
  },
  {
    gameId: 'nds-metroid-prime-hunters',
    system: 'nds',
    rating: 'approved',
    reason: 'Wiimmfi-enabled; deterministic physics tested extensively with melonDS.',
    recommendedMaxPlayers: 4,
  },

  // ── Wii ────────────────────────────────────────────────────────────────────
  {
    gameId: 'wii-mario-kart-wii',
    system: 'wii',
    rating: 'approved',
    reason: 'Best-tested Wii online title; Dolphin + Wiimmfi is stable when everyone matches region, NAND, and controller settings.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'wii-super-smash-bros-brawl',
    system: 'wii',
    rating: 'caution',
    reason: 'Works online, but Brawl is delay-based and benefits from conservative buffer settings plus identical Gecko/Wiimmfi patch state.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'wii-new-super-mario-bros-wii',
    system: 'wii',
    rating: 'caution',
    reason: 'Co-op is playable in Dolphin Netplay, but physics interactions and bubble-respawn timing can drift during long sessions.',
    recommendedMaxPlayers: 4,
  },

  // ── PSP ────────────────────────────────────────────────────────────────────
  {
    gameId: 'psp-monster-hunter-freedom-unite',
    system: 'psp',
    rating: 'caution',
    reason: 'Ad-hoc multiplayer via PPSSPP; monster AI has non-deterministic elements at high latency.',
    recommendedMaxPlayers: 4,
  },
  {
    gameId: 'psp-dissidia-final-fantasy',
    system: 'psp',
    rating: 'caution',
    reason: 'Ad-hoc fighter; works for casual play but competitive use not recommended.',
    recommendedMaxPlayers: 2,
  },
];

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/** Look up the netplay entry for a specific game. Returns `null` when not listed. */
export function getNetplayEntry(gameId: string): NetplayEntry | null {
  return NETPLAY_WHITELIST.find((e) => e.gameId === gameId) ?? null;
}

/**
 * Returns `true` when a game is explicitly `'approved'` for netplay.
 * Unlisted games are treated as `'caution'` (not blocked, but no guarantee).
 */
export function isNetplayApproved(gameId: string): boolean {
  return getNetplayEntry(gameId)?.rating === 'approved';
}

/**
 * Returns `true` when a game is explicitly rated `'incompatible'`.
 * Use this to block room creation for known-broken titles.
 */
export function isNetplayIncompatible(gameId: string): boolean {
  return getNetplayEntry(gameId)?.rating === 'incompatible';
}

/**
 * Returns a warning string for a caution- or incompatible-rated game,
 * or `null` when the game is approved / not listed.
 */
export function getNetplayWarning(gameId: string): string | null {
  const entry = getNetplayEntry(gameId);
  if (!entry || entry.rating === 'approved') return null;
  return entry.reason;
}

/**
 * All approved entries for a given system.
 * Useful for "recommended netplay titles" UI surfaces.
 */
export function approvedGamesForSystem(system: string): NetplayEntry[] {
  return NETPLAY_WHITELIST.filter(
    (e) => e.system === system.toLowerCase() && e.rating === 'approved',
  );
}
