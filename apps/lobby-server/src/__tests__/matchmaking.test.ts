/**
 * Tests for the matchmaking queue.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { openDatabase } from '../db';
import { MatchmakingQueue } from '../matchmaking';

describe('MatchmakingQueue', () => {
  let queue: MatchmakingQueue;

  beforeEach(() => {
    const db = openDatabase(':memory:');
    queue = new MatchmakingQueue(db);
  });

  it('joins the queue', () => {
    const entry = queue.join({ playerId: 'p1', displayName: 'Player1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    expect(entry.playerId).toBe('p1');
    expect(queue.isQueued('p1')).toBe(true);
  });

  it('returns existing entry on duplicate join', () => {
    const e1 = queue.join({ playerId: 'p1', displayName: 'Player1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    const e2 = queue.join({ playerId: 'p1', displayName: 'Player1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    expect(e1.joinedAt).toBe(e2.joinedAt);
    expect(queue.getAll().length).toBe(1);
  });

  it('leaves the queue', () => {
    queue.join({ playerId: 'p1', displayName: 'Player1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    expect(queue.leave('p1')).toBe(true);
    expect(queue.isQueued('p1')).toBe(false);
  });

  it('returns false when leaving if not in queue', () => {
    expect(queue.leave('ghost')).toBe(false);
  });

  it('flushes a match when enough players are queued', () => {
    queue.join({ playerId: 'p1', displayName: 'P1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    queue.join({ playerId: 'p2', displayName: 'P2', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    const matches = queue.flushMatches();
    expect(matches.length).toBe(1);
    expect(matches[0].players.length).toBe(2);
    // Both players should be removed from queue
    expect(queue.isQueued('p1')).toBe(false);
    expect(queue.isQueued('p2')).toBe(false);
  });

  it('does not flush a match with insufficient players', () => {
    queue.join({ playerId: 'p1', displayName: 'P1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 4 });
    queue.join({ playerId: 'p2', displayName: 'P2', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 4 });
    const matches = queue.flushMatches();
    expect(matches.length).toBe(0);
  });

  it('does not mix different games', () => {
    queue.join({ playerId: 'p1', displayName: 'P1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    queue.join({ playerId: 'p2', displayName: 'P2', gameId: 'smash', gameTitle: 'Smash', system: 'n64', maxPlayers: 2 });
    const matches = queue.flushMatches();
    expect(matches.length).toBe(0);
  });

  it('creates multiple matches when many players are queued', () => {
    for (let i = 1; i <= 4; i++) {
      queue.join({ playerId: `p${i}`, displayName: `P${i}`, gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    }
    const matches = queue.flushMatches();
    expect(matches.length).toBe(2);
  });

  it('getAll returns current queue entries', () => {
    queue.join({ playerId: 'p1', displayName: 'P1', gameId: 'mk64', gameTitle: 'MK64', system: 'n64', maxPlayers: 2 });
    queue.join({ playerId: 'p2', displayName: 'P2', gameId: 'smash', gameTitle: 'Smash', system: 'n64', maxPlayers: 4 });
    const all = queue.getAll();
    expect(all.length).toBe(2);
  });
});
