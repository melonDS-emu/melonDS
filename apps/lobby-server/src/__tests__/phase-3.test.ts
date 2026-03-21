/**
 * Phase 3 — First Full Multiplayer Loop
 *
 * Tests for:
 *  - Host creates a public room and receives a room code
 *  - Host creates a private room (isPublic: false)
 *  - Friend joins by room code
 *  - Ready states: guest toggles ready, host is ready by default
 *  - startGame succeeds when all players are ready
 *  - startGame is rejected when any player is not ready
 *  - Relay port is preserved on the Room object after being set by the handler
 *  - Per-player session tokens: room carries relay port for fresh-token generation on rejoin
 *  - Spectator can join an in-progress room by code (stable spectator support)
 *  - Host leaves: host migrates to the next player
 *  - Guest leaves during in-game: room stays alive with remaining players
 *  - Last player leaves: room is deleted
 *  - Disconnect cleanup: disconnectPlayer removes from all rooms
 *  - Reconnect: rejoinRoom / rejoinByCode allows re-entry to an in-game session
 *  - rejoinRoom is rejected for rooms not in-game status
 *  - rejoinRoom preserves the relay port so the handler can issue a fresh session token
 *  - listPublicRooms only returns waiting public rooms
 */

import { describe, it, expect } from 'vitest';
import { LobbyManager } from '../lobby-manager';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function makeRoom(manager: LobbyManager, opts: { isPublic?: boolean; maxPlayers?: number } = {}) {
  return manager.createRoom(
    'host-1',
    'Test Room',
    'mk64',
    'Mario Kart 64',
    'n64',
    opts.isPublic ?? true,
    opts.maxPlayers ?? 4,
    'HostPlayer',
  );
}

// ---------------------------------------------------------------------------
// Room creation
// ---------------------------------------------------------------------------

describe('Phase 3 — room creation', () => {
  it('creates a public room with a 6-character room code', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { isPublic: true });
    expect(room.roomCode).toMatch(/^[A-Z2-9]{6}$/);
    expect(room.isPublic).toBe(true);
    expect(room.status).toBe('waiting');
  });

  it('creates a private room (isPublic: false)', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { isPublic: false });
    expect(room.isPublic).toBe(false);
  });

  it('host is ready by default when creating a room', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr);
    expect(room.players[0].readyState).toBe('ready');
    expect(room.players[0].isHost).toBe(true);
  });

  it('room starts in waiting status', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr);
    expect(room.status).toBe('waiting');
  });
});

// ---------------------------------------------------------------------------
// Join by room code
// ---------------------------------------------------------------------------

describe('Phase 3 — join by room code', () => {
  it('friend can join a waiting room by code', () => {
    const mgr = new LobbyManager();
    const created = makeRoom(mgr);
    const joined = mgr.joinByCode(created.roomCode, 'guest-1', 'GuestPlayer');
    expect(joined).not.toBeNull();
    expect(joined!.players).toHaveLength(2);
    expect(joined!.players[1].displayName).toBe('GuestPlayer');
    expect(joined!.players[1].readyState).toBe('not-ready');
  });

  it('join-by-code is case-insensitive', () => {
    const mgr = new LobbyManager();
    const created = makeRoom(mgr);
    const lower = created.roomCode.toLowerCase();
    const joined = mgr.joinByCode(lower, 'guest-2', 'CaseGuest');
    expect(joined).not.toBeNull();
  });

  it('returns null for an unknown room code', () => {
    const mgr = new LobbyManager();
    const result = mgr.joinByCode('XXXXXX', 'guest-x', 'Nobody');
    expect(result).toBeNull();
  });

  it('cannot join a room that has already started', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.startGame(room.id, 'host-1');

    const late = mgr.joinByCode(room.roomCode, 'late-joiner', 'LateJoiner');
    expect(late).toBeNull();
  });

  it('cannot join a full room', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest1');

    const overflow = mgr.joinByCode(room.roomCode, 'guest-2', 'Guest2');
    expect(overflow).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// Ready states
// ---------------------------------------------------------------------------

describe('Phase 3 — ready states', () => {
  it('toggleReady flips guest from not-ready to ready', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr);
    mgr.joinRoom(room.id, 'guest-1', 'Guest');

    const toggled = mgr.toggleReady(room.id, 'guest-1');
    const guest = toggled!.players.find((p) => p.id === 'guest-1')!;
    expect(guest.readyState).toBe('ready');
  });

  it('toggleReady flips ready player back to not-ready', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr);
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');
    const toggled = mgr.toggleReady(room.id, 'guest-1');
    const guest = toggled!.players.find((p) => p.id === 'guest-1')!;
    expect(guest.readyState).toBe('not-ready');
  });

  it('toggleReady returns null for an unknown room', () => {
    const mgr = new LobbyManager();
    expect(mgr.toggleReady('no-room', 'guest-1')).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// Game-start handshake
// ---------------------------------------------------------------------------

describe('Phase 3 — game-start handshake', () => {
  it('startGame succeeds when all players are ready', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');

    const started = mgr.startGame(room.id, 'host-1');
    expect(started).not.toBeNull();
    expect(started!.status).toBe('in-game');
  });

  it('startGame is rejected when a guest is not ready', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    // Guest stays not-ready

    const result = mgr.startGame(room.id, 'host-1');
    expect(result).toBeNull();
  });

  it('startGame is rejected when called by a non-host player', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');

    const result = mgr.startGame(room.id, 'guest-1');
    expect(result).toBeNull();
  });

  it('solo host can start immediately (only player is ready)', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    const started = mgr.startGame(room.id, 'host-1');
    expect(started).not.toBeNull();
    expect(started!.status).toBe('in-game');
  });

  it('startGame cannot be called twice on the same room', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr);
    mgr.startGame(room.id, 'host-1');
    const second = mgr.startGame(room.id, 'host-1');
    expect(second).toBeNull();
  });

  it('relay port can be set on the room object after startGame (simulating handler allocation)', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');

    const started = mgr.startGame(room.id, 'host-1');
    expect(started).not.toBeNull();

    // The handler sets relayPort on the room after calling relay.allocateSession()
    started!.relayPort = 9001;
    const fetched = mgr.getRoom(room.id);
    expect(fetched!.relayPort).toBe(9001);
  });

  it('relay port is preserved on the room and visible after rejoin', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');

    const started = mgr.startGame(room.id, 'host-1');
    // Simulate handler allocating relay port
    started!.relayPort = 9042;

    // Guest disconnects then rejoins
    mgr.leaveRoom(room.id, 'guest-1');
    const rejoined = mgr.rejoinRoom(room.id, 'guest-1-new', 'Guest');
    expect(rejoined).not.toBeNull();
    // The handler reads room.relayPort to include in room-rejoined; verify it persists
    expect(rejoined!.relayPort).toBe(9042);
  });
});

// ---------------------------------------------------------------------------
// Spectator support
// ---------------------------------------------------------------------------

describe('Phase 3 — spectator support', () => {
  it('spectator can join an in-progress room by code', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');
    mgr.startGame(room.id, 'host-1');

    const spectated = mgr.joinByCodeAsSpectator(room.roomCode, 'spec-1', 'Viewer');
    expect(spectated).not.toBeNull();
    expect(spectated!.spectators).toHaveLength(1);
    expect(spectated!.spectators[0].displayName).toBe('Viewer');
  });

  it('spectator can join a waiting room by code', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr);
    const spectated = mgr.joinByCodeAsSpectator(room.roomCode, 'spec-1', 'Watcher');
    expect(spectated).not.toBeNull();
    expect(spectated!.spectators[0].id).toBe('spec-1');
  });

  it('spectator leave removes them from the room', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr);
    mgr.joinAsSpectator(room.id, 'spec-1', 'Viewer');

    const after = mgr.leaveRoom(room.id, 'spec-1');
    expect(after).not.toBeNull();
    expect(after!.spectators).toHaveLength(0);
  });
});

// ---------------------------------------------------------------------------
// Disconnect and clean leave
// ---------------------------------------------------------------------------

describe('Phase 3 — clean leave / disconnect rules', () => {
  it('guest leaving keeps the room alive with remaining players', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');
    mgr.startGame(room.id, 'host-1');

    const after = mgr.leaveRoom(room.id, 'guest-1');
    expect(after).not.toBeNull();
    expect(after!.players).toHaveLength(1);
    expect(after!.players[0].id).toBe('host-1');
  });

  it('last player leaving deletes the room', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    const deleted = mgr.leaveRoom(room.id, 'host-1');
    expect(deleted).toBeNull();
    expect(mgr.getRoom(room.id)).toBeNull();
  });

  it('disconnectPlayer cleans up all rooms the player is in', () => {
    const mgr = new LobbyManager();
    const room1 = makeRoom(mgr, { maxPlayers: 4 });
    mgr.joinRoom(room1.id, 'guest-1', 'Guest');

    const affected = mgr.disconnectPlayer('guest-1');
    expect(affected.size).toBeGreaterThanOrEqual(1);
    const updatedRoom = mgr.getRoom(room1.id);
    expect(updatedRoom!.players.some((p) => p.id === 'guest-1')).toBe(false);
  });
});

// ---------------------------------------------------------------------------
// Host migration
// ---------------------------------------------------------------------------

describe('Phase 3 — host migration', () => {
  it('host role transfers to next player when host leaves', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest1');
    mgr.joinRoom(room.id, 'guest-2', 'Guest2');

    const after = mgr.leaveRoom(room.id, 'host-1');
    expect(after).not.toBeNull();
    expect(after!.hostId).toBe('guest-1');
    expect(after!.players[0].isHost).toBe(true);
    expect(after!.players[0].id).toBe('guest-1');
  });

  it('new host is the only host after migration', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest1');

    const after = mgr.leaveRoom(room.id, 'host-1');
    expect(after!.players.filter((p) => p.isHost)).toHaveLength(1);
    expect(after!.players[0].isHost).toBe(true);
  });

  it('player slots are renumbered contiguously after departure', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest1');
    mgr.joinRoom(room.id, 'guest-2', 'Guest2');

    mgr.leaveRoom(room.id, 'guest-1');
    const updated = mgr.getRoom(room.id)!;
    const slots = updated.players.map((p) => p.slot);
    expect(slots).toEqual([0, 1]);
  });
});

// ---------------------------------------------------------------------------
// Reconnect / rejoin
// ---------------------------------------------------------------------------

describe('Phase 3 — reconnect to in-progress session', () => {
  it('rejoinRoom allows a new connection to enter an in-game session', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest');
    mgr.toggleReady(room.id, 'guest-1');
    mgr.startGame(room.id, 'host-1');

    // Guest disconnects
    mgr.leaveRoom(room.id, 'guest-1');

    // Guest reconnects with a new session ID
    const rejoined = mgr.rejoinRoom(room.id, 'guest-1-new', 'Guest');
    expect(rejoined).not.toBeNull();
    expect(rejoined!.players.some((p) => p.id === 'guest-1-new')).toBe(true);
    // Rejoining player starts in ready state
    const rejoinedPlayer = rejoined!.players.find((p) => p.id === 'guest-1-new')!;
    expect(rejoinedPlayer.readyState).toBe('ready');
  });

  it('rejoinByCode resolves room by code and rejoins an in-game session', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    mgr.startGame(room.id, 'host-1');

    const rejoined = mgr.rejoinByCode(room.roomCode, 'late-1', 'LatePlayer');
    expect(rejoined).not.toBeNull();
    expect(rejoined!.players.some((p) => p.id === 'late-1')).toBe(true);
  });

  it('rejoinRoom returns null for a waiting room (game not yet started)', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    const result = mgr.rejoinRoom(room.id, 'newcomer', 'NewPlayer');
    expect(result).toBeNull();
  });

  it('rejoinRoom returns null for a non-existent room', () => {
    const mgr = new LobbyManager();
    expect(mgr.rejoinRoom('no-such-room', 'p1', 'Player')).toBeNull();
  });

  it('rejoinByCode returns null for an unknown code', () => {
    const mgr = new LobbyManager();
    expect(mgr.rejoinByCode('ZZZZZZ', 'p1', 'Player')).toBeNull();
  });

  it('rejoinRoom returns existing entry if player is already in the in-game room', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 4 });
    mgr.startGame(room.id, 'host-1');

    // Host tries to rejoin while already in
    const result = mgr.rejoinRoom(room.id, 'host-1', 'HostPlayer');
    expect(result).not.toBeNull();
    expect(result!.players.filter((p) => p.id === 'host-1')).toHaveLength(1);
  });

  it('rejoinRoom rejects when in-game room is at capacity', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { maxPlayers: 2 });
    mgr.joinRoom(room.id, 'guest-1', 'Guest1');
    mgr.toggleReady(room.id, 'guest-1');
    mgr.startGame(room.id, 'host-1');

    // Both slots filled — should reject
    const result = mgr.rejoinRoom(room.id, 'overflow', 'Overflow');
    expect(result).toBeNull();
  });
});

// ---------------------------------------------------------------------------
// Public room listing
// ---------------------------------------------------------------------------

describe('Phase 3 — public room listing', () => {
  it('listPublicRooms only returns waiting public rooms', () => {
    const mgr = new LobbyManager();
    const pub = makeRoom(mgr, { isPublic: true });
    makeRoom(mgr, { isPublic: false });

    const list = mgr.listPublicRooms();
    expect(list.every((r) => r.isPublic)).toBe(true);
    expect(list.every((r) => r.status === 'waiting')).toBe(true);
    expect(list.find((r) => r.id === pub.id)).toBeDefined();
  });

  it('in-game public room is excluded from the public listing', () => {
    const mgr = new LobbyManager();
    const room = makeRoom(mgr, { isPublic: true });
    mgr.startGame(room.id, 'host-1');

    const list = mgr.listPublicRooms();
    expect(list.find((r) => r.id === room.id)).toBeUndefined();
  });
});

// ---------------------------------------------------------------------------
// Full multiplayer session walkthrough
// ---------------------------------------------------------------------------

describe('Phase 3 — full session walkthrough', () => {
  it('host room → friend joins → both ready → session starts → both leave cleanly', () => {
    const mgr = new LobbyManager();

    // Step 1: host creates a room
    const room = mgr.createRoom(
      'host-player',
      'MK64 Race',
      'mk64',
      'Mario Kart 64',
      'n64',
      true,
      4,
      'HostUser',
    );
    expect(room.status).toBe('waiting');
    expect(room.roomCode).toMatch(/^[A-Z2-9]{6}$/);

    // Step 2: friend joins by room code
    const afterJoin = mgr.joinByCode(room.roomCode, 'friend-player', 'FriendUser');
    expect(afterJoin).not.toBeNull();
    expect(afterJoin!.players).toHaveLength(2);

    // Step 3: friend marks ready (host is ready by default)
    const afterReady = mgr.toggleReady(room.id, 'friend-player');
    expect(afterReady!.players.find((p) => p.id === 'friend-player')!.readyState).toBe('ready');

    // Step 4: host starts the game
    const started = mgr.startGame(room.id, 'host-player');
    expect(started).not.toBeNull();
    expect(started!.status).toBe('in-game');

    // Step 5: friend leaves cleanly
    const afterFriendLeave = mgr.leaveRoom(room.id, 'friend-player');
    expect(afterFriendLeave).not.toBeNull();
    expect(afterFriendLeave!.players).toHaveLength(1);

    // Step 6: host leaves — room is deleted
    const afterHostLeave = mgr.leaveRoom(room.id, 'host-player');
    expect(afterHostLeave).toBeNull();
    expect(mgr.getRoom(room.id)).toBeNull();
  });
});
