/**
 * Phase 15 — Community Hub, Game Ratings & Ranked Play
 *
 * Tests for: GameRatingsStore, ActivityFeedStore, RankingStore (ELO).
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { GameRatingsStore } from '../game-ratings';
import { ActivityFeedStore } from '../activity-feed';
import { RankingStore, updateElo, calcTier } from '../ranking-store';

// ---------------------------------------------------------------------------
// GameRatingsStore
// ---------------------------------------------------------------------------

describe('GameRatingsStore', () => {
  let store: GameRatingsStore;

  beforeEach(() => {
    store = new GameRatingsStore();
  });

  it('submits a new review', () => {
    const r = store.upsertReview('game-1', 'Pokémon Red', 'player-1', 'Alice', 5, 'Classic!');
    expect(r.rating).toBe(5);
    expect(r.gameId).toBe('game-1');
    expect(r.playerName).toBe('Alice');
    expect(r.text).toBe('Classic!');
  });

  it('upserts an existing review for the same player+game', () => {
    store.upsertReview('game-1', 'Pokémon Red', 'player-1', 'Alice', 4);
    const updated = store.upsertReview('game-1', 'Pokémon Red', 'player-1', 'Alice', 2, 'Changed mind');
    expect(updated.rating).toBe(2);
    expect(updated.text).toBe('Changed mind');
    expect(store.getReviewsForGame('game-1')).toHaveLength(1);
  });

  it('throws when rating is out of range', () => {
    expect(() => store.upsertReview('g', 'G', 'p', 'P', 0)).toThrow();
    expect(() => store.upsertReview('g', 'G', 'p', 'P', 6)).toThrow();
  });

  it('retrieves reviews for a game', () => {
    store.upsertReview('game-1', 'Title', 'p1', 'Alice', 5);
    store.upsertReview('game-1', 'Title', 'p2', 'Bob', 3);
    store.upsertReview('game-2', 'Other', 'p1', 'Alice', 4);
    expect(store.getReviewsForGame('game-1')).toHaveLength(2);
    expect(store.getReviewsForGame('game-2')).toHaveLength(1);
  });

  it('retrieves reviews by player', () => {
    store.upsertReview('game-1', 'G1', 'p1', 'Alice', 5);
    store.upsertReview('game-2', 'G2', 'p1', 'Alice', 3);
    store.upsertReview('game-1', 'G1', 'p2', 'Bob', 4);
    expect(store.getReviewsByPlayer('p1')).toHaveLength(2);
    expect(store.getReviewsByPlayer('p2')).toHaveLength(1);
  });

  it('deletes a review by owner', () => {
    const r = store.upsertReview('game-1', 'G', 'p1', 'Alice', 5);
    expect(store.deleteReview(r.id, 'p1')).toBe(true);
    expect(store.getReviewsForGame('game-1')).toHaveLength(0);
  });

  it('refuses to delete a review by non-owner', () => {
    const r = store.upsertReview('game-1', 'G', 'p1', 'Alice', 5);
    expect(store.deleteReview(r.id, 'p2')).toBe(false);
    expect(store.getReviewsForGame('game-1')).toHaveLength(1);
  });

  it('computes summary with correct average and distribution', () => {
    store.upsertReview('g', 'G', 'p1', 'A', 4);
    store.upsertReview('g', 'G', 'p2', 'B', 2);
    const summary = store.getSummaryForGame('g');
    expect(summary).not.toBeNull();
    expect(summary!.averageRating).toBe(3.0);
    expect(summary!.reviewCount).toBe(2);
    expect(summary!.distribution[4]).toBe(1);
    expect(summary!.distribution[2]).toBe(1);
  });

  it('returns null summary for game with no reviews', () => {
    expect(store.getSummaryForGame('nonexistent')).toBeNull();
  });

  it('returns top-rated games sorted by average rating', () => {
    store.upsertReview('g1', 'G1', 'p1', 'A', 5);
    store.upsertReview('g2', 'G2', 'p1', 'A', 3);
    store.upsertReview('g3', 'G3', 'p1', 'A', 4);
    const top = store.getTopRatedGames(3);
    expect(top[0].gameId).toBe('g1');
    expect(top[1].gameId).toBe('g3');
    expect(top[2].gameId).toBe('g2');
  });
});

// ---------------------------------------------------------------------------
// ActivityFeedStore
// ---------------------------------------------------------------------------

describe('ActivityFeedStore', () => {
  let feed: ActivityFeedStore;

  beforeEach(() => {
    feed = new ActivityFeedStore();
  });

  it('records an event', () => {
    const ev = feed.record('session-started', 'Alice', 'Alice started Pokémon Red', 'Pokémon Red');
    expect(ev.type).toBe('session-started');
    expect(ev.playerName).toBe('Alice');
    expect(ev.subject).toBe('Pokémon Red');
    expect(ev.id).toBeTruthy();
  });

  it('retrieves recent events in reverse-chronological order', () => {
    feed.record('session-started', 'Alice', 'a1');
    feed.record('achievement-unlocked', 'Bob', 'a2');
    const events = feed.getRecent(10);
    expect(events).toHaveLength(2);
    // Most recent first
    expect(events[0].playerName).toBe('Bob');
    expect(events[1].playerName).toBe('Alice');
  });

  it('filters by event type', () => {
    feed.record('session-started', 'Alice', 'a1');
    feed.record('achievement-unlocked', 'Bob', 'a2');
    feed.record('review-submitted', 'Carol', 'a3');
    const achievements = feed.getRecent(10, { type: 'achievement-unlocked' });
    expect(achievements).toHaveLength(1);
    expect(achievements[0].playerName).toBe('Bob');
  });

  it('filters by player name (case-insensitive)', () => {
    feed.record('session-started', 'Alice', 'a1');
    feed.record('session-ended', 'alice', 'a2');
    feed.record('session-started', 'Bob', 'a3');
    const aliceEvents = feed.getRecent(10, { playerName: 'ALICE' });
    expect(aliceEvents).toHaveLength(2);
  });

  it('respects limit', () => {
    for (let i = 0; i < 10; i++) feed.record('session-started', `P${i}`, `ev${i}`);
    expect(feed.getRecent(3)).toHaveLength(3);
  });

  it('tracks total count', () => {
    feed.record('session-started', 'A', 'x');
    feed.record('session-ended', 'B', 'y');
    expect(feed.count()).toBe(2);
  });
});

// ---------------------------------------------------------------------------
// RankingStore — ELO helpers
// ---------------------------------------------------------------------------

describe('ELO helpers', () => {
  it('calcTier returns correct tier for ELO values', () => {
    expect(calcTier(900)).toBe('Bronze');
    expect(calcTier(1100)).toBe('Silver');
    expect(calcTier(1200)).toBe('Gold');
    expect(calcTier(1400)).toBe('Platinum');
    expect(calcTier(1600)).toBe('Diamond');
  });

  it('updateElo increases winner and decreases loser', () => {
    const [newA, newB] = updateElo(1000, 1000, 1);
    expect(newA).toBeGreaterThan(1000);
    expect(newB).toBeLessThan(1000);
  });

  it('updateElo is symmetric on draws', () => {
    const [newA, newB] = updateElo(1000, 1000, 0);
    expect(newA).toBe(1000);
    expect(newB).toBe(1000);
  });

  it('updateElo gives smaller gain for beating lower-rated opponent', () => {
    const [winVsWeak] = updateElo(1400, 1000, 1);
    const [winVsStrong] = updateElo(1000, 1400, 1);
    expect(winVsStrong - 1000).toBeGreaterThan(winVsWeak - 1400);
  });
});

// ---------------------------------------------------------------------------
// RankingStore
// ---------------------------------------------------------------------------

describe('RankingStore', () => {
  let store: RankingStore;

  beforeEach(() => {
    store = new RankingStore();
  });

  it('records a match and updates both players', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game-1', 'Mario Kart 64', 1);
    const alice = store.getPlayerRank('p1');
    const bob = store.getPlayerRank('p2');
    expect(alice).not.toBeNull();
    expect(bob).not.toBeNull();
    expect(alice!.wins).toBe(1);
    expect(bob!.losses).toBe(1);
    expect(alice!.elo).toBeGreaterThan(1000);
    expect(bob!.elo).toBeLessThan(1000);
  });

  it('returns null for player with no ranked games', () => {
    expect(store.getPlayerRank('nobody')).toBeNull();
  });

  it('global leaderboard is sorted by ELO descending', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'g', 'G', 1); // Alice wins
    store.recordMatch('p1', 'Alice', 'p3', 'Carol', 'g', 'G', 1); // Alice wins again
    const lb = store.getGlobalLeaderboard();
    expect(lb[0].playerName).toBe('Alice');
  });

  it('tracks per-game rankings separately', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game-1', 'MK64', 1);
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game-2', 'Smash', -1);
    const aliceGame1 = store.getPlayerGameRank('p1', 'game-1');
    const aliceGame2 = store.getPlayerGameRank('p1', 'game-2');
    expect(aliceGame1).not.toBeNull();
    expect(aliceGame2).not.toBeNull();
    expect(aliceGame1!.wins).toBe(1);
    expect(aliceGame2!.losses).toBe(1);
  });

  it('game leaderboard only shows players who played that game', () => {
    store.recordMatch('p1', 'Alice', 'p2', 'Bob', 'game-1', 'MK64', 1);
    store.recordMatch('p3', 'Carol', 'p4', 'Dave', 'game-2', 'Smash', 1);
    const lb = store.getGameLeaderboard('game-1');
    const names = lb.map(r => r.playerName);
    expect(names).toContain('Alice');
    expect(names).toContain('Bob');
    expect(names).not.toContain('Carol');
    expect(names).not.toContain('Dave');
  });

  it('draw increments draws for both players', () => {
    store.recordMatch('p1', 'A', 'p2', 'B', 'g', 'G', 0);
    expect(store.getPlayerRank('p1')!.draws).toBe(1);
    expect(store.getPlayerRank('p2')!.draws).toBe(1);
  });
});
