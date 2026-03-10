import { Link } from 'react-router-dom';
import type { ApiGame } from '../types/api';

/** Badge colour config for well-known badge names. */
const BADGE_STYLES: Record<string, { bg: string; color: string }> = {
  'WFC Online':     { bg: '#3b82f6', color: 'white' },
  'Touch Controls': { bg: '#E87722', color: 'white' },
  'Download Play':  { bg: '#7c5cbf', color: 'white' },
  'Party Favorite': { bg: 'var(--color-oasis-yellow)', color: '#1a1025' },
  'Great Online':   { bg: 'var(--color-oasis-green)', color: '#1a1025' },
  'Best with Friends': { bg: 'var(--color-oasis-accent)', color: 'white' },
};

/** DS-specific compatibility badges shown on game cards. */
const DS_COMPATIBILITY_BADGES = ['WFC Online', 'Touch Controls', 'Download Play'] as const;

function BadgePill({ label }: { label: string }) {
  const style = BADGE_STYLES[label] ?? { bg: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' };
  return (
    <span
      className="text-[9px] font-bold px-1.5 py-0.5 rounded-full"
      style={{ backgroundColor: style.bg, color: style.color }}
    >
      {label}
    </span>
  );
}

export function GameCard({ game }: { game: ApiGame }) {
  const is4PParty = game.maxPlayers >= 4 && game.tags.includes('Party');
  const isNds = game.system === 'NDS';
  // DS-specific compatibility badges shown on the card (max 2 to keep it compact)
  const dsBadges = isNds ? game.badges.filter((b) => (DS_COMPATIBILITY_BADGES as readonly string[]).includes(b)).slice(0, 2) : [];

  return (
    <Link
      to={`/game/${game.id}`}
      className="rounded-xl p-3 transition-transform hover:scale-[1.03] active:scale-[0.98] block"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      <div
        className="w-full aspect-square rounded-lg flex items-center justify-center text-4xl mb-2 relative"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
      >
        {game.coverEmoji}
        {is4PParty && (
          <span
            className="absolute bottom-1 right-1 text-[9px] font-bold px-1.5 py-0.5 rounded-full"
            style={{ backgroundColor: 'var(--color-oasis-yellow)', color: '#1a1025' }}
          >
            Best 4P
          </span>
        )}
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
      {/* DS-specific compatibility badges */}
      {dsBadges.length > 0 && (
        <div className="flex flex-wrap gap-1 mt-1">
          {dsBadges.map((badge) => (
            <BadgePill key={badge} label={badge} />
          ))}
        </div>
      )}
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
