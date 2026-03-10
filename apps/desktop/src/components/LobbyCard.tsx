import { Link } from 'react-router-dom';
import type { MockLobby } from '../data/mock-games';

export function LobbyCard({ lobby }: { lobby: MockLobby }) {
  return (
    <Link
      to={`/lobby/${lobby.id}`}
      className="rounded-xl p-4 transition-transform hover:scale-[1.02] active:scale-[0.98] block"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      <div className="flex items-center justify-between mb-2">
        <h3 className="text-sm font-bold truncate">{lobby.name}</h3>
        <span
          className="px-1.5 py-0.5 rounded text-[9px] font-bold flex-shrink-0"
          style={{ backgroundColor: lobby.systemColor, color: 'white' }}
        >
          {lobby.system}
        </span>
      </div>
      <p className="text-xs mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {lobby.gameTitle}
      </p>
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Host: {lobby.host}
          </span>
        </div>
        <div className="flex items-center gap-1">
          <span className="text-xs font-bold" style={{ color: 'var(--color-oasis-green)' }}>
            {lobby.playerCount}/{lobby.maxPlayers}
          </span>
          <span className="text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>players</span>
        </div>
      </div>
    </Link>
  );
}
