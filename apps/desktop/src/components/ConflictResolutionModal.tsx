import { formatSaveDate, formatFileSize, type SaveRecord } from '../lib/save-service';

interface ConflictResolutionModalProps {
  save: SaveRecord;
  onResolve: (resolution: 'keep-local' | 'use-cloud') => void;
  onCancel: () => void;
}

/**
 * ConflictResolutionModal
 *
 * Shown when a save's syncStatus is 'conflict'. Lets the user compare
 * both the local and cloud versions and choose which one to keep — or
 * cancel to take no action yet (the safe default).
 */
export function ConflictResolutionModal({ save, onResolve, onCancel }: ConflictResolutionModalProps) {
  return (
    /* Backdrop */
    <div
      className="fixed inset-0 z-50 flex items-center justify-center p-4"
      style={{ backgroundColor: 'rgba(0,0,0,0.7)' }}
      onClick={onCancel}
    >
      {/* Dialog */}
      <div
        className="w-full max-w-lg rounded-2xl p-6 shadow-2xl"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
        onClick={(e) => e.stopPropagation()}
      >
        {/* Header */}
        <div className="flex items-start gap-3 mb-4">
          <span className="text-2xl">⚠️</span>
          <div>
            <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-text)' }}>
              Save Conflict
            </h2>
            <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Two different versions of{' '}
              <span style={{ color: 'var(--color-oasis-accent-light)' }}>&ldquo;{save.label}&rdquo;</span>{' '}
              ({save.gameTitle}) exist — one on this device and one in the cloud.
              You need to choose which one to keep.
            </p>
          </div>
        </div>

        {/* Warning banner */}
        <div
          className="rounded-xl px-4 py-3 mb-5 text-sm"
          style={{ backgroundColor: 'rgba(248,113,113,0.12)', color: 'var(--color-oasis-red)' }}
        >
          ⚠️ The version you do <strong>not</strong> choose will be permanently replaced.
          If you are unsure, click <strong>Cancel</strong> to decide later.
        </div>

        {/* Version comparison */}
        <div className="grid grid-cols-2 gap-3 mb-6">
          {/* Local version */}
          <div
            className="rounded-xl p-4"
            style={{ backgroundColor: 'var(--color-oasis-card)' }}
          >
            <p className="text-xs font-bold uppercase tracking-wider mb-2" style={{ color: 'var(--color-oasis-accent-light)' }}>
              💾 This Device
            </p>
            <p className="text-sm font-medium mb-1" style={{ color: 'var(--color-oasis-text)' }}>
              Local copy
            </p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Last saved: {formatSaveDate(save.updatedAt)}
            </p>
            <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Size: {formatFileSize(save.sizeBytes)}
            </p>
          </div>

          {/* Cloud version */}
          <div
            className="rounded-xl p-4"
            style={{ backgroundColor: 'var(--color-oasis-card)' }}
          >
            <p className="text-xs font-bold uppercase tracking-wider mb-2" style={{ color: 'var(--color-oasis-yellow)' }}>
              ☁️ Cloud
            </p>
            <p className="text-sm font-medium mb-1" style={{ color: 'var(--color-oasis-text)' }}>
              Cloud backup
            </p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Last saved: {save.cloudUpdatedAt ? formatSaveDate(save.cloudUpdatedAt) : 'Unknown'}
            </p>
            <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Size: {formatFileSize(save.sizeBytes)}
            </p>
          </div>
        </div>

        {/* Actions */}
        <div className="flex flex-col gap-2">
          {/* Cancel — safe default, visually prominent */}
          <button
            onClick={onCancel}
            className="w-full py-2.5 rounded-xl text-sm font-semibold transition-colors"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          >
            Cancel — Decide Later
          </button>

          <div className="grid grid-cols-2 gap-2">
            <button
              onClick={() => onResolve('keep-local')}
              className="py-2 rounded-xl text-xs font-medium transition-colors"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)' }}
            >
              Keep My Local Copy
            </button>
            <button
              onClick={() => onResolve('use-cloud')}
              className="py-2 rounded-xl text-xs font-medium transition-colors"
              style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)' }}
            >
              Use Cloud Version
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
