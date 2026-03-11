import { Link } from 'react-router-dom';
import type { MockLobby } from '../data/mock-games';

export function LobbyCard({ lobby }: { lobby: MockLobby }) {
  const fillPct = Math.round((lobby.playerCount / lobby.maxPlayers) * 100);
  const isFull  = lobby.playerCount >= lobby.maxPlayers;

  return (
    <Link
      to={`/lobby/${lobby.id}`}
      className="n-card rounded-2xl p-4 block"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      {/* Header row */}
      <div className="flex items-start justify-between gap-2 mb-2">
        <h3 className="text-sm font-black truncate leading-tight">{lobby.name}</h3>
        <span
          className="px-2 py-0.5 rounded-full text-[9px] font-black flex-shrink-0"
          style={{ backgroundColor: lobby.systemColor, color: 'white' }}
        >
          {lobby.system}
        </span>
      </div>

      {/* Game title */}
      <p className="text-xs font-semibold mb-3 truncate" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {lobby.gameTitle}
      </p>

      {/* Footer row */}
      <div className="flex items-center justify-between">
        <span className="text-[11px] font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Host: <span style={{ color: 'var(--color-oasis-text)' }}>{lobby.host}</span>
        </span>
        <span
          className="text-xs font-black px-2 py-0.5 rounded-full"
          style={{
            backgroundColor: isFull ? 'rgba(230,0,18,0.15)' : 'rgba(0,209,112,0.15)',
            color: isFull ? 'var(--color-oasis-accent)' : 'var(--color-oasis-green)',
          }}
        >
          {lobby.playerCount}/{lobby.maxPlayers}
        </span>
      </div>

      {/* Capacity bar */}
      <div className="mt-2 h-1 rounded-full overflow-hidden" style={{ backgroundColor: 'rgba(255,255,255,0.07)' }}>
        <div
          className="h-full rounded-full transition-all"
          style={{
            width: `${fillPct}%`,
            backgroundColor: isFull ? 'var(--color-oasis-accent)' : 'var(--color-oasis-green)',
          }}
        />
      </div>
    </Link>
  );
}
