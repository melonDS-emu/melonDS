/**
 * Tests for the TournamentStore.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { TournamentStore } from '../tournament-store';

// ─── helpers ─────────────────────────────────────────────────────────────────

const GAME = { gameId: 'mk64', gameTitle: 'Mario Kart 64', system: 'N64' };

function makeTournament(store: TournamentStore, players: string[], name = 'Test Cup') {
  return store.create({ name, ...GAME, players });
}

// ─── tests ───────────────────────────────────────────────────────────────────

describe('TournamentStore', () => {
  let store: TournamentStore;

  beforeEach(() => {
    store = new TournamentStore();
  });

  // ── Creation ─────────────────────────────────────────────────────────────

  it('creates a 2-player tournament with 1 match', () => {
    const t = makeTournament(store, ['Alice', 'Bob']);
    expect(t.players).toEqual(['Alice', 'Bob']);
    expect(t.matches).toHaveLength(1);
    expect(t.matches[0].playerA).toBe('Alice');
    expect(t.matches[0].playerB).toBe('Bob');
    expect(t.status).toBe('active');
    expect(t.winner).toBeNull();
  });

  it('creates a 4-player tournament with 3 matches across 2 rounds', () => {
    const t = makeTournament(store, ['A', 'B', 'C', 'D']);
    expect(t.matches).toHaveLength(3);
    const round1 = t.matches.filter((m) => m.round === 1);
    const round2 = t.matches.filter((m) => m.round === 2);
    expect(round1).toHaveLength(2);
    expect(round2).toHaveLength(1);
  });

  it('pads odd player counts to next power of 2 with BYE', () => {
    const t = makeTournament(store, ['A', 'B', 'C']); // → padded to 4
    const players = t.matches.flatMap((m) => [m.playerA, m.playerB]);
    expect(players).toContain('BYE');
  });

  it('auto-advances BYE matches on creation', () => {
    const t = makeTournament(store, ['A', 'B', 'C']); // 3 players → 4-slot bracket
    // One round-1 match has a BYE and should be auto-completed
    const byeMatch = t.matches.find(
      (m) => m.round === 1 && (m.playerA === 'BYE' || m.playerB === 'BYE')
    );
    expect(byeMatch?.status).toBe('completed');
    expect(byeMatch?.winner).not.toBeNull();
  });

  it('throws when fewer than 2 players', () => {
    expect(() => makeTournament(store, ['Solo'])).toThrow('at least 2');
  });

  it('throws when more than 16 players', () => {
    const players = Array.from({ length: 17 }, (_, i) => `P${i}`);
    expect(() => makeTournament(store, players)).toThrow('Maximum 16');
  });

  // ── Retrieval ─────────────────────────────────────────────────────────────

  it('retrieves a tournament by id', () => {
    const t = makeTournament(store, ['A', 'B']);
    expect(store.get(t.id)).toEqual(t);
  });

  it('returns undefined for unknown id', () => {
    expect(store.get('nonexistent')).toBeUndefined();
  });

  it('lists tournaments and includes all created entries', () => {
    const t1 = makeTournament(store, ['A', 'B'], 'Cup 1');
    const t2 = makeTournament(store, ['C', 'D'], 'Cup 2');
    const list = store.getAll();
    expect(list).toHaveLength(2);
    const ids = list.map((t) => t.id);
    expect(ids).toContain(t1.id);
    expect(ids).toContain(t2.id);
  });

  // ── Result recording ──────────────────────────────────────────────────────

  it('records a match result and marks match completed', () => {
    const t = makeTournament(store, ['Alice', 'Bob']);
    const match = t.matches[0];
    const updated = store.recordResult(t.id, match.id, 'Alice');
    const updatedMatch = updated.matches.find((m) => m.id === match.id)!;
    expect(updatedMatch.winner).toBe('Alice');
    expect(updatedMatch.status).toBe('completed');
  });

  it('completes the tournament after the final match', () => {
    const t = makeTournament(store, ['Alice', 'Bob']);
    const final = t.matches[0];
    const updated = store.recordResult(t.id, final.id, 'Alice');
    expect(updated.status).toBe('completed');
    expect(updated.winner).toBe('Alice');
  });

  it('correctly advances winner through a 4-player bracket', () => {
    const t = makeTournament(store, ['A', 'B', 'C', 'D']);
    const [sf1, sf2] = t.matches.filter((m) => m.round === 1).sort((a, b) => a.slot - b.slot);
    let updated = store.recordResult(t.id, sf1.id, sf1.playerA!);
    updated = store.recordResult(updated.id, sf2.id, sf2.playerA!);
    // Final should now have the two semi-final winners
    const final = updated.matches.find((m) => m.round === 2)!;
    expect(final.playerA).toBe(sf1.playerA);
    expect(final.playerB).toBe(sf2.playerA);
    // Record final
    const completed = store.recordResult(updated.id, final.id, sf1.playerA!);
    expect(completed.status).toBe('completed');
    expect(completed.winner).toBe(sf1.playerA);
  });

  it('throws when recording result for a non-existent tournament', () => {
    expect(() => store.recordResult('bad-id', 'bad-match', 'Alice')).toThrow('not found');
  });

  it('throws when recording result for a non-existent match', () => {
    const t = makeTournament(store, ['A', 'B']);
    expect(() => store.recordResult(t.id, 'bad-match-id', 'A')).toThrow('not found');
  });

  it('throws when the winner is not a match participant', () => {
    const t = makeTournament(store, ['Alice', 'Bob']);
    expect(() => store.recordResult(t.id, t.matches[0].id, 'Carlos')).toThrow('not a participant');
  });

  it('throws when trying to record result on an already completed match', () => {
    const t = makeTournament(store, ['Alice', 'Bob']);
    const match = t.matches[0];
    store.recordResult(t.id, match.id, 'Alice');
    expect(() => store.recordResult(t.id, match.id, 'Bob')).toThrow('already completed');
  });

  it('throws when tournament is already completed', () => {
    const t = makeTournament(store, ['Alice', 'Bob']);
    store.recordResult(t.id, t.matches[0].id, 'Alice');
    expect(() => store.recordResult(t.id, t.matches[0].id, 'Alice')).toThrow();
  });

  // ── Count ─────────────────────────────────────────────────────────────────

  it('tracks tournament count', () => {
    expect(store.count()).toBe(0);
    makeTournament(store, ['A', 'B']);
    expect(store.count()).toBe(1);
    makeTournament(store, ['C', 'D']);
    expect(store.count()).toBe(2);
  });
});
