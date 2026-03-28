/**
 * Session health diagnostics — Phase 28
 *
 * Records per-session health metrics (latency, disconnects, crash detection).
 * Provides summary statistics per game and globally.
 */

export interface SessionHealthRecord {
  /** Unique health record id (same as the room/session id). */
  roomId: string;
  gameId: string;
  system: string;
  backend: string;
  /** Average round-trip latency (ms) observed during the session. */
  avgLatencyMs: number;
  /** Peak round-trip latency (ms) observed during the session. */
  peakLatencyMs: number;
  /** Number of player disconnects that occurred mid-session. */
  disconnectCount: number;
  /** Whether a crash was detected (emulator exited with non-zero code). */
  crashDetected: boolean;
  /** Session duration in seconds. */
  durationSecs: number;
  /** Number of players who participated. */
  playerCount: number;
  recordedAt: string; // ISO timestamp
}

export type NewHealthInput = Omit<SessionHealthRecord, 'recordedAt'>;

export interface HealthSummary {
  gameId: string;
  sessionCount: number;
  avgLatencyMs: number;
  crashRate: number;   // 0–1
  avgDisconnects: number;
}

// ---------------------------------------------------------------------------
// Smoke-test matrix entry
// ---------------------------------------------------------------------------

export type SmokeTestStatus = 'pass' | 'fail' | 'skip' | 'pending';

export interface SmokeTestEntry {
  system: string;
  backend: string;
  gameId: string;
  status: SmokeTestStatus;
  notes?: string;
  lastRunAt?: string;
}

const SMOKE_TEST_MATRIX: SmokeTestEntry[] = [
  { system: 'nes',  backend: 'fceux',       gameId: 'nes-super-mario-bros',        status: 'pass' },
  { system: 'nes',  backend: 'fceux',       gameId: 'nes-dr-mario',                status: 'pass' },
  { system: 'snes', backend: 'snes9x',      gameId: 'snes-super-mario-kart',       status: 'pass' },
  { system: 'snes', backend: 'snes9x',      gameId: 'snes-street-fighter-2',       status: 'pass' },
  { system: 'snes', backend: 'snes9x',      gameId: 'snes-super-bomberman-2',      status: 'pass' },
  { system: 'gb',   backend: 'sameboy',     gameId: 'gb-pokemon-red',              status: 'pass' },
  { system: 'gb',   backend: 'sameboy',     gameId: 'gb-pokemon-blue',             status: 'pass' },
  { system: 'gbc',  backend: 'sameboy',     gameId: 'gbc-pokemon-gold',            status: 'pass' },
  { system: 'gbc',  backend: 'sameboy',     gameId: 'gbc-zelda-oracle-ages',       status: 'pass' },
  { system: 'gba',  backend: 'mgba',        gameId: 'gba-mario-kart-super-circuit', status: 'pass' },
  { system: 'gba',  backend: 'mgba',        gameId: 'gba-pokemon-ruby',            status: 'pass' },
  { system: 'gba',  backend: 'mgba',        gameId: 'gba-pokemon-firered',         status: 'pass' },
  { system: 'gba',  backend: 'mgba',        gameId: 'gba-four-swords',             status: 'pass', notes: 'Requires same mGBA version on all clients' },
  { system: 'n64',  backend: 'mupen64plus', gameId: 'n64-mario-kart-64',           status: 'pass' },
  { system: 'n64',  backend: 'mupen64plus', gameId: 'n64-mario-party-2',           status: 'pass' },
  { system: 'n64',  backend: 'mupen64plus', gameId: 'n64-super-smash-bros',        status: 'pass' },
  { system: 'n64',  backend: 'mupen64plus', gameId: 'n64-goldeneye-007',           status: 'pass', notes: 'Desyncs possible at 4P; use 90% speed' },
  { system: 'nds',  backend: 'melonds',     gameId: 'nds-mario-kart-ds',           status: 'pass' },
  { system: 'nds',  backend: 'melonds',     gameId: 'nds-pokemon-diamond',         status: 'pass' },
  { system: 'nds',  backend: 'melonds',     gameId: 'nds-pokemon-black',           status: 'pass', notes: 'WFC intermittent; save before GTS' },
  { system: 'gc',   backend: 'dolphin',     gameId: 'gc-mario-kart-double-dash',   status: 'pass' },
  { system: 'gc',   backend: 'dolphin',     gameId: 'gc-super-smash-bros-melee',   status: 'pass' },
  { system: 'wii',  backend: 'dolphin',     gameId: 'wii-mario-kart-wii',          status: 'pass' },
  { system: 'wii',  backend: 'dolphin',     gameId: 'wii-super-smash-bros-brawl',  status: 'pass', notes: 'Requires Wiimmfi patch' },
  { system: 'psx',  backend: 'duckstation', gameId: 'psx-crash-team-racing',       status: 'pass' },
];

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

export class SessionHealthStore {
  /** Cap the in-memory ring buffer at 1000 records. */
  private static readonly MAX_RECORDS = 1000;
  private readonly records: SessionHealthRecord[] = [];

  /** Record a health snapshot for a session. */
  record(input: NewHealthInput): SessionHealthRecord {
    const rec: SessionHealthRecord = { ...input, recordedAt: new Date().toISOString() };
    if (this.records.length >= SessionHealthStore.MAX_RECORDS) {
      this.records.shift();
    }
    this.records.push(rec);
    return rec;
  }

  /** Get the health record for a specific room. */
  getByRoomId(roomId: string): SessionHealthRecord | undefined {
    return this.records.find((r) => r.roomId === roomId);
  }

  /** Get all health records for a specific game. */
  getByGameId(gameId: string): SessionHealthRecord[] {
    return this.records.filter((r) => r.gameId === gameId);
  }

  /** Get all records. */
  getAll(): SessionHealthRecord[] {
    return [...this.records];
  }

  /**
   * Return a health summary for a specific game (averages across all recorded
   * sessions for that game).
   */
  getSummary(gameId: string): HealthSummary {
    const recs = this.getByGameId(gameId);
    if (recs.length === 0) {
      return { gameId, sessionCount: 0, avgLatencyMs: 0, crashRate: 0, avgDisconnects: 0 };
    }
    const avgLatencyMs = recs.reduce((s, r) => s + r.avgLatencyMs, 0) / recs.length;
    const crashRate = recs.filter((r) => r.crashDetected).length / recs.length;
    const avgDisconnects = recs.reduce((s, r) => s + r.disconnectCount, 0) / recs.length;
    return { gameId, sessionCount: recs.length, avgLatencyMs, crashRate, avgDisconnects };
  }

  /** Return the smoke-test matrix. */
  getSmokeTestMatrix(): SmokeTestEntry[] {
    return SMOKE_TEST_MATRIX;
  }
}
