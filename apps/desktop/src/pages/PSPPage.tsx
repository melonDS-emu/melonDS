import { useState, useEffect, useCallback } from 'react';
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

const SYSTEM_COLOR = '#5E5E5E';
const SYSTEM_COLOR_DARK = '#1a1a1a';
const SYSTEM_EMOJI = '⬛';

type ActiveTab = 'lobby' | 'leaderboard';

interface PSPGame {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  competitive?: boolean;
  coopOnly?: boolean;
}

interface PSPRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const PSP_GAMES: PSPGame[] = [
  { id: 'psp-monster-hunter-fu',          title: 'Monster Hunter Freedom Unite', genre: 'Action RPG',       genreEmoji: '🐉', maxPlayers: 4, coopOnly: true },
  { id: 'psp-peace-walker',               title: 'MGS: Peace Walker',            genre: 'Action',           genreEmoji: '🐍', maxPlayers: 4, coopOnly: true },
  { id: 'psp-patapon',                    title: 'Patapon',                      genre: 'Rhythm Strategy',  genreEmoji: '🥁', maxPlayers: 4, coopOnly: true },
  { id: 'psp-wipeout-pulse',              title: 'WipEout Pulse',                genre: 'Racing',           genreEmoji: '🛸', maxPlayers: 8, competitive: true },
  { id: 'psp-tekken-dark-resurrection',   title: 'Tekken: Dark Resurrection',    genre: 'Fighting',         genreEmoji: '🥊', maxPlayers: 2, competitive: true },
  { id: 'psp-gta-vice-city-stories',      title: 'GTA: Vice City Stories',       genre: 'Action',           genreEmoji: '🌴', maxPlayers: 2, competitive: true },
  { id: 'psp-lumines',                    title: 'Lumines',                      genre: 'Puzzle',           genreEmoji: '🎵', maxPlayers: 2, competitive: true },
  { id: 'psp-god-of-war-chains',          title: 'God of War: Chains of Olympus',genre: 'Action',           genreEmoji: '⚡', maxPlayers: 1 },
  { id: 'psp-crisis-core-ff7',            title: 'Crisis Core: FF7',             genre: 'Action RPG',       genreEmoji: '⚔️', maxPlayers: 1 },
];

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
// Rank badge helper
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

function PSPBanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(60,60,60,0.2)', border: '1px solid rgba(100,100,100,0.35)' }}
    >
      <span className="text-xl">{SYSTEM_EMOJI}</span>
      <div>
        <p className="text-sm font-bold" style={{ color: '#a3a3a3' }}>
          Sony PlayStation Portable Relay Netplay via PPSSPP
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          All PSP sessions use RetroOasis TCP relay netplay with PPSSPP ad-hoc server bridging.
          No port forwarding required. ROM files (.iso, .cso, .pbp) are auto-detected from your
          library. Up to 8 players supported for select titles.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function PSPGameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: PSPGame;
  onHost: (game: PSPGame) => void;
  onQuickMatch: (game: PSPGame) => void;
  openRooms: number;
}) {
  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: 'linear-gradient(135deg, rgba(60,60,60,0.18), rgba(10,10,10,0.1))',
        border: '1px solid rgba(100,100,100,0.2)',
      }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2">
          <span className="text-2xl">{game.genreEmoji}</span>
          <div>
            <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.genre} · {game.maxPlayers === 1 ? '1P' : game.maxPlayers === 2 ? '2P' : `Up to ${game.maxPlayers}P`}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1">
          {game.competitive && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(251,146,60,0.15)', color: '#fb923c', border: '1px solid rgba(251,146,60,0.3)' }}
            >
              Competitive
            </span>
          )}
          {game.coopOnly && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(74,222,128,0.1)', color: '#4ade80', border: '1px solid rgba(74,222,128,0.25)' }}
            >
              Co-op
            </span>
          )}
          {openRooms > 0 && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(85,85,85,0.2)', color: '#a3a3a3', border: '1px solid rgba(85,85,85,0.4)' }}
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
          style={{ background: 'linear-gradient(90deg, #3c3c3c, #0a0a0a)', color: '#fff' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: 'rgba(60,60,60,0.18)', border: '1px solid rgba(100,100,100,0.35)', color: '#a3a3a3' }}
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
  const [rankings, setRankings] = useState<PSPRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: PSPRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">{SYSTEM_EMOJI}</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some PSP sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(60,60,60,0.12)', border: '1px solid rgba(100,100,100,0.18)' }}
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

function LobbyPanel({ games }: { games: PSPGame[] }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<PSPGame | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const handleQuickMatch = useCallback(
    async (game: PSPGame) => {
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
    <div className="space-y-6">
      <PSPBanner />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(60,60,60,0.2)', color: '#a3a3a3', border: '1px solid rgba(100,100,100,0.3)' }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {games.map((game) => (
          <PSPGameCard
            key={game.id}
            game={game}
            onHost={setHostGame}
            onQuickMatch={handleQuickMatch}
            openRooms={openRoomsFor(game.id)}
          />
        ))}
      </div>

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
        <JoinRoomModal onConfirm={(code, dn) => { joinByCode(code, dn); setShowJoin(false); }} onClose={() => setShowJoin(false)} initialCode={joinCode} />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// PSPPage
// ---------------------------------------------------------------------------

export default function PSPPage() {
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
        style={{ background: 'linear-gradient(135deg, rgba(94,94,94,0.18) 0%, rgba(94,94,94,0.05) 60%, transparent 100%)', border: '1px solid rgba(94,94,94,0.25)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          {SYSTEM_EMOJI}
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Sony PlayStation Portable</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            PPSSPP · Ad-hoc server relay · Up to 8 players · No port forwarding needed
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {PSP_GAMES.length} games
          </span>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            7th Generation · 2005
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(94,94,94,0.2)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(94,94,94,0.15)', color: '#a3a3a3', borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={PSP_GAMES} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
    </div>
  );
}
