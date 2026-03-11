/**
 * Tests for the in-memory session history tracker.
 */
import { describe, it, expect, beforeEach } from 'vitest';
import { SessionHistory } from '../session-history';

describe('SessionHistory', () => {
  let history: SessionHistory;

  beforeEach(() => {
    history = new SessionHistory();
  });

  it('starts a session and records it', () => {
    const record = history.startSession('room-1', 'mk64', 'Mario Kart 64', 'n64', ['Alice', 'Bob']);
    expect(record.roomId).toBe('room-1');
    expect(record.gameTitle).toBe('Mario Kart 64');
    expect(record.players).toEqual(['Alice', 'Bob']);
    expect(record.endedAt).toBeNull();
    expect(record.durationSecs).toBeNull();
  });

  it('returns the session via getByRoomId', () => {
    history.startSession('room-2', 'smb3', 'SMB3', 'nes', ['Player1']);
    const record = history.getByRoomId('room-2');
    expect(record).not.toBeNull();
    expect(record!.gameTitle).toBe('SMB3');
  });

  it('ends a session and records endedAt and duration', async () => {
    history.startSession('room-3', 'mk64', 'Mario Kart 64', 'n64', ['Alice']);
    // Small delay so duration is > 0
    await new Promise((r) => setTimeout(r, 50));
    const ended = history.endSession('room-3');
    expect(ended).not.toBeNull();
    expect(ended!.endedAt).not.toBeNull();
    expect(ended!.durationSecs).toBeGreaterThanOrEqual(0);
  });

  it('returns null when ending a non-existent session', () => {
    expect(history.endSession('ghost-room')).toBeNull();
  });

  it('does not double-end a session', async () => {
    history.startSession('room-4', 'mk64', 'MK64', 'n64', ['Alice']);
    await new Promise((r) => setTimeout(r, 20));
    history.endSession('room-4');
    const second = history.endSession('room-4');
    expect(second).toBeNull();
  });

  it('returns all sessions ordered newest first', async () => {
    history.startSession('room-a', 'g1', 'Game 1', 'n64', []);
    // Small delay to ensure room-b has a later timestamp
    await new Promise((r) => setTimeout(r, 5));
    history.startSession('room-b', 'g2', 'Game 2', 'n64', []);
    const all = history.getAll();
    expect(all.length).toBe(2);
    // Newest should come first (room-b was started after room-a)
    expect(all[0].roomId).toBe('room-b');
    expect(all[1].roomId).toBe('room-a');
  });

  it('getCompleted returns only ended sessions', () => {
    history.startSession('room-c', 'g1', 'G1', 'n64', []);
    history.startSession('room-d', 'g2', 'G2', 'n64', []);
    history.endSession('room-c');

    const completed = history.getCompleted();
    expect(completed.length).toBe(1);
    expect(completed[0].roomId).toBe('room-c');
  });
});
