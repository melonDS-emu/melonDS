/**
 * Tests for the AchievementStore.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { AchievementStore, ACHIEVEMENT_DEFS } from '../achievement-store';
import type { SessionRecord } from '../session-history';

// ─── helpers ─────────────────────────────────────────────────────────────────

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

function makeSessions(n: number, overrides: Partial<SessionRecord> = {}): SessionRecord[] {
  return Array.from({ length: n }, (_, i) =>
    makeSession({
      id: `id-${i}`,
      roomId: `room-${i}`,
      startedAt: new Date(Date.now() - i * 3_600_000).toISOString(),
      endedAt: new Date(Date.now() - i * 3_600_000 + 600_000).toISOString(),
      ...overrides,
    })
  );
}

// ─── tests ───────────────────────────────────────────────────────────────────

describe('AchievementStore', () => {
  let store: AchievementStore;

  beforeEach(() => {
    store = new AchievementStore();
  });

  it('returns all definitions', () => {
    expect(store.getDefinitions()).toBe(ACHIEVEMENT_DEFS);
    expect(store.getDefinitions().length).toBeGreaterThan(0);
  });

  it('returns empty earned list for unknown player', () => {
    expect(store.getEarned('unknown')).toEqual([]);
  });

  it('creates initial state for a new player', () => {
    const state = store.getState('p1', 'Alice');
    expect(state.playerId).toBe('p1');
    expect(state.displayName).toBe('Alice');
    expect(state.earned).toEqual([]);
  });

  it('unlocks first-session after one completed session', () => {
    const sessions = [makeSession()];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('first-session');
  });

  it('does not re-unlock already-earned achievements', () => {
    const sessions = makeSessions(1);
    store.checkAndUnlock('p1', 'Alice', sessions);
    const newly2 = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly2.map((a) => a.id)).not.toContain('first-session');
  });

  it('unlocks sessions-5 after 5 completed sessions', () => {
    const sessions = makeSessions(5);
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('sessions-5');
  });

  it('does not unlock sessions-5 with fewer than 5 sessions', () => {
    const sessions = makeSessions(4);
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).not.toContain('sessions-5');
  });

  it('unlocks marathon after 10 hours total playtime', () => {
    // 36 sessions of 1000s each = 36000s = 10h
    const sessions = makeSessions(36, { durationSecs: 1000 });
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('marathon');
  });

  it('unlocks long-session for a 60-min session', () => {
    const sessions = [makeSession({ durationSecs: 3601 })];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('long-session');
  });

  it('does not unlock long-session for a short session', () => {
    const sessions = [makeSession({ durationSecs: 1000 })];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).not.toContain('long-session');
  });

  it('unlocks party-of-four for a 4-player session', () => {
    const sessions = [makeSession({ playerCount: 4, players: ['A', 'B', 'C', 'D'] })];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('party-of-four');
  });

  it('does not unlock party-of-four for a 2-player session', () => {
    const sessions = [makeSession({ playerCount: 2 })];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).not.toContain('party-of-four');
  });

  it('unlocks n64-debut for an N64 session', () => {
    const sessions = [makeSession({ system: 'N64' })];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('n64-debut');
  });

  it('unlocks nds-debut for an NDS session', () => {
    const sessions = [makeSession({ system: 'NDS' })];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('nds-debut');
  });

  it('unlocks mario-kart-fan after 5 Mario Kart sessions', () => {
    const sessions = makeSessions(5, { gameTitle: 'Mario Kart 64' });
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('mario-kart-fan');
  });

  it('unlocks pokemon-trainer after 3 Pokémon sessions', () => {
    const sessions = makeSessions(3, { gameTitle: 'Pokémon Red' });
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('pokemon-trainer');
  });

  it('unlocks retro-purist after playing 4 systems', () => {
    const sessions = [
      makeSession({ system: 'NES' }),
      makeSession({ system: 'SNES' }),
      makeSession({ system: 'GB' }),
      makeSession({ system: 'N64' }),
    ];
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('retro-purist');
  });

  it('unlocks daily-player after sessions on 7 distinct days', () => {
    const sessions = Array.from({ length: 7 }, (_, i) => {
      const d = new Date(Date.now() - i * 86_400_000);
      return makeSession({
        id: `day-${i}`,
        roomId: `r-${i}`,
        startedAt: d.toISOString(),
        endedAt: d.toISOString(),
      });
    });
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).toContain('daily-player');
  });

  it('unlocks comeback-king after a 7+ day gap', () => {
    const early = makeSession({
      id: 'e1',
      roomId: 'r-early',
      startedAt: new Date('2025-01-01T10:00:00Z').toISOString(),
      endedAt: new Date('2025-01-01T10:30:00Z').toISOString(),
    });
    const late = makeSession({
      id: 'l1',
      roomId: 'r-late',
      startedAt: new Date('2025-01-15T10:00:00Z').toISOString(),
      endedAt: new Date('2025-01-15T10:30:00Z').toISOString(),
    });
    const newly = store.checkAndUnlock('p1', 'Alice', [early, late]);
    expect(newly.map((a) => a.id)).toContain('comeback-king');
  });

  it('does not unlock comeback-king with no big gap', () => {
    const sessions = makeSessions(5); // all within a few hours
    const newly = store.checkAndUnlock('p1', 'Alice', sessions);
    expect(newly.map((a) => a.id)).not.toContain('comeback-king');
  });

  it('accumulates earned achievements across calls', () => {
    store.checkAndUnlock('p1', 'Alice', makeSessions(1));
    store.checkAndUnlock('p1', 'Alice', makeSessions(5));
    const earned = store.getEarned('p1');
    const ids = earned.map((e) => e.achievementId);
    expect(ids).toContain('first-session');
    expect(ids).toContain('sessions-5');
  });
});
