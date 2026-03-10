import { useState, useEffect } from 'react';
import { useParams, Link, useNavigate } from 'react-router-dom';
import { useGame } from '../lib/use-games';
import { HostRoomModal } from '../components/HostRoomModal';
import { useLobby } from '../context/LobbyContext';

/** System-specific quick-start hints shown on the game detail page. */
function getPartyHint(game: { system: string; maxPlayers: number; tags: string[] }): string | null {
  if (game.system === 'N64' && game.maxPlayers >= 4) {
    if (game.tags.includes('Party')) {
      return '🎉 Perfect for a full party of 4 — grab your friends and launch a room!';
    }
    return '👾 Best with 4 players — fill those slots for the full N64 experience.';
  }
  if (game.system === 'NDS' && game.maxPlayers >= 4) {
    if (game.tags.includes('Party')) {
      return '🎉 Party game for up to 4 — touch screen mini-games await!';
    }
    return '🎮 Best with 4 players — share your room code and race to fill the slots.';
  }
  if (game.system === 'NDS' && game.tags.includes('WFC')) {
    return '🌐 WFC-enabled — connects through Wiimmfi automatically. Just start a room!';
  }
  if (game.maxPlayers >= 4) {
    return `🎮 Supports up to ${game.maxPlayers} players — more is merrier!`;
  }
  return null;
}

/** Controller tip shown on N64 game detail pages. */
const N64_CONTROLLER_TIP =
  '🕹️ Controller tip: Use an Xbox or PlayStation controller — the left stick maps to the N64 analog stick automatically. A gamepad is strongly recommended for analog-heavy games.';

/** Dual-screen tip shown on NDS game detail pages. */
const NDS_DUAL_SCREEN_TIP =
  '📱 Dual-screen tip: melonDS shows the top screen above and the bottom (touch) screen below. Move your mouse to the bottom screen area and click to interact with touch controls. Use F11 to swap screens if needed.';

/** WFC tip shown on NDS games that support Pokémon Wi-Fi Connection. */
const NDS_WFC_TIP =
  '🌐 Wi-Fi Connection: This game uses Wiimmfi — a free replacement for Nintendo\'s discontinued online service. RetroOasis sets it up automatically when you host a room. No extra configuration needed!';

export function GameDetailsPage() {
  const { gameId } = useParams<{ gameId: string }>();
  const navigate = useNavigate();
  const { createRoom, currentRoom } = useLobby();
  const [showHost, setShowHost] = useState(false);

  const { data: game, loading, error } = useGame(gameId);

  // Navigate to lobby after room is created
  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  if (loading) {
    return (
      <div className="text-center py-12">
        <p style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>
      </div>
    );
  }

  if (error || !game) {
    return (
      <div className="text-center py-12">
        <p style={{ color: 'var(--color-oasis-text-muted)' }}>Game not found.</p>
        <Link to="/" className="underline text-sm" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ← Back to Home
        </Link>
      </div>
    );
  }

  const partyHint = getPartyHint(game);
  const isN64 = game.system === 'N64';
  const isNDS = game.system === 'NDS';
  const isNdsWfc = isNDS && game.tags.includes('WFC');

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
              {isN64 && (
                <span
                  className="px-2 py-0.5 rounded text-[10px] font-bold"
                  style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
                >
                  N64
                </span>
              )}
              {isNDS && (
                <span
                  className="px-2 py-0.5 rounded text-[10px] font-bold"
                  style={{ backgroundColor: '#E87722', color: 'white' }}
                >
                  Dual Screen
                </span>
              )}
              {isNdsWfc && (
                <span
                  className="px-2 py-0.5 rounded text-[10px] font-bold"
                  style={{ backgroundColor: '#2563eb', color: 'white' }}
                >
                  WFC Online
                </span>
              )}
            </div>
            <h1 className="text-2xl font-bold mb-2">{game.title}</h1>
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.description}
            </p>
          </div>
        </div>

        {/* Party hint banner */}
        {partyHint && (
          <div
            className="mt-4 px-4 py-3 rounded-xl text-sm font-semibold"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-accent-light)' }}
          >
            {partyHint}
          </div>
        )}

        {/* N64 analog stick note */}
        {isN64 && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            {N64_CONTROLLER_TIP}
          </div>
        )}

        {/* NDS dual-screen tip */}
        {isNDS && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            {NDS_DUAL_SCREEN_TIP}
          </div>
        )}

        {/* NDS WFC tip */}
        {isNdsWfc && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'rgba(37,99,235,0.15)', color: '#93c5fd' }}
          >
            {NDS_WFC_TIP}
          </div>
        )}

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
