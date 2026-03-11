import { Link } from 'react-router-dom';
import type { ApiGame } from '../types/api';

/** System cover gradients referenced via CSS variables defined in index.css. */
const SYSTEM_GRADIENT_VAR: Record<string, string> = {
  NES:  'var(--gradient-nes)',
  SNES: 'var(--gradient-snes)',
  GB:   'var(--gradient-gb)',
  GBC:  'var(--gradient-gbc)',
  GBA:  'var(--gradient-gba)',
  N64:  'var(--gradient-n64)',
  NDS:  'var(--gradient-nds)',
};

/** Badge colour config for well-known badge names. */
const BADGE_STYLES: Record<string, { bg: string; color: string }> = {
  'WFC Online':        { bg: '#0097e0', color: 'white' },
  'Touch Controls':    { bg: '#e67e22', color: 'white' },
  'Download Play':     { bg: '#6a3de8', color: 'white' },
  'Party Favorite':    { bg: 'var(--color-oasis-yellow)', color: '#1a1a2e' },
  'Great Online':      { bg: 'var(--color-oasis-green)',  color: '#0a0a0a' },
  'Best with Friends': { bg: 'var(--color-oasis-accent)', color: 'white' },
};

const BADGE_FALLBACK = { bg: 'rgba(255,255,255,0.10)', color: 'var(--color-oasis-text-muted)' };

const DS_COMPATIBILITY_BADGES = ['WFC Online', 'Touch Controls', 'Download Play'] as const;

function BadgePill({ label }: { label: string }) {
  const style = BADGE_STYLES[label] ?? BADGE_FALLBACK;
  return (
    <span
      className="text-[9px] font-black px-1.5 py-0.5 rounded-full"
      style={{ backgroundColor: style.bg, color: style.color }}
    >
      {label}
    </span>
  );
}

export function GameCard({ game }: { game: ApiGame }) {
  const is4PParty = game.maxPlayers >= 4 && game.tags.includes('Party');
  const isNds = game.system === 'NDS';
  const dsBadges = isNds
    ? game.badges.filter((b) => (DS_COMPATIBILITY_BADGES as readonly string[]).includes(b)).slice(0, 2)
    : [];
  const coverGradient = SYSTEM_GRADIENT_VAR[game.system] ?? 'var(--gradient-default)';

  return (
    <Link
      to={`/game/${game.id}`}
      className="n-card rounded-2xl overflow-hidden block"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      {/* Cover art area — coloured gradient per system */}
      <div
        className="w-full aspect-square flex items-center justify-center text-5xl relative"
        style={{ background: coverGradient }}
      >
        {game.coverEmoji}
        {is4PParty && (
          <span
            className="absolute top-2 right-2 text-[9px] font-black px-1.5 py-0.5 rounded-full shadow-sm"
            style={{ backgroundColor: 'var(--color-oasis-yellow)', color: '#1a1a2e' }}
          >
            Best 4P
          </span>
        )}
        {/* System badge pinned bottom-left */}
        <span
          className="absolute bottom-2 left-2 px-2 py-0.5 rounded-full text-[9px] font-black shadow-sm"
          style={{ backgroundColor: game.systemColor, color: 'white' }}
        >
          {game.system}
        </span>
      </div>

      {/* Card body */}
      <div className="p-3">
        <div className="flex items-center justify-between mb-1">
          <p className="text-sm font-black truncate flex-1">{game.title}</p>
          <span className="text-[10px] font-bold ml-2 flex-shrink-0" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {game.maxPlayers}P
          </span>
        </div>

        {/* DS compatibility badges */}
        {dsBadges.length > 0 && (
          <div className="flex flex-wrap gap-1 mb-1">
            {dsBadges.map((badge) => (
              <BadgePill key={badge} label={badge} />
            ))}
          </div>
        )}

        {/* Tag pills */}
        <div className="flex flex-wrap gap-1">
          {game.tags.slice(0, 3).map((tag) => (
            <span
              key={tag}
              className="text-[9px] font-bold px-1.5 py-0.5 rounded-full"
              style={{ backgroundColor: 'rgba(255,255,255,0.08)', color: 'var(--color-oasis-text-muted)' }}
            >
              {tag}
            </span>
          ))}
        </div>
      </div>
    </Link>
  );
}
