import { useState, useEffect, useCallback, useMemo } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

// NGP = monochrome, NGPC = color; unified page with a system toggle
const NGP_COLOR = '#C8A000';
const NGP_COLOR_DARK = '#805A00';
const NGPC_COLOR = '#E8B800';
const NGPC_COLOR_DARK = '#A07000';

type ActiveTab = 'lobby' | 'leaderboard';
type SystemMode = 'ngp' | 'ngpc';
type GenreFilter = 'all' | 'Fighting' | 'Action' | 'Sports' | 'Puzzle' | 'RPG' | 'Shoot-em-up';

interface NGPGame {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  singleOnly?: boolean;
  competitive?: boolean;
  system: 'ngp' | 'ngpc' | 'both';
  year?: number;
}

interface NGPRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const NGP_GAMES: NGPGame[] = [
  // Fighting — the heart of the NGP library
  { id: 'ngp-king-of-fighters-r2',         title: 'The King of Fighters R-2',    genre: 'Fighting',    genreEmoji: '👊', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  { id: 'ngp-snk-vs-capcom-match-of-millennium', title: 'SNK vs. Capcom: Match of the Millennium', genre: 'Fighting', genreEmoji: '⚔️', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  { id: 'ngp-king-of-fighters-r1',         title: 'The King of Fighters R-1',    genre: 'Fighting',    genreEmoji: '👊', maxPlayers: 2, competitive: true, system: 'ngp',  year: 1998 },
  { id: 'ngp-fatal-fury-first-contact',    title: 'Fatal Fury: First Contact',   genre: 'Fighting',    genreEmoji: '💥', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  { id: 'ngp-samurai-shodown-2',           title: 'Samurai Shodown! 2',          genre: 'Fighting',    genreEmoji: '🗡️', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  // Action
  { id: 'ngp-sonic-pocket-adventure',      title: 'Sonic Pocket Adventure',      genre: 'Action',      genreEmoji: '💨', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  { id: 'ngp-metal-slug-1st-mission',      title: 'Metal Slug: 1st Mission',     genre: 'Action',      genreEmoji: '🔫', maxPlayers: 1, singleOnly: true,  system: 'ngp',  year: 1999 },
  { id: 'ngp-metal-slug-2nd-mission',      title: 'Metal Slug: 2nd Mission',     genre: 'Action',      genreEmoji: '🔫', maxPlayers: 1, singleOnly: true,  system: 'ngpc', year: 2000 },
  { id: 'ngp-biomotor-unitron',            title: 'BioMotor Unitron',            genre: 'RPG',         genreEmoji: '🤖', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  // Shoot-em-up
  { id: 'ngp-dive-alert',                  title: 'Dive Alert',                  genre: 'Shoot-em-up', genreEmoji: '🌊', maxPlayers: 1, singleOnly: true,  system: 'ngpc', year: 1999 },
  // Sports
  { id: 'ngp-baseball-stars-color',        title: 'Baseball Stars Color',        genre: 'Sports',      genreEmoji: '⚾', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  { id: 'ngp-pocket-tennis-color',         title: 'Pocket Tennis Color',         genre: 'Sports',      genreEmoji: '🎾', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  { id: 'ngp-pocket-golf',                 title: 'Pocket Golf',                 genre: 'Sports',      genreEmoji: '⛳', maxPlayers: 2, competitive: true, system: 'ngp',  year: 1998 },
  // Puzzle
  { id: 'ngp-puzzle-link',                 title: 'Puzzle Link',                 genre: 'Puzzle',      genreEmoji: '🧩', maxPlayers: 2, competitive: true, system: 'ngp',  year: 1998 },
  { id: 'ngp-bust-a-move-pocket',          title: 'Bust-A-Move Pocket',          genre: 'Puzzle',      genreEmoji: '🫧', maxPlayers: 2, competitive: true, system: 'ngpc', year: 1999 },
  // RPG
  { id: 'ngp-faselei',                     title: 'Faselei!',                    genre: 'RPG',         genreEmoji: '🤖', maxPlayers: 1, singleOnly: true,  system: 'ngpc', year: 1999 },
];

const ALL_GENRES: GenreFilter[] = ['all', 'Fighting', 'Action', 'Shoot-em-up', 'RPG', 'Sports', 'Puzzle'];

// ---------------------------------------------------------------------------
// API helpers
// ---------------------------------------------------------------------------

async function apiFetch<T>(path: string, opts?: RequestInit): Promise<T> {
  const res = await fetch(`${API}${path}`, {
    headers: { 'Content-Type': 'application/json' },
    ...opts,
  });
  if (!res.ok) {
    const err = await res.json().catch(() => ({ error: 'Unknown error' }));
    throw new Error((err as { error?: string }).error ?? 'Request failed');
  }
  return res.json() as Promise<T>;
}

// ---------------------------------------------------------------------------
// Rank badge
// ---------------------------------------------------------------------------

function RankBadge({ rank }: { rank: number }) {
  const medals: Record<number, string> = { 1: '🥇', 2: '🥈', 3: '🥉' };
  return (
    <span className="text-lg w-8 text-center" title={`Rank #${rank}`}>
      {medals[rank] ?? `#${rank}`}
    </span>
  );
}

// ---------------------------------------------------------------------------
// Info banner
// ---------------------------------------------------------------------------

function NGPBanner({ mode }: { mode: SystemMode }) {
  const color = mode === 'ngpc' ? NGPC_COLOR : NGP_COLOR;
  const colorMid = mode === 'ngpc' ? 'rgba(232,184,0,0.18)' : 'rgba(200,160,0,0.18)';
  const ext = mode === 'ngpc' ? '.ngc' : '.ngp';

  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: mode === 'ngpc' ? 'rgba(232,184,0,0.08)' : 'rgba(200,160,0,0.08)', border: `1px solid ${colorMid}` }}
    >
      <span className="text-xl mt-0.5">{mode === 'ngpc' ? '🌈' : '⬛'}</span>
      <div className="space-y-1">
        <p className="text-sm font-bold" style={{ color }}>
          SNK Neo Geo Pocket {mode === 'ngpc' ? 'Color' : ''} · Relay Netplay via RetroArch + Beetle NeoPop
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          All sessions use RetroOasis TCP relay netplay — no port forwarding required. ROM files{' '}
          <code className="mx-0.5 px-1 rounded" style={{ background: colorMid, color }}>
            {ext}
          </code>{' '}
          are auto-detected from your library. Recommended core:{' '}
          <code className="ml-0.5 px-1 rounded" style={{ background: colorMid, color }}>
            mednafen_ngp_libretro.so
          </code>
        </p>
        <p className="text-xs font-semibold" style={{ color: '#fbbf24' }}>
          ℹ️ Link cable multiplayer supported — head-to-head via relay for fighting and sports games!
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Genre filter bar
// ---------------------------------------------------------------------------

function GenreFilterBar({
  active,
  onChange,
  counts,
  mode,
}: {
  active: GenreFilter;
  onChange: (g: GenreFilter) => void;
  counts: Record<string, number>;
  mode: SystemMode;
}) {
  const color = mode === 'ngpc' ? NGPC_COLOR : NGP_COLOR;
  const colorMid = mode === 'ngpc' ? 'rgba(232,184,0,0.18)' : 'rgba(200,160,0,0.18)';

  return (
    <div className="flex flex-wrap gap-2">
      {ALL_GENRES.map((g) => {
        const isActive = active === g;
        const count = g === 'all' ? counts['__total'] ?? 0 : (counts[g] ?? 0);
        return (
          <button
            key={g}
            onClick={() => onChange(g)}
            className="px-3 py-1 rounded-full text-xs font-semibold transition-all"
            style={
              isActive
                ? { background: color, color: '#0a0a0a' }
                : { background: colorMid, color, border: `1px solid ${colorMid}` }
            }
          >
            {g === 'all' ? 'All' : g} {count > 0 && <span className="opacity-70">({count})</span>}
          </button>
        );
      })}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function NGPGameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
  mode,
}: {
  game: NGPGame;
  onHost: (game: NGPGame) => void;
  onQuickMatch: (game: NGPGame) => void;
  openRooms: number;
  mode: SystemMode;
}) {
  const color = mode === 'ngpc' ? NGPC_COLOR : NGP_COLOR;
  const colorDark = mode === 'ngpc' ? NGPC_COLOR_DARK : NGP_COLOR_DARK;
  const colorMid = mode === 'ngpc' ? 'rgba(232,184,0,0.18)' : 'rgba(200,160,0,0.18)';

  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: mode === 'ngpc'
          ? 'linear-gradient(135deg, rgba(232,184,0,0.12), rgba(160,112,0,0.06))'
          : 'linear-gradient(135deg, rgba(200,160,0,0.12), rgba(128,90,0,0.06))',
        border: `1px solid ${colorMid}`,
      }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2 min-w-0">
          <span className="text-2xl shrink-0">{game.genreEmoji}</span>
          <div className="min-w-0">
            <p className="text-sm font-bold text-white leading-tight truncate">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.genre} · {game.singleOnly ? '1P' : `Up to ${game.maxPlayers}P`}
              {game.year && ` · ${game.year}`}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1 shrink-0">
          {game.singleOnly && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(148,163,184,0.1)', color: '#94a3b8', border: '1px solid rgba(148,163,184,0.25)' }}
            >
              Single Player
            </span>
          )}
          {game.competitive && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(251,146,60,0.15)', color: '#fb923c', border: '1px solid rgba(251,146,60,0.3)' }}
            >
              Competitive
            </span>
          )}
          {openRooms > 0 && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(96,165,250,0.1)', color: '#60a5fa', border: '1px solid rgba(96,165,250,0.25)' }}
            >
              {openRooms} open
            </span>
          )}
        </div>
      </div>

      <div className="flex gap-2 mt-auto">
        <button
          onClick={() => onQuickMatch(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: `linear-gradient(90deg, ${color}, ${colorDark})`, color: '#0a0a0a' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: colorMid, border: `1px solid ${colorMid}`, color }}
        >
          🎮 Host Room
        </button>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel({ mode }: { mode: SystemMode }) {
  const [rankings, setRankings] = useState<NGPRanking[]>([]);
  const [loading, setLoading] = useState(true);
  const color = mode === 'ngpc' ? NGPC_COLOR : NGP_COLOR;
  const colorMid = mode === 'ngpc' ? 'rgba(232,184,0,0.18)' : 'rgba(200,160,0,0.18)';

  useEffect(() => {
    apiFetch<{ leaderboard: NGPRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">{mode === 'ngpc' ? '🌈' : '⬛'}</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">
          Play some Neo Geo Pocket {mode === 'ngpc' ? 'Color' : ''} sessions to appear here!
        </p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: mode === 'ngpc' ? 'rgba(232,184,0,0.08)' : 'rgba(200,160,0,0.08)', border: `1px solid ${colorMid}` }}
        >
          <RankBadge rank={i + 1} />
          <span className="flex-1 text-sm font-semibold" style={{ color }}>{r.displayName}</span>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {r.sessions} session{r.sessions !== 1 ? 's' : ''}
          </span>
        </div>
      ))}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({ mode }: { mode: SystemMode }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<NGPGame | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');
  const [genreFilter, setGenreFilter] = useState<GenreFilter>('all');

  const systemGames = useMemo(
    () => NGP_GAMES.filter((g) => g.system === mode || g.system === 'both'),
    [mode],
  );

  const genreCounts = useMemo(() => {
    const counts: Record<string, number> = { __total: systemGames.length };
    for (const g of systemGames) {
      counts[g.genre] = (counts[g.genre] ?? 0) + 1;
    }
    return counts;
  }, [systemGames]);

  const filteredGames = useMemo(
    () => (genreFilter === 'all' ? systemGames : systemGames.filter((g) => g.genre === genreFilter)),
    [systemGames, genreFilter],
  );

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const totalOpenRooms = useMemo(
    () => publicRooms.filter((r) => r.status === 'waiting' && systemGames.some((g) => g.id === r.gameId)).length,
    [publicRooms, systemGames],
  );

  const handleQuickMatch = useCallback(
    async (game: NGPGame) => {
      const open = publicRooms.find((r) => r.gameId === game.id && r.status === 'waiting');
      if (open) {
        setJoinCode(open.roomCode);
        setShowJoin(true);
      } else {
        setHostGame(game);
        setNotification(`No open ${game.title} rooms — hosting one for you!`);
        setTimeout(() => setNotification(''), 3000);
      }
    },
    [publicRooms],
  );

  const color = mode === 'ngpc' ? NGPC_COLOR : NGP_COLOR;
  const colorMid = mode === 'ngpc' ? 'rgba(232,184,0,0.18)' : 'rgba(200,160,0,0.18)';

  return (
    <div className="space-y-5">
      <NGPBanner mode={mode} />

      {/* Stats row */}
      <div className="flex gap-3 flex-wrap">
        {[
          { label: 'Games', value: systemGames.length },
          { label: 'Open Rooms', value: totalOpenRooms },
          { label: 'Players Online', value: publicRooms.filter((r) => r.status === 'waiting').reduce((n, r) => n + (r.players?.length ?? 0), 0) },
        ].map(({ label, value }) => (
          <div
            key={label}
            className="flex-1 min-w-[80px] rounded-xl px-3 py-2 text-center"
            style={{ background: mode === 'ngpc' ? 'rgba(232,184,0,0.08)' : 'rgba(200,160,0,0.08)', border: `1px solid ${colorMid}` }}
          >
            <p className="text-lg font-black" style={{ color }}>{value}</p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</p>
          </div>
        ))}
      </div>

      {/* Genre filter */}
      <GenreFilterBar active={genreFilter} onChange={setGenreFilter} counts={genreCounts} mode={mode} />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: mode === 'ngpc' ? 'rgba(232,184,0,0.1)' : 'rgba(200,160,0,0.1)', color, border: `1px solid ${colorMid}` }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {filteredGames.map((game) => (
          <NGPGameCard
            key={game.id}
            game={game}
            onHost={setHostGame}
            onQuickMatch={handleQuickMatch}
            openRooms={openRoomsFor(game.id)}
            mode={mode}
          />
        ))}
      </div>

      {filteredGames.length === 0 && (
        <div className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No games found for this genre filter.
        </div>
      )}

      {!displayName && (
        <p className="text-center text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Set a display name in Settings to host or join rooms.
        </p>
      )}

      {hostGame && (
        <HostRoomModal
          preselectedGameId={hostGame.id}
          onConfirm={(payload, dn) => { createRoom(payload, dn); setHostGame(null); }}
          onClose={() => setHostGame(null)}
        />
      )}
      {showJoin && (
        <JoinRoomModal
          onConfirm={(code, dn) => { joinByCode(code, dn); setShowJoin(false); }}
          onClose={() => setShowJoin(false)}
          initialCode={joinCode}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// NGPPage (combined NGP + NGPC)
// ---------------------------------------------------------------------------

export default function NGPPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [systemMode, setSystemMode] = useState<SystemMode>('ngpc');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
  ];

  const color = systemMode === 'ngpc' ? NGPC_COLOR : NGP_COLOR;
  const colorMid = systemMode === 'ngpc' ? 'rgba(232,184,0,0.18)' : 'rgba(200,160,0,0.18)';

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: `linear-gradient(135deg, rgba(232,184,0,0.14) 0%, rgba(232,184,0,0.04) 60%, transparent 100%)`, border: `1px solid ${colorMid}` }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{
            background: systemMode === 'ngpc'
              ? `linear-gradient(135deg, ${NGPC_COLOR}, ${NGPC_COLOR_DARK})`
              : `linear-gradient(135deg, ${NGP_COLOR}, ${NGP_COLOR_DARK})`,
          }}
        >
          {systemMode === 'ngpc' ? '🌈' : '⬛'}
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Neo Geo Pocket</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            SNK · Online relay via RetroArch + Beetle NeoPop · Link cable multiplayer
          </p>
        </div>
        {/* System mode toggle */}
        <div className="flex rounded-xl overflow-hidden shrink-0" style={{ border: `1px solid ${colorMid}` }}>
          {(['ngp', 'ngpc'] as SystemMode[]).map((m) => (
            <button
              key={m}
              onClick={() => setSystemMode(m)}
              className="px-3 py-1.5 text-xs font-bold transition-all"
              style={
                systemMode === m
                  ? { background: color, color: '#0a0a0a' }
                  : { color: 'var(--color-oasis-text-muted)' }
              }
            >
              {m === 'ngp' ? 'NGP' : 'NGPC'}
            </button>
          ))}
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: colorMid }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: systemMode === 'ngpc' ? 'rgba(232,184,0,0.15)' : 'rgba(200,160,0,0.15)', color, borderBottom: `2px solid ${color}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel mode={systemMode} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel mode={systemMode} />}
    </div>
  );
}
