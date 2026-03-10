export interface SaveSlot {
  id: string;
  gameId: string;
  slotIndex: number;
  filePath: string;
  fileSize: number;
  createdAt: string;
  updatedAt: string;
  isSaveState: boolean;
  isAutoSave: boolean;
  label?: string;
  screenshotPath?: string;
}

export interface CloudSaveRecord {
  id: string;
  gameId: string;
  userId: string;
  remoteKey: string;
  localPath: string;
  lastSyncedAt: string;
  lastModifiedAt: string;
  syncStatus: SyncStatus;
  fileHash: string;
  fileSize: number;
}

export type SyncStatus = 'synced' | 'pending-upload' | 'pending-download' | 'conflict' | 'error';

export interface SaveConflict {
  id: string;
  gameId: string;
  localSave: SaveSlot;
  cloudSave: CloudSaveRecord;
  detectedAt: string;
  resolution?: 'keep-local' | 'keep-cloud' | 'keep-both';
}
