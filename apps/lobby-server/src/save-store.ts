/**
 * In-memory save-file store with conflict detection.
 *
 * Provides a lightweight server-side save sync mechanism. Each save slot is
 * keyed by (gameId, name) and stores base64-encoded file data together with
 * modification timestamps used for conflict detection.
 *
 * In a production deployment this would write to disk or a database; for now
 * all data is in-memory and lost on server restart.
 */

import { randomUUID } from 'crypto';

export interface SaveRecord {
  id: string;
  gameId: string;
  /** Human-readable slot name, e.g. "Slot 1" or the save file basename. */
  name: string;
  /** Base64-encoded save file data. */
  data: string;
  /** MIME type of the save data. Defaults to application/octet-stream. */
  mimeType: string;
  /** ISO timestamp of the last upload. */
  updatedAt: string;
  /** ISO timestamp of the first upload. */
  createdAt: string;
  /** Monotonically increasing version counter for conflict detection. */
  version: number;
}

export interface ConflictError {
  conflict: true;
  message: string;
  serverVersion: number;
  serverUpdatedAt: string;
}

/** Maximum number of save records to retain per game. */
const MAX_SAVES_PER_GAME = 100;

export class SaveStore {
  /** saveId -> SaveRecord */
  private saves: Map<string, SaveRecord> = new Map();
  /** `${gameId}::${name}` -> saveId — for deduplication by slot name */
  private bySlot: Map<string, string> = new Map();

  /**
   * Upload (create or replace) a save for a game slot.
   *
   * Pass `expectedVersion` to enable optimistic-concurrency conflict detection.
   * If the stored version differs from `expectedVersion` a ConflictError is returned.
   */
  put(
    gameId: string,
    name: string,
    data: string,
    mimeType = 'application/octet-stream',
    expectedVersion?: number
  ): SaveRecord | ConflictError {
    const slotKey = `${gameId}::${name}`;
    const existingId = this.bySlot.get(slotKey);

    if (existingId) {
      const existing = this.saves.get(existingId)!;

      // Conflict detection: if caller provided expectedVersion and it doesn't match
      if (expectedVersion !== undefined && existing.version !== expectedVersion) {
        return {
          conflict: true,
          message: `Conflict: server has version ${existing.version}, expected ${expectedVersion}.`,
          serverVersion: existing.version,
          serverUpdatedAt: existing.updatedAt,
        };
      }

      // Overwrite the existing record
      existing.data = data;
      existing.mimeType = mimeType;
      existing.updatedAt = new Date().toISOString();
      existing.version += 1;
      return { ...existing };
    }

    // Enforce per-game limit
    const existingForGame = this.listForGame(gameId);
    if (existingForGame.length >= MAX_SAVES_PER_GAME) {
      // Evict the oldest save
      const oldest = existingForGame.sort((a, b) => a.updatedAt.localeCompare(b.updatedAt))[0];
      this.bySlot.delete(`${oldest.gameId}::${oldest.name}`);
      this.saves.delete(oldest.id);
    }

    const now = new Date().toISOString();
    const record: SaveRecord = {
      id: randomUUID(),
      gameId,
      name,
      data,
      mimeType,
      createdAt: now,
      updatedAt: now,
      version: 1,
    };

    this.saves.set(record.id, record);
    this.bySlot.set(slotKey, record.id);
    return { ...record };
  }

  /**
   * Retrieve a save record by its ID.
   */
  get(saveId: string): SaveRecord | null {
    return this.saves.get(saveId) ?? null;
  }

  /**
   * List all saves for a given game (without data payload for compactness).
   */
  listForGame(gameId: string): Omit<SaveRecord, 'data'>[] {
    const results: Omit<SaveRecord, 'data'>[] = [];
    for (const record of this.saves.values()) {
      if (record.gameId === gameId) {
        // eslint-disable-next-line @typescript-eslint/no-unused-vars
        const { data: _data, ...meta } = record;
        results.push(meta);
      }
    }
    return results.sort((a, b) => b.updatedAt.localeCompare(a.updatedAt));
  }

  /**
   * Delete a save by ID.
   * Returns true if the save was found and deleted, false otherwise.
   */
  delete(saveId: string, gameId: string): boolean {
    const record = this.saves.get(saveId);
    if (!record || record.gameId !== gameId) return false;
    this.bySlot.delete(`${record.gameId}::${record.name}`);
    this.saves.delete(saveId);
    return true;
  }
}
