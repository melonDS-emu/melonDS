/**
 * Phase 30 — QoL & "Wow" Layer
 *
 * PatchStore — tracks safe, community-vetted patches and mods for retro games.
 * Only patches flagged as `safe: true` are returned by the public REST API.
 * "Safe" means the patch is purely informational (translation, QoL fix, or
 * official bug-fix) and contains no copyrighted ROM data — RetroOasis never
 * distributes ROM files or patch binaries, only metadata about them.
 */

export type PatchType = 'translation' | 'qol' | 'bugfix' | 'difficulty' | 'enhancement';

export interface GamePatch {
  id: string;
  gameId: string;
  name: string;
  description: string;
  type: PatchType;
  /** True when the patch contains no copyrighted data and is safe to reference. */
  safe: boolean;
  version: string;
  /** Informational URL where players can read about the patch (not a download). */
  infoUrl?: string;
  /** Short step-by-step instructions for applying the patch. */
  instructions: string[];
  /** Tags like 'recommended', 'multiplayer-compatible', 'beginner-friendly'. */
  tags: string[];
}

// ---------------------------------------------------------------------------
// Seed data — curated safe patches across supported games
// ---------------------------------------------------------------------------

const SEED_PATCHES: GamePatch[] = [
  // ── NES ────────────────────────────────────────────────────────────────────
  {
    id: 'nes-smb-qol-lives',
    gameId: 'nes-super-mario-bros',
    name: 'No Time Limit',
    description:
      'Removes the per-level time limit so players can explore at their own pace. Great for newcomers.',
    type: 'qol',
    safe: true,
    version: '1.0',
    instructions: [
      'Apply the IPS patch to a clean (USA) Super Mario Bros. ROM using Lunar IPS or FLIPS.',
      'Name the patched ROM so you can distinguish it from the original.',
    ],
    tags: ['beginner-friendly', 'qol'],
  },
  {
    id: 'nes-contra-qol-lives',
    gameId: 'nes-contra',
    name: 'Extra Lives Patch',
    description:
      'Increases the starting life count from 3 to 10, making Contra more approachable for co-op beginners.',
    type: 'qol',
    safe: true,
    version: '1.1',
    instructions: [
      'Apply using an IPS patcher to the USA Contra ROM.',
      'Host your room with the patched ROM — guest should use the same patch.',
    ],
    tags: ['beginner-friendly', 'multiplayer-compatible'],
  },
  {
    id: 'nes-mega-man-2-qol-energy',
    gameId: 'nes-mega-man-2',
    name: 'Weapon Energy Refill',
    description:
      'Refills all weapon energy at stage start instead of only E-tanks. Reduces backtracking.',
    type: 'qol',
    safe: true,
    version: '1.0',
    instructions: [
      'Apply the patch to a clean USA Mega Man 2 ROM.',
      'Works in solo play — not recommended for competitive netplay.',
    ],
    tags: ['qol', 'recommended'],
  },

  // ── SNES ───────────────────────────────────────────────────────────────────
  {
    id: 'snes-sf2t-fix-desyncs',
    gameId: 'snes-street-fighter-ii-turbo',
    name: 'Netplay Desync Fix',
    description:
      'Corrects a random number generator desync that can cause player states to diverge in co-op and VS modes.',
    type: 'bugfix',
    safe: true,
    version: '1.2',
    instructions: [
      'Apply the IPS patch to a clean USA Street Fighter II Turbo ROM.',
      'Both players must use the identically patched ROM.',
      'Verify ROM checksums match before starting the session.',
    ],
    tags: ['multiplayer-compatible', 'recommended'],
  },
  {
    id: 'snes-dkc-enhancement-hd',
    gameId: 'snes-donkey-kong-country',
    name: 'DKC Uncensored Restoration',
    description:
      'Restores minor content differences between regional ROM versions for consistency in international lobbies.',
    type: 'enhancement',
    safe: true,
    version: '2.0',
    instructions: [
      'Apply to a clean SNES DKC ROM matching the target region.',
      'Most useful when your lobby has players from different regions.',
    ],
    tags: ['multiplayer-compatible'],
  },

  // ── GB / GBC ────────────────────────────────────────────────────────────────
  {
    id: 'gb-pokemon-red-qol-poison',
    gameId: 'gb-pokemon-red',
    name: 'Poison Overworld Fix',
    description:
      'Prevents Poison from knocking out your Pokémon on the overworld, matching the mechanic introduced in later generations.',
    type: 'bugfix',
    safe: true,
    version: '1.0',
    instructions: [
      'Apply the IPS patch to a clean Pokémon Red (USA) ROM.',
      'Works in single-player and does not affect link-cable compatibility.',
    ],
    tags: ['qol', 'beginner-friendly'],
  },
  {
    id: 'gbc-pokemon-crystal-fixes',
    gameId: 'gbc-pokemon-crystal',
    name: 'Crystal Polished Fixes',
    description:
      'Community bug-fix compilation: fixes move animations, stat experience, and several minor text errors.',
    type: 'bugfix',
    safe: true,
    version: '3.0',
    infoUrl: 'https://www.romhacking.net/hacks/5298/',
    instructions: [
      'Apply to a clean Pokémon Crystal (USA) ROM.',
      'All trade/battle partners should use the same patched ROM for link play.',
    ],
    tags: ['recommended', 'multiplayer-compatible'],
  },

  // ── GBA ────────────────────────────────────────────────────────────────────
  {
    id: 'gba-pokemon-ruby-qol-repel',
    gameId: 'gba-pokemon-ruby',
    name: 'Repel Reminder Patch',
    description:
      'Asks if you want to use another Repel when one runs out — a QoL feature backported from Gen V.',
    type: 'qol',
    safe: true,
    version: '1.0',
    instructions: [
      'Apply to a clean Pokémon Ruby (USA) ROM.',
      'Does not affect link cable / wireless adapter compatibility.',
    ],
    tags: ['qol', 'beginner-friendly'],
  },
  {
    id: 'gba-four-swords-netplay-fix',
    gameId: 'gba-zelda-four-swords',
    name: 'Four Swords VBA-M Sync Fix',
    description:
      'Patches a known desync trigger that affects the VBA-M link cable emulation in 4-player mode.',
    type: 'bugfix',
    safe: true,
    version: '1.1',
    instructions: [
      'All four players must apply this patch to the same base ROM.',
      'Host must also enable "Force-sync frames" in VBA-M network settings.',
    ],
    tags: ['multiplayer-compatible', 'recommended'],
  },

  // ── N64 ────────────────────────────────────────────────────────────────────
  {
    id: 'n64-mario-kart-64-widescreen',
    gameId: 'n64-mario-kart-64',
    name: 'Widescreen 16:9 Patch',
    description:
      'Stretches the display to native 16:9 without distortion for modern monitors.',
    type: 'enhancement',
    safe: true,
    version: '1.3',
    instructions: [
      'Apply to a clean MK64 (USA) ROM using an N64 patcher.',
      'Enable "16:9 Correction" in Mupen64Plus display options.',
      'Cosmetic only — does not affect gameplay or netplay sync.',
    ],
    tags: ['enhancement', 'multiplayer-compatible'],
  },
  {
    id: 'n64-smash-64-netplay-fix',
    gameId: 'n64-super-smash-bros',
    name: 'Smash 64 Netplay Pack',
    description:
      'Combines the Netplay-optimised ROM patch with the standard tournament texture pack for consistent online play.',
    type: 'bugfix',
    safe: true,
    version: '1.5',
    instructions: [
      'Apply IPS patch to a clean Smash 64 (USA) ROM.',
      'Both players must use identical ROMs for a fair match.',
    ],
    tags: ['recommended', 'multiplayer-compatible'],
  },
  {
    id: 'n64-mario-party-2-speed',
    gameId: 'n64-mario-party-2',
    name: 'Mini-Game Speedup Patch',
    description:
      'Shortens the interstitial animations between mini-games, reducing a 10-board session by ~15 minutes.',
    type: 'qol',
    safe: true,
    version: '1.0',
    instructions: [
      'Apply to a clean Mario Party 2 (USA) ROM.',
      'All four players need the same patch for network-synced animation timing.',
    ],
    tags: ['qol', 'multiplayer-compatible', 'recommended'],
  },

  // ── NDS ────────────────────────────────────────────────────────────────────
  {
    id: 'nds-mario-kart-ds-region-fix',
    gameId: 'nds-mario-kart-ds',
    name: 'MK:DS Region Unlock',
    description:
      'Removes the region lock so USA and EUR players can race together through Wiimmfi lobbies.',
    type: 'bugfix',
    safe: true,
    version: '2.1',
    instructions: [
      'Apply to your regional ROM using an NDS patcher.',
      'All players in a mixed-region lobby need this patch.',
    ],
    tags: ['multiplayer-compatible', 'recommended'],
  },
  {
    id: 'nds-pokemon-diamond-qol-exp',
    gameId: 'nds-pokemon-diamond',
    name: 'Experience All Fix',
    description:
      'Backports the Exp. Share "all party members gain XP" behavior from Gen VI+, reducing grinding.',
    type: 'qol',
    safe: true,
    version: '1.0',
    instructions: [
      'Apply to a clean Pokémon Diamond (USA) ROM.',
      'Trade/battle link features are unaffected.',
    ],
    tags: ['qol', 'beginner-friendly'],
  },

  // ── Wii ────────────────────────────────────────────────────────────────────
  {
    id: 'wii-mario-kart-wii-gecko-qol',
    gameId: 'wii-mario-kart-wii',
    name: 'MKWii Gecko Codes — QoL Pack',
    description:
      'Community Gecko code set enabling 200cc test mode, no-snaking toggle, and item count HUD.',
    type: 'qol',
    safe: true,
    version: '4.0',
    instructions: [
      'Enable Gecko cheat support in Dolphin (Properties → Patches tab).',
      'Paste the provided GCT code list into the game\'s cheat config.',
      'All online players should agree on which codes to enable before racing.',
    ],
    tags: ['qol', 'multiplayer-compatible'],
  },

  // ── PSX ────────────────────────────────────────────────────────────────────
  {
    id: 'psx-ctr-enhancement-60fps',
    gameId: 'psx-crash-team-racing',
    name: 'CTR 60 FPS Unlock',
    description:
      'Patches the frame-limiter to allow DuckStation to render at 60 FPS instead of 30.',
    type: 'enhancement',
    safe: true,
    version: '1.0',
    instructions: [
      'Apply the patch to a clean CTR (USA) disc image.',
      'Enable in DuckStation: System → Enable 60fps patch.',
      'Note: online netplay partners need the same patch for sync.',
    ],
    tags: ['enhancement', 'multiplayer-compatible'],
  },

  // ── PSP ────────────────────────────────────────────────────────────────────
  {
    id: 'psp-mhfu-english-fix',
    gameId: 'psp-monster-hunter-freedom-unite',
    name: 'MHFU English Text Improvement Pack',
    description:
      'Community re-translation that improves weapon/monster descriptions for clarity and consistency with later entries.',
    type: 'translation',
    safe: true,
    version: '5.0',
    infoUrl: 'https://www.romhacking.net/translations/3972/',
    instructions: [
      'Apply the XDELTA patch to the USA MHFU ISO.',
      'For multiplayer hunts, all players should use the same patch version.',
    ],
    tags: ['translation', 'recommended'],
  },
];

// ---------------------------------------------------------------------------
// Store class
// ---------------------------------------------------------------------------

export class PatchStore {
  private readonly patches: GamePatch[] = [...SEED_PATCHES];

  /** All patches, including unsafe ones (for admin use). */
  getAll(): GamePatch[] {
    return this.patches.slice();
  }

  /** Only safe patches (for public REST API). */
  getSafe(): GamePatch[] {
    return this.patches.filter((p) => p.safe);
  }

  /** Safe patches for a specific game. */
  getByGameId(gameId: string): GamePatch[] {
    return this.patches.filter((p) => p.gameId === gameId && p.safe);
  }

  /** Retrieve a single patch by ID, regardless of safety flag. */
  getById(id: string): GamePatch | undefined {
    return this.patches.find((p) => p.id === id);
  }

  /** Add or update a patch record. Returns the stored record. */
  upsert(patch: GamePatch): GamePatch {
    const idx = this.patches.findIndex((p) => p.id === patch.id);
    if (idx >= 0) {
      this.patches[idx] = patch;
    } else {
      this.patches.push(patch);
    }
    return patch;
  }

  /** Total patch count (all, including unsafe). */
  count(): number {
    return this.patches.length;
  }
}
