import { useState, useEffect } from 'react';
import { useParams, Link, useNavigate } from 'react-router-dom';
import { MOCK_GAMES } from '../data/mock-games';
import { HostRoomModal } from '../components/HostRoomModal';
import { useLobby } from '../context/LobbyContext';

export function GameDetailsPage() {
  const { gameId } = useParams<{ gameId: string }>();
  const navigate = useNavigate();
  const { createRoom, currentRoom } = useLobby();
  const [showHost, setShowHost] = useState(false);

  const game = MOCK_GAMES.find((g) => g.id === gameId);

  // Navigate to lobby after room is created
  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  if (!game) {
    return (
      <div className="text-center py-12">
        <p style={{ color: 'var(--color-oasis-text-muted)' }}>Game not found.</p>
        <Link to="/" className="underline text-sm" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ← Back to Home
        </Link>
      </div>
    );
  }

  return (
    <div className="max-w-3xl">
      <Link to="/" className="text-sm mb-4 inline-block" style={{ color: 'var(--color-oasis-text-muted)' }}>
        ← Back
      </Link>

      <div className="rounded-2xl p-6" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        {/* Header */}
        <div className="flex items-start gap-6">
          <div
            className="w-28 h-28 rounded-xl flex items-center justify-center text-5xl flex-shrink-0"
            style={{ backgroundColor: 'var(--color-oasis-surface)' }}
          >
            {game.coverEmoji}
          </div>
          <div className="flex-1">
            <div className="flex items-center gap-2 mb-1">
              <span
                className="px-2 py-0.5 rounded text-[10px] font-bold"
                style={{ backgroundColor: game.systemColor, color: 'white' }}
              >
                {game.system}
              </span>
              <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Up to {game.maxPlayers} players
              </span>
            </div>
            <h1 className="text-2xl font-bold mb-2">{game.title}</h1>
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.description}
            </p>
          </div>
        </div>

        {/* Tags */}
        <div className="flex flex-wrap gap-2 mt-4">
          {game.tags.map((tag) => (
            <span
              key={tag}
              className="px-2.5 py-1 rounded-full text-xs font-semibold"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-accent-light)' }}
            >
              {tag}
            </span>
          ))}
        </div>

        {/* Badges */}
        <div className="flex flex-wrap gap-2 mt-3">
          {game.badges.map((badge) => (
            <span
              key={badge}
              className="px-2.5 py-1 rounded-full text-xs font-semibold"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              {badge}
            </span>
          ))}
        </div>

        {/* Actions */}
        <div className="flex gap-3 mt-6">
          <button
            onClick={() => setShowHost(true)}
            className="flex-1 py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          >
            🎮 Host Lobby
          </button>
          <button
            className="flex-1 py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text)' }}
          >
            👥 Invite Friends
          </button>
        </div>
      </div>

      {showHost && (
        <HostRoomModal
          preselectedGameId={game.id}
          onConfirm={(payload, displayName) => {
            createRoom(payload, displayName);
            setShowHost(false);
          }}
          onClose={() => setShowHost(false)}
        />
      )}
    </div>
  );
}
