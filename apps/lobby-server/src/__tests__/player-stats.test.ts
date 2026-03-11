/**
 * Tests for the player-stats aggregation functions.
 */
import { describe, it, expect } from 'vitest';
import { computePlayerStats, computeLeaderboard } from '../player-stats';
import type { SessionRecord } from '../session-history';

function makeSession(overrides: Partial<SessionRecord> = {}): SessionRecord {
  const now = new Date().toISOString();
  return {
    id: 'test-id',
    roomId: 'room-1',
    gameId: 'test-game',
    gameTitle: 'Test Game',
    system: 'NES',
    startedAt: now,
    endedAt: now,
    durationSecs: 600,
    players: ['Alice', 'Bob'],
    playerCount: 2,
    ...overrides,
  };
}

describe('computePlayerStats', () => {
  it('returns zero stats for an empty session list', () => {
    const stats = computePlayerStats('p1', 'Alice', []);
    expect(stats.totalSessions).toBe(0);
    expect(stats.completedSessions).toBe(0);
    expect(stats.totalPlaytimeSecs).toBe(0);
    expect(stats.avgSessionSecs).toBe(0);
    expect(stats.longestSessionSecs).toBe(0);
    expect(stats.uniquePlaymates).toBe(0);
    expect(stats.bySystem).toEqual([]);
    expect(stats.topGames).toEqual([]);
    expect(stats.activeDays).toBe(0);
    expect(stats.firstSessionAt).toBeNull();
    expect(stats.lastSessionAt).toBeNull();
  });

  it('counts total and completed sessions', () => {
    const sessions = [
      makeSession({ endedAt: null, durationSecs: null }),
      makeSession({ durationSecs: 300 }),
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.totalSessions).toBe(2);
    expect(stats.completedSessions).toBe(1);
  });

  it('sums playtime from completed sessions only', () => {
    const sessions = [
      makeSession({ durationSecs: 600 }),
      makeSession({ durationSecs: 400 }),
      makeSession({ endedAt: null, durationSecs: null }),
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.totalPlaytimeSecs).toBe(1000);
    expect(stats.avgSessionSecs).toBe(500);
  });

  it('finds longest session', () => {
    const sessions = [
      makeSession({ durationSecs: 200 }),
      makeSession({ durationSecs: 3000 }),
      makeSession({ durationSecs: 100 }),
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.longestSessionSecs).toBe(3000);
  });

  it('counts unique playmates (excluding self)', () => {
    const sessions = [
      makeSession({ players: ['Alice', 'Bob', 'Carol'] }),
      makeSession({ players: ['Alice', 'Dave'] }),
      makeSession({ players: ['Alice', 'Bob'] }), // Bob already counted
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.uniquePlaymates).toBe(3); // Bob, Carol, Dave
  });

  it('groups sessions by system', () => {
    const sessions = [
      makeSession({ system: 'NES' }),
      makeSession({ system: 'NES' }),
      makeSession({ system: 'N64' }),
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.bySystem[0].system).toBe('NES');
    expect(stats.bySystem[0].sessions).toBe(2);
    expect(stats.bySystem[1].system).toBe('N64');
    expect(stats.bySystem[1].sessions).toBe(1);
  });

  it('returns top-5 games sorted by session count', () => {
    const sessions = [
      makeSession({ gameId: 'game-a', gameTitle: 'Game A' }),
      makeSession({ gameId: 'game-a', gameTitle: 'Game A' }),
      makeSession({ gameId: 'game-b', gameTitle: 'Game B' }),
      makeSession({ gameId: 'game-c', gameTitle: 'Game C' }),
      makeSession({ gameId: 'game-c', gameTitle: 'Game C' }),
      makeSession({ gameId: 'game-c', gameTitle: 'Game C' }),
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.topGames[0].gameId).toBe('game-c');
    expect(stats.topGames[0].sessions).toBe(3);
    expect(stats.topGames[1].gameId).toBe('game-a');
    expect(stats.uniqueGamesCount).toBe(3); // game-a, game-b, game-c
  });

  it('uniqueGamesCount reflects all distinct games, not just top 5', () => {
    const sessions = Array.from({ length: 8 }, (_, i) =>
      makeSession({ gameId: `game-${i}`, gameTitle: `Game ${i}` })
    );
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.topGames.length).toBe(5); // capped at 5
    expect(stats.uniqueGamesCount).toBe(8); // all 8 distinct games
  });

  it('counts active days', () => {
    const sessions = [
      makeSession({ startedAt: '2025-01-01T10:00:00Z' }),
      makeSession({ startedAt: '2025-01-01T14:00:00Z' }), // same day — should not double-count
      makeSession({ startedAt: '2025-01-03T10:00:00Z' }),
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.activeDays).toBe(2);
  });

  it('records first and last session timestamps', () => {
    const sessions = [
      makeSession({ startedAt: '2025-03-01T10:00:00Z' }),
      makeSession({ startedAt: '2025-01-01T10:00:00Z' }),
      makeSession({ startedAt: '2025-06-01T10:00:00Z' }),
    ];
    const stats = computePlayerStats('p1', 'Alice', sessions);
    expect(stats.firstSessionAt).toBe('2025-01-01T10:00:00Z');
    expect(stats.lastSessionAt).toBe('2025-06-01T10:00:00Z');
  });
});

describe('computeLeaderboard', () => {
  const statsA = computePlayerStats('p1', 'Alice', [
    makeSession(), makeSession(), makeSession(),
  ]);
  const statsB = computePlayerStats('p2', 'Bob', [
    makeSession(), makeSession(),
  ]);
  const statsC = computePlayerStats('p3', 'Carol', [makeSession()]);

  it('returns players ranked by sessions (default)', () => {
    const lb = computeLeaderboard([statsB, statsC, statsA]);
    expect(lb[0].displayName).toBe('Alice');
    expect(lb[0].rank).toBe(1);
    expect(lb[1].displayName).toBe('Bob');
    expect(lb[2].displayName).toBe('Carol');
  });

  it('ranks by playtime when metric=playtime', () => {
    const s1 = computePlayerStats('p1', 'Alice', [makeSession({ durationSecs: 100 })]);
    const s2 = computePlayerStats('p2', 'Bob', [makeSession({ durationSecs: 9000 })]);
    const lb = computeLeaderboard([s1, s2], 'playtime');
    expect(lb[0].displayName).toBe('Bob');
  });

  it('ranks by unique games count when metric=games (not capped at top-5)', () => {
    // Alice has 8 unique games, Bob has 3
    const aliceSessions = Array.from({ length: 8 }, (_, i) =>
      makeSession({ gameId: `game-${i}`, gameTitle: `Game ${i}` })
    );
    const bobSessions = Array.from({ length: 3 }, (_, i) =>
      makeSession({ gameId: `game-${i}`, gameTitle: `Game ${i}` })
    );
    const sA = computePlayerStats('p1', 'Alice', aliceSessions);
    const sB = computePlayerStats('p2', 'Bob', bobSessions);
    const lb = computeLeaderboard([sA, sB], 'games');
    expect(lb[0].displayName).toBe('Alice');
    expect(lb[0].value).toBe(8);
    expect(lb[1].value).toBe(3);
  });

  it('respects the limit parameter', () => {
    const all = [statsA, statsB, statsC];
    const lb = computeLeaderboard(all, 'sessions', 2);
    expect(lb.length).toBe(2);
  });

  it('assigns sequential ranks', () => {
    const lb = computeLeaderboard([statsA, statsB, statsC]);
    expect(lb.map((e) => e.rank)).toEqual([1, 2, 3]);
  });
});
