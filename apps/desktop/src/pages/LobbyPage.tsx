import { useParams, Link } from 'react-router-dom';

const MOCK_LOBBY_DETAIL = {
  id: 'lobby-1',
  name: "Mario Kart Party!",
  host: 'Player2',
  gameTitle: 'Mario Kart 64',
  system: 'N64',
  roomCode: 'MK64-ABCD',
  players: [
    { name: 'Player2', ready: true, isHost: true, slot: 1 },
    { name: 'You', ready: false, isHost: false, slot: 2 },
  ],
  maxPlayers: 4,
};

export function LobbyPage() {
  const { lobbyId } = useParams<{ lobbyId: string }>();

  return (
    <div className="max-w-2xl">
      <Link to="/" className="text-sm mb-4 inline-block" style={{ color: 'var(--color-oasis-text-muted)' }}>
        ← Leave Room
      </Link>

      <div className="rounded-2xl p-6" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        <div className="flex items-center justify-between mb-4">
          <div>
            <h1 className="text-xl font-bold">{MOCK_LOBBY_DETAIL.name}</h1>
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {MOCK_LOBBY_DETAIL.gameTitle} · {MOCK_LOBBY_DETAIL.system}
            </p>
          </div>
          <div className="text-right">
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>Room Code</p>
            <p className="text-sm font-mono font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
              {MOCK_LOBBY_DETAIL.roomCode}
            </p>
          </div>
        </div>

        {/* Player list */}
        <div className="space-y-2 mb-6">
          <p className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Players ({MOCK_LOBBY_DETAIL.players.length}/{MOCK_LOBBY_DETAIL.maxPlayers})
          </p>
          {MOCK_LOBBY_DETAIL.players.map((player) => (
            <div
              key={player.name}
              className="flex items-center justify-between p-3 rounded-xl"
              style={{ backgroundColor: 'var(--color-oasis-surface)' }}
            >
              <div className="flex items-center gap-3">
                <div className="w-8 h-8 rounded-full flex items-center justify-center text-sm font-bold"
                  style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}>
                  P{player.slot}
                </div>
                <div>
                  <p className="text-sm font-medium">
                    {player.name}
                    {player.isHost && (
                      <span className="ml-2 text-[10px] px-1.5 py-0.5 rounded" style={{ backgroundColor: 'var(--color-oasis-yellow)', color: '#1a1025' }}>
                        HOST
                      </span>
                    )}
                  </p>
                </div>
              </div>
              <span
                className="text-xs font-semibold px-2 py-1 rounded"
                style={{
                  backgroundColor: player.ready ? 'var(--color-oasis-green)' : 'var(--color-oasis-surface)',
                  color: player.ready ? '#1a1025' : 'var(--color-oasis-text-muted)',
                }}
              >
                {player.ready ? '✓ Ready' : 'Not Ready'}
              </span>
            </div>
          ))}

          {/* Empty slots */}
          {Array.from({ length: MOCK_LOBBY_DETAIL.maxPlayers - MOCK_LOBBY_DETAIL.players.length }).map((_, i) => (
            <div
              key={`empty-${i}`}
              className="flex items-center justify-center p-3 rounded-xl border border-dashed"
              style={{ borderColor: 'var(--color-oasis-text-muted)', opacity: 0.3 }}
            >
              <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Open Slot
              </p>
            </div>
          ))}
        </div>

        {/* Actions */}
        <div className="flex gap-3">
          <button
            className="flex-1 py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{ backgroundColor: 'var(--color-oasis-green)', color: '#1a1025' }}
          >
            ✓ Ready Up
          </button>
          <button
            className="py-3 px-6 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            Leave
          </button>
        </div>

        {/* Lobby ID for debugging */}
        <p className="text-[10px] text-center mt-4" style={{ color: 'var(--color-oasis-text-muted)', opacity: 0.5 }}>
          Lobby: {lobbyId}
        </p>
      </div>
    </div>
  );
}
