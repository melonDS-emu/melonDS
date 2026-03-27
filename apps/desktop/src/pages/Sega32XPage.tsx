import { useState, useEffect, useCallback, useMemo } from 'react';
import { Link } from 'react-router-dom';
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

const SYSTEM_COLOR = '#CC0000';
const SYSTEM_COLOR_DARK = '#880000';
const SYSTEM_COLOR_MID = 'rgba(204,0,0,0.18)';

type ActiveTab = 'lobby' | 'leaderboard';
type GenreFilter = 'all' | 'Platformer' | 'Action' | 'Shoot-em-up' | 'Fighting' | 'Sports' | 'Racing' | 'Beat-em-up';

interface Game32X {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  singleOnly?: boolean;
  competitive?: boolean;
  coopOnly?: boolean;
  year?: number;
}

interface Ranking32X {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const GAMES_32X: Game32X[] = [
  // Platformer
  { id: '32x-knuckles-chaotix',           title: "Knuckles' Chaotix",         genre: 'Platformer',  genreEmoji: '💎', maxPlayers: 2, coopOnly: true,    year: 1995 },
  { id: '32x-sonic-mars',                 title: 'Sonic Mars (Unreleased)',    genre: 'Platformer',  genreEmoji: '💨', maxPlayers: 1, singleOnly: true,  year: 1995 },
  // Action
  { id: '32x-doom',                       title: 'DOOM',                      genre: 'Action',      genreEmoji: '🔫', maxPlayers: 1, singleOnly: true,  year: 1994 },
  { id: '32x-shadow-squadron',            title: 'Shadow Squadron',            genre: 'Action',      genreEmoji: '🚀', maxPlayers: 1, singleOnly: true,  year: 1994 },
  { id: '32x-star-wars-arcade',           title: 'Star Wars Arcade',          genre: 'Action',      genreEmoji: '⭐', maxPlayers: 2, competitive: true, year: 1994 },
  { id: '32x-bc-racers',                  title: 'BC Racers',                 genre: 'Action',      genreEmoji: '🦕', maxPlayers: 2, competitive: true, year: 1994 },
  // Shoot-em-up
  { id: '32x-space-harrier',              title: 'Space Harrier',             genre: 'Shoot-em-up', genreEmoji: '🌌', maxPlayers: 1, singleOnly: true,  year: 1994 },
  { id: '32x-after-burner-complete',      title: 'After Burner Complete',     genre: 'Shoot-em-up', genreEmoji: '✈️', maxPlayers: 1, singleOnly: true,  year: 1994 },
  { id: '32x-kolibri',                    title: 'Kolibri',                   genre: 'Shoot-em-up', genreEmoji: '🐦', maxPlayers: 2, competitive: true, year: 1995 },
  // Fighting
  { id: '32x-mortal-kombat-ii',           title: 'Mortal Kombat II',          genre: 'Fighting',    genreEmoji: '🩸', maxPlayers: 2, competitive: true, year: 1994 },
  { id: '32x-WWF-raw',                    title: 'WWF Raw',                   genre: 'Fighting',    genreEmoji: '🥊', maxPlayers: 4, competitive: true, year: 1994 },
  // Beat-em-up
  { id: '32x-golden-axe-the-duel',        title: 'Golden Axe: The Duel',      genre: 'Beat-em-up',  genreEmoji: '⚔️', maxPlayers: 2, competitive: true, year: 1995 },
  // Sports
  { id: '32x-nba-jam-tournament-edition', title: 'NBA Jam Tournament Edition', genre: 'Sports',     genreEmoji: '🏀', maxPlayers: 4, competitive: true, year: 1994 },
  { id: '32x-golf-magazine',              title: 'Golf Magazine: 36 Great Holes', genre: 'Sports',  genreEmoji: '⛳', maxPlayers: 4, competitive: true, year: 1994 },
  // Racing
  { id: '32x-virtua-racing-deluxe',       title: 'Virtua Racing Deluxe',      genre: 'Racing',      genreEmoji: '🏎️', maxPlayers: 2, competitive: true, year: 1994 },
  { id: '32x-motocross-championship',     title: 'Motocross Championship',    genre: 'Racing',      genreEmoji: '🏍️', maxPlayers: 1, singleOnly: true,  year: 1994 },
];

const ALL_GENRES: GenreFilter[] = ['all', 'Platformer', 'Action', 'Shoot-em-up', 'Fighting', 'Beat-em-up', 'Sports', 'Racing'];

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

function Banner32X() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(204,0,0,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
    >
      <span className="text-xl mt-0.5">🔴</span>
      <div className="space-y-1">
        <p className="text-sm font-bold" style={{ color: '#fca5a5' }}>
          Sega 32X · Relay Netplay via RetroArch + PicoDrive
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          All sessions use RetroOasis TCP relay netplay — no port forwarding required. ROM files{' '}
          <code className="mx-0.5 px-1 rounded" style={{ background: SYSTEM_COLOR_MID, color: '#fca5a5' }}>
            .32x
          </code>{' '}
          are auto-detected from your library. Recommended core:{' '}
          <code className="ml-0.5 px-1 rounded" style={{ background: SYSTEM_COLOR_MID, color: '#fca5a5' }}>
            picodrive_libretro.so
          </code>
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          32X BIOS required:{' '}
          {['32X_M_BIOS.BIN', '32X_S_BIOS.BIN'].map((f) => (
            <code key={f} className="mx-0.5 px-1 rounded" style={{ background: SYSTEM_COLOR_MID, color: '#fca5a5' }}>
              {f}
            </code>
          ))}{' '}
          — place in your RetroArch{' '}
          <code className="mx-0.5 px-1 rounded" style={{ background: SYSTEM_COLOR_MID, color: '#fca5a5' }}>
            system/
          </code>{' '}
          directory.
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Looking for Genesis games?{' '}
          <Link to="/genesis" className="underline" style={{ color: '#fca5a5' }}>
            Visit the Genesis page →
          </Link>
          {' '}· Master System?{' '}
          <Link to="/sms" className="underline" style={{ color: '#fca5a5' }}>
            Visit the SMS page →
          </Link>
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
}: {
  active: GenreFilter;
  onChange: (g: GenreFilter) => void;
  counts: Record<string, number>;
}) {
  return (
    <div className="flex flex-wrap gap-2">
      {ALL_GENRES.map((g) => {
        const count = g === 'all' ? GAMES_32X.length : (counts[g] ?? 0);
        const isActive = active === g;
        return (
          <button
            key={g}
            onClick={() => onChange(g)}
            className="px-3 py-1 rounded-full text-xs font-semibold transition-all"
            style={
              isActive
                ? { background: SYSTEM_COLOR, color: '#fff' }
                : { background: SYSTEM_COLOR_MID, color: '#fca5a5', border: `1px solid ${SYSTEM_COLOR_MID}` }
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

function GameCard32X({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: Game32X;
  onHost: (game: Game32X) => void;
  onQuickMatch: (game: Game32X) => void;
  openRooms: number;
}) {
  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: `linear-gradient(135deg, rgba(204,0,0,0.12), rgba(100,0,0,0.06))`,
        border: `1px solid ${SYSTEM_COLOR_MID}`,
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
          {game.coopOnly && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold whitespace-nowrap"
              style={{ background: 'rgba(74,222,128,0.1)', color: '#4ade80', border: '1px solid rgba(74,222,128,0.25)' }}
            >
              Co-op
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
          style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})`, color: '#fff' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: SYSTEM_COLOR_MID, border: `1px solid rgba(204,0,0,0.35)`, color: '#fca5a5' }}
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

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<Ranking32X[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: Ranking32X[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🔴</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some 32X sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(204,0,0,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
        >
          <RankBadge rank={i + 1} />
          <span className="flex-1 text-sm font-semibold text-white">{r.displayName}</span>
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

function LobbyPanel({ games }: { games: Game32X[] }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<Game32X | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');
  const [genreFilter, setGenreFilter] = useState<GenreFilter>('all');

  const genreCounts = useMemo(() => {
    const counts: Record<string, number> = {};
    for (const g of games) {
      counts[g.genre] = (counts[g.genre] ?? 0) + 1;
    }
    return counts;
  }, [games]);

  const filteredGames = useMemo(
    () => (genreFilter === 'all' ? games : games.filter((g) => g.genre === genreFilter)),
    [games, genreFilter],
  );

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const totalOpenRooms = useMemo(
    () => publicRooms.filter((r) => r.status === 'waiting' && games.some((g) => g.id === r.gameId)).length,
    [publicRooms, games],
  );

  const handleQuickMatch = useCallback(
    async (game: Game32X) => {
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

  return (
    <div className="space-y-5">
      <Banner32X />

      {/* Stats row */}
      <div className="flex gap-3 flex-wrap">
        {[
          { label: 'Games', value: games.length },
          { label: 'Open Rooms', value: totalOpenRooms },
          { label: 'Players Online', value: publicRooms.filter((r) => r.status === 'waiting').reduce((n, r) => n + (r.players?.length ?? 0), 0) },
        ].map(({ label, value }) => (
          <div
            key={label}
            className="flex-1 min-w-[80px] rounded-xl px-3 py-2 text-center"
            style={{ background: 'rgba(204,0,0,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
          >
            <p className="text-lg font-black" style={{ color: '#fca5a5' }}>{value}</p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</p>
          </div>
        ))}
      </div>

      {/* Genre filter */}
      <GenreFilterBar active={genreFilter} onChange={setGenreFilter} counts={genreCounts} />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(204,0,0,0.1)', color: '#fca5a5', border: `1px solid rgba(204,0,0,0.3)` }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {filteredGames.map((game) => (
          <GameCard32X
            key={game.id}
            game={game}
            onHost={setHostGame}
            onQuickMatch={handleQuickMatch}
            openRooms={openRoomsFor(game.id)}
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
// Sega32XPage
// ---------------------------------------------------------------------------

export default function Sega32XPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Header */}
      <div className="flex items-center gap-4">
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          🔴
        </div>
        <div>
          <h1 className="text-3xl font-black text-white">Sega 32X</h1>
          <p className="text-sm mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Genesis add-on · Generation 5 · Online relay via RetroArch + PicoDrive
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {GAMES_32X.length} games · Up to 4 players · No port forwarding needed
          </p>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: SYSTEM_COLOR_MID }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(204,0,0,0.15)', color: '#fca5a5', borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={GAMES_32X} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
    </div>
  );
}
