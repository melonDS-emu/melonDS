/**
 * Compatibility badge pipeline — Phase 28
 *
 * Tracks per-game compatibility status on a per-backend basis.
 * Statuses mirror common emulator compatibility tiers:
 *   verified  — plays flawlessly; online tested by the RetroOasis team
 *   playable  — minor issues, online play works reliably
 *   partial   — some features broken; online play possible with workarounds
 *   broken    — crashes or unplayable online
 *   untested  — no test data yet
 */

export type CompatibilityStatus = 'verified' | 'playable' | 'partial' | 'broken' | 'untested';

export interface CompatibilityRecord {
  gameId: string;
  system: string;
  backend: string;
  status: CompatibilityStatus;
  /** Free-form notes (optional): known caveats, required settings, etc. */
  notes?: string;
  testedAt: string; // ISO timestamp
  testedBy: string; // display name or 'system'
}

export interface CompatibilitySummary {
  gameId: string;
  bestStatus: CompatibilityStatus;
  records: CompatibilityRecord[];
}

// ---------------------------------------------------------------------------
// Top-25 multiplayer pre-seed data
// ---------------------------------------------------------------------------

/** Priority order used when picking the "best" status for a summary. */
const STATUS_RANK: Record<CompatibilityStatus, number> = {
  verified: 0,
  playable: 1,
  partial: 2,
  broken: 3,
  untested: 4,
};

const SEED_DATA: Omit<CompatibilityRecord, 'testedAt' | 'testedBy'>[] = [
  // NES
  { gameId: 'nes-super-mario-bros',   system: 'nes', backend: 'fceux',         status: 'verified' },
  { gameId: 'nes-dr-mario',           system: 'nes', backend: 'fceux',         status: 'verified' },
  // SNES
  { gameId: 'snes-super-mario-kart',  system: 'snes', backend: 'snes9x',       status: 'verified' },
  { gameId: 'snes-street-fighter-2',  system: 'snes', backend: 'snes9x',       status: 'verified' },
  { gameId: 'snes-super-bomberman-2', system: 'snes', backend: 'snes9x',       status: 'verified' },
  // GBA
  { gameId: 'gba-mario-kart-super-circuit', system: 'gba', backend: 'mgba',    status: 'verified' },
  { gameId: 'gba-pokemon-ruby',       system: 'gba', backend: 'mgba',          status: 'verified' },
  { gameId: 'gba-pokemon-firered',    system: 'gba', backend: 'mgba',          status: 'verified' },
  { gameId: 'gba-four-swords',        system: 'gba', backend: 'mgba',          status: 'playable' },
  // GB / GBC
  { gameId: 'gb-pokemon-red',         system: 'gb',  backend: 'sameboy',       status: 'verified' },
  { gameId: 'gb-pokemon-blue',        system: 'gb',  backend: 'sameboy',       status: 'verified' },
  { gameId: 'gbc-pokemon-gold',       system: 'gbc', backend: 'sameboy',       status: 'verified' },
  { gameId: 'gbc-zelda-oracle-ages',  system: 'gbc', backend: 'sameboy',       status: 'playable' },
  // N64
  { gameId: 'n64-mario-kart-64',      system: 'n64', backend: 'mupen64plus',   status: 'verified' },
  { gameId: 'n64-mario-party-2',      system: 'n64', backend: 'mupen64plus',   status: 'verified' },
  { gameId: 'n64-super-smash-bros',   system: 'n64', backend: 'mupen64plus',   status: 'playable' },
  { gameId: 'n64-goldeneye-007',      system: 'n64', backend: 'mupen64plus',   status: 'playable' },
  // NDS
  { gameId: 'nds-mario-kart-ds',      system: 'nds', backend: 'melonds',       status: 'verified' },
  { gameId: 'nds-pokemon-diamond',    system: 'nds', backend: 'melonds',       status: 'verified' },
  { gameId: 'nds-pokemon-black',      system: 'nds', backend: 'melonds',       status: 'playable' },
  // GameCube
  { gameId: 'gc-mario-kart-double-dash', system: 'gc', backend: 'dolphin',     status: 'verified' },
  { gameId: 'gc-super-smash-bros-melee', system: 'gc', backend: 'dolphin',     status: 'verified' },
  // Wii
  { gameId: 'wii-mario-kart-wii',     system: 'wii', backend: 'dolphin',       status: 'verified' },
  { gameId: 'wii-super-smash-bros-brawl', system: 'wii', backend: 'dolphin',   status: 'playable' },
  // PSX
  { gameId: 'psx-crash-team-racing',  system: 'psx', backend: 'duckstation',   status: 'playable' },
];

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

export class CompatibilityStore {
  /** Map of `${gameId}:${backend}` -> CompatibilityRecord */
  private readonly records = new Map<string, CompatibilityRecord>();

  constructor() {
    this._seed();
  }

  private _key(gameId: string, backend: string): string {
    return `${gameId}:${backend}`;
  }

  private _seed(): void {
    const now = new Date().toISOString();
    for (const entry of SEED_DATA) {
      const record: CompatibilityRecord = { ...entry, testedAt: now, testedBy: 'system' };
      this.records.set(this._key(record.gameId, record.backend), record);
    }
  }

  /** Upsert a compatibility record. */
  set(record: Omit<CompatibilityRecord, 'testedAt'> & { testedAt?: string }): CompatibilityRecord {
    const full: CompatibilityRecord = {
      ...record,
      testedAt: record.testedAt ?? new Date().toISOString(),
    };
    this.records.set(this._key(full.gameId, full.backend), full);
    return full;
  }

  /** Get compatibility records for a specific game (all backends). */
  getByGameId(gameId: string): CompatibilityRecord[] {
    return [...this.records.values()].filter((r) => r.gameId === gameId);
  }

  /** Get compatibility records for all games in a system. */
  getBySystem(system: string): CompatibilityRecord[] {
    return [...this.records.values()].filter((r) => r.system === system);
  }

  /** Get all compatibility records. */
  getAll(): CompatibilityRecord[] {
    return [...this.records.values()];
  }

  /**
   * Return a summary for a game: the "best" status across all backends plus
   * the full list of per-backend records.
   */
  getSummary(gameId: string): CompatibilitySummary {
    const records = this.getByGameId(gameId);
    const bestStatus = records.reduce<CompatibilityStatus>((best, r) => {
      return STATUS_RANK[r.status] < STATUS_RANK[best] ? r.status : best;
    }, 'untested');
    return { gameId, bestStatus, records };
  }

  /**
   * Return a compatibility summary for every game that has at least one
   * record, sorted by gameId.
   */
  getAllSummaries(): CompatibilitySummary[] {
    const gameIds = [...new Set([...this.records.values()].map((r) => r.gameId))];
    return gameIds.sort().map((id) => this.getSummary(id));
  }
}
