import { Link } from 'react-router-dom';
import type { ApiGame } from '../types/api';

/** System cover gradients referenced via CSS variables defined in index.css. */
const SYSTEM_GRADIENT_VAR: Record<string, string> = {
  // Nintendo
  NES:     'var(--gradient-nes)',
  SNES:    'var(--gradient-snes)',
  GB:      'var(--gradient-gb)',
  GBC:     'var(--gradient-gbc)',
  GBA:     'var(--gradient-gba)',
  N64:     'var(--gradient-n64)',
  NDS:     'var(--gradient-nds)',
  GC:      'var(--gradient-gc)',
  Wii:     'linear-gradient(135deg, #6b7280 0%, #9ca3af 50%, #6b7280 100%)',
  'Wii U': 'linear-gradient(135deg, #004d63 0%, #009AC7 100%)',
  '3DS':   'var(--gradient-3ds)',
  // Sega
  Genesis:   'linear-gradient(135deg, #003a73 0%, #0066CC 100%)',
  SMS:       'linear-gradient(135deg, #001a66 0%, #003399 100%)',
  Dreamcast: 'linear-gradient(135deg, #cc4400 0%, #FF6600 100%)',
  // Sony
  PSX: 'linear-gradient(135deg, #404040 0%, #808080 100%)',
  PS2: 'linear-gradient(135deg, #00254f 0%, #00439C 100%)',
  PSP: 'linear-gradient(135deg, #0a0a0a 0%, #2a2a3a 100%)',
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

export function GameCard({ game, showAchievementBadge = false }: { game: ApiGame; showAchievementBadge?: boolean }) {
  const is4PParty = game.maxPlayers >= 4 && game.tags.includes('Party');
  const isNds = game.system === 'NDS';
  const dsBadges = isNds
    ? game.badges.filter((b) => (DS_COMPATIBILITY_BADGES as readonly string[]).includes(b)).slice(0, 2)
    : [];
  const coverGradient = SYSTEM_GRADIENT_VAR[game.system] ?? 'var(--gradient-default)';

  return (
    <Link
      to={`/game/${game.id}`}
      className="n-card rounded-2xl overflow-hidden block shimmer-overlay"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      {/* Cover art area — gradient per system + shine overlay */}
      <div
        className="w-full aspect-square flex items-center justify-center text-5xl relative overflow-hidden"
        style={{ background: coverGradient }}
      >
        {/* Subtle inner glow */}
        <div
          className="absolute inset-0 pointer-events-none"
          style={{ background: 'radial-gradient(ellipse at 30% 30%, rgba(255,255,255,0.12) 0%, transparent 60%)' }}
        />
        <span className="relative z-10 drop-shadow-lg">{game.coverEmoji}</span>
        {is4PParty && (
          <span
            className="absolute top-2 right-2 text-[9px] font-black px-1.5 py-0.5 rounded-full shadow-sm z-10"
            style={{ backgroundColor: 'var(--color-oasis-yellow)', color: '#1a1a2e' }}
          >
            4P
          </span>
        )}
        {showAchievementBadge && (
          <span
            className="absolute top-2 left-2 text-[9px] font-black px-1.5 py-0.5 rounded-full shadow-sm z-10"
            style={{ backgroundColor: 'rgba(234,179,8,0.85)', color: '#1a1a2e' }}
            title="Achievements available"
          >
            🏅
          </span>
        )}
        {/* System badge pinned bottom-left */}
        <span
          className="absolute bottom-2 left-2 px-2 py-0.5 rounded-full text-[9px] font-black shadow-sm z-10"
          style={{
            backgroundColor: 'var(--color-glass-dark)',
            backdropFilter: 'blur(4px)',
            border: '1px solid rgba(255,255,255,0.15)',
            color: 'white',
          }}
        >
          {game.system}
        </span>
      </div>

      {/* Card body */}
      <div className="p-3">
        <div className="flex items-center justify-between mb-1.5">
          <p className="text-sm font-black truncate flex-1 leading-tight">{game.title}</p>
          <span
            className="text-[9px] font-bold ml-2 flex-shrink-0 px-1.5 py-0.5 rounded-full"
            style={{
              backgroundColor: 'rgba(255,255,255,0.06)',
              color: 'var(--color-oasis-text-muted)',
            }}
          >
            {game.maxPlayers}P
          </span>
        </div>

        {/* DS compatibility badges */}
        {dsBadges.length > 0 && (
          <div className="flex flex-wrap gap-1 mb-1.5">
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
              style={{ backgroundColor: 'rgba(255,255,255,0.06)', color: 'var(--color-oasis-text-muted)' }}
            >
              {tag}
            </span>
          ))}
        </div>
      </div>
    </Link>
  );
}
