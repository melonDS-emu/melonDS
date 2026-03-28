import { useState, type FormEvent } from 'react';

interface JoinRoomModalProps {
  initialCode?: string;
  onConfirm: (roomCode: string, displayName: string) => void;
  onSpectate?: (roomCode: string, displayName: string) => void;
  onClose: () => void;
}

export function JoinRoomModal({ initialCode, onConfirm, onSpectate, onClose }: JoinRoomModalProps) {
  const [roomCode, setRoomCode] = useState(initialCode?.toUpperCase() ?? '');
  const [displayName, setDisplayName] = useState(
    () => localStorage.getItem('retro-oasis-display-name') ?? ''
  );

  function handleSubmit(e: FormEvent<HTMLFormElement>) {
    e.preventDefault();
    if (!displayName.trim() || !roomCode.trim()) return;
    localStorage.setItem('retro-oasis-display-name', displayName.trim());
    onConfirm(roomCode.trim().toUpperCase(), displayName.trim());
  }

  function handleSpectate() {
    if (!displayName.trim() || !roomCode.trim()) return;
    localStorage.setItem('retro-oasis-display-name', displayName.trim());
    onSpectate?.(roomCode.trim().toUpperCase(), displayName.trim());
  }

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center p-4"
      style={{ backgroundColor: 'rgba(0,0,0,0.7)' }}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div
        className="w-full max-w-sm rounded-2xl p-6"
        style={{ backgroundColor: 'var(--color-oasis-card)' }}
      >
        <h2 className="text-xl font-bold mb-4">🚪 Join a Room</h2>

        <form onSubmit={handleSubmit} className="space-y-4">
          {/* Display name */}
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Your Name
            </label>
            <input
              type="text"
              value={displayName}
              onChange={(e) => setDisplayName(e.target.value)}
              placeholder="Enter your display name"
              maxLength={20}
              required
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
              }}
            />
          </div>

          {/* Room code */}
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Room Code
            </label>
            <input
              type="text"
              value={roomCode}
              onChange={(e) => setRoomCode(e.target.value.toUpperCase())}
              placeholder="e.g. AB3XYZ"
              maxLength={8}
              required
              className="w-full px-3 py-2 rounded-xl text-sm font-mono tracking-widest outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-accent-light)',
              }}
            />
          </div>

          {/* Actions */}
          <div className="flex gap-3 pt-2">
            <button
              type="button"
              onClick={onClose}
              className="flex-1 py-3 rounded-xl font-bold text-sm"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              Cancel
            </button>
            {onSpectate && (
              <button
                type="button"
                onClick={handleSpectate}
                disabled={!displayName.trim() || !roomCode.trim()}
                className="flex-1 py-3 rounded-xl font-bold text-sm disabled:opacity-40"
                style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text)' }}
              >
                👁 Spectate
              </button>
            )}
            <button
              type="submit"
              className="flex-1 py-3 rounded-xl font-bold text-sm transition-transform hover:scale-[1.02] active:scale-[0.98]"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              Join Room
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
