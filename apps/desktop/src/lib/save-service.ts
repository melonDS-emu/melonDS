/**
 * Browser-compatible save management service.
 *
 * Stores save records in localStorage so the UI works without a live emulator.
 * In production this would be replaced (or augmented) with Electron IPC calls
 * that delegate to the Node.js-backed SaveManager in packages/save-system.
 */

export type SyncStatus =
  | 'local-only'
  | 'synced'
  | 'pending-upload'
  | 'pending-download'
  | 'conflict';

export type SaveSource = 'local' | 'cloud' | 'imported';

export interface SaveRecord {
  id: string;
  gameId: string;
  gameTitle: string;
  system: string;
  slotIndex: number;
  label: string;
  /** Approximate file size in bytes (0 for placeholder/mock records). */
  sizeBytes: number;
  createdAt: string;
  updatedAt: string;
  isSaveState: boolean;
  isAutoSave: boolean;
  source: SaveSource;
  syncStatus: SyncStatus;
  /** ISO timestamp of the last successful cloud sync. */
  lastSyncedAt?: string;
  /** ISO timestamp of the cloud version (used for conflict display). */
  cloudUpdatedAt?: string;
}

// ─── Seed data ───────────────────────────────────────────────────────────────

const SEED_SAVES: SaveRecord[] = [
  // Super Bomberman — one synced, one local-only
  {
    id: 'snes-super-bomberman-0',
    gameId: 'snes-super-bomberman',
    gameTitle: 'Super Bomberman',
    system: 'SNES',
    slotIndex: 0,
    label: 'World 3 – Boss reached',
    sizeBytes: 8192,
    createdAt: '2026-03-01T10:00:00Z',
    updatedAt: '2026-03-08T18:30:00Z',
    isSaveState: false,
    isAutoSave: false,
    source: 'local',
    syncStatus: 'synced',
    lastSyncedAt: '2026-03-08T18:35:00Z',
  },
  {
    id: 'snes-super-bomberman-1',
    gameId: 'snes-super-bomberman',
    gameTitle: 'Super Bomberman',
    system: 'SNES',
    slotIndex: 1,
    label: 'Auto-save',
    sizeBytes: 8192,
    createdAt: '2026-03-09T20:10:00Z',
    updatedAt: '2026-03-09T20:10:00Z',
    isSaveState: true,
    isAutoSave: true,
    source: 'local',
    syncStatus: 'local-only',
  },
  // Mario Kart 64 — conflict between local and cloud
  {
    id: 'n64-mario-kart-64-0',
    gameId: 'n64-mario-kart-64',
    gameTitle: 'Mario Kart 64',
    system: 'N64',
    slotIndex: 0,
    label: 'All cups unlocked',
    sizeBytes: 32768,
    createdAt: '2026-02-20T15:00:00Z',
    updatedAt: '2026-03-07T12:45:00Z',
    isSaveState: false,
    isAutoSave: false,
    source: 'local',
    syncStatus: 'conflict',
    lastSyncedAt: '2026-03-05T09:00:00Z',
    cloudUpdatedAt: '2026-03-09T08:00:00Z',
  },
  // Super Smash Bros. — pending upload
  {
    id: 'n64-super-smash-bros-0',
    gameId: 'n64-super-smash-bros',
    gameTitle: 'Super Smash Bros.',
    system: 'N64',
    slotIndex: 0,
    label: 'All characters unlocked',
    sizeBytes: 16384,
    createdAt: '2026-03-03T14:00:00Z',
    updatedAt: '2026-03-10T09:00:00Z',
    isSaveState: false,
    isAutoSave: false,
    source: 'local',
    syncStatus: 'pending-upload',
    lastSyncedAt: '2026-03-05T14:00:00Z',
  },
  // Pokémon Red — pending download (cloud has a newer save)
  {
    id: 'gb-pokemon-red-0',
    gameId: 'gb-pokemon-red',
    gameTitle: 'Pokémon Red',
    system: 'GB',
    slotIndex: 0,
    label: 'Badge 7',
    sizeBytes: 32768,
    createdAt: '2026-01-10T12:00:00Z',
    updatedAt: '2026-01-15T18:00:00Z',
    isSaveState: false,
    isAutoSave: false,
    source: 'cloud',
    syncStatus: 'pending-download',
    lastSyncedAt: '2026-01-15T18:05:00Z',
    cloudUpdatedAt: '2026-03-09T22:00:00Z',
  },
  // Tetris — imported from an external file
  {
    id: 'gb-tetris-0',
    gameId: 'gb-tetris',
    gameTitle: 'Tetris',
    system: 'GB',
    slotIndex: 0,
    label: 'Imported – High Score',
    sizeBytes: 2048,
    createdAt: '2026-03-10T11:00:00Z',
    updatedAt: '2026-03-10T11:00:00Z',
    isSaveState: false,
    isAutoSave: false,
    source: 'imported',
    syncStatus: 'local-only',
  },
];

// ─── Storage helpers ──────────────────────────────────────────────────────────

const STORAGE_KEY = 'retro-oasis-saves';
const SEEDED_KEY = 'retro-oasis-saves-seeded';

function ensureSeeded(): void {
  if (!localStorage.getItem(SEEDED_KEY)) {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(SEED_SAVES));
    localStorage.setItem(SEEDED_KEY, '1');
  }
}

function load(): SaveRecord[] {
  ensureSeeded();
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) return JSON.parse(raw) as SaveRecord[];
  } catch {
    /* corrupted storage — fall back to seed */
  }
  return [...SEED_SAVES];
}

function persist(saves: SaveRecord[]): void {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(saves));
  } catch {
    /* storage quota exceeded — ignore */
  }
}

// ─── Public API ───────────────────────────────────────────────────────────────

/** Return all save records. */
export function listAllSaves(): SaveRecord[] {
  return load();
}

/** Return save records for a specific game. */
export function listSavesForGame(gameId: string): SaveRecord[] {
  return load().filter((s) => s.gameId === gameId);
}

/** Delete a save by ID. Returns true if something was removed. */
export function deleteSave(id: string): boolean {
  const saves = load();
  const next = saves.filter((s) => s.id !== id);
  if (next.length === saves.length) return false;
  persist(next);
  return true;
}

/** Rename a save's label. Returns the updated record, or null if not found. */
export function renameSave(id: string, newLabel: string): SaveRecord | null {
  const saves = load();
  const target = saves.find((s) => s.id === id);
  if (!target) return null;
  target.label = newLabel.trim() || target.label;
  persist(saves);
  return { ...target };
}

/**
 * Resolve a sync conflict.
 * - 'keep-local' — marks the local copy as the authoritative version and queues
 *   an upload so the cloud matches.
 * - 'use-cloud' — marks the cloud copy as authoritative and queues a download
 *   so the local copy is replaced.
 */
export function resolveConflict(id: string, resolution: 'keep-local' | 'use-cloud'): SaveRecord | null {
  const saves = load();
  const target = saves.find((s) => s.id === id);
  if (!target || target.syncStatus !== 'conflict') return null;

  if (resolution === 'keep-local') {
    target.syncStatus = 'pending-upload';
    target.source = 'local';
  } else {
    target.syncStatus = 'pending-download';
    target.source = 'cloud';
  }
  target.updatedAt = new Date().toISOString();
  persist(saves);
  return { ...target };
}

/**
 * Simulate completing a pending sync operation.
 * In production this would be driven by the actual cloud sync result.
 */
export function completePendingSync(id: string): SaveRecord | null {
  const saves = load();
  const target = saves.find((s) => s.id === id);
  if (!target) return null;
  if (target.syncStatus !== 'pending-upload' && target.syncStatus !== 'pending-download') {
    return null;
  }
  target.syncStatus = 'synced';
  target.lastSyncedAt = new Date().toISOString();
  persist(saves);
  return { ...target };
}

/**
 * Add a save record imported from a file the user selected.
 * The `file` parameter is the browser File object; we store metadata only
 * (the actual bytes are handled by the emulator layer in production).
 */
export function importSaveFromFile(
  file: File,
  gameId: string,
  gameTitle: string,
  system: string,
  label: string
): SaveRecord {
  const saves = load();
  const existingForGame = saves.filter((s) => s.gameId === gameId);
  const slotIndex = existingForGame.length;
  const now = new Date().toISOString();

  const record: SaveRecord = {
    id: `${gameId}-imported-${Date.now()}`,
    gameId,
    gameTitle,
    system,
    slotIndex,
    label: label.trim() || `Imported slot ${slotIndex}`,
    sizeBytes: file.size,
    createdAt: now,
    updatedAt: now,
    isSaveState: file.name.endsWith('.state'),
    isAutoSave: false,
    source: 'imported',
    syncStatus: 'local-only',
  };

  saves.push(record);
  persist(saves);
  return record;
}

/**
 * Trigger a browser download of a save record's data.
 * Since we don't have real binary data in the browser context, we export
 * the save metadata as JSON. In production this would be the actual save file.
 */
export function exportSave(record: SaveRecord): void {
  const payload = JSON.stringify(record, null, 2);
  const blob = new Blob([payload], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  const ext = record.isSaveState ? 'state' : 'sav';
  a.href = url;
  a.download = `${record.gameId}-slot${record.slotIndex}.${ext}`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// ─── Display helpers ──────────────────────────────────────────────────────────

export interface SyncStatusDisplay {
  label: string;
  color: string;
  icon: string;
  description: string;
}

export function getSyncStatusDisplay(status: SyncStatus): SyncStatusDisplay {
  switch (status) {
    case 'synced':
      return {
        label: 'Synced',
        color: 'var(--color-oasis-green)',
        icon: '✓',
        description: 'This save is backed up to the cloud.',
      };
    case 'local-only':
      return {
        label: 'Local Only',
        color: 'var(--color-oasis-text-muted)',
        icon: '💾',
        description: 'This save exists only on this device and is not backed up.',
      };
    case 'pending-upload':
      return {
        label: 'Waiting to Back Up',
        color: 'var(--color-oasis-yellow)',
        icon: '⬆',
        description: 'This save will be uploaded to the cloud when you go online.',
      };
    case 'pending-download':
      return {
        label: 'Update Available',
        color: 'var(--color-oasis-yellow)',
        icon: '⬇',
        description: 'A newer version of this save is available in the cloud.',
      };
    case 'conflict':
      return {
        label: 'Conflict',
        color: 'var(--color-oasis-red)',
        icon: '⚠',
        description: 'Your local save and the cloud version are different. You need to choose which one to keep.',
      };
  }
}

export function formatFileSize(bytes: number): string {
  if (bytes === 0) return '—';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

export function formatSaveDate(iso: string): string {
  const d = new Date(iso);
  return d.toLocaleString(undefined, {
    month: 'short',
    day: 'numeric',
    year: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  });
}

/** Group an array of saves by gameId, preserving insertion order of first occurrence. */
export function groupByGame(saves: SaveRecord[]): Map<string, SaveRecord[]> {
  const map = new Map<string, SaveRecord[]>();
  for (const s of saves) {
    const group = map.get(s.gameId) ?? [];
    group.push(s);
    map.set(s.gameId, group);
  }
  return map;
}
