/**
 * Ranked play — ELO-based ranking store.
 *
 * Rooms tagged as "ranked" cause the winner/loser ELO scores to update when
 * a session ends.  Casual rooms have no effect on rankings.
 *
 * ELO constants are kept simple (K=32, start=1000) for a retro gaming context.
 * Rankings are global across all games and also tracked per-game.
 *
 * In-memory implementation; suitable for dev/demo. For production attach a
 * SQLite backend by calling `SqliteRankingStore`.
 */

import type { DatabaseType } from './db';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export type RankMode = 'casual' | 'ranked';

export interface PlayerRank {
  playerId: string;
  playerName: string;
  elo: number;
  wins: number;
  losses: number;
  draws: number;
  gamesPlayed: number;
  /** Rank tier derived from ELO */
  tier: RankTier;
  updatedAt: string;
}

export type RankTier = 'Bronze' | 'Silver' | 'Gold' | 'Platinum' | 'Diamond';

export interface GameRank {
  gameId: string;
  gameTitle: string;
  playerId: string;
  playerName: string;
  elo: number;
  wins: number;
  losses: number;
  gamesPlayed: number;
  updatedAt: string;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const ELO_START = 1000;
const ELO_K = 32;

export function calcTier(elo: number): RankTier {
  if (elo >= 1600) return 'Diamond';
  if (elo >= 1400) return 'Platinum';
  if (elo >= 1200) return 'Gold';
  if (elo >= 1100) return 'Silver';
  return 'Bronze';
}

/** Returns new ELO scores for [playerA, playerB]. outcome: 1=A wins, 0=draw, -1=B wins */
export function updateElo(
  eloA: number,
  eloB: number,
  outcome: 1 | 0 | -1,
): [number, number] {
  const expectedA = 1 / (1 + Math.pow(10, (eloB - eloA) / 400));
  const expectedB = 1 - expectedA;
  const scoreA = outcome === 1 ? 1 : outcome === 0 ? 0.5 : 0;
  const scoreB = 1 - scoreA;
  const newA = Math.round(eloA + ELO_K * (scoreA - expectedA));
  const newB = Math.round(eloB + ELO_K * (scoreB - expectedB));
  return [newA, newB];
}

// ---------------------------------------------------------------------------
// In-memory store
// ---------------------------------------------------------------------------

export class RankingStore {
  /** Key: playerId */
  protected global: Map<string, PlayerRank> = new Map();
  /** Key: `${gameId}:${playerId}` */
  protected perGame: Map<string, GameRank> = new Map();

  private ensureGlobal(playerId: string, playerName: string): PlayerRank {
    if (!this.global.has(playerId)) {
      this.global.set(playerId, {
        playerId,
        playerName,
        elo: ELO_START,
        wins: 0,
        losses: 0,
        draws: 0,
        gamesPlayed: 0,
        tier: 'Bronze',
        updatedAt: new Date().toISOString(),
      });
    }
    return this.global.get(playerId)!;
  }

  private ensureGame(
    playerId: string,
    playerName: string,
    gameId: string,
    gameTitle: string,
  ): GameRank {
    const key = `${gameId}:${playerId}`;
    if (!this.perGame.has(key)) {
      this.perGame.set(key, {
        gameId,
        gameTitle,
        playerId,
        playerName,
        elo: ELO_START,
        wins: 0,
        losses: 0,
        gamesPlayed: 0,
        updatedAt: new Date().toISOString(),
      });
    }
    return this.perGame.get(key)!;
  }

  /**
   * Record a ranked match result between two players.
   * @param outcome 1 = playerA wins, -1 = playerB wins, 0 = draw
   */
  recordMatch(
    playerAId: string,
    playerAName: string,
    playerBId: string,
    playerBName: string,
    gameId: string,
    gameTitle: string,
    outcome: 1 | 0 | -1,
  ): void {
    const now = new Date().toISOString();

    // Global rankings
    const rankA = this.ensureGlobal(playerAId, playerAName);
    const rankB = this.ensureGlobal(playerBId, playerBName);
    const [newEloA, newEloB] = updateElo(rankA.elo, rankB.elo, outcome);
    rankA.elo = newEloA;
    rankA.tier = calcTier(newEloA);
    rankA.gamesPlayed += 1;
    rankA.updatedAt = now;
    rankB.elo = newEloB;
    rankB.tier = calcTier(newEloB);
    rankB.gamesPlayed += 1;
    rankB.updatedAt = now;
    if (outcome === 1) { rankA.wins += 1; rankB.losses += 1; }
    else if (outcome === -1) { rankB.wins += 1; rankA.losses += 1; }
    else { rankA.draws += 1; rankB.draws += 1; }

    // Per-game rankings
    const grA = this.ensureGame(playerAId, playerAName, gameId, gameTitle);
    const grB = this.ensureGame(playerBId, playerBName, gameId, gameTitle);
    const [ngA, ngB] = updateElo(grA.elo, grB.elo, outcome);
    grA.elo = ngA; grA.gamesPlayed += 1; grA.updatedAt = now;
    grB.elo = ngB; grB.gamesPlayed += 1; grB.updatedAt = now;
    if (outcome === 1) { grA.wins += 1; grB.losses += 1; }
    else if (outcome === -1) { grB.wins += 1; grA.losses += 1; }
  }

  getGlobalLeaderboard(limit = 20): PlayerRank[] {
    return [...this.global.values()]
      .filter(r => r.gamesPlayed > 0)
      .sort((a, b) => b.elo - a.elo)
      .slice(0, limit);
  }

  getGameLeaderboard(gameId: string, limit = 20): GameRank[] {
    return [...this.perGame.values()]
      .filter(r => r.gameId === gameId && r.gamesPlayed > 0)
      .sort((a, b) => b.elo - a.elo)
      .slice(0, limit);
  }

  getPlayerRank(playerId: string): PlayerRank | null {
    return this.global.get(playerId) ?? null;
  }

  getPlayerGameRank(playerId: string, gameId: string): GameRank | null {
    return this.perGame.get(`${gameId}:${playerId}`) ?? null;
  }
}

// ---------------------------------------------------------------------------
// SQLite-backed implementation
// ---------------------------------------------------------------------------

interface GlobalRankRow {
  player_id: string;
  player_name: string;
  elo: number;
  wins: number;
  losses: number;
  draws: number;
  games_played: number;
  updated_at: string;
}

interface GameRankRow {
  game_id: string;
  game_title: string;
  player_id: string;
  player_name: string;
  elo: number;
  wins: number;
  losses: number;
  games_played: number;
  updated_at: string;
}

function rowToGlobal(row: GlobalRankRow): PlayerRank {
  return {
    playerId: row.player_id,
    playerName: row.player_name,
    elo: row.elo,
    wins: row.wins,
    losses: row.losses,
    draws: row.draws,
    gamesPlayed: row.games_played,
    tier: calcTier(row.elo),
    updatedAt: row.updated_at,
  };
}

function rowToGame(row: GameRankRow): GameRank {
  return {
    gameId: row.game_id,
    gameTitle: row.game_title,
    playerId: row.player_id,
    playerName: row.player_name,
    elo: row.elo,
    wins: row.wins,
    losses: row.losses,
    gamesPlayed: row.games_played,
    updatedAt: row.updated_at,
  };
}

export class SqliteRankingStore extends RankingStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    super();
    this.db = db;
  }

  override recordMatch(
    playerAId: string,
    playerAName: string,
    playerBId: string,
    playerBName: string,
    gameId: string,
    gameTitle: string,
    outcome: 1 | 0 | -1,
  ): void {
    const now = new Date().toISOString();
    const upsertGlobal = (playerId: string, playerName: string) => {
      const row = this.db
        .prepare('SELECT * FROM global_rankings WHERE player_id = ?')
        .get(playerId) as GlobalRankRow | undefined;
      if (!row) {
        this.db
          .prepare(
            `INSERT INTO global_rankings (player_id, player_name, elo, wins, losses, draws, games_played, updated_at)
             VALUES (?, ?, ?, 0, 0, 0, 0, ?)`,
          )
          .run(playerId, playerName, ELO_START, now);
        return this.db
          .prepare('SELECT * FROM global_rankings WHERE player_id = ?')
          .get(playerId) as GlobalRankRow;
      }
      return row;
    };
    const upsertGame = (playerId: string, playerName: string) => {
      const key = { gameId, playerId };
      const row = this.db
        .prepare('SELECT * FROM game_rankings WHERE game_id = ? AND player_id = ?')
        .get(gameId, playerId) as GameRankRow | undefined;
      if (!row) {
        this.db
          .prepare(
            `INSERT INTO game_rankings (game_id, game_title, player_id, player_name, elo, wins, losses, games_played, updated_at)
             VALUES (?, ?, ?, ?, ?, 0, 0, 0, ?)`,
          )
          .run(gameId, gameTitle, playerId, playerName, ELO_START, now);
        return this.db
          .prepare('SELECT * FROM game_rankings WHERE game_id = ? AND player_id = ?')
          .get(gameId, playerId) as GameRankRow;
      }
      return row;
    };
    void key; // suppress unused warning
    const gA = upsertGlobal(playerAId, playerAName);
    const gB = upsertGlobal(playerBId, playerBName);
    const [newGA, newGB] = updateElo(gA.elo, gB.elo, outcome);
    const [wA, lA, dA] = outcome === 1 ? [1, 0, 0] : outcome === -1 ? [0, 1, 0] : [0, 0, 1];
    const [wB, lB, dB] = outcome === 1 ? [0, 1, 0] : outcome === -1 ? [1, 0, 0] : [0, 0, 1];
    this.db
      .prepare(
        `UPDATE global_rankings SET elo=?, wins=wins+?, losses=losses+?, draws=draws+?, games_played=games_played+1, updated_at=? WHERE player_id=?`,
      )
      .run(newGA, wA, lA, dA, now, playerAId);
    this.db
      .prepare(
        `UPDATE global_rankings SET elo=?, wins=wins+?, losses=losses+?, draws=draws+?, games_played=games_played+1, updated_at=? WHERE player_id=?`,
      )
      .run(newGB, wB, lB, dB, now, playerBId);

    const pgA = upsertGame(playerAId, playerAName);
    const pgB = upsertGame(playerBId, playerBName);
    const [newPA, newPB] = updateElo(pgA.elo, pgB.elo, outcome);
    this.db
      .prepare(
        `UPDATE game_rankings SET elo=?, wins=wins+?, losses=losses+?, games_played=games_played+1, updated_at=? WHERE game_id=? AND player_id=?`,
      )
      .run(newPA, wA, lA, now, gameId, playerAId);
    this.db
      .prepare(
        `UPDATE game_rankings SET elo=?, wins=wins+?, losses=losses+?, games_played=games_played+1, updated_at=? WHERE game_id=? AND player_id=?`,
      )
      .run(newPB, wB, lB, now, gameId, playerBId);
  }

  override getGlobalLeaderboard(limit = 20): PlayerRank[] {
    const rows = this.db
      .prepare(
        'SELECT * FROM global_rankings WHERE games_played > 0 ORDER BY elo DESC LIMIT ?',
      )
      .all(limit) as GlobalRankRow[];
    return rows.map(rowToGlobal);
  }

  override getGameLeaderboard(gameId: string, limit = 20): GameRank[] {
    const rows = this.db
      .prepare(
        'SELECT * FROM game_rankings WHERE game_id = ? AND games_played > 0 ORDER BY elo DESC LIMIT ?',
      )
      .all(gameId, limit) as GameRankRow[];
    return rows.map(rowToGame);
  }

  override getPlayerRank(playerId: string): PlayerRank | null {
    const row = this.db
      .prepare('SELECT * FROM global_rankings WHERE player_id = ?')
      .get(playerId) as GlobalRankRow | undefined;
    return row ? rowToGlobal(row) : null;
  }

  override getPlayerGameRank(playerId: string, gameId: string): GameRank | null {
    const row = this.db
      .prepare('SELECT * FROM game_rankings WHERE game_id = ? AND player_id = ?')
      .get(gameId, playerId) as GameRankRow | undefined;
    return row ? rowToGame(row) : null;
  }
}
