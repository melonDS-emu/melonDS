/**
 * Tournament engine — single-elimination bracket creation, match scheduling,
 * and result recording.
 *
 * Supports 2–16 players (padded to the next power of two with BYEs).
 * All state is in-memory; wire a SQLite backend using the same pattern as
 * SqliteSessionHistory if persistence across restarts is needed.
 */

import { randomUUID } from 'crypto';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type TournamentStatus = 'pending' | 'active' | 'completed';
export type MatchStatus = 'pending' | 'in-progress' | 'completed';

export interface TournamentMatch {
  id: string;
  round: number;   // 1 = first round (e.g. quarterfinals), higher = later rounds
  slot: number;    // 0-based position within the round
  playerA: string | null;  // display name, or null for a BYE seed
  playerB: string | null;
  winner: string | null;
  status: MatchStatus;
}

export interface Tournament {
  id: string;
  name: string;
  gameId: string;
  gameTitle: string;
  system: string;
  /** Ordered participant display names supplied at creation time. */
  players: string[];
  matches: TournamentMatch[];
  status: TournamentStatus;
  winner: string | null;
  createdAt: string;
  updatedAt: string;
}

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

export class TournamentStore {
  private tournaments = new Map<string, Tournament>();

  // ── Queries ───────────────────────────────────────────────────────────────

  getAll(): Tournament[] {
    return [...this.tournaments.values()].sort((a, b) =>
      b.createdAt.localeCompare(a.createdAt)
    );
  }

  get(id: string): Tournament | undefined {
    return this.tournaments.get(id);
  }

  // ── Create ────────────────────────────────────────────────────────────────

  /**
   * Create a new single-elimination tournament.
   *
   * Player list is padded to the next power of two.  BYE slots advance
   * automatically when `recordResult` is called with a null opponent.
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
    if (params.players.length > 16) {
      throw new Error('Maximum 16 players per tournament.');
    }

    const id = randomUUID();
    const now = new Date().toISOString();

    // Pad to next power of two
    const size = nextPow2(params.players.length);
    const seeded = [...params.players];
    while (seeded.length < size) seeded.push('BYE');

    const matches = buildBracket(seeded);
    const tournament: Tournament = {
      id,
      name: params.name,
      gameId: params.gameId,
      gameTitle: params.gameTitle,
      system: params.system,
      players: params.players,
      matches,
      status: 'active' as TournamentStatus,
      winner: null,
      createdAt: now,
      updatedAt: now,
    };

    // Auto-advance any first-round BYE matches
    this.autoAdvanceByes(tournament);

    this.tournaments.set(id, tournament);
    return tournament;
  }

  // ── Result recording ──────────────────────────────────────────────────────

  /**
   * Record the result of a match.  Advances the winner to the next round's
   * slot and marks the tournament as completed when the final is decided.
   *
   * Returns the updated tournament or throws if the match/tournament is not found.
   */
  recordResult(tournamentId: string, matchId: string, winner: string): Tournament {
    const tournament = this.tournaments.get(tournamentId);
    if (!tournament) throw new Error(`Tournament ${tournamentId} not found.`);
    if (tournament.status === 'completed') {
      throw new Error('Tournament is already completed.');
    }

    const match = tournament.matches.find((m) => m.id === matchId);
    if (!match) throw new Error(`Match ${matchId} not found.`);
    if (match.status === 'completed') {
      throw new Error('Match is already completed.');
    }
    if (match.playerA !== winner && match.playerB !== winner) {
      throw new Error(`${winner} is not a participant in this match.`);
    }

    match.winner = winner;
    match.status = 'completed';

    // Advance winner to the next round
    const maxRound = Math.max(...tournament.matches.map((m) => m.round));
    if (match.round < maxRound) {
      const nextRound = match.round + 1;
      const nextSlot = Math.floor(match.slot / 2);
      const nextMatch = tournament.matches.find(
        (m) => m.round === nextRound && m.slot === nextSlot
      );
      if (nextMatch) {
        if (match.slot % 2 === 0) {
          nextMatch.playerA = winner;
        } else {
          nextMatch.playerB = winner;
        }
        // Auto-advance if opponent is a BYE
        if (nextMatch.playerA === 'BYE' || nextMatch.playerB === 'BYE') {
          const realPlayer = nextMatch.playerA !== 'BYE' ? nextMatch.playerA : nextMatch.playerB;
          if (realPlayer) {
            nextMatch.winner = realPlayer;
            nextMatch.status = 'completed';
            // Recurse to propagate through further BYE chains
            this.autoAdvanceByes(tournament);
          }
        }
      }
    }

    // Check if tournament is complete (all matches in the final round done)
    const finalRound = tournament.matches.filter((m) => m.round === maxRound);
    if (finalRound.every((m) => m.status === 'completed')) {
      tournament.status = 'completed';
      tournament.winner = finalRound[finalRound.length - 1].winner;
    }

    tournament.updatedAt = new Date().toISOString();
    return tournament;
  }

  // ── Helpers ───────────────────────────────────────────────────────────────

  /** Auto-advance any match where one side is a BYE (no real opponent). */
  private autoAdvanceByes(tournament: Tournament): void {
    let changed = true;
    while (changed) {
      changed = false;
      for (const match of tournament.matches) {
        if (match.status === 'completed') continue;
        const aIsBye = match.playerA === 'BYE';
        const bIsBye = match.playerB === 'BYE';
        if ((aIsBye || bIsBye) && (match.playerA !== null && match.playerB !== null)) {
          const winner = aIsBye ? match.playerB! : match.playerA!;
          match.winner = winner;
          match.status = 'completed';
          // Propagate to next round
          const maxRound = Math.max(...tournament.matches.map((m) => m.round));
          if (match.round < maxRound) {
            const nextSlot = Math.floor(match.slot / 2);
            const next = tournament.matches.find(
              (m) => m.round === match.round + 1 && m.slot === nextSlot
            );
            if (next) {
              if (match.slot % 2 === 0) next.playerA = winner;
              else next.playerB = winner;
            }
          }
          changed = true;
        }
      }
    }
  }

  count(): number {
    return this.tournaments.size;
  }
}

// ---------------------------------------------------------------------------
// Bracket builder
// ---------------------------------------------------------------------------

/**
 * Build a single-elimination bracket for the given seeded list (length must
 * be a power of two; null/BYE entries are allowed).
 *
 * Round numbering: round 1 is the first round played; the final is round
 * log2(size).
 */
function buildBracket(seeds: string[]): TournamentMatch[] {
  const size = seeds.length; // power of two
  const rounds = Math.log2(size);
  const matches: TournamentMatch[] = [];

  // Build round 1 matches
  for (let i = 0; i < size / 2; i++) {
    matches.push({
      id: randomUUID(),
      round: 1,
      slot: i,
      playerA: seeds[i * 2],
      playerB: seeds[i * 2 + 1],
      winner: null,
      status: 'pending',
    });
  }

  // Build placeholder matches for subsequent rounds
  for (let r = 2; r <= rounds; r++) {
    const matchesInRound = size / Math.pow(2, r);
    for (let i = 0; i < matchesInRound; i++) {
      matches.push({
        id: randomUUID(),
        round: r,
        slot: i,
        playerA: null,
        playerB: null,
        winner: null,
        status: 'pending',
      });
    }
  }

  return matches;
}

function nextPow2(n: number): number {
  let p = 1;
  while (p < n) p *= 2;
  return p;
}
