/**
 * Desktop save-backup service.
 *
 * Wraps the lobby-server backup REST API so the UI can:
 *  - create manual backups
 *  - trigger pre-session backups before launching a game
 *  - sync post-session after a game exits
 *  - view backup history
 *  - restore from any backup or the last-known-good snapshot
 *
 * When the server is unreachable, operations fail gracefully and the UI
 * falls back to local localStorage state (the save-service layer).
 */

const SERVER_BASE =
  (import.meta.env.VITE_LOBBY_URL as string | undefined) ?? 'http://localhost:8080';

export type BackupReason = 'pre-session' | 'post-session' | 'manual' | 'last-known-good';

export interface BackupMeta {
  id: string;
  gameId: string;
  saveName: string;
  mimeType: string;
  reason: BackupReason;
  isLastKnownGood: boolean;
  saveVersion: number;
  createdAt: string;
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

async function apiFetch(path: string, init?: RequestInit): Promise<Response> {
  return fetch(`${SERVER_BASE}${path}`, {
    headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) },
    ...init,
  });
}

// ─── Backup CRUD ──────────────────────────────────────────────────────────────

/**
 * Create a manual backup for a specific save slot.
 *
 * @param gameId     Game identifier
 * @param saveName   Save-slot name (must match SaveRecord.name on the server)
 * @param data       Base64-encoded save-file contents
 * @param options    Optional reason, mimeType, and version overrides
 */
export async function createBackup(
  gameId: string,
  saveName: string,
  data: string,
  options: { reason?: BackupReason; mimeType?: string; saveVersion?: number } = {}
): Promise<BackupMeta | null> {
  try {
    const res = await apiFetch(`/api/saves/${encodeURIComponent(gameId)}/backup`, {
      method: 'POST',
      body: JSON.stringify({ saveName, data, ...options }),
    });
    if (!res.ok) return null;
    return (await res.json()) as BackupMeta;
  } catch {
    return null;
  }
}

/**
 * List all backups for a game (most recent first, no data payload).
 */
export async function listBackups(gameId: string): Promise<BackupMeta[]> {
  try {
    const res = await apiFetch(`/api/saves/${encodeURIComponent(gameId)}/backups`);
    if (!res.ok) return [];
    return (await res.json()) as BackupMeta[];
  } catch {
    return [];
  }
}

/**
 * Delete a backup by ID.
 */
export async function deleteBackup(gameId: string, backupId: string): Promise<boolean> {
  try {
    const res = await apiFetch(
      `/api/saves/${encodeURIComponent(gameId)}/backups/${encodeURIComponent(backupId)}`,
      { method: 'DELETE' }
    );
    return res.ok;
  } catch {
    return false;
  }
}

/**
 * Restore a backup — copies the backup data back into the canonical save slot.
 * Returns the updated SaveRecord on success.
 */
export async function restoreFromBackup(
  gameId: string,
  backupId: string
): Promise<Record<string, unknown> | null> {
  try {
    const res = await apiFetch(
      `/api/saves/${encodeURIComponent(gameId)}/backups/${encodeURIComponent(backupId)}/restore`,
      { method: 'POST' }
    );
    if (!res.ok) return null;
    return (await res.json()) as Record<string, unknown>;
  } catch {
    return null;
  }
}

/**
 * Mark a backup as the last-known-good for its save slot.
 */
export async function markAsLastKnownGood(gameId: string, backupId: string): Promise<boolean> {
  try {
    const res = await apiFetch(
      `/api/saves/${encodeURIComponent(gameId)}/backups/${encodeURIComponent(backupId)}/mark-lkg`,
      { method: 'POST' }
    );
    return res.ok;
  } catch {
    return false;
  }
}

/**
 * Get the last-known-good backup for a save slot.
 * Returns null when no last-known-good backup exists.
 */
export async function getLastKnownGood(
  gameId: string,
  saveName: string
): Promise<BackupMeta | null> {
  try {
    const res = await apiFetch(
      `/api/saves/${encodeURIComponent(gameId)}/last-known-good?name=${encodeURIComponent(saveName)}`
    );
    if (res.status === 404) return null;
    if (!res.ok) return null;
    return (await res.json()) as BackupMeta;
  } catch {
    return null;
  }
}

// ─── Session lifecycle ────────────────────────────────────────────────────────

export interface SessionSlot {
  saveName: string;
  /** Base64-encoded save-file data. */
  data: string;
  mimeType?: string;
  version?: number;
}

/**
 * Pre-session backup: snapshot all provided save slots before a game starts.
 *
 * Call this just before invoking the emulator so that a safe recovery point
 * exists if the session crashes or corrupts the save.
 *
 * Returns the created backup metadata records.
 */
export async function preSessionBackup(
  gameId: string,
  slots: SessionSlot[]
): Promise<BackupMeta[]> {
  try {
    const res = await apiFetch(`/api/saves/${encodeURIComponent(gameId)}/session-start`, {
      method: 'POST',
      body: JSON.stringify({ slots }),
    });
    if (!res.ok) return [];
    const body = (await res.json()) as { backups: BackupMeta[] };
    return body.backups ?? [];
  } catch {
    return [];
  }
}

/**
 * Post-session sync: upload updated save slots after the emulator exits.
 *
 * When `cleanExit` is true (i.e. the emulator exited normally without a crash),
 * the server automatically marks the uploaded saves as "last known good".
 *
 * Returns the backup metadata for the post-session snapshots.
 */
export async function postSessionSync(
  gameId: string,
  slots: SessionSlot[],
  cleanExit: boolean
): Promise<BackupMeta[]> {
  try {
    const res = await apiFetch(`/api/saves/${encodeURIComponent(gameId)}/session-end`, {
      method: 'POST',
      body: JSON.stringify({ slots, cleanExit }),
    });
    if (!res.ok) return [];
    const body = (await res.json()) as { backups: BackupMeta[] };
    return body.backups ?? [];
  } catch {
    return [];
  }
}

// ─── Display helpers ──────────────────────────────────────────────────────────

export interface BackupReasonDisplay {
  label: string;
  icon: string;
  color: string;
}

export function getBackupReasonDisplay(reason: BackupReason): BackupReasonDisplay {
  switch (reason) {
    case 'pre-session':
      return { label: 'Pre-session', icon: '🔒', color: 'var(--color-oasis-accent-light)' };
    case 'post-session':
      return { label: 'Post-session', icon: '✅', color: 'var(--color-oasis-green)' };
    case 'manual':
      return { label: 'Manual', icon: '💾', color: 'var(--color-oasis-text-muted)' };
    case 'last-known-good':
      return { label: 'Last Known Good', icon: '⭐', color: 'var(--color-oasis-yellow)' };
  }
}
