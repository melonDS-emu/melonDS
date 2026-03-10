import { useState } from 'react';
import { MOCK_GAMES } from '../data/mock-games';
import type { CreateRoomPayload } from '../services/lobby-types';

interface HostRoomModalProps {
  preselectedGameId?: string;
  onConfirm: (payload: Omit<CreateRoomPayload, 'displayName'>, displayName: string) => void;
  onClose: () => void;
}

export function HostRoomModal({ preselectedGameId, onConfirm, onClose }: HostRoomModalProps) {
  const defaultGame = MOCK_GAMES.find((g) => g.id === preselectedGameId) ?? MOCK_GAMES[0];

  const [roomName, setRoomName] = useState(`${defaultGame.title} Room`);
  const [selectedGameId, setSelectedGameId] = useState(defaultGame.id);
  const [displayName, setDisplayName] = useState(
    () => localStorage.getItem('retro-oasis-display-name') ?? ''
  );
  const [isPublic, setIsPublic] = useState(true);

  const selectedGame = MOCK_GAMES.find((g) => g.id === selectedGameId) ?? MOCK_GAMES[0];

  function handleGameChange(gameId: string) {
    const game = MOCK_GAMES.find((g) => g.id === gameId);
    setSelectedGameId(gameId);
    if (game) setRoomName(`${game.title} Room`);
  }

  function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    if (!displayName.trim()) return;
    localStorage.setItem('retro-oasis-display-name', displayName.trim());

    onConfirm(
      {
        name: roomName.trim() || `${selectedGame.title} Room`,
        gameId: selectedGame.id,
        gameTitle: selectedGame.title,
        system: selectedGame.system,
        isPublic,
        maxPlayers: selectedGame.maxPlayers,
      },
      displayName.trim()
    );
  }

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center p-4"
      style={{ backgroundColor: 'rgba(0,0,0,0.7)' }}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div
        className="w-full max-w-md rounded-2xl p-6"
        style={{ backgroundColor: 'var(--color-oasis-card)' }}
      >
        <h2 className="text-xl font-bold mb-4">🎮 Host a Room</h2>

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
                border: '1px solid transparent',
              }}
            />
          </div>

          {/* Game selection */}
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Game
            </label>
            <select
              value={selectedGameId}
              onChange={(e) => handleGameChange(e.target.value)}
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
              }}
            >
              {MOCK_GAMES.map((g) => (
                <option key={g.id} value={g.id}>
                  {g.coverEmoji} {g.title} ({g.system}) — {g.maxPlayers}P
                </option>
              ))}
            </select>
          </div>

          {/* Room name */}
          <div>
            <label className="block text-xs font-semibold mb-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Room Name
            </label>
            <input
              type="text"
              value={roomName}
              onChange={(e) => setRoomName(e.target.value)}
              placeholder="Room name"
              maxLength={40}
              className="w-full px-3 py-2 rounded-xl text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
              }}
            />
          </div>

          {/* Public toggle */}
          <div className="flex items-center justify-between">
            <span className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Public Room
            </span>
            <button
              type="button"
              onClick={() => setIsPublic((v) => !v)}
              className="w-11 h-6 rounded-full transition-colors relative"
              style={{ backgroundColor: isPublic ? 'var(--color-oasis-accent)' : 'var(--color-oasis-surface)' }}
            >
              <span
                className="absolute top-0.5 w-5 h-5 rounded-full bg-white transition-transform"
                style={{ transform: isPublic ? 'translateX(22px)' : 'translateX(2px)' }}
              />
            </button>
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
            <button
              type="submit"
              className="flex-1 py-3 rounded-xl font-bold text-sm transition-transform hover:scale-[1.02] active:scale-[0.98]"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              Create Room
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
