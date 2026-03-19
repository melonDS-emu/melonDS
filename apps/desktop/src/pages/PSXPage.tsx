import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';
import type { CreateRoomPayload } from '../services/lobby-types';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

const SYSTEM_COLOR = '#808080';
const SYSTEM_EMOJI = '🎮';

type ActiveTab = 'lobby' | 'leaderboard';

interface PSXGame {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  competitive?: boolean;
  coopOnly?: boolean;
}

interface PSXRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const PSX_GAMES: PSXGame[] = [
  { id: 'psx-tekken-3',                 title: 'Tekken 3',                    genre: 'Fighting',    genreEmoji: '👊', maxPlayers: 2, competitive: true },
  { id: 'psx-street-fighter-alpha-3',   title: 'Street Fighter Alpha 3',      genre: 'Fighting',    genreEmoji: '🥋', maxPlayers: 2, competitive: true },
  { id: 'psx-tony-hawks-pro-skater-2',  title: "Tony Hawk's Pro Skater 2",    genre: 'Sports',      genreEmoji: '🛹', maxPlayers: 2, competitive: true },
  { id: 'psx-twisted-metal-2',          title: 'Twisted Metal 2',             genre: 'Combat',      genreEmoji: '💀', maxPlayers: 2, competitive: true },
  { id: 'psx-crash-bash',               title: 'Crash Bash',                  genre: 'Party',       genreEmoji: '🐸', maxPlayers: 4, competitive: true },
  { id: 'psx-worms-armageddon',         title: 'Worms Armageddon',            genre: 'Strategy',    genreEmoji: '🪱', maxPlayers: 4, competitive: true },
  { id: 'psx-crash-bandicoot',          title: 'Crash Bandicoot',             genre: 'Platformer',  genreEmoji: '🐛', maxPlayers: 1 },
  { id: 'psx-castlevania-sotn',         title: 'Castlevania: SOTN',           genre: 'Action RPG',  genreEmoji: '🏰', maxPlayers: 1 },
  { id: 'psx-metal-gear-solid',         title: 'Metal Gear Solid',            genre: 'Stealth',     genreEmoji: '🐍', maxPlayers: 1 },
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

function PSXBanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(128,128,128,0.1)', border: '1px solid rgba(128,128,128,0.35)' }}
    >
      <span className="text-xl">{SYSTEM_EMOJI}</span>
      <div>
        <p className="text-sm font-bold" style={{ color: '#d1d5db' }}>
          Sony PlayStation Relay Netplay via DuckStation
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          All PSX sessions use RetroOasis TCP relay netplay with DuckStation. No port forwarding
          required. ROM files (.bin/.cue, .iso, .chd, .pbp) are auto-detected from your library.
          Requires PS1 BIOS image for best compatibility.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function PSXGameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: PSXGame;
  onHost: (game: PSXGame) => void;
  onQuickMatch: (game: PSXGame) => void;
  openRooms: number;
}) {
  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: 'linear-gradient(135deg, rgba(128,128,128,0.12), rgba(51,51,51,0.06))',
        border: '1px solid rgba(128,128,128,0.2)',
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
              style={{ background: 'rgba(209,213,219,0.1)', color: '#d1d5db', border: '1px solid rgba(209,213,219,0.25)' }}
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
          style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, #333333)`, color: '#fff' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: 'rgba(128,128,128,0.12)', border: '1px solid rgba(128,128,128,0.35)', color: '#d1d5db' }}
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
  const [rankings, setRankings] = useState<PSXRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: PSXRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
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
        <p className="text-sm mt-1">Play some PlayStation sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(128,128,128,0.08)', border: '1px solid rgba(128,128,128,0.18)' }}
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

function LobbyPanel({ games }: { games: PSXGame[] }) {
  const { rooms, displayName } = useLobby();
  const [hostGame, setHostGame] = useState<PSXGame | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');

  const openRoomsFor = useCallback(
    (gameId: string) => rooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [rooms],
  );

  const handleQuickMatch = useCallback(
    async (game: PSXGame) => {
      const open = rooms.find((r) => r.gameId === game.id && r.status === 'waiting');
      if (open) {
        setJoinCode(open.roomCode);
        setShowJoin(true);
      } else {
        setHostGame(game);
        setNotification(`No open ${game.title} rooms — hosting one for you!`);
        setTimeout(() => setNotification(''), 3000);
      }
    },
    [rooms],
  );

  const buildPayload = useCallback(
    (game: PSXGame): Partial<CreateRoomPayload> => ({
      gameId: game.id,
      system: 'psx',
      maxPlayers: game.maxPlayers,
    }),
    [],
  );

  return (
    <div className="space-y-6">
      <PSXBanner />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(128,128,128,0.1)', color: '#d1d5db', border: '1px solid rgba(128,128,128,0.3)' }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {games.map((game) => (
          <PSXGameCard
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
          open
          onClose={() => setHostGame(null)}
          initialPayload={buildPayload(hostGame)}
        />
      )}
      {showJoin && (
        <JoinRoomModal open onClose={() => setShowJoin(false)} initialCode={joinCode} />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// PSXPage
// ---------------------------------------------------------------------------

export default function PSXPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Header */}
      <div className="flex items-center gap-4">
        <span className="text-5xl">{SYSTEM_EMOJI}</span>
        <div>
          <h1 className="text-3xl font-black text-white">Sony PlayStation</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Online relay sessions via DuckStation · Up to 4 players · No port forwarding needed
          </p>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(128,128,128,0.2)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(128,128,128,0.15)', color: '#d1d5db', borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={PSX_GAMES} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
    </div>
  );
}
