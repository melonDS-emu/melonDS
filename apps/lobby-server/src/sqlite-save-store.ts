/**
 * SQLite-backed save-file store.
 *
 * Persists save records to the database so they survive server restarts.
 * The interface is intentionally identical to the in-memory `SaveStore`
 * so it can be swapped in transparently.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';
import type { SaveRecord, ConflictError } from './save-store';

interface SaveRow {
  id: string;
  game_id: string;
  name: string;
  data: string;
  mime_type: string;
  created_at: string;
  updated_at: string;
  version: number;
}

function rowToRecord(row: SaveRow): SaveRecord {
  return {
    id: row.id,
    gameId: row.game_id,
    name: row.name,
    data: row.data,
    mimeType: row.mime_type,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
    version: row.version,
  };
}

/** Maximum number of save records to retain per game. */
const MAX_SAVES_PER_GAME = 100;

export class SqliteSaveStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  put(
    gameId: string,
    name: string,
    data: string,
    mimeType = 'application/octet-stream',
    expectedVersion?: number
  ): SaveRecord | ConflictError {
    const existing = this.db
      .prepare<[string, string], SaveRow>('SELECT * FROM saves WHERE game_id = ? AND name = ?')
      .get(gameId, name);

    if (existing) {
      // Conflict detection
      if (expectedVersion !== undefined && existing.version !== expectedVersion) {
        return {
          conflict: true,
          message: `Conflict: server has version ${existing.version}, expected ${expectedVersion}.`,
          serverVersion: existing.version,
          serverUpdatedAt: existing.updated_at,
        };
      }

      const now = new Date().toISOString();
      this.db.prepare(`
        UPDATE saves SET data = ?, mime_type = ?, updated_at = ?, version = version + 1
        WHERE id = ?
      `).run(data, mimeType, now, existing.id);

      return rowToRecord(
        this.db.prepare<string, SaveRow>('SELECT * FROM saves WHERE id = ?').get(existing.id)!
      );
    }

    // Enforce per-game limit
    const count = (this.db.prepare<string, { cnt: number }>('SELECT COUNT(*) AS cnt FROM saves WHERE game_id = ?').get(gameId) ?? { cnt: 0 }).cnt;
    if (count >= MAX_SAVES_PER_GAME) {
      // Evict the oldest save for this game
      const oldest = this.db
        .prepare<string, { id: string }>('SELECT id FROM saves WHERE game_id = ? ORDER BY updated_at ASC LIMIT 1')
        .get(gameId);
      if (oldest) {
        this.db.prepare('DELETE FROM saves WHERE id = ?').run(oldest.id);
      }
    }

    const id = randomUUID();
    const now = new Date().toISOString();
    this.db.prepare(`
      INSERT INTO saves (id, game_id, name, data, mime_type, created_at, updated_at, version)
      VALUES (?, ?, ?, ?, ?, ?, ?, 1)
    `).run(id, gameId, name, data, mimeType, now, now);

    return rowToRecord(this.db.prepare<string, SaveRow>('SELECT * FROM saves WHERE id = ?').get(id)!);
  }

  get(saveId: string): SaveRecord | null {
    const row = this.db.prepare<string, SaveRow>('SELECT * FROM saves WHERE id = ?').get(saveId);
    return row ? rowToRecord(row) : null;
  }

  listForGame(gameId: string): Omit<SaveRecord, 'data'>[] {
    const rows = this.db
      .prepare<string, SaveRow>('SELECT id, game_id, name, mime_type, created_at, updated_at, version FROM saves WHERE game_id = ? ORDER BY updated_at DESC')
      .all(gameId);
    return rows.map((r) => ({
      id: r.id,
      gameId: r.game_id,
      name: r.name,
      mimeType: r.mime_type,
      createdAt: r.created_at,
      updatedAt: r.updated_at,
      version: r.version,
    }));
  }

  delete(saveId: string, gameId: string): boolean {
    const row = this.db.prepare<string, SaveRow>('SELECT * FROM saves WHERE id = ?').get(saveId);
    if (!row || row.game_id !== gameId) return false;
    this.db.prepare('DELETE FROM saves WHERE id = ?').run(saveId);
    return true;
  }
}
