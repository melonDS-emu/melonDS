/**
 * SQLite-backed direct-message store (Phase 14).
 *
 * Follows the SqliteSessionHistory / SqliteAchievementStore pattern.
 * Activated at runtime when DB_PATH env var is set.
 */

import type { DatabaseType } from './db';
import { MessageStore, type DirectMessage, type ConversationSummary } from './message-store';

interface MessageRow {
  id: string;
  from_player: string;
  to_player: string;
  content: string;
  sent_at: string;
  read_at: string | null;
}

function rowToMessage(row: MessageRow): DirectMessage {
  return {
    id: row.id,
    fromPlayer: row.from_player,
    toPlayer: row.to_player,
    content: row.content,
    sentAt: row.sent_at,
    readAt: row.read_at,
  };
}

export class SqliteMessageStore extends MessageStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    super();
    this.db = db;
  }

  // ---------------------------------------------------------------------------
  // Write — override in-memory methods to persist to SQLite
  // ---------------------------------------------------------------------------

  override sendMessage(fromPlayer: string, toPlayer: string, content: string): DirectMessage {
    const msg = super.sendMessage(fromPlayer, toPlayer, content);
    this.db
      .prepare(
        `INSERT INTO direct_messages (id, from_player, to_player, content, sent_at, read_at)
         VALUES (?, ?, ?, ?, ?, NULL)`
      )
      .run(msg.id, msg.fromPlayer, msg.toPlayer, msg.content, msg.sentAt);
    return msg;
  }

  override markRead(fromPlayer: string, toPlayer: string): void {
    this.db
      .prepare(
        `UPDATE direct_messages
         SET read_at = ?
         WHERE from_player = ? AND to_player = ? AND read_at IS NULL`
      )
      .run(new Date().toISOString(), fromPlayer, toPlayer);
    // Sync in-memory state
    super.markRead(fromPlayer, toPlayer);
  }

  // ---------------------------------------------------------------------------
  // Read — load from SQLite instead of in-memory map
  // ---------------------------------------------------------------------------

  override getConversation(player1: string, player2: string, limit = 50): DirectMessage[] {
    const rows = this.db
      .prepare<[string, string, string, string, number], MessageRow>(
        `SELECT id, from_player, to_player, content, sent_at, read_at
         FROM direct_messages
         WHERE (from_player = ? AND to_player = ?)
            OR (from_player = ? AND to_player = ?)
         ORDER BY sent_at ASC
         LIMIT ?`
      )
      .all(player1, player2, player2, player1, limit);
    return rows.map(rowToMessage);
  }

  override getUnreadCount(toPlayer: string): number {
    const row = this.db
      .prepare<string, { cnt: number }>(
        `SELECT COUNT(*) as cnt FROM direct_messages WHERE to_player = ? AND read_at IS NULL`
      )
      .get(toPlayer);
    return row?.cnt ?? 0;
  }

  override getRecentConversations(player: string): ConversationSummary[] {
    // Single query: for each peer, get the most recent message and unread count.
    // SQLite doesn't support window functions in older versions, so we use a
    // correlated subquery approach that is still a single round-trip.
    interface ConvRow {
      peer: string;
      id: string;
      from_player: string;
      to_player: string;
      content: string;
      sent_at: string;
      read_at: string | null;
      unread_count: number;
    }
    const rows = this.db
      .prepare<[string, string, string, string, string, string, string, string], ConvRow>(
        `SELECT
           peers.peer,
           m.id,
           m.from_player,
           m.to_player,
           m.content,
           m.sent_at,
           m.read_at,
           (SELECT COUNT(*) FROM direct_messages
            WHERE from_player = peers.peer AND to_player = ? AND read_at IS NULL) AS unread_count
         FROM (
           SELECT DISTINCT
             CASE WHEN from_player = ? THEN to_player ELSE from_player END AS peer
           FROM direct_messages
           WHERE from_player = ? OR to_player = ?
         ) AS peers
         JOIN direct_messages m ON
           ((m.from_player = ? AND m.to_player = peers.peer) OR
            (m.from_player = peers.peer AND m.to_player = ?))
           AND m.sent_at = (
             SELECT MAX(sent_at) FROM direct_messages
             WHERE (from_player = ? AND to_player = peers.peer)
                OR (from_player = peers.peer AND to_player = ?)
           )
         ORDER BY m.sent_at DESC`
      )
      .all(player, player, player, player, player, player, player, player);

    return rows.map((row) => ({
      peer: row.peer,
      lastMessage: {
        id: row.id,
        fromPlayer: row.from_player,
        toPlayer: row.to_player,
        content: row.content,
        sentAt: row.sent_at,
        readAt: row.read_at,
      },
      unreadCount: row.unread_count,
    }));
  }

  // ---------------------------------------------------------------------------
  // Initialisation: pre-load rows from DB into the in-memory map for lookups
  // ---------------------------------------------------------------------------

  hydrate(): void {
    const rows = this.db
      .prepare<[], MessageRow>(
        `SELECT id, from_player, to_player, content, sent_at, read_at FROM direct_messages`
      )
      .all();
    for (const row of rows) {
      this.messages.set(row.id, rowToMessage(row));
    }
  }
}
