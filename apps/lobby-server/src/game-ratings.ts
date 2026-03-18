/**
 * Game ratings and reviews store.
 *
 * Players can submit a 1-5 star rating and optional short review text for
 * any game in the catalog. One rating per player per game (upsert).
 *
 * In-memory by default; pass a SQLite database to enable persistence.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export interface GameReview {
  id: string;
  gameId: string;
  gameTitle: string;
  playerId: string;
  playerName: string;
  rating: number; // 1-5
  text?: string;
  createdAt: string;
  updatedAt: string;
}

export interface GameRatingSummary {
  gameId: string;
  gameTitle: string;
  averageRating: number;
  reviewCount: number;
  distribution: Record<1 | 2 | 3 | 4 | 5, number>;
}

// ---------------------------------------------------------------------------
// In-memory implementation
// ---------------------------------------------------------------------------

export class GameRatingsStore {
  protected reviews: Map<string, GameReview> = new Map();

  /** Submit or update a rating. Returns the upserted review. */
  upsertReview(
    gameId: string,
    gameTitle: string,
    playerId: string,
    playerName: string,
    rating: number,
    text?: string,
  ): GameReview {
    if (rating < 1 || rating > 5) throw new Error('Rating must be between 1 and 5');
    // Find existing review by this player for this game
    const existing = [...this.reviews.values()].find(
      r => r.gameId === gameId && r.playerId === playerId,
    );
    if (existing) {
      const updated: GameReview = {
        ...existing,
        rating,
        text: text ?? existing.text,
        updatedAt: new Date().toISOString(),
      };
      this.reviews.set(existing.id, updated);
      return updated;
    }
    const review: GameReview = {
      id: randomUUID(),
      gameId,
      gameTitle,
      playerId,
      playerName,
      rating,
      text,
      createdAt: new Date().toISOString(),
      updatedAt: new Date().toISOString(),
    };
    this.reviews.set(review.id, review);
    return review;
  }

  getReviewsForGame(gameId: string): GameReview[] {
    return [...this.reviews.values()]
      .filter(r => r.gameId === gameId)
      .sort((a, b) => b.createdAt.localeCompare(a.createdAt));
  }

  getReviewsByPlayer(playerId: string): GameReview[] {
    return [...this.reviews.values()]
      .filter(r => r.playerId === playerId)
      .sort((a, b) => b.updatedAt.localeCompare(a.updatedAt));
  }

  deleteReview(reviewId: string, requestingPlayerId: string): boolean {
    const review = this.reviews.get(reviewId);
    if (!review || review.playerId !== requestingPlayerId) return false;
    this.reviews.delete(reviewId);
    return true;
  }

  getSummaryForGame(gameId: string): GameRatingSummary | null {
    const gameReviews = this.getReviewsForGame(gameId);
    if (gameReviews.length === 0) return null;
    const distribution: Record<number, number> = { 1: 0, 2: 0, 3: 0, 4: 0, 5: 0 };
    let total = 0;
    for (const r of gameReviews) {
      distribution[r.rating] = (distribution[r.rating] ?? 0) + 1;
      total += r.rating;
    }
    return {
      gameId,
      gameTitle: gameReviews[0].gameTitle,
      averageRating: Math.round((total / gameReviews.length) * 10) / 10,
      reviewCount: gameReviews.length,
      distribution: distribution as Record<1 | 2 | 3 | 4 | 5, number>,
    };
  }

  /** Returns summaries for all games that have at least one review, sorted by average rating desc. */
  getTopRatedGames(limit = 10): GameRatingSummary[] {
    const gameIds = [...new Set([...this.reviews.values()].map(r => r.gameId))];
    return gameIds
      .map(gid => this.getSummaryForGame(gid))
      .filter((s): s is GameRatingSummary => s !== null)
      .sort((a, b) => b.averageRating - a.averageRating || b.reviewCount - a.reviewCount)
      .slice(0, limit);
  }
}

// ---------------------------------------------------------------------------
// SQLite-backed implementation
// ---------------------------------------------------------------------------

interface ReviewRow {
  id: string;
  game_id: string;
  game_title: string;
  player_id: string;
  player_name: string;
  rating: number;
  text: string | null;
  created_at: string;
  updated_at: string;
}

function rowToReview(row: ReviewRow): GameReview {
  return {
    id: row.id,
    gameId: row.game_id,
    gameTitle: row.game_title,
    playerId: row.player_id,
    playerName: row.player_name,
    rating: row.rating,
    text: row.text ?? undefined,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
  };
}

export class SqliteGameRatingsStore extends GameRatingsStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    super();
    this.db = db;
  }

  override upsertReview(
    gameId: string,
    gameTitle: string,
    playerId: string,
    playerName: string,
    rating: number,
    text?: string,
  ): GameReview {
    if (rating < 1 || rating > 5) throw new Error('Rating must be between 1 and 5');
    const now = new Date().toISOString();
    const existing = this.db
      .prepare('SELECT * FROM game_reviews WHERE game_id = ? AND player_id = ?')
      .get(gameId, playerId) as ReviewRow | undefined;
    if (existing) {
      this.db
        .prepare(
          'UPDATE game_reviews SET rating = ?, text = ?, updated_at = ? WHERE id = ?',
        )
        .run(rating, text ?? existing.text, now, existing.id);
      return rowToReview({ ...existing, rating, text: text ?? existing.text, updated_at: now });
    }
    const id = randomUUID();
    this.db
      .prepare(
        `INSERT INTO game_reviews (id, game_id, game_title, player_id, player_name, rating, text, created_at, updated_at)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
      )
      .run(id, gameId, gameTitle, playerId, playerName, rating, text ?? null, now, now);
    return {
      id,
      gameId,
      gameTitle,
      playerId,
      playerName,
      rating,
      text,
      createdAt: now,
      updatedAt: now,
    };
  }

  override getReviewsForGame(gameId: string): GameReview[] {
    const rows = this.db
      .prepare('SELECT * FROM game_reviews WHERE game_id = ? ORDER BY created_at DESC')
      .all(gameId) as ReviewRow[];
    return rows.map(rowToReview);
  }

  override getReviewsByPlayer(playerId: string): GameReview[] {
    const rows = this.db
      .prepare('SELECT * FROM game_reviews WHERE player_id = ? ORDER BY updated_at DESC')
      .all(playerId) as ReviewRow[];
    return rows.map(rowToReview);
  }

  override deleteReview(reviewId: string, requestingPlayerId: string): boolean {
    const row = this.db
      .prepare('SELECT player_id FROM game_reviews WHERE id = ?')
      .get(reviewId) as { player_id: string } | undefined;
    if (!row || row.player_id !== requestingPlayerId) return false;
    this.db.prepare('DELETE FROM game_reviews WHERE id = ?').run(reviewId);
    return true;
  }

  override getSummaryForGame(gameId: string): GameRatingSummary | null {
    const rows = this.db
      .prepare('SELECT * FROM game_reviews WHERE game_id = ?')
      .all(gameId) as ReviewRow[];
    if (rows.length === 0) return null;
    const distribution: Record<number, number> = { 1: 0, 2: 0, 3: 0, 4: 0, 5: 0 };
    let total = 0;
    for (const r of rows) {
      distribution[r.rating] = (distribution[r.rating] ?? 0) + 1;
      total += r.rating;
    }
    return {
      gameId,
      gameTitle: rows[0].game_title,
      averageRating: Math.round((total / rows.length) * 10) / 10,
      reviewCount: rows.length,
      distribution: distribution as Record<1 | 2 | 3 | 4 | 5, number>,
    };
  }

  override getTopRatedGames(limit = 10): GameRatingSummary[] {
    const rows = this.db
      .prepare(
        `SELECT game_id, game_title, AVG(rating) as avg_rating, COUNT(*) as cnt
         FROM game_reviews
         GROUP BY game_id
         ORDER BY avg_rating DESC, cnt DESC
         LIMIT ?`,
      )
      .all(limit) as { game_id: string; game_title: string; avg_rating: number; cnt: number }[];
    return rows.map(row => {
      const dist: Record<number, number> = { 1: 0, 2: 0, 3: 0, 4: 0, 5: 0 };
      const ratingRows = this.db
        .prepare('SELECT rating FROM game_reviews WHERE game_id = ?')
        .all(row.game_id) as { rating: number }[];
      for (const r of ratingRows) dist[r.rating] = (dist[r.rating] ?? 0) + 1;
      return {
        gameId: row.game_id,
        gameTitle: row.game_title,
        averageRating: Math.round(row.avg_rating * 10) / 10,
        reviewCount: row.cnt,
        distribution: dist as Record<1 | 2 | 3 | 4 | 5, number>,
      };
    });
  }
}
