/**
 * In-memory save-backup store.
 *
 * Keeps pre-session snapshots, manual backups, and the "last known good"
 * marker so users can always recover if a session corrupts their save.
 *
 * BackupReason values:
 *   'pre-session'     — automatic snapshot taken just before a game session starts
 *   'post-session'    — snapshot taken after a session ends successfully
 *   'manual'          — user-triggered backup from the Saves UI
 *   'last-known-good' — explicitly promoted backup that the user trusts
 */

import { randomUUID } from 'crypto';

export type BackupReason = 'pre-session' | 'post-session' | 'manual' | 'last-known-good';

export interface SaveBackupRecord {
  id: string;
  gameId: string;
  /** Save slot name this backup came from (matches SaveRecord.name). */
  saveName: string;
  /** Base64-encoded save file data. */
  data: string;
  mimeType: string;
  reason: BackupReason;
  /** True when this backup has been explicitly marked as the last known good. */
  isLastKnownGood: boolean;
  /** Version counter of the source save at backup time (0 if unknown). */
  saveVersion: number;
  createdAt: string;
}

/** Maximum backups retained per (gameId, saveName) pair. */
const MAX_BACKUPS_PER_SLOT = 20;

export class SaveBackupStore {
  private backups: Map<string, SaveBackupRecord> = new Map();

  // ─── Core CRUD ────────────────────────────────────────────────────────────

  /**
   * Create a new backup.  Automatically evicts the oldest non-last-known-good
   * backup if the per-slot limit would be exceeded.
   */
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

    // Enforce per-slot limit before inserting.
    const existing = this.listForSlot(gameId, saveName);
    if (existing.length >= MAX_BACKUPS_PER_SLOT) {
      // Evict oldest that is NOT last-known-good
      const evictable = existing
        .filter((b) => !b.isLastKnownGood)
        .sort((a, b) => a.createdAt.localeCompare(b.createdAt));
      if (evictable.length > 0) {
        this.backups.delete(evictable[0].id);
      }
    }

    const record: SaveBackupRecord = {
      id: randomUUID(),
      gameId,
      saveName,
      data,
      mimeType,
      reason,
      isLastKnownGood: reason === 'last-known-good',
      saveVersion,
      createdAt: new Date().toISOString(),
    };

    this.backups.set(record.id, record);
    return { ...record };
  }

  /** Get a single backup by ID. */
  get(backupId: string): SaveBackupRecord | null {
    return this.backups.get(backupId) ?? null;
  }

  /** List all backups for a game (most recent first, data excluded). */
  listForGame(gameId: string): Omit<SaveBackupRecord, 'data'>[] {
    const results: Omit<SaveBackupRecord, 'data'>[] = [];
    for (const b of this.backups.values()) {
      if (b.gameId === gameId) {
        // eslint-disable-next-line @typescript-eslint/no-unused-vars
        const { data: _data, ...meta } = b;
        results.push(meta);
      }
    }
    return results.sort((a, b) => b.createdAt.localeCompare(a.createdAt));
  }

  /** List backups for a specific (gameId, saveName) slot. */
  listForSlot(gameId: string, saveName: string): SaveBackupRecord[] {
    return Array.from(this.backups.values())
      .filter((b) => b.gameId === gameId && b.saveName === saveName)
      .sort((a, b) => b.createdAt.localeCompare(a.createdAt));
  }

  /** Delete a backup by ID. Returns true if found and removed. */
  delete(backupId: string): boolean {
    return this.backups.delete(backupId);
  }

  // ─── Last-known-good ──────────────────────────────────────────────────────

  /**
   * Mark a backup as the last known good for its (gameId, saveName) slot.
   * Clears the flag from any previous last-known-good backup for the same slot.
   */
  markAsLastKnownGood(backupId: string): boolean {
    const target = this.backups.get(backupId);
    if (!target) return false;

    // Clear existing last-known-good for this slot
    for (const b of this.backups.values()) {
      if (b.gameId === target.gameId && b.saveName === target.saveName && b.isLastKnownGood) {
        b.isLastKnownGood = false;
        b.reason = 'manual';
      }
    }

    target.isLastKnownGood = true;
    target.reason = 'last-known-good';
    return true;
  }

  /**
   * Get the most recent backup marked as last-known-good for a slot.
   * Returns null if none is marked.
   */
  getLastKnownGood(gameId: string, saveName: string): SaveBackupRecord | null {
    const candidates = this.listForSlot(gameId, saveName).filter((b) => b.isLastKnownGood);
    return candidates.length > 0 ? candidates[0] : null;
  }

  // ─── Session lifecycle helpers ────────────────────────────────────────────

  /**
   * Pre-session backup: snapshot all slots for a game before launching.
   * Returns the created backup records (without data for compactness).
   */
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

  /**
   * Post-session sync: snapshot updated slots after a session ends.
   * Automatically marks as last-known-good when the session completed cleanly.
   */
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
      // Re-read from the store so the returned metadata reflects any mutations
      // (e.g. isLastKnownGood = true) applied by markAsLastKnownGood above.
      const stored = this.backups.get(b.id) ?? b;
      // eslint-disable-next-line @typescript-eslint/no-unused-vars
      const { data: _data, ...meta } = stored;
      return meta;
    });
  }
}
