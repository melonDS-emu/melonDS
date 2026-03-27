import { useState, useEffect, useCallback } from 'react';
import { Link } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';
import {
  fetchRetroGameDefs,
  fetchRetroGameProgress,
  gamesWithAchievements,
  gameIdToTitle,
  type RetroGameAchievementDef,
} from '../lib/retro-achievement-service';
import { isRAConnected, getRACredentials } from '../lib/retro-achievements-settings';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

const WII_COLOR = '#94a3b8';
const WII_LIGHT = '#cbd5e1';

type ActiveTab = 'lobby' | 'leaderboard' | 'achievements';

interface WiiGame {
  id: string;
  title: string;
  genre: string;
  genreEmoji: string;
  maxPlayers: number;
  motionPlus?: boolean;
  coopOnly?: boolean;
  badges?: string[];
}

interface WiiRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game definitions
// ---------------------------------------------------------------------------

const WII_GAMES: WiiGame[] = [
  {
    id: 'wii-mario-kart-wii',
    title: 'Mario Kart Wii',
    genre: 'Racing',
    genreEmoji: '🏍️',
    maxPlayers: 4,
    badges: ['Party Favorite', 'Great Online'],
  },
  {
    id: 'wii-super-smash-bros-brawl',
    title: 'Super Smash Bros. Brawl',
    genre: 'Fighting',
    genreEmoji: '🥊',
    maxPlayers: 4,
    badges: ['Party Favorite', 'Competitive'],
  },
  {
    id: 'wii-new-super-mario-bros-wii',
    title: 'New Super Mario Bros. Wii',
    genre: 'Platformer',
    genreEmoji: '🍄',
    maxPlayers: 4,
    coopOnly: true,
    badges: ['Co-op Favorite', 'Best with Friends'],
  },
  {
    id: 'wii-wii-sports',
    title: 'Wii Sports',
    genre: 'Sports',
    genreEmoji: '🎳',
    maxPlayers: 4,
    badges: ['Best with Friends', 'Casual'],
  },
  {
    id: 'wii-wii-sports-resort',
    title: 'Wii Sports Resort',
    genre: 'Sports',
    genreEmoji: '🏄',
    maxPlayers: 4,
    motionPlus: true,
    badges: ['Best with Friends', 'Casual'],
  },
  {
    id: 'wii-mario-party-8',
    title: 'Mario Party 8',
    genre: 'Party',
    genreEmoji: '🎲',
    maxPlayers: 4,
    badges: ['Party Favorite', 'Best with Friends'],
  },
  {
    id: 'wii-mario-party-9',
    title: 'Mario Party 9',
    genre: 'Party',
    genreEmoji: '🚗',
    maxPlayers: 4,
    badges: ['Party Favorite', 'Best with Friends'],
  },
  {
    id: 'wii-donkey-kong-country-returns',
    title: 'Donkey Kong Country Returns',
    genre: 'Platformer',
    genreEmoji: '🦍',
    maxPlayers: 2,
    coopOnly: true,
    badges: ['Co-op Favorite'],
  },
  {
    id: 'wii-kirby-return-to-dream-land',
    title: "Kirby's Return to Dream Land",
    genre: 'Platformer',
    genreEmoji: '⭐',
    maxPlayers: 4,
    coopOnly: true,
    badges: ['Co-op Favorite', 'Best with Friends'],
  },
];

// ---------------------------------------------------------------------------
// API helper
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
// Dolphin info banner
// ---------------------------------------------------------------------------

function DolphinBanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ background: 'rgba(148,163,184,0.07)', border: '1px solid rgba(148,163,184,0.22)' }}
    >
      <span className="text-xl mt-0.5">🐬</span>
      <div>
        <p className="text-sm font-bold" style={{ color: WII_LIGHT }}>
          Dolphin Emulator · Wiimmfi Netplay
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          RetroOasis rooms coordinate Dolphin sessions, surface open lobbies, and keep players in
          sync before launch. Many Wii games still require Dolphin Netplay setup or Wiimmfi
          patching. Wii images{' '}
          <span className="font-semibold text-white">.wbfs / .iso / .rvz</span> are auto-detected
          from your ROM library. MotionPlus titles are flagged below.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function WiiGameCard({
  game,
  openRooms,
  onHost,
  onQuickMatch,
}: {
  game: WiiGame;
  openRooms: number;
  onHost: (game: WiiGame) => void;
  onQuickMatch: (game: WiiGame) => void;
}) {
  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: 'linear-gradient(135deg, rgba(148,163,184,0.08), rgba(148,163,184,0.02))',
        border: '1px solid rgba(148,163,184,0.16)',
      }}
    >
      {/* Header row */}
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-3">
          <span className="text-3xl leading-none">{game.genreEmoji}</span>
          <div>
            <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.genre} · {game.coopOnly ? `${game.maxPlayers}P Co-op` : `Up to ${game.maxPlayers}P`}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1 shrink-0">
          {game.motionPlus && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(251,191,36,0.15)', color: '#fbbf24', border: '1px solid rgba(251,191,36,0.3)' }}
            >
              MotionPlus
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
              style={{ background: 'rgba(0,209,112,0.12)', color: '#00d170', border: '1px solid rgba(0,209,112,0.3)' }}
            >
              {openRooms} open
            </span>
          )}
        </div>
      </div>

      {/* Badges */}
      {game.badges && game.badges.length > 0 && (
        <div className="flex flex-wrap gap-1.5">
          {game.badges.map((b) => (
            <span
              key={b}
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(148,163,184,0.1)', color: WII_LIGHT, border: '1px solid rgba(148,163,184,0.2)' }}
            >
              {b}
            </span>
          ))}
        </div>
      )}

      {/* Actions */}
      <div className="flex gap-2 mt-auto">
        <button
          onClick={() => onQuickMatch(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: 'linear-gradient(90deg, #94a3b8, #64748b)', color: '#0f172a' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: 'rgba(148,163,184,0.08)', border: '1px solid rgba(148,163,184,0.28)', color: WII_LIGHT }}
        >
          🎮 Host Room
        </button>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({ games }: { games: WiiGame[] }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<WiiGame | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const handleQuickMatch = useCallback(
    async (game: WiiGame) => {
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
      <DolphinBanner />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(148,163,184,0.08)', color: WII_LIGHT, border: '1px solid rgba(148,163,184,0.22)' }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {games.map((game) => (
          <WiiGameCard
            key={game.id}
            game={game}
            openRooms={openRoomsFor(game.id)}
            onHost={setHostGame}
            onQuickMatch={handleQuickMatch}
          />
        ))}
      </div>

      {!displayName && (
        <p className="text-center text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Set a display name in{' '}
          <Link to="/settings" className="underline" style={{ color: WII_LIGHT }}>
            Settings
          </Link>{' '}
          to host or join rooms.
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
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<WiiRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: WiiRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🐬</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some Wii sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2 max-w-lg">
      <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
        Top players ranked by session count
      </p>
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(148,163,184,0.06)', border: '1px solid rgba(148,163,184,0.14)' }}
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
// Achievements panel
// ---------------------------------------------------------------------------

const WII_ACHIEVEMENT_GAMES = gamesWithAchievements().filter((id) => id.startsWith('wii-'));

function WiiAchievementsPanel() {
  const raConnected = isRAConnected();
  const { username } = getRACredentials();
  const [selectedGame, setSelectedGame] = useState<string>(WII_ACHIEVEMENT_GAMES[0] ?? '');
  const [defs, setDefs] = useState<RetroGameAchievementDef[]>([]);
  const [earnedIds, setEarnedIds] = useState<Set<string>>(new Set());
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    if (!selectedGame) return;
    setLoading(true);
    Promise.all([
      fetchRetroGameDefs(selectedGame),
      raConnected ? fetchRetroGameProgress(username, selectedGame) : Promise.resolve(null),
    ])
      .then(([gameDefs, progress]) => {
        setDefs(gameDefs);
        setEarnedIds(new Set((progress?.earned ?? []).map((e) => e.achievementId)));
      })
      .catch(() => { setDefs([]); setEarnedIds(new Set()); })
      .finally(() => setLoading(false));
  }, [selectedGame, raConnected, username]);

  if (!raConnected) {
    return (
      <div
        className="rounded-2xl p-6 text-center space-y-3"
        style={{ background: 'rgba(148,163,184,0.06)', border: '1px solid rgba(148,163,184,0.15)' }}
      >
        <p className="text-3xl">🏅</p>
        <p className="font-semibold text-white">RetroAchievements not connected</p>
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <Link to="/settings" className="underline" style={{ color: WII_LIGHT }}>
            Sign in to RetroAchievements
          </Link>{' '}
          to track your Wii progress.
        </p>
      </div>
    );
  }

  if (WII_ACHIEVEMENT_GAMES.length === 0) {
    return (
      <div className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-3xl mb-2">🏅</p>
        <p className="font-semibold">No achievement data available for Wii yet.</p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      <div className="flex flex-wrap gap-2">
        {WII_ACHIEVEMENT_GAMES.map((id) => (
          <button
            key={id}
            onClick={() => setSelectedGame(id)}
            className="px-3 py-1.5 rounded-xl text-xs font-semibold transition-all"
            style={
              selectedGame === id
                ? { background: 'rgba(148,163,184,0.15)', color: WII_LIGHT, border: '1px solid rgba(148,163,184,0.4)' }
                : { background: 'rgba(255,255,255,0.04)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }
            }
          >
            {gameIdToTitle(id)}
          </button>
        ))}
      </div>

      {loading ? (
        <div className="flex items-center justify-center py-12">
          <div className="animate-spin text-3xl">🐬</div>
        </div>
      ) : defs.length === 0 ? (
        <div className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <p className="text-3xl mb-2">🏅</p>
          <p className="font-semibold">No achievements defined for this game yet.</p>
        </div>
      ) : (
        <div className="space-y-2">
          <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {earnedIds.size} / {defs.length} unlocked
          </p>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-2">
            {defs.map((def) => {
              const unlocked = earnedIds.has(def.id);
              return (
                <div
                  key={def.id}
                  className="rounded-xl p-3 flex gap-3 items-start"
                  style={{
                    background: unlocked ? 'rgba(148,163,184,0.1)' : 'rgba(255,255,255,0.02)',
                    border: unlocked ? '1px solid rgba(148,163,184,0.32)' : '1px solid rgba(255,255,255,0.06)',
                    opacity: unlocked ? 1 : 0.55,
                  }}
                >
                  <span className="text-2xl">{def.badge}</span>
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center gap-2">
                      <p className="text-sm font-semibold text-white">{def.title}</p>
                      <span
                        className="text-[10px] font-bold px-1.5 py-0.5 rounded-full"
                        style={{
                          background: unlocked ? 'rgba(148,163,184,0.22)' : 'rgba(255,255,255,0.06)',
                          color: unlocked ? WII_LIGHT : 'var(--color-oasis-text-muted)',
                        }}
                      >
                        {def.points}pts
                      </span>
                    </div>
                    <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {def.description}
                    </p>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// WiiPage
// ---------------------------------------------------------------------------

export default function WiiPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'achievements', label: '🏅 Achievements' },
  ];

  const coopCount = WII_GAMES.filter((g) => g.coopOnly).length;
  const motionPlusCount = WII_GAMES.filter((g) => g.motionPlus).length;

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{
          background: 'linear-gradient(135deg, rgba(148,163,184,0.14) 0%, rgba(148,163,184,0.04) 60%, transparent 100%)',
          border: '1px solid rgba(148,163,184,0.2)',
        }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: 'linear-gradient(135deg, #94a3b8, #475569)' }}
        >
          🐬
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Nintendo Wii</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Dolphin · Wiimmfi netplay · Up to 4 players · No port forwarding needed
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <div className="flex flex-wrap justify-end gap-1.5">
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(148,163,184,0.14)', color: WII_LIGHT }}
            >
              {WII_GAMES.length} games
            </span>
            {coopCount > 0 && (
              <span
                className="text-xs px-2 py-0.5 rounded-full font-semibold"
                style={{ background: 'rgba(74,222,128,0.1)', color: '#4ade80', border: '1px solid rgba(74,222,128,0.2)' }}
              >
                {coopCount} co-op
              </span>
            )}
            {motionPlusCount > 0 && (
              <span
                className="text-xs px-2 py-0.5 rounded-full font-semibold"
                style={{ background: 'rgba(251,191,36,0.12)', color: '#fbbf24', border: '1px solid rgba(251,191,36,0.25)' }}
              >
                {motionPlusCount} MotionPlus
              </span>
            )}
          </div>
          <span className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            7th Generation · 2006
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(148,163,184,0.14)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(148,163,184,0.1)', color: WII_LIGHT, borderBottom: `2px solid ${WII_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={WII_GAMES} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'achievements' && <WiiAchievementsPanel />}
    </div>
  );
}
