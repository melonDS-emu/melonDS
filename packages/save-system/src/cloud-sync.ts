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
 * CloudSyncService manages synchronization of saves between local storage
 * and a remote cloud backend.
 *
 * This is a scaffold — actual cloud storage integration (S3, etc.) would
 * be added in a later phase.
 */
export class CloudSyncService {
  private syncRecords: Map<string, CloudSaveMetadata> = new Map();

  /**
   * Register a local save for cloud sync tracking.
   */
  trackSave(gameId: string, userId: string, localPath: string): CloudSaveMetadata {
    const key = `${userId}/${gameId}`;
    const now = new Date().toISOString();

    const record: CloudSaveMetadata = {
      gameId,
      userId,
      remoteKey: key,
      localPath,
      lastSyncedAt: now,
      lastModifiedAt: now,
      syncStatus: 'synced',
      fileHash: '', // TODO: Compute actual file hash
      fileSize: 0,
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
   * Trigger an upload of a local save to the cloud.
   */
  async upload(gameId: string, userId: string): Promise<boolean> {
    const key = `${userId}/${gameId}`;
    const record = this.syncRecords.get(key);
    if (!record) return false;

    record.syncStatus = 'pending-upload';
    // TODO: Actually upload to cloud storage
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
    // TODO: Actually download from cloud storage
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
