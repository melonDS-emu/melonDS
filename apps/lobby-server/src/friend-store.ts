/**
 * Server-side friend list and friend-request management.
 *
 * Friends are stored by persistent player ID.  A two-step flow is used:
 *   1. Player A sends a friend request to player B (`addRequest`).
 *   2. Player B accepts (`acceptRequest`) or declines (`declineRequest`).
 *   3. On accept, a symmetric friendship row is inserted for both parties.
 *
 * All data is persisted to SQLite so it survives server restarts.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';

export type FriendRequestStatus = 'pending' | 'accepted' | 'declined';

export interface FriendRequest {
  id: string;
  fromId: string;
  toId: string;
  status: FriendRequestStatus;
  createdAt: string;
  updatedAt: string;
}

export interface FriendEntry {
  userId: string;
  friendId: string;
  createdAt: string;
}

interface FriendRequestRow {
  id: string;
  from_id: string;
  to_id: string;
  status: string;
  created_at: string;
  updated_at: string;
}

interface FriendRow {
  user_id: string;
  friend_id: string;
  created_at: string;
}

function rowToRequest(row: FriendRequestRow): FriendRequest {
  return {
    id: row.id,
    fromId: row.from_id,
    toId: row.to_id,
    status: row.status as FriendRequestStatus,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
  };
}

export class FriendStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  // ---------------------------------------------------------------------------
  // Friend list
  // ---------------------------------------------------------------------------

  /**
   * List all friend IDs for a given user.
   */
  getFriends(userId: string): FriendEntry[] {
    return this.db
      .prepare<string, FriendRow>('SELECT * FROM friends WHERE user_id = ?')
      .all(userId)
      .map((r) => ({ userId: r.user_id, friendId: r.friend_id, createdAt: r.created_at }));
  }

  /**
   * Check whether two users are friends.
   */
  areFriends(userId: string, friendId: string): boolean {
    const row = this.db
      .prepare<[string, string], { user_id: string }>('SELECT user_id FROM friends WHERE user_id = ? AND friend_id = ?')
      .get(userId, friendId);
    return !!row;
  }

  /**
   * Remove a friendship (both directions).
   * Returns true if the friendship existed and was removed.
   */
  removeFriend(userId: string, friendId: string): boolean {
    const result = this.db
      .prepare('DELETE FROM friends WHERE (user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)')
      .run(userId, friendId, friendId, userId);
    return result.changes > 0;
  }

  // ---------------------------------------------------------------------------
  // Friend requests
  // ---------------------------------------------------------------------------

  /**
   * Create a new friend request from `fromId` to `toId`.
   * Returns null if a request already exists in either direction or they are
   * already friends.
   */
  addRequest(fromId: string, toId: string): FriendRequest | null {
    if (fromId === toId) return null;
    if (this.areFriends(fromId, toId)) return null;

    // Check for existing pending request in either direction
    const existing = this.db
      .prepare<[string, string, string, string], FriendRequestRow>(`
        SELECT * FROM friend_requests
        WHERE ((from_id = ? AND to_id = ?) OR (from_id = ? AND to_id = ?))
          AND status = 'pending'
      `)
      .get(fromId, toId, toId, fromId);
    if (existing) return rowToRequest(existing);

    const id = randomUUID();
    const now = new Date().toISOString();
    this.db.prepare(`
      INSERT INTO friend_requests (id, from_id, to_id, status, created_at, updated_at)
      VALUES (?, ?, ?, 'pending', ?, ?)
    `).run(id, fromId, toId, now, now);

    return rowToRequest(
      this.db.prepare<string, FriendRequestRow>('SELECT * FROM friend_requests WHERE id = ?').get(id)!
    );
  }

  /**
   * Accept a pending friend request.
   * Inserts symmetric friendship rows.  Returns the updated request or null.
   */
  acceptRequest(requestId: string, acceptingUserId: string): FriendRequest | null {
    const row = this.db
      .prepare<string, FriendRequestRow>(`SELECT * FROM friend_requests WHERE id = ? AND status = 'pending'`)
      .get(requestId);
    if (!row || row.to_id !== acceptingUserId) return null;

    const now = new Date().toISOString();
    this.db.prepare(`UPDATE friend_requests SET status = 'accepted', updated_at = ? WHERE id = ?`).run(now, requestId);

    // Insert symmetric friendship
    const insert = this.db.prepare('INSERT OR IGNORE INTO friends (user_id, friend_id, created_at) VALUES (?, ?, ?)');
    insert.run(row.from_id, row.to_id, now);
    insert.run(row.to_id, row.from_id, now);

    return rowToRequest(
      this.db.prepare<string, FriendRequestRow>('SELECT * FROM friend_requests WHERE id = ?').get(requestId)!
    );
  }

  /**
   * Decline a pending friend request.
   */
  declineRequest(requestId: string, decliningUserId: string): FriendRequest | null {
    const row = this.db
      .prepare<string, FriendRequestRow>(`SELECT * FROM friend_requests WHERE id = ? AND status = 'pending'`)
      .get(requestId);
    if (!row || row.to_id !== decliningUserId) return null;

    const now = new Date().toISOString();
    this.db.prepare(`UPDATE friend_requests SET status = 'declined', updated_at = ? WHERE id = ?`).run(now, requestId);

    return rowToRequest(
      this.db.prepare<string, FriendRequestRow>('SELECT * FROM friend_requests WHERE id = ?').get(requestId)!
    );
  }

  /**
   * List all pending requests addressed to a user (incoming).
   */
  getPendingRequests(toId: string): FriendRequest[] {
    return this.db
      .prepare<string, FriendRequestRow>(`SELECT * FROM friend_requests WHERE to_id = ? AND status = 'pending' ORDER BY created_at DESC`)
      .all(toId)
      .map(rowToRequest);
  }

  /**
   * List all requests sent by a user (outgoing).
   */
  getSentRequests(fromId: string): FriendRequest[] {
    return this.db
      .prepare<string, FriendRequestRow>('SELECT * FROM friend_requests WHERE from_id = ? ORDER BY created_at DESC')
      .all(fromId)
      .map(rowToRequest);
  }
}
