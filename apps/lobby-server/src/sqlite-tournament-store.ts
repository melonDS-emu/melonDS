/**
 * SQLite-backed tournament store.
 *
 * Persists tournaments and their match brackets to the database so they
 * survive server restarts.  Implements the same public API as the in-memory
 * `TournamentStore` so it can be used as a drop-in replacement when
 * `DB_PATH` is set.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';
import type { Tournament, TournamentMatch, TournamentStatus, MatchStatus } from './tournament-store';

// ---------------------------------------------------------------------------
// Row shapes
// ---------------------------------------------------------------------------

interface TournamentRow {
  id: string;
  name: string;
  game_id: string;
  game_title: string;
  system: string;
  status: TournamentStatus;
  winner: string | null;
  created_at: string;
  updated_at: string;
}

interface MatchRow {
  id: string;
  tournament_id: string;
  round: number;
  slot: number;
  player_a: string | null;
  player_b: string | null;
  winner: string | null;
  status: MatchStatus;
}

interface PlayerRow {
  display_name: string;
  seed: number;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function toTournament(row: TournamentRow, players: string[], matches: TournamentMatch[]): Tournament {
  return {
    id: row.id,
    name: row.name,
    gameId: row.game_id,
    gameTitle: row.game_title,
    system: row.system,
    players,
    matches,
    status: row.status,
    winner: row.winner,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
  };
}

function toMatch(row: MatchRow): TournamentMatch {
  return {
    id: row.id,
    round: row.round,
    slot: row.slot,
    playerA: row.player_a,
    playerB: row.player_b,
    winner: row.winner,
    status: row.status,
  };
}

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

export class SqliteTournamentStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  // ── Private helpers ────────────────────────────────────────────────────

  private getPlayers(tournamentId: string): string[] {
    return this.db
      .prepare<string, PlayerRow>('SELECT display_name, seed FROM tournament_players WHERE tournament_id = ? ORDER BY seed ASC')
      .all(tournamentId)
      .map((r) => r.display_name);
  }

  private getMatches(tournamentId: string): TournamentMatch[] {
    return this.db
      .prepare<string, MatchRow>('SELECT * FROM tournament_matches WHERE tournament_id = ? ORDER BY round ASC, slot ASC')
      .all(tournamentId)
      .map(toMatch);
  }

  private load(id: string): Tournament | undefined {
    const row = this.db
      .prepare<string, TournamentRow>('SELECT * FROM tournaments WHERE id = ?')
      .get(id);
    if (!row) return undefined;
    return toTournament(row, this.getPlayers(id), this.getMatches(id));
  }

  private updateTimestamp(id: string) {
    this.db
      .prepare('UPDATE tournaments SET updated_at = ? WHERE id = ?')
      .run(new Date().toISOString(), id);
  }

  // ── Public API (mirrors TournamentStore) ─────────────────────────────

  getAll(): Tournament[] {
    const rows = this.db
      .prepare<[], TournamentRow>('SELECT * FROM tournaments ORDER BY created_at DESC')
      .all();
    return rows.map((r) => toTournament(r, this.getPlayers(r.id), this.getMatches(r.id)));
  }

  get(id: string): Tournament | undefined {
    return this.load(id);
  }

  /**
   * Create a new single-elimination tournament.
   *
   * Replicates the bracket-generation logic from `TournamentStore.create()`.
   */
  create(params: {
    name: string;
    gameId: string;
    gameTitle: string;
    system: string;
    players: string[];
  }): Tournament {
    if (params.players.length < 2) {
      throw new Error('A tournament requires at least 2 players.');
    }

    const id = randomUUID();
    const now = new Date().toISOString();

    // Pad players to the next power of two (BYE slots = null)
    let size = 2;
    while (size < params.players.length) size *= 2;
    const seeded: (string | null)[] = [...params.players];
    while (seeded.length < size) seeded.push(null);

    // Build first-round matches
    const matches: TournamentMatch[] = [];
    const rounds = Math.log2(size);
    for (let slot = 0; slot < size / 2; slot++) {
      const matchId = randomUUID();
      matches.push({
        id: matchId,
        round: 1,
        slot,
        playerA: seeded[slot * 2],
        playerB: seeded[slot * 2 + 1],
        winner: null,
        status: 'pending',
      });
    }

    // Build subsequent rounds (empty slots to be filled as results come in)
    for (let r = 2; r <= rounds; r++) {
      const matchCount = size / Math.pow(2, r);
      for (let slot = 0; slot < matchCount; slot++) {
        matches.push({
          id: randomUUID(),
          round: r,
          slot,
          playerA: null,
          playerB: null,
          winner: null,
          status: 'pending',
        });
      }
    }

    // Persist
    this.db.prepare(`
      INSERT INTO tournaments (id, name, game_id, game_title, system, status, winner, created_at, updated_at)
      VALUES (?, ?, ?, ?, ?, 'pending', NULL, ?, ?)
    `).run(id, params.name, params.gameId, params.gameTitle, params.system, now, now);

    const insertPlayer = this.db.prepare(
      'INSERT INTO tournament_players (tournament_id, display_name, seed) VALUES (?, ?, ?)'
    );
    params.players.forEach((name, seed) => insertPlayer.run(id, name, seed));

    const insertMatch = this.db.prepare(`
      INSERT INTO tournament_matches (id, tournament_id, round, slot, player_a, player_b, winner, status)
      VALUES (?, ?, ?, ?, ?, ?, NULL, 'pending')
    `);
    for (const m of matches) {
      insertMatch.run(m.id, id, m.round, m.slot, m.playerA, m.playerB);
    }

    // Auto-advance BYE slots in round 1
    const updated = this.load(id)!;
    for (const m of updated.matches.filter((m) => m.round === 1)) {
      if (m.playerA !== null && m.playerB === null) {
        this._applyResult(id, m.id, m.playerA);
      } else if (m.playerA === null && m.playerB !== null) {
        this._applyResult(id, m.id, m.playerB);
      }
    }

    return this.load(id)!;
  }

  /** Internal — apply a result and advance the bracket. */
  private _applyResult(tournamentId: string, matchId: string, winner: string): Tournament | null {
    const tournament = this.load(tournamentId);
    if (!tournament) return null;

    const match = tournament.matches.find((m) => m.id === matchId);
    if (!match) return null;
    if (match.winner) return tournament; // already decided

    // Save winner
    this.db.prepare(
      "UPDATE tournament_matches SET winner = ?, status = 'completed' WHERE id = ?"
    ).run(winner, matchId);

    // Advance winner to next round
    const nextRound = match.round + 1;
    const nextSlot = Math.floor(match.slot / 2);
    const nextMatch = tournament.matches.find(
      (m) => m.round === nextRound && m.slot === nextSlot
    );
    if (nextMatch) {
      const isSlotA = match.slot % 2 === 0;
      if (isSlotA) {
        this.db.prepare('UPDATE tournament_matches SET player_a = ? WHERE id = ?').run(winner, nextMatch.id);
      } else {
        this.db.prepare('UPDATE tournament_matches SET player_b = ? WHERE id = ?').run(winner, nextMatch.id);
      }
    } else {
      // Final round won — mark tournament complete
      this.db.prepare(
        "UPDATE tournaments SET status = 'completed', winner = ?, updated_at = ? WHERE id = ?"
      ).run(winner, new Date().toISOString(), tournamentId);
      return this.load(tournamentId)!;
    }

    this.updateTimestamp(tournamentId);
    return this.load(tournamentId)!;
  }

  recordResult(tournamentId: string, matchId: string, winner: string): Tournament | null {
    return this._applyResult(tournamentId, matchId, winner);
  }

  /**
   * Return all tournaments in which a given player participated, won, or
   * reached the final.  Used for the Profile page tournament history.
   */
  getPlayerTournaments(displayName: string): Tournament[] {
    const rows = this.db
      .prepare<string, TournamentRow>(`
        SELECT t.* FROM tournaments t
        JOIN tournament_players tp ON tp.tournament_id = t.id
        WHERE tp.display_name = ?
        ORDER BY t.created_at DESC
      `)
      .all(displayName);
    return rows.map((r) => toTournament(r, this.getPlayers(r.id), this.getMatches(r.id)));
  }
}
