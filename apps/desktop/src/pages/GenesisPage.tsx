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

const SYSTEM_COLOR = '#0066CC';
const SYSTEM_COLOR_DARK = '#003366';
const SYSTEM_COLOR_MID = 'rgba(0,102,204,0.18)';

type ActiveTab = 'lobby' | 'leaderboard';

type GenreFilter = 'all' | 'Fighting' | 'Beat-em-up' | 'Platformer' | 'Run-and-Gun' | 'Sports' | 'Adventure' | 'Racing';

interface GenesisGame {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  competitive?: boolean;
  coopOnly?: boolean;
  singleOnly?: boolean;
  year?: number;
}

interface GenesisRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions (overhauled — 16 games)
// ---------------------------------------------------------------------------

const GENESIS_GAMES: GenesisGame[] = [
  // Fighting
  { id: 'genesis-mortal-kombat-3',            title: 'Mortal Kombat 3',             genre: 'Fighting',    genreEmoji: '🩸', maxPlayers: 2, competitive: true,  year: 1995 },
  { id: 'genesis-super-street-fighter-ii',    title: 'Super Street Fighter II',     genre: 'Fighting',    genreEmoji: '👊', maxPlayers: 2, competitive: true,  year: 1994 },
  // Beat-em-up
  { id: 'genesis-streets-of-rage-2',          title: 'Streets of Rage 2',           genre: 'Beat-em-up',  genreEmoji: '🥊', maxPlayers: 2, coopOnly: true,    year: 1992 },
  { id: 'genesis-streets-of-rage-3',          title: 'Streets of Rage 3',           genre: 'Beat-em-up',  genreEmoji: '🥊', maxPlayers: 2, coopOnly: true,    year: 1994 },
  { id: 'genesis-golden-axe',                 title: 'Golden Axe',                  genre: 'Beat-em-up',  genreEmoji: '⚔️', maxPlayers: 2, coopOnly: true,    year: 1989 },
  { id: 'genesis-x-men-2-clone-wars',         title: 'X-Men 2: Clone Wars',         genre: 'Beat-em-up',  genreEmoji: '⚡', maxPlayers: 2, coopOnly: true,    year: 1995 },
  { id: 'genesis-battletoads-double-dragon',  title: 'Battletoads & Double Dragon', genre: 'Beat-em-up',  genreEmoji: '🐸', maxPlayers: 2, coopOnly: true,    year: 1993 },
  // Platformer
  { id: 'genesis-sonic-the-hedgehog-2',       title: 'Sonic the Hedgehog 2',        genre: 'Platformer',  genreEmoji: '💨', maxPlayers: 2, competitive: true, year: 1992 },
  { id: 'genesis-sonic-3',                    title: 'Sonic the Hedgehog 3',        genre: 'Platformer',  genreEmoji: '💨', maxPlayers: 2, competitive: true, year: 1994 },
  { id: 'genesis-earthworm-jim-2',            title: 'Earthworm Jim 2',             genre: 'Platformer',  genreEmoji: '🐛', maxPlayers: 2, competitive: true, year: 1995 },
  // Run-and-Gun
  { id: 'genesis-contra-hard-corps',          title: 'Contra: Hard Corps',          genre: 'Run-and-Gun', genreEmoji: '🔫', maxPlayers: 2, coopOnly: true,    year: 1994 },
  { id: 'genesis-gunstar-heroes',             title: 'Gunstar Heroes',              genre: 'Run-and-Gun', genreEmoji: '💥', maxPlayers: 2, coopOnly: true,    year: 1993 },
  // Sports
  { id: 'genesis-nba-jam',                    title: 'NBA Jam',                     genre: 'Sports',      genreEmoji: '🏀', maxPlayers: 2, competitive: true, year: 1993 },
  { id: 'genesis-mutant-league-football',     title: 'Mutant League Football',      genre: 'Sports',      genreEmoji: '🏈', maxPlayers: 2, competitive: true, year: 1993 },
  // Adventure
  { id: 'genesis-toejam-and-earl',            title: 'ToeJam & Earl',               genre: 'Adventure',   genreEmoji: '👽', maxPlayers: 2, coopOnly: true,    year: 1991 },
  // Racing
  { id: 'genesis-road-rash-ii',               title: 'Road Rash II',                genre: 'Racing',      genreEmoji: '🏍️', maxPlayers: 2, competitive: true, year: 1993 },
];

const ALL_GENRES: GenreFilter[] = ['all', 'Fighting', 'Beat-em-up', 'Platformer', 'Run-and-Gun', 'Sports', 'Adventure', 'Racing'];

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

function GenesisBanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(0,102,204,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
    >
      <span className="text-xl mt-0.5">🔵</span>
      <div className="space-y-1">
        <p className="text-sm font-bold" style={{ color: '#60a5fa' }}>
          SEGA Genesis / Mega Drive · Relay Netplay via RetroArch + Genesis Plus GX
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          All sessions use RetroOasis TCP relay netplay — no port forwarding required.
          ROM files{' '}
          {['.md', '.bin', '.gen', '.smd'].map((ext) => (
            <code
              key={ext}
              className="mx-0.5 px-1 rounded"
              style={{ background: 'rgba(0,102,204,0.18)', color: '#93c5fd' }}
            >
              {ext}
            </code>
          ))}{' '}
          are auto-detected from your library. Recommended core:
          <code className="ml-1 px-1 rounded" style={{ background: 'rgba(0,102,204,0.18)', color: '#93c5fd' }}>
            genesis_plus_gx_libretro.so
          </code>
        </p>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Looking for CD-ROM games?{' '}
          <Link to="/segacd" className="underline" style={{ color: '#60a5fa' }}>
            Visit the Sega CD page →
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
        const count = g === 'all' ? GENESIS_GAMES.length : (counts[g] ?? 0);
        const isActive = active === g;
        return (
          <button
            key={g}
            onClick={() => onChange(g)}
            className="px-3 py-1 rounded-full text-xs font-semibold transition-all"
            style={
              isActive
                ? { background: SYSTEM_COLOR, color: '#fff' }
                : { background: SYSTEM_COLOR_MID, color: '#93c5fd', border: `1px solid ${SYSTEM_COLOR_MID}` }
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

function GenesisGameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: GenesisGame;
  onHost: (game: GenesisGame) => void;
  onQuickMatch: (game: GenesisGame) => void;
  openRooms: number;
}) {
  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: `linear-gradient(135deg, rgba(0,102,204,0.12), rgba(0,51,102,0.06))`,
        border: `1px solid ${SYSTEM_COLOR_MID}`,
      }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2 min-w-0">
          <span className="text-2xl shrink-0">{game.genreEmoji}</span>
          <div className="min-w-0">
            <p className="text-sm font-bold text-white leading-tight truncate">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.genre} · {game.maxPlayers === 2 ? '2P' : `Up to ${game.maxPlayers}P`}
              {game.year && ` · ${game.year}`}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1 shrink-0">
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
          style={{ background: SYSTEM_COLOR_MID, border: `1px solid rgba(0,102,204,0.35)`, color: '#60a5fa' }}
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
  const [rankings, setRankings] = useState<GenesisRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: GenesisRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🔵</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some Genesis sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(0,102,204,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
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

function LobbyPanel({ games }: { games: GenesisGame[] }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<GenesisGame | null>(null);
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
    async (game: GenesisGame) => {
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
      <GenesisBanner />

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
            style={{ background: 'rgba(0,102,204,0.08)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
          >
            <p className="text-lg font-black" style={{ color: '#60a5fa' }}>{value}</p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</p>
          </div>
        ))}
      </div>

      {/* Genre filter */}
      <GenreFilterBar active={genreFilter} onChange={setGenreFilter} counts={genreCounts} />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(0,102,204,0.1)', color: '#60a5fa', border: `1px solid rgba(0,102,204,0.3)` }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {filteredGames.map((game) => (
          <GenesisGameCard
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
// GenesisPage
// ---------------------------------------------------------------------------

export default function GenesisPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: 'linear-gradient(135deg, rgba(0,102,204,0.18) 0%, rgba(0,102,204,0.05) 60%, transparent 100%)', border: `1px solid ${SYSTEM_COLOR_MID}` }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          ⬢
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">SEGA Genesis</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Also known as the Mega Drive · Online relay via RetroArch + Genesis Plus GX
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <div className="flex items-center gap-2">
            <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {GENESIS_GAMES.length} games
            </span>
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(0,102,204,0.15)', color: '#60a5fa' }}
            >
              2 players
            </span>
          </div>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            4th Generation · 1988
          </span>
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
                ? { background: 'rgba(0,102,204,0.15)', color: '#60a5fa', borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={GENESIS_GAMES} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
    </div>
  );
}
