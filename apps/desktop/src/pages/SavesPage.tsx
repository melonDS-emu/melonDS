import { useState, useRef, useCallback } from 'react';
import {
  listAllSaves,
  deleteSave,
  resolveConflict,
  completePendingSync,
  importSaveFromFile,
  exportSave,
  getSyncStatusDisplay,
  groupByGame,
  formatFileSize,
  formatSaveDate,
  type SaveRecord,
  type SyncStatus,
} from '../lib/save-service';
import { ConflictResolutionModal } from '../components/ConflictResolutionModal';

// ─── Types ────────────────────────────────────────────────────────────────────

interface ImportPendingState {
  file: File;
  /** Pre-filled game suggestion (empty string if unknown) */
  gameId: string;
  gameTitle: string;
  system: string;
  label: string;
}

interface DeleteConfirmState {
  save: SaveRecord;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

const SYSTEM_COLORS: Record<string, string> = {
  NES: '#CC0000',
  SNES: '#7B5EA7',
  GB: '#6B8E23',
  GBC: '#32CD32',
  GBA: '#663399',
  N64: '#009900',
  NDS: '#E87722',
};

function systemColor(system: string): string {
  return SYSTEM_COLORS[system] ?? 'var(--color-oasis-accent)';
}

// ─── Sub-components ───────────────────────────────────────────────────────────

function SyncBadge({ status }: { status: SyncStatus }) {
  const d = getSyncStatusDisplay(status);
  return (
    <span
      className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full text-[11px] font-semibold"
      style={{ backgroundColor: `${d.color}20`, color: d.color }}
      title={d.description}
    >
      {d.icon} {d.label}
    </span>
  );
}

function SourceBadge({ source }: { source: SaveRecord['source'] }) {
  const map = {
    local: { label: 'Local', color: 'var(--color-oasis-text-muted)' },
    cloud: { label: 'Cloud', color: 'var(--color-oasis-accent-light)' },
    imported: { label: 'Imported', color: 'var(--color-oasis-yellow)' },
  } as const;
  const { label, color } = map[source];
  return (
    <span
      className="inline-flex px-1.5 py-0.5 rounded text-[10px] font-medium"
      style={{ backgroundColor: `${color}15`, color }}
    >
      {label}
    </span>
  );
}

interface SaveRowProps {
  save: SaveRecord;
  onDelete: (save: SaveRecord) => void;
  onExport: (save: SaveRecord) => void;
  onResolveConflict: (save: SaveRecord) => void;
  onRestore: (save: SaveRecord) => void;
}

function SaveRow({ save, onDelete, onExport, onResolveConflict, onRestore }: SaveRowProps) {
  const hasConflict = save.syncStatus === 'conflict';
  const hasPendingDownload = save.syncStatus === 'pending-download';
  const statusDisplay = getSyncStatusDisplay(save.syncStatus);

  return (
    <div
      className="rounded-xl px-4 py-3 flex items-start gap-3"
      style={{
        backgroundColor: 'var(--color-oasis-card)',
        border: hasConflict ? '1px solid rgba(248,113,113,0.4)' : '1px solid transparent',
      }}
    >
      {/* Icon */}
      <div
        className="w-9 h-9 rounded-lg flex items-center justify-center text-base flex-shrink-0 mt-0.5"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
      >
        {save.isAutoSave ? '🔁' : save.isSaveState ? '📸' : '💾'}
      </div>

      {/* Info */}
      <div className="flex-1 min-w-0">
        <div className="flex items-center gap-2 flex-wrap">
          <span className="text-sm font-semibold truncate" style={{ color: 'var(--color-oasis-text)' }}>
            {save.label}
          </span>
          {save.isAutoSave && (
            <span className="text-[10px] px-1.5 py-0.5 rounded" style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}>
              Auto
            </span>
          )}
        </div>

        <div className="flex items-center gap-2 mt-1 flex-wrap">
          <SyncBadge status={save.syncStatus} />
          <SourceBadge source={save.source} />
        </div>

        <div className="mt-1.5 space-y-0.5">
          <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Saved {formatSaveDate(save.updatedAt)} · {formatFileSize(save.sizeBytes)}
          </p>
          {save.lastSyncedAt && (
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Last synced {formatSaveDate(save.lastSyncedAt)}
            </p>
          )}
          {hasConflict && save.cloudUpdatedAt && (
            <p className="text-xs" style={{ color: 'var(--color-oasis-red)' }}>
              Cloud version: {formatSaveDate(save.cloudUpdatedAt)}
            </p>
          )}
        </div>

        {/* Inline help for conflict */}
        {hasConflict && (
          <p className="text-xs mt-1.5" style={{ color: 'var(--color-oasis-red)' }}>
            {statusDisplay.description}
          </p>
        )}
      </div>

      {/* Actions */}
      <div className="flex items-center gap-1.5 flex-shrink-0 mt-0.5">
        {hasConflict && (
          <button
            onClick={() => onResolveConflict(save)}
            className="px-2.5 py-1 rounded-lg text-xs font-semibold transition-colors"
            style={{ backgroundColor: 'rgba(248,113,113,0.2)', color: 'var(--color-oasis-red)' }}
            title="Resolve this conflict"
          >
            Resolve
          </button>
        )}
        {hasPendingDownload && (
          <button
            onClick={() => onRestore(save)}
            className="px-2.5 py-1 rounded-lg text-xs font-semibold transition-colors"
            style={{ backgroundColor: 'rgba(251,191,36,0.15)', color: 'var(--color-oasis-yellow)' }}
            title="Restore from cloud"
          >
            Restore
          </button>
        )}
        <button
          onClick={() => onExport(save)}
          className="p-1.5 rounded-lg text-sm transition-colors"
          style={{ color: 'var(--color-oasis-text-muted)' }}
          title="Export this save"
        >
          ⬇
        </button>
        <button
          onClick={() => onDelete(save)}
          className="p-1.5 rounded-lg text-sm transition-colors"
          style={{ color: 'var(--color-oasis-text-muted)' }}
          title="Delete this save"
        >
          🗑
        </button>
      </div>
    </div>
  );
}

// ─── Import overlay ───────────────────────────────────────────────────────────

interface ImportOverlayProps {
  pending: ImportPendingState;
  onChange: (partial: Partial<ImportPendingState>) => void;
  onConfirm: () => void;
  onCancel: () => void;
}

function ImportOverlay({ pending, onChange, onConfirm, onCancel }: ImportOverlayProps) {
  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center p-4"
      style={{ backgroundColor: 'rgba(0,0,0,0.7)' }}
      onClick={onCancel}
    >
      <div
        className="w-full max-w-md rounded-2xl p-6 shadow-2xl"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
        onClick={(e) => e.stopPropagation()}
      >
        <h2 className="text-lg font-bold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
          Import Save File
        </h2>
        <p className="text-sm mb-5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Importing <strong style={{ color: 'var(--color-oasis-accent-light)' }}>{pending.file.name}</strong>{' '}
          ({formatFileSize(pending.file.size)}).
          Fill in the details below so RetroOasis knows which game this save belongs to.
        </p>

        <div className="space-y-3">
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Game Title
            </label>
            <input
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid transparent' }}
              value={pending.gameTitle}
              placeholder="e.g. Super Bomberman"
              onChange={(e) => onChange({ gameTitle: e.target.value })}
            />
          </div>
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              System
            </label>
            <input
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid transparent' }}
              value={pending.system}
              placeholder="e.g. SNES"
              onChange={(e) => onChange({ system: e.target.value })}
            />
          </div>
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Save Label (optional)
            </label>
            <input
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)', border: '1px solid transparent' }}
              value={pending.label}
              placeholder="e.g. World 4 start"
              onChange={(e) => onChange({ label: e.target.value })}
            />
          </div>
        </div>

        <div className="flex gap-2 mt-6">
          <button
            onClick={onCancel}
            className="flex-1 py-2.5 rounded-xl text-sm font-medium"
            style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)' }}
          >
            Cancel
          </button>
          <button
            onClick={onConfirm}
            disabled={!pending.gameTitle.trim() || !pending.system.trim()}
            className="flex-1 py-2.5 rounded-xl text-sm font-semibold transition-opacity"
            style={{
              backgroundColor: 'var(--color-oasis-accent)',
              color: 'white',
              opacity: pending.gameTitle.trim() && pending.system.trim() ? 1 : 0.45,
            }}
          >
            Import Save
          </button>
        </div>
      </div>
    </div>
  );
}

// ─── RestoreConfirmModal ──────────────────────────────────────────────────────

interface RestoreConfirmProps {
  save: SaveRecord;
  onConfirm: () => void;
  onCancel: () => void;
}

function RestoreConfirmModal({ save, onConfirm, onCancel }: RestoreConfirmProps) {
  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center p-4"
      style={{ backgroundColor: 'rgba(0,0,0,0.7)' }}
      onClick={onCancel}
    >
      <div
        className="w-full max-w-md rounded-2xl p-6 shadow-2xl"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
        onClick={(e) => e.stopPropagation()}
      >
        <h2 className="text-lg font-bold mb-2" style={{ color: 'var(--color-oasis-text)' }}>
          Restore from Cloud?
        </h2>
        <p className="text-sm mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
          A newer version of{' '}
          <strong style={{ color: 'var(--color-oasis-accent-light)' }}>&ldquo;{save.label}&rdquo;</strong>{' '}
          ({save.gameTitle}) is available in the cloud.
        </p>
        {save.cloudUpdatedAt && (
          <p className="text-sm mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Cloud version was saved on {formatSaveDate(save.cloudUpdatedAt)}.
          </p>
        )}
        <div
          className="rounded-xl px-4 py-3 mb-5 text-sm"
          style={{ backgroundColor: 'rgba(251,191,36,0.12)', color: 'var(--color-oasis-yellow)' }}
        >
          Your current local copy will be replaced with the cloud version.
          Export your local copy first if you want to keep it.
        </div>
        <div className="flex gap-2">
          <button
            onClick={onCancel}
            className="flex-1 py-2.5 rounded-xl text-sm font-medium"
            style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)' }}
          >
            Cancel
          </button>
          <button
            onClick={onConfirm}
            className="flex-1 py-2.5 rounded-xl text-sm font-semibold"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          >
            Restore
          </button>
        </div>
      </div>
    </div>
  );
}

// ─── Delete confirm inline ────────────────────────────────────────────────────

interface DeleteConfirmBannerProps {
  save: SaveRecord;
  onConfirm: () => void;
  onCancel: () => void;
}

function DeleteConfirmBanner({ save, onConfirm, onCancel }: DeleteConfirmBannerProps) {
  return (
    <div
      className="rounded-xl px-4 py-3 flex items-center gap-3"
      style={{ backgroundColor: 'rgba(248,113,113,0.12)', border: '1px solid rgba(248,113,113,0.35)' }}
    >
      <p className="flex-1 text-sm" style={{ color: 'var(--color-oasis-text)' }}>
        Delete <strong>&ldquo;{save.label}&rdquo;</strong>?{' '}
        <span style={{ color: 'var(--color-oasis-text-muted)' }}>This cannot be undone.</span>
      </p>
      <button
        onClick={onCancel}
        className="px-3 py-1 rounded-lg text-xs font-medium"
        style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)' }}
      >
        Cancel
      </button>
      <button
        onClick={onConfirm}
        className="px-3 py-1 rounded-lg text-xs font-semibold"
        style={{ backgroundColor: 'var(--color-oasis-red)', color: 'white' }}
      >
        Delete
      </button>
    </div>
  );
}

// ─── Main page ────────────────────────────────────────────────────────────────

export function SavesPage() {
  const [saves, setSaves] = useState<SaveRecord[]>(() => listAllSaves());
  const [query, setQuery] = useState('');
  const [conflictTarget, setConflictTarget] = useState<SaveRecord | null>(null);
  const [restoreTarget, setRestoreTarget] = useState<SaveRecord | null>(null);
  const [deleteConfirm, setDeleteConfirm] = useState<DeleteConfirmState | null>(null);
  const [importPending, setImportPending] = useState<ImportPendingState | null>(null);
  const [toastMessage, setToastMessage] = useState<string | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const refresh = useCallback(() => setSaves(listAllSaves()), []);

  function showToast(msg: string) {
    setToastMessage(msg);
    setTimeout(() => setToastMessage(null), 3000);
  }

  // ─── Filtered + grouped saves ─────────────────────────────────────────────

  const filtered = query.trim()
    ? saves.filter(
        (s) =>
          s.gameTitle.toLowerCase().includes(query.toLowerCase()) ||
          s.label.toLowerCase().includes(query.toLowerCase()) ||
          s.system.toLowerCase().includes(query.toLowerCase())
      )
    : saves;

  const grouped = groupByGame(filtered);

  // ─── Stats ────────────────────────────────────────────────────────────────

  const conflictCount = saves.filter((s) => s.syncStatus === 'conflict').length;
  const syncedCount = saves.filter((s) => s.syncStatus === 'synced').length;
  const totalCount = saves.length;

  // ─── Handlers ─────────────────────────────────────────────────────────────

  function handleDeleteRequest(save: SaveRecord) {
    setDeleteConfirm({ save });
  }

  function handleDeleteConfirm() {
    if (!deleteConfirm) return;
    deleteSave(deleteConfirm.save.id);
    refresh();
    setDeleteConfirm(null);
    showToast(`Deleted "${deleteConfirm.save.label}"`);
  }

  function handleExport(save: SaveRecord) {
    exportSave(save);
    showToast(`Exporting "${save.label}"…`);
  }

  function handleConflictResolve(resolution: 'keep-local' | 'use-cloud') {
    if (!conflictTarget) return;
    resolveConflict(conflictTarget.id, resolution);
    refresh();
    setConflictTarget(null);
    const msg =
      resolution === 'keep-local'
        ? 'Your local copy will be uploaded to the cloud.'
        : 'The cloud version will be downloaded to this device.';
    showToast(msg);
  }

  function handleRestoreConfirm() {
    if (!restoreTarget) return;
    completePendingSync(restoreTarget.id);
    refresh();
    setRestoreTarget(null);
    showToast(`Restoring "${restoreTarget.label}" from cloud…`);
  }

  function handleFileSelected(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0];
    if (!file) return;
    // Reset input so the same file can be selected again later
    e.target.value = '';
    setImportPending({
      file,
      gameId: '',
      gameTitle: '',
      system: '',
      label: '',
    });
  }

  function handleImportConfirm() {
    if (!importPending || !importPending.gameTitle.trim() || !importPending.system.trim()) return;
    const gameId = importPending.gameId || importPending.gameTitle.toLowerCase().replace(/\s+/g, '-');
    importSaveFromFile(
      importPending.file,
      gameId,
      importPending.gameTitle,
      importPending.system,
      importPending.label
    );
    refresh();
    setImportPending(null);
    showToast(`Imported "${importPending.file.name}"`);
  }

  // ─── Render ───────────────────────────────────────────────────────────────

  return (
    <div className="max-w-3xl">
      {/* Header */}
      <div className="flex items-start justify-between mb-6">
        <div>
          <h1 className="text-2xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            💾 My Saves
          </h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Manage your game saves — view sync status, export, import, or restore from cloud.
          </p>
        </div>
        <button
          onClick={() => fileInputRef.current?.click()}
          className="flex-shrink-0 px-4 py-2 rounded-xl text-sm font-semibold transition-colors"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
        >
          + Import Save
        </button>
        <input
          ref={fileInputRef}
          type="file"
          accept=".sav,.state,.srm,.sav.gz"
          className="hidden"
          onChange={handleFileSelected}
        />
      </div>

      {/* Stats bar */}
      <div className="grid grid-cols-3 gap-3 mb-5">
        <StatCard label="Total Saves" value={String(totalCount)} color="var(--color-oasis-text)" />
        <StatCard
          label="Synced to Cloud"
          value={String(syncedCount)}
          color="var(--color-oasis-green)"
        />
        <StatCard
          label="Conflicts"
          value={String(conflictCount)}
          color={conflictCount > 0 ? 'var(--color-oasis-red)' : 'var(--color-oasis-text-muted)'}
        />
      </div>

      {/* Conflict alert bar */}
      {conflictCount > 0 && (
        <div
          className="rounded-xl px-4 py-3 mb-5 text-sm flex items-center gap-2"
          style={{ backgroundColor: 'rgba(248,113,113,0.12)', color: 'var(--color-oasis-red)' }}
        >
          <span>⚠️</span>
          <span>
            {conflictCount === 1
              ? '1 save has a conflict that needs your attention.'
              : `${conflictCount} saves have conflicts that need your attention.`}{' '}
            Scroll down to find them and choose which version to keep.
          </span>
        </div>
      )}

      {/* Search */}
      <div className="relative mb-5">
        <span
          className="absolute left-3 top-1/2 -translate-y-1/2 text-sm pointer-events-none"
          style={{ color: 'var(--color-oasis-text-muted)' }}
        >
          🔍
        </span>
        <input
          className="w-full pl-9 pr-3 py-2 rounded-xl text-sm outline-none"
          style={{
            backgroundColor: 'var(--color-oasis-card)',
            color: 'var(--color-oasis-text)',
            border: '1px solid transparent',
          }}
          placeholder="Search by game or label…"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
        />
      </div>

      {/* Empty state */}
      {totalCount === 0 && (
        <div className="text-center py-16">
          <p className="text-4xl mb-3">💾</p>
          <p className="font-semibold mb-1" style={{ color: 'var(--color-oasis-text)' }}>
            No saves yet
          </p>
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Start a game to create your first save, or import an existing save file above.
          </p>
        </div>
      )}

      {/* No search results */}
      {totalCount > 0 && filtered.length === 0 && (
        <p className="text-center py-8" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No saves match &ldquo;{query}&rdquo;.
        </p>
      )}

      {/* Game sections */}
      <div className="space-y-6">
        {Array.from(grouped.entries()).map(([gameId, gameSaves]) => {
          const representative = gameSaves[0];
          const hasConflict = gameSaves.some((s) => s.syncStatus === 'conflict');
          return (
            <div key={gameId}>
              {/* Game heading */}
              <div className="flex items-center gap-2 mb-2">
                <span
                  className="px-2 py-0.5 rounded text-[10px] font-bold"
                  style={{ backgroundColor: systemColor(representative.system), color: 'white' }}
                >
                  {representative.system}
                </span>
                <h2 className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                  {representative.gameTitle}
                </h2>
                {hasConflict && (
                  <span
                    className="px-1.5 py-0.5 rounded text-[10px] font-semibold"
                    style={{ backgroundColor: 'rgba(248,113,113,0.2)', color: 'var(--color-oasis-red)' }}
                  >
                    ⚠ Conflict
                  </span>
                )}
                <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {gameSaves.length} {gameSaves.length === 1 ? 'save' : 'saves'}
                </span>
              </div>

              {/* Save rows */}
              <div className="space-y-2">
                {gameSaves.map((save) =>
                  deleteConfirm?.save.id === save.id ? (
                    <DeleteConfirmBanner
                      key={save.id}
                      save={save}
                      onConfirm={handleDeleteConfirm}
                      onCancel={() => setDeleteConfirm(null)}
                    />
                  ) : (
                    <SaveRow
                      key={save.id}
                      save={save}
                      onDelete={handleDeleteRequest}
                      onExport={handleExport}
                      onResolveConflict={setConflictTarget}
                      onRestore={setRestoreTarget}
                    />
                  )
                )}
              </div>
            </div>
          );
        })}
      </div>

      {/* Legend */}
      {totalCount > 0 && (
        <div
          className="mt-8 rounded-xl p-4"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          <p className="text-xs font-semibold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Status Guide
          </p>
          <div className="grid grid-cols-2 gap-x-6 gap-y-1.5">
            {(['synced', 'local-only', 'pending-upload', 'pending-download', 'conflict'] as const).map(
              (status) => {
                const d = getSyncStatusDisplay(status);
                return (
                  <div key={status} className="flex items-start gap-1.5">
                    <span className="text-xs font-semibold" style={{ color: d.color }}>
                      {d.icon} {d.label}
                    </span>
                    <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      — {d.description}
                    </span>
                  </div>
                );
              }
            )}
          </div>
        </div>
      )}

      {/* Modals */}
      {conflictTarget && (
        <ConflictResolutionModal
          save={conflictTarget}
          onResolve={handleConflictResolve}
          onCancel={() => setConflictTarget(null)}
        />
      )}

      {restoreTarget && (
        <RestoreConfirmModal
          save={restoreTarget}
          onConfirm={handleRestoreConfirm}
          onCancel={() => setRestoreTarget(null)}
        />
      )}

      {importPending && (
        <ImportOverlay
          pending={importPending}
          onChange={(partial) => setImportPending((p) => ({ ...p!, ...partial }))}
          onConfirm={handleImportConfirm}
          onCancel={() => setImportPending(null)}
        />
      )}

      {/* Toast */}
      {toastMessage && (
        <div
          className="fixed bottom-6 left-1/2 -translate-x-1/2 px-5 py-3 rounded-xl text-sm font-medium shadow-lg z-50"
          style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text)', border: '1px solid rgba(255,255,255,0.08)' }}
        >
          {toastMessage}
        </div>
      )}
    </div>
  );
}

// ─── StatCard ─────────────────────────────────────────────────────────────────

function StatCard({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <div
      className="rounded-xl p-4"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      <p className="text-2xl font-bold" style={{ color }}>
        {value}
      </p>
      <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {label}
      </p>
    </div>
  );
}
