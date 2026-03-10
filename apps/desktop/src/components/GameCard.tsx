import { Link } from 'react-router-dom';
import type { MockGame } from '../data/mock-games';

export function GameCard({ game }: { game: MockGame }) {
  return (
    <Link
      to={`/game/${game.id}`}
      className="rounded-xl p-3 transition-transform hover:scale-[1.03] active:scale-[0.98] block"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      <div
        className="w-full aspect-square rounded-lg flex items-center justify-center text-4xl mb-2"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
      >
        {game.coverEmoji}
      </div>
      <div className="flex items-center gap-1.5 mb-1">
        <span
          className="px-1.5 py-0.5 rounded text-[9px] font-bold"
          style={{ backgroundColor: game.systemColor, color: 'white' }}
        >
          {game.system}
        </span>
        <span className="text-[10px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {game.maxPlayers}P
        </span>
      </div>
      <p className="text-sm font-semibold truncate">{game.title}</p>
      <div className="flex flex-wrap gap-1 mt-1">
        {game.tags.slice(0, 3).map((tag) => (
          <span
            key={tag}
            className="text-[9px] px-1.5 py-0.5 rounded-full"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            {tag}
          </span>
        ))}
      </div>
    </Link>
  );
}
