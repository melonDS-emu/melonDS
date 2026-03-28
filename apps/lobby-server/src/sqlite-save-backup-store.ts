/**
 * SQLite-backed save-backup store.
 *
 * Schema lives in db.ts (save_backups table).  Interface is identical to
 * the in-memory SaveBackupStore so it can be swapped in transparently.
 */

import { randomUUID } from 'crypto';
import type { DatabaseType } from './db';
import type { SaveBackupRecord, BackupReason } from './save-backup-store';

interface BackupRow {
  id: string;
  game_id: string;
  save_name: string;
  data: string;
  mime_type: string;
  reason: string;
  is_last_known_good: number;
  save_version: number;
  created_at: string;
}

function rowToRecord(row: BackupRow): SaveBackupRecord {
  return {
    id: row.id,
    gameId: row.game_id,
    saveName: row.save_name,
    data: row.data,
    mimeType: row.mime_type,
    reason: row.reason as BackupReason,
    isLastKnownGood: row.is_last_known_good === 1,
    saveVersion: row.save_version,
    createdAt: row.created_at,
  };
}

/** Maximum backups retained per (gameId, saveName) pair. */
const MAX_BACKUPS_PER_SLOT = 20;

export class SqliteSaveBackupStore {
  private db: DatabaseType;

  constructor(db: DatabaseType) {
    this.db = db;
  }

  createBackup(
    gameId: string,
    saveName: string,
    data: string,
    options: {
      reason?: BackupReason;
      mimeType?: string;
      saveVersion?: number;
    } = {}
  ): SaveBackupRecord {
    const { reason = 'manual', mimeType = 'application/octet-stream', saveVersion = 0 } = options;

    // Enforce per-slot limit
    const count = (
      this.db
        .prepare<[string, string], { cnt: number }>(
          'SELECT COUNT(*) AS cnt FROM save_backups WHERE game_id = ? AND save_name = ?'
        )
        .get(gameId, saveName) ?? { cnt: 0 }
    ).cnt;

    if (count >= MAX_BACKUPS_PER_SLOT) {
      // Evict oldest non-last-known-good backup for this slot
      const oldest = this.db
        .prepare<[string, string], { id: string }>(
          'SELECT id FROM save_backups WHERE game_id = ? AND save_name = ? AND is_last_known_good = 0 ORDER BY created_at ASC LIMIT 1'
        )
        .get(gameId, saveName);
      if (oldest) {
        this.db.prepare('DELETE FROM save_backups WHERE id = ?').run(oldest.id);
      }
    }

    const id = randomUUID();
    const now = new Date().toISOString();
    const isLkg = reason === 'last-known-good' ? 1 : 0;

    this.db
      .prepare(
        `INSERT INTO save_backups
           (id, game_id, save_name, data, mime_type, reason, is_last_known_good, save_version, created_at)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`
      )
      .run(id, gameId, saveName, data, mimeType, reason, isLkg, saveVersion, now);

    return rowToRecord(
      this.db.prepare<string, BackupRow>('SELECT * FROM save_backups WHERE id = ?').get(id)!
    );
  }

  get(backupId: string): SaveBackupRecord | null {
    const row = this.db
      .prepare<string, BackupRow>('SELECT * FROM save_backups WHERE id = ?')
      .get(backupId);
    return row ? rowToRecord(row) : null;
  }

  listForGame(gameId: string): Omit<SaveBackupRecord, 'data'>[] {
    const rows = this.db
      .prepare<string, BackupRow>(
        'SELECT id, game_id, save_name, mime_type, reason, is_last_known_good, save_version, created_at FROM save_backups WHERE game_id = ? ORDER BY created_at DESC'
      )
      .all(gameId);
    return rows.map((r) => ({
      id: r.id,
      gameId: r.game_id,
      saveName: r.save_name,
      mimeType: r.mime_type,
      reason: r.reason as BackupReason,
      isLastKnownGood: r.is_last_known_good === 1,
      saveVersion: r.save_version,
      createdAt: r.created_at,
    }));
  }

  listForSlot(gameId: string, saveName: string): SaveBackupRecord[] {
    const rows = this.db
      .prepare<[string, string], BackupRow>(
        'SELECT * FROM save_backups WHERE game_id = ? AND save_name = ? ORDER BY created_at DESC'
      )
      .all(gameId, saveName);
    return rows.map(rowToRecord);
  }

  delete(backupId: string): boolean {
    const info = this.db.prepare('DELETE FROM save_backups WHERE id = ?').run(backupId);
    return info.changes > 0;
  }

  markAsLastKnownGood(backupId: string): boolean {
    const target = this.get(backupId);
    if (!target) return false;

    // Clear any existing last-known-good for this slot
    this.db
      .prepare(
        'UPDATE save_backups SET is_last_known_good = 0, reason = ? WHERE game_id = ? AND save_name = ? AND is_last_known_good = 1'
      )
      .run('manual', target.gameId, target.saveName);

    this.db
      .prepare(
        "UPDATE save_backups SET is_last_known_good = 1, reason = 'last-known-good' WHERE id = ?"
      )
      .run(backupId);

    return true;
  }

  getLastKnownGood(gameId: string, saveName: string): SaveBackupRecord | null {
    const row = this.db
      .prepare<[string, string], BackupRow>(
        'SELECT * FROM save_backups WHERE game_id = ? AND save_name = ? AND is_last_known_good = 1 ORDER BY created_at DESC LIMIT 1'
      )
      .get(gameId, saveName);
    return row ? rowToRecord(row) : null;
  }

  preSessionBackup(
    gameId: string,
    slots: Array<{ saveName: string; data: string; mimeType?: string; version?: number }>
  ): Omit<SaveBackupRecord, 'data'>[] {
    return slots.map((slot) => {
      const b = this.createBackup(gameId, slot.saveName, slot.data, {
        reason: 'pre-session',
        mimeType: slot.mimeType,
        saveVersion: slot.version,
      });
      // eslint-disable-next-line @typescript-eslint/no-unused-vars
      const { data: _data, ...meta } = b;
      return meta;
    });
  }

  postSessionSync(
    gameId: string,
    slots: Array<{ saveName: string; data: string; mimeType?: string; version?: number }>,
    cleanExit: boolean
  ): Omit<SaveBackupRecord, 'data'>[] {
    return slots.map((slot) => {
      const b = this.createBackup(gameId, slot.saveName, slot.data, {
        reason: 'post-session',
        mimeType: slot.mimeType,
        saveVersion: slot.version,
      });
      if (cleanExit) {
        this.markAsLastKnownGood(b.id);
      }
      // Re-fetch from the DB so the returned metadata reflects any mutations
      // (e.g. isLastKnownGood = true) applied by markAsLastKnownGood above.
      const updated = this.get(b.id) ?? b;
      // eslint-disable-next-line @typescript-eslint/no-unused-vars
      const { data: _data, ...meta } = updated;
      return meta;
    });
  }
}
