import * as crypto from 'crypto';
import * as fs from 'fs';

export interface CloudSaveMetadata {
  gameId: string;
  userId: string;
  remoteKey: string;
  localPath: string;
  lastSyncedAt: string;
  lastModifiedAt: string;
  syncStatus: 'synced' | 'pending-upload' | 'pending-download' | 'conflict' | 'error';
  fileHash: string;
  fileSize: number;
}

/**
 * Compute the SHA-256 hash of a local file.
 * Returns an empty string if the file cannot be read (e.g. does not exist yet).
 */
export function computeFileHash(filePath: string): string {
  try {
    const buf = fs.readFileSync(filePath);
    return crypto.createHash('sha256').update(buf).digest('hex');
  } catch {
    return '';
  }
}

/**
 * CloudSyncService manages synchronization of saves between local storage
 * and a remote cloud backend.
 *
 * File hashes are computed with SHA-256 so that unchanged files are detected
 * quickly and unnecessary uploads are avoided.
 *
 * Note: actual cloud storage integration (S3, etc.) would be added as a
 * concrete adapter implementing the CloudStorageAdapter interface below.
 */

/** Minimal interface a real cloud adapter must satisfy. */
export interface CloudStorageAdapter {
  /** Upload a Buffer and return a remote ETag/hash string. */
  upload(key: string, data: Buffer): Promise<string>;
  /** Download a remote file, returning its contents. */
  download(key: string): Promise<Buffer>;
}

export class CloudSyncService {
  private syncRecords: Map<string, CloudSaveMetadata> = new Map();
  private adapter: CloudStorageAdapter | null;

  /**
   * @param adapter  Optional cloud storage adapter.  When omitted the service
   *                 tracks sync state locally but skips actual network I/O —
   *                 useful in tests and when running fully offline.
   */
  constructor(adapter: CloudStorageAdapter | null = null) {
    this.adapter = adapter;
  }

  /**
   * Register a local save for cloud sync tracking.
   * Computes the file hash immediately so the initial state is accurate.
   */
  trackSave(gameId: string, userId: string, localPath: string): CloudSaveMetadata {
    const key = `${userId}/${gameId}`;
    const now = new Date().toISOString();

    const fileHash = computeFileHash(localPath);
    let fileSize = 0;
    try {
      fileSize = fs.statSync(localPath).size;
    } catch {
      // File may not exist yet
    }

    const record: CloudSaveMetadata = {
      gameId,
      userId,
      remoteKey: key,
      localPath,
      lastSyncedAt: now,
      lastModifiedAt: now,
      syncStatus: 'synced',
      fileHash,
      fileSize,
    };

    this.syncRecords.set(key, record);
    return record;
  }

  /**
   * Get the sync status for a game's save.
   */
  getSyncStatus(gameId: string, userId: string): CloudSaveMetadata | null {
    return this.syncRecords.get(`${userId}/${gameId}`) ?? null;
  }

  /**
   * Mark a save as locally modified and needing upload.
   * Re-computes the file hash to reflect the current on-disk state.
   */
  markDirty(gameId: string, userId: string): boolean {
    const key = `${userId}/${gameId}`;
    const record = this.syncRecords.get(key);
    if (!record) return false;

    const newHash = computeFileHash(record.localPath);
    if (newHash !== '' && newHash === record.fileHash) {
      // File unchanged — nothing to do
      return false;
    }

    record.fileHash = newHash;
    record.syncStatus = 'pending-upload';
    record.lastModifiedAt = new Date().toISOString();
    try {
      record.fileSize = fs.statSync(record.localPath).size;
    } catch {
      // Ignore stat errors
    }
    return true;
  }

  /**
   * Check whether the remote version conflicts with the local version.
   * Marks the record as 'conflict' if the remote has a different hash and
   * was modified more recently than the last local sync.
   */
  checkForConflict(
    gameId: string,
    userId: string,
    remoteHash: string,
    remoteModifiedAt: string
  ): boolean {
    const key = `${userId}/${gameId}`;
    const record = this.syncRecords.get(key);
    if (!record) return false;

    const localIsDirty = record.syncStatus === 'pending-upload';
    const hashDiffers = remoteHash !== '' && remoteHash !== record.fileHash;
    const remoteIsNewer = remoteModifiedAt > record.lastSyncedAt;

    if (hashDiffers && localIsDirty && remoteIsNewer) {
      record.syncStatus = 'conflict';
      return true;
    }

    if (hashDiffers && remoteIsNewer) {
      // Remote is newer but we have no local changes — schedule a download
      record.syncStatus = 'pending-download';
    }

    return false;
  }

  /**
   * Trigger an upload of a local save to the cloud.
   */
  async upload(gameId: string, userId: string): Promise<boolean> {
    const key = `${userId}/${gameId}`;
    const record = this.syncRecords.get(key);
    if (!record) return false;

    record.syncStatus = 'pending-upload';

    if (this.adapter) {
      try {
        const data = fs.readFileSync(record.localPath);
        const remoteHash = await this.adapter.upload(record.remoteKey, data);
        record.fileHash = remoteHash || computeFileHash(record.localPath);
        record.fileSize = data.length;
      } catch {
        record.syncStatus = 'error';
        return false;
      }
    }

    record.syncStatus = 'synced';
    record.lastSyncedAt = new Date().toISOString();
    return true;
  }

  /**
   * Trigger a download of a cloud save to local storage.
   */
  async download(gameId: string, userId: string): Promise<boolean> {
    const key = `${userId}/${gameId}`;
    const record = this.syncRecords.get(key);
    if (!record) return false;

    record.syncStatus = 'pending-download';

    if (this.adapter) {
      try {
        const data = await this.adapter.download(record.remoteKey);
        fs.writeFileSync(record.localPath, data);
        record.fileHash = computeFileHash(record.localPath);
        record.fileSize = data.length;
      } catch {
        record.syncStatus = 'error';
        return false;
      }
    }

    record.syncStatus = 'synced';
    record.lastSyncedAt = new Date().toISOString();
    return true;
  }

  /**
   * Resolve a save conflict.
   */
  async resolveConflict(
    gameId: string,
    userId: string,
    resolution: 'keep-local' | 'keep-cloud'
  ): Promise<boolean> {
    const key = `${userId}/${gameId}`;
    const record = this.syncRecords.get(key);
    if (!record || record.syncStatus !== 'conflict') return false;

    if (resolution === 'keep-local') {
      return this.upload(gameId, userId);
    } else {
      return this.download(gameId, userId);
    }
  }

  /**
   * List all tracked saves for a user.
   */
  listTrackedSaves(userId: string): CloudSaveMetadata[] {
    return Array.from(this.syncRecords.values()).filter(
      (r) => r.userId === userId
    );
  }
}
