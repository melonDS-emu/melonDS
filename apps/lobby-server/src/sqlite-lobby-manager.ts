/**
 * SQLite-backed lobby/room manager.
 *
 * Persists rooms and their players to the database so they survive server
 * restarts.  Rooms that were `in-game` at shutdown are transitioned to
 * `closed` on the next boot to give a clean state.
 *
 * The interface is intentionally identical to the in-memory `LobbyManager`
 * so it can be swapped in transparently.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';
import type { Room, RoomPlayer, RoomSpectator, LobbyStatus, ConnectionQuality } from './types';

function generateRoomCode(): string {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  let code = '';
  for (let i = 0; i < 6; i++) {
    code += chars[Math.floor(Math.random() * chars.length)];
  }
  return code;
}

/** Shape of a row from the `rooms` table. */
interface RoomRow {
  id: string;
  name: string;
  host_id: string;
  game_id: string;
  game_title: string;
  system: string;
  is_public: number;
  room_code: string;
  max_players: number;
  status: string;
  relay_port: number | null;
  theme: string | null;
  rank_mode: string | null;
  created_at: string;
}

/** Shape of a row from the `room_players` table. */
interface RoomPlayerRow {
  room_id: string;
  player_id: string;
  display_name: string;
  ready_state: string;
  slot: number;
  is_host: number;
  joined_at: string;
  connection_quality: string;
  latency_ms: number | null;
}

/** Shape of a row from the `room_spectators` table. */
interface RoomSpectatorRow {
  room_id: string;
  spectator_id: string;
  display_name: string;
  joined_at: string;
}

function rowToPlayer(row: RoomPlayerRow): RoomPlayer {
  return {
    id: row.player_id,
    displayName: row.display_name,
    readyState: row.ready_state as RoomPlayer['readyState'],
    slot: row.slot,
    isHost: row.is_host === 1,
    joinedAt: row.joined_at,
    connectionQuality: row.connection_quality as ConnectionQuality,
    latencyMs: row.latency_ms ?? undefined,
  };
}

function rowToSpectator(row: RoomSpectatorRow): RoomSpectator {
  return {
    id: row.spectator_id,
    displayName: row.display_name,
    joinedAt: row.joined_at,
  };
}

export class SqliteLobbyManager {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
    // On startup, close any rooms that were left in-game — they can't be resumed.
    this.db.prepare(`UPDATE rooms SET status = 'closed' WHERE status = 'in-game'`).run();
  }

  // ---------------------------------------------------------------------------
  // Private helpers
  // ---------------------------------------------------------------------------

  private _fetchRoom(roomId: string): Room | null {
    const row = this.db.prepare<string, RoomRow>('SELECT * FROM rooms WHERE id = ?').get(roomId);
    if (!row) return null;
    return this.hydrateRoom(row);
  }

  private hydrateRoom(row: RoomRow): Room {
    const playerRows = this.db
      .prepare<string, RoomPlayerRow>('SELECT * FROM room_players WHERE room_id = ? ORDER BY slot ASC')
      .all(row.id);
    const spectatorRows = this.db
      .prepare<string, RoomSpectatorRow>('SELECT * FROM room_spectators WHERE room_id = ?')
      .all(row.id);

    return {
      id: row.id,
      name: row.name,
      hostId: row.host_id,
      gameId: row.game_id,
      gameTitle: row.game_title,
      system: row.system,
      isPublic: row.is_public === 1,
      roomCode: row.room_code,
      maxPlayers: row.max_players,
      status: row.status as LobbyStatus,
      relayPort: row.relay_port ?? undefined,
      theme: row.theme ?? undefined,
      rankMode: (row.rank_mode as 'casual' | 'ranked' | null) ?? 'casual',
      createdAt: row.created_at,
      players: playerRows.map(rowToPlayer),
      spectators: spectatorRows.map(rowToSpectator),
    };
  }

  // ---------------------------------------------------------------------------
  // Public API (mirrors LobbyManager)
  // ---------------------------------------------------------------------------

  createRoom(
    hostId: string,
    name: string,
    gameId: string,
    gameTitle: string,
    system: string,
    isPublic: boolean,
    maxPlayers: number,
    displayName: string,
    theme?: string,
    rankMode?: 'casual' | 'ranked',
  ): Room {
    const roomId = randomUUID();
    const roomCode = generateRoomCode();
    const now = new Date().toISOString();

    this.db.prepare(`
      INSERT INTO rooms (id, name, host_id, game_id, game_title, system, is_public, room_code, max_players, status, theme, rank_mode, created_at)
      VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'waiting', ?, ?, ?)
    `).run(roomId, name, hostId, gameId, gameTitle, system, isPublic ? 1 : 0, roomCode, maxPlayers, theme ?? null, rankMode ?? 'casual', now);

    this.db.prepare(`
      INSERT INTO room_players (room_id, player_id, display_name, ready_state, slot, is_host, joined_at, connection_quality)
      VALUES (?, ?, ?, 'ready', 0, 1, ?, 'unknown')
    `).run(roomId, hostId, displayName, now);

    return this._fetchRoom(roomId)!;
  }

  joinRoom(roomId: string, playerId: string, displayName: string): Room | null {
    const room = this._fetchRoom(roomId);
    if (!room) return null;
    if (room.status !== 'waiting') return null;
    if (room.players.length >= room.maxPlayers) return null;
    if (room.players.some((p) => p.id === playerId)) return room;

    const slot = room.players.length;
    const now = new Date().toISOString();

    this.db.prepare(`
      INSERT INTO room_players (room_id, player_id, display_name, ready_state, slot, is_host, joined_at, connection_quality)
      VALUES (?, ?, ?, 'not-ready', ?, 0, ?, 'unknown')
    `).run(roomId, playerId, displayName, slot, now);

    return this._fetchRoom(roomId)!;
  }

  joinAsSpectator(roomId: string, spectatorId: string, displayName: string): Room | null {
    const room = this._fetchRoom(roomId);
    if (!room) return null;
    if (room.status === 'closed') return null;
    if (room.spectators.some((s) => s.id === spectatorId)) return room;

    this.db.prepare(`
      INSERT INTO room_spectators (room_id, spectator_id, display_name, joined_at)
      VALUES (?, ?, ?, ?)
    `).run(roomId, spectatorId, displayName, new Date().toISOString());

    return this._fetchRoom(roomId)!;
  }

  joinByCode(roomCode: string, playerId: string, displayName: string): Room | null {
    const row = this.db
      .prepare<string, { id: string }>(`SELECT id FROM rooms WHERE room_code = ? AND status = 'waiting'`)
      .get(roomCode.toUpperCase());
    if (!row) return null;
    return this.joinRoom(row.id, playerId, displayName);
  }

  joinByCodeAsSpectator(roomCode: string, spectatorId: string, displayName: string): Room | null {
    const row = this.db
      .prepare<string, { id: string }>(`SELECT id FROM rooms WHERE room_code = ? AND status != 'closed'`)
      .get(roomCode.toUpperCase());
    if (!row) return null;
    return this.joinAsSpectator(row.id, spectatorId, displayName);
  }

  rejoinRoom(roomId: string, playerId: string, displayName: string): Room | null {
    const room = this._fetchRoom(roomId);
    if (!room) return null;
    if (room.status !== 'in-game') return null;
    if (room.players.some((p) => p.id === playerId)) return room;
    if (room.players.length >= room.maxPlayers) return null;

    const slot = room.players.length;
    const now = new Date().toISOString();

    this.db.prepare(`
      INSERT INTO room_players (room_id, player_id, display_name, ready_state, slot, is_host, joined_at, connection_quality)
      VALUES (?, ?, ?, 'ready', ?, 0, ?, 'unknown')
    `).run(roomId, playerId, displayName, slot, now);

    return this._fetchRoom(roomId)!;
  }

  rejoinByCode(roomCode: string, playerId: string, displayName: string): Room | null {
    const row = this.db
      .prepare<string, { id: string }>(`SELECT id FROM rooms WHERE room_code = ? AND status = 'in-game'`)
      .get(roomCode.toUpperCase());
    if (!row) return null;
    return this.rejoinRoom(row.id, playerId, displayName);
  }

  leaveRoom(roomId: string, playerId: string): Room | null {
    const room = this._fetchRoom(roomId);
    if (!room) return null;

    // Spectator leaving
    if (room.spectators.some((s) => s.id === playerId)) {
      this.db.prepare('DELETE FROM room_spectators WHERE room_id = ? AND spectator_id = ?').run(roomId, playerId);
      return this._fetchRoom(roomId);
    }

    // Player leaving
    this.db.prepare('DELETE FROM room_players WHERE room_id = ? AND player_id = ?').run(roomId, playerId);

    const remaining = this.db
      .prepare<string, RoomPlayerRow>('SELECT * FROM room_players WHERE room_id = ? ORDER BY slot ASC')
      .all(roomId);

    if (remaining.length === 0) {
      this.db.prepare(`UPDATE rooms SET status = 'closed' WHERE id = ?`).run(roomId);
      return null;
    }

    // Transfer host if host left
    if (room.hostId === playerId) {
      const newHost = remaining[0];
      this.db.prepare('UPDATE rooms SET host_id = ? WHERE id = ?').run(newHost.player_id, roomId);
      this.db.prepare('UPDATE room_players SET is_host = 1 WHERE room_id = ? AND player_id = ?').run(roomId, newHost.player_id);
    }

    // Renumber slots
    for (let i = 0; i < remaining.length; i++) {
      this.db.prepare('UPDATE room_players SET slot = ? WHERE room_id = ? AND player_id = ?').run(i, roomId, remaining[i].player_id);
    }

    return this._fetchRoom(roomId)!;
  }

  toggleReady(roomId: string, playerId: string): Room | null {
    const room = this._fetchRoom(roomId);
    if (!room) return null;
    const player = room.players.find((p) => p.id === playerId);
    if (!player) return null;

    const newState = player.readyState === 'ready' ? 'not-ready' : 'ready';
    this.db.prepare('UPDATE room_players SET ready_state = ? WHERE room_id = ? AND player_id = ?').run(newState, roomId, playerId);
    return this._fetchRoom(roomId)!;
  }

  startGame(roomId: string, playerId: string): Room | null {
    const room = this._fetchRoom(roomId);
    if (!room) return null;
    if (room.status !== 'waiting') return null;
    if (room.hostId !== playerId) return null;

    const allReady = room.players.every((p) => p.readyState === 'ready');
    if (!allReady) return null;

    this.db.prepare(`UPDATE rooms SET status = 'in-game' WHERE id = ?`).run(roomId);
    return this._fetchRoom(roomId)!;
  }

  updateConnectionQuality(
    roomId: string,
    playerId: string,
    quality: ConnectionQuality,
    latencyMs: number
  ): void {
    this.db.prepare('UPDATE room_players SET connection_quality = ?, latency_ms = ? WHERE room_id = ? AND player_id = ?')
      .run(quality, latencyMs, roomId, playerId);
  }

  setRelayPort(roomId: string, port: number): void {
    this.db.prepare('UPDATE rooms SET relay_port = ? WHERE id = ?').run(port, roomId);
  }

  listPublicRooms(): Room[] {
    const rows = this.db
      .prepare<[], RoomRow>(`SELECT * FROM rooms WHERE is_public = 1 AND status = 'waiting'`)
      .all();
    return rows.map((r) => this.hydrateRoom(r));
  }

  /** Public alias matching the LobbyManager interface. */
  getRoom(roomId: string): Room | null {
    return this._fetchRoom(roomId);
  }

  getRoomById(roomId: string): Room | null {
    return this._fetchRoom(roomId);
  }

  /** Find any rooms where this player is an active participant. */
  getRoomsForPlayer(playerId: string): Room[] {
    const rows = this.db
      .prepare<string, { room_id: string }>('SELECT DISTINCT room_id FROM room_players WHERE player_id = ?')
      .all(playerId);
    return rows.map((r) => this._fetchRoom(r.room_id)).filter(Boolean) as Room[];
  }

  /** Get all player IDs in a room (for broadcasting), including spectators. */
  getRoomPlayerIds(roomId: string): string[] {
    const players = this.db
      .prepare<string, { player_id: string }>('SELECT player_id FROM room_players WHERE room_id = ?')
      .all(roomId);
    const spectators = this.db
      .prepare<string, { spectator_id: string }>('SELECT spectator_id FROM room_spectators WHERE room_id = ?')
      .all(roomId);
    return [...players.map((p) => p.player_id), ...spectators.map((s) => s.spectator_id)];
  }

  /**
   * Remove a player from all rooms they are in (called on disconnect).
   * Returns a map of roomId → updated Room (or null if room was deleted).
   */
  disconnectPlayer(playerId: string): Map<string, Room | null> {
    const affected = new Map<string, Room | null>();

    const playerRoomIds = this.db
      .prepare<string, { room_id: string }>('SELECT DISTINCT room_id FROM room_players WHERE player_id = ?')
      .all(playerId);
    const spectatorRoomIds = this.db
      .prepare<string, { room_id: string }>('SELECT DISTINCT room_id FROM room_spectators WHERE spectator_id = ?')
      .all(playerId);

    const allRoomIds = new Set([
      ...playerRoomIds.map((r) => r.room_id),
      ...spectatorRoomIds.map((r) => r.room_id),
    ]);

    for (const roomId of allRoomIds) {
      const updated = this.leaveRoom(roomId, playerId);
      affected.set(roomId, updated);
    }

    return affected;
  }
}
