/**
 * Tests for the SQLite-backed stores (LobbyManager, SessionHistory, SaveStore).
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { openDatabase } from '../db';
import { SqliteLobbyManager } from '../sqlite-lobby-manager';
import { SqliteSessionHistory } from '../sqlite-session-history';
import { SqliteSaveStore } from '../sqlite-save-store';
import { SqliteAchievementStore } from '../sqlite-achievement-store';

// ---------------------------------------------------------------------------
// SqliteLobbyManager
// ---------------------------------------------------------------------------

describe('SqliteLobbyManager', () => {
  let manager: SqliteLobbyManager;

  beforeEach(() => {
    const db = openDatabase(':memory:');
    manager = new SqliteLobbyManager(db);
  });

  it('creates a room and returns it', () => {
    const room = manager.createRoom('host-1', 'Test Room', 'mk64', 'Mario Kart 64', 'n64', true, 4, 'Host');
    expect(room.name).toBe('Test Room');
    expect(room.hostId).toBe('host-1');
    expect(room.players).toHaveLength(1);
    expect(room.players[0].isHost).toBe(true);
    expect(room.players[0].displayName).toBe('Host');
    expect(room.status).toBe('waiting');
  });

  it('retrieves a room by ID', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 4, 'Host');
    const fetched = manager.getRoom(room.id);
    expect(fetched).not.toBeNull();
    expect(fetched!.id).toBe(room.id);
  });

  it('joins a room by room code', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 4, 'Host');
    const updated = manager.joinByCode(room.roomCode, 'player-2', 'Guest');
    expect(updated).not.toBeNull();
    expect(updated!.players).toHaveLength(2);
  });

  it('refuses to join a full room', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 1, 'Host');
    const result = manager.joinRoom(room.id, 'player-2', 'Guest');
    expect(result).toBeNull();
  });

  it('toggles ready state', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 4, 'Host');
    manager.joinRoom(room.id, 'player-2', 'Guest');
    const updated = manager.toggleReady(room.id, 'player-2');
    const guest = updated!.players.find((p) => p.id === 'player-2');
    expect(guest?.readyState).toBe('ready');
  });

  it('starts a game when all players are ready', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 2, 'Host');
    manager.joinRoom(room.id, 'player-2', 'Guest');
    manager.toggleReady(room.id, 'player-2');
    const started = manager.startGame(room.id, 'host-1');
    expect(started?.status).toBe('in-game');
  });

  it('returns null from startGame when not all are ready', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 2, 'Host');
    manager.joinRoom(room.id, 'player-2', 'Guest');
    const result = manager.startGame(room.id, 'host-1');
    expect(result).toBeNull();
  });

  it('transfers host on host departure', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 4, 'Host');
    manager.joinRoom(room.id, 'player-2', 'Guest');
    const updated = manager.leaveRoom(room.id, 'host-1');
    expect(updated).not.toBeNull();
    expect(updated!.hostId).toBe('player-2');
    expect(updated!.players[0].isHost).toBe(true);
  });

  it('deletes room when last player leaves', () => {
    const room = manager.createRoom('host-1', 'Room', 'mk64', 'MK64', 'n64', true, 4, 'Host');
    const result = manager.leaveRoom(room.id, 'host-1');
    expect(result).toBeNull();
  });

  it('lists public waiting rooms', () => {
    manager.createRoom('h1', 'Public', 'mk64', 'MK64', 'n64', true, 4, 'H1');
    manager.createRoom('h2', 'Private', 'mk64', 'MK64', 'n64', false, 4, 'H2');
    const rooms = manager.listPublicRooms();
    expect(rooms.length).toBe(1);
    expect(rooms[0].name).toBe('Public');
  });

  it('disconnects a player from all their rooms', () => {
    const r1 = manager.createRoom('host-1', 'R1', 'mk64', 'MK64', 'n64', true, 4, 'Host1');
    const r2 = manager.createRoom('host-2', 'R2', 'mk64', 'MK64', 'n64', true, 4, 'Host2');
    manager.joinRoom(r1.id, 'player-3', 'P3');
    manager.joinRoom(r2.id, 'player-3', 'P3');
    const affected = manager.disconnectPlayer('player-3');
    expect(affected.size).toBe(2);
  });
});

// ---------------------------------------------------------------------------
// SqliteSessionHistory
// ---------------------------------------------------------------------------

describe('SqliteSessionHistory', () => {
  let history: SqliteSessionHistory;

  beforeEach(() => {
    const db = openDatabase(':memory:');
    history = new SqliteSessionHistory(db);
  });

  it('starts and retrieves a session', () => {
    const record = history.startSession('room-1', 'mk64', 'Mario Kart 64', 'n64', ['Alice', 'Bob']);
    expect(record.roomId).toBe('room-1');
    expect(record.players).toContain('Alice');
    expect(record.endedAt).toBeNull();
  });

  it('ends a session and records duration', async () => {
    history.startSession('room-2', 'mk64', 'MK64', 'n64', ['Alice']);
    await new Promise((r) => setTimeout(r, 50));
    const ended = history.endSession('room-2');
    expect(ended?.endedAt).not.toBeNull();
    expect(ended?.durationSecs).toBeGreaterThanOrEqual(0);
  });

  it('returns null on double-end', () => {
    history.startSession('room-3', 'mk64', 'MK64', 'n64', []);
    history.endSession('room-3');
    expect(history.endSession('room-3')).toBeNull();
  });

  it('getAll returns sessions ordered newest first', async () => {
    history.startSession('room-a', 'g1', 'G1', 'n64', []);
    await new Promise((r) => setTimeout(r, 5));
    history.startSession('room-b', 'g2', 'G2', 'n64', []);
    const all = history.getAll();
    expect(all[0].roomId).toBe('room-b');
  });

  it('getMostPlayedGames aggregates completed sessions', () => {
    history.startSession('r1', 'mk64', 'MK64', 'n64', ['Alice']);
    history.endSession('r1');
    history.startSession('r2', 'mk64', 'MK64', 'n64', ['Bob']);
    history.endSession('r2');
    history.startSession('r3', 'smash', 'Smash', 'n64', ['Alice']);
    history.endSession('r3');

    const top = history.getMostPlayedGames(5);
    expect(top[0].gameId).toBe('mk64');
    expect(top[0].sessionCount).toBe(2);
  });

  it('getTotalPlayTimeSecs returns sum of durations', () => {
    history.startSession('r1', 'g1', 'G', 'n64', []);
    history.endSession('r1');
    const total = history.getTotalPlayTimeSecs();
    expect(total).toBeGreaterThanOrEqual(0);
  });

  it('getPlayerStats returns per-player info', () => {
    history.startSession('r1', 'mk64', 'MK64', 'n64', ['Alice', 'Bob']);
    history.endSession('r1');
    history.startSession('r2', 'mk64', 'MK64', 'n64', ['Alice']);
    history.endSession('r2');

    const stats = history.getPlayerStats('Alice');
    expect(stats.sessionCount).toBe(2);
    expect(stats.mostPlayedGame).toBe('MK64');
  });
});

// ---------------------------------------------------------------------------
// SqliteSaveStore
// ---------------------------------------------------------------------------

describe('SqliteSaveStore', () => {
  let store: SqliteSaveStore;
  const GAME_ID = 'mk64';
  const DATA = Buffer.from('save-data').toString('base64');

  beforeEach(() => {
    const db = openDatabase(':memory:');
    store = new SqliteSaveStore(db);
  });

  it('creates a save record', () => {
    const result = store.put(GAME_ID, 'Slot 1', DATA);
    expect('conflict' in result).toBe(false);
    const record = result as import('../save-store').SaveRecord;
    expect(record.gameId).toBe(GAME_ID);
    expect(record.version).toBe(1);
  });

  it('overwrites an existing slot and bumps version', () => {
    store.put(GAME_ID, 'Slot 1', DATA);
    const updated = store.put(GAME_ID, 'Slot 1', Buffer.from('v2').toString('base64')) as import('../save-store').SaveRecord;
    expect(updated.version).toBe(2);
  });

  it('detects conflicts on stale expectedVersion', () => {
    store.put(GAME_ID, 'Slot 1', DATA); // v1
    store.put(GAME_ID, 'Slot 1', Buffer.from('v2').toString('base64')); // v2
    const result = store.put(GAME_ID, 'Slot 1', Buffer.from('v3').toString('base64'), 'application/octet-stream', 1);
    expect('conflict' in result && (result as import('../save-store').ConflictError).conflict).toBe(true);
  });

  it('lists saves without data field', () => {
    store.put(GAME_ID, 'Slot 1', DATA);
    store.put(GAME_ID, 'Slot 2', DATA);
    store.put('other', 'Slot 1', DATA);
    const list = store.listForGame(GAME_ID);
    expect(list.length).toBe(2);
    for (const item of list) {
      expect('data' in item).toBe(false);
    }
  });

  it('deletes a save', () => {
    const created = store.put(GAME_ID, 'Slot 1', DATA) as import('../save-store').SaveRecord;
    expect(store.delete(created.id, GAME_ID)).toBe(true);
    expect(store.get(created.id)).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// SqliteAchievementStore
// ---------------------------------------------------------------------------

describe('SqliteAchievementStore', () => {
  let store: SqliteAchievementStore;

  function makeSession(overrides: Partial<import('../session-history').SessionRecord> = {}): import('../session-history').SessionRecord {
    const now = new Date().toISOString();
    return {
      id: 'sess-1',
      roomId: 'room-1',
      gameId: 'mk64',
      gameTitle: 'Mario Kart 64',
      system: 'N64',
      startedAt: now,
      endedAt: now,
      durationSecs: 600,
      players: ['Alice', 'Bob'],
      playerCount: 2,
      ...overrides,
    };
  }

  beforeEach(() => {
    const db = openDatabase(':memory:');
    store = new SqliteAchievementStore(db);
  });

  it('returns all achievement definitions', () => {
    expect(store.getDefinitions().length).toBeGreaterThan(0);
  });

  it('returns empty array for unknown player', () => {
    expect(store.getEarned('unknown')).toEqual([]);
  });

  it('unlocks first-session on first completed session', () => {
    const sessions = [makeSession()];
    const unlocked = store.checkAndUnlock('Alice', 'Alice', sessions);
    expect(unlocked.some((d) => d.id === 'first-session')).toBe(true);
  });

  it('persists unlocked achievements across store instances', () => {
    const db = openDatabase(':memory:');
    const store1 = new SqliteAchievementStore(db);
    const sessions = [makeSession()];
    store1.checkAndUnlock('Alice', 'Alice', sessions);

    const store2 = new SqliteAchievementStore(db);
    const earned = store2.getEarned('Alice');
    expect(earned.some((e) => e.achievementId === 'first-session')).toBe(true);
  });

  it('does not re-unlock already earned achievements', () => {
    const sessions = [makeSession()];
    store.checkAndUnlock('Alice', 'Alice', sessions);
    const second = store.checkAndUnlock('Alice', 'Alice', sessions);
    expect(second.some((d) => d.id === 'first-session')).toBe(false);
  });

  it('unlocks n64-debut for an N64 session', () => {
    const sessions = [makeSession({ system: 'N64' })];
    const unlocked = store.checkAndUnlock('Alice', 'Alice', sessions);
    expect(unlocked.some((d) => d.id === 'n64-debut')).toBe(true);
  });

  it('getState initialises meta row on first call', () => {
    const state = store.getState('p1', 'Player1');
    expect(state.playerId).toBe('p1');
    expect(state.displayName).toBe('Player1');
  });

  it('playerCount tracks unique players', () => {
    store.checkAndUnlock('Alice', 'Alice', [makeSession()]);
    store.checkAndUnlock('Bob', 'Bob', [makeSession()]);
    expect(store.playerCount()).toBe(2);
  });
});
