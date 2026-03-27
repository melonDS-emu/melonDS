import { useState, useEffect, useCallback } from 'react';
import { Link } from 'react-router-dom';
import { useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import type { MockGame } from '../data/mock-games';
import { MOCK_GAMES } from '../data/mock-games';
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

const SNES_GAMES = MOCK_GAMES.filter((g) => g.system === 'SNES');
const SNES_COLOR = '#7c3aed';
const SNES_LIGHT = '#a78bfa';

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

type ActiveTab = 'lobby' | 'leaderboard' | 'achievements';

interface SnesRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

async function apiFetch<T>(path: string): Promise<T> {
  const res = await fetch(`${API}${path}`, { headers: { 'Content-Type': 'application/json' } });
  if (!res.ok) throw new Error('Request failed');
  return res.json() as Promise<T>;
}

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

function Snes9xBanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ background: 'rgba(124,58,237,0.08)', border: '1px solid rgba(124,58,237,0.25)' }}
    >
      <span className="text-xl mt-0.5">🕹️</span>
      <div>
        <p className="text-sm font-bold" style={{ color: SNES_LIGHT }}>
          Snes9x Emulator · 2–4 Player Online Relay
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Snes9x provides robust relay netplay for 2–4 players. Automatic{' '}
          <span className="font-semibold text-white">.sfc / .smc</span> ROM detection. Fallback to
          bsnes (high accuracy) or RetroArch (snes9x core). The{' '}
          <span className="font-semibold text-white">accurate</span> performance preset enables
          bsnes-level cycle accuracy.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function SNESGameCard({
  game,
  openRooms,
  onHost,
  onQuickMatch,
}: {
  game: MockGame;
  openRooms: number;
  onHost: (game: MockGame) => void;
  onQuickMatch: (game: MockGame) => void;
}) {
  const multiPlayer = game.maxPlayers > 1;

  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{ background: 'linear-gradient(135deg, rgba(124,58,237,0.08), rgba(124,58,237,0.02))', border: '1px solid rgba(124,58,237,0.18)' }}
    >
      {/* Header row */}
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-3">
          <span className="text-3xl leading-none">{game.coverEmoji}</span>
          <div>
            <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.maxPlayers === 1 ? 'Single Player' : `Up to ${game.maxPlayers}P`}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1 shrink-0">
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

      {/* Description */}
      <p className="text-xs leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {game.description}
      </p>

      {/* Tags + badges */}
      <div className="flex flex-wrap gap-1.5">
        {game.tags.map((t) => (
          <span
            key={t}
            className="text-xs px-2 py-0.5 rounded-full"
            style={{ background: 'rgba(255,255,255,0.06)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }}
          >
            {t}
          </span>
        ))}
        {game.badges.map((b) => (
          <span
            key={b}
            className="text-xs px-2 py-0.5 rounded-full font-semibold"
            style={{ background: 'rgba(124,58,237,0.15)', color: SNES_LIGHT, border: '1px solid rgba(124,58,237,0.3)' }}
          >
            {b}
          </span>
        ))}
      </div>

      {/* Actions */}
      {multiPlayer && (
        <div className="flex gap-2 mt-auto">
          <button
            onClick={() => onQuickMatch(game)}
            className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
            style={{ background: `linear-gradient(90deg, ${SNES_COLOR}, #3b1a8c)`, color: '#fff' }}
          >
            ⚡ Quick Match
          </button>
          <button
            onClick={() => onHost(game)}
            className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
            style={{ background: 'rgba(124,58,237,0.1)', border: `1px solid rgba(124,58,237,0.35)`, color: SNES_LIGHT }}
          >
            🕹️ Host Room
          </button>
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({ onJoin }: { onJoin: (code: string) => void }) {
  const { publicRooms, createRoom } = useLobby();
  const [hostGame, setHostGame] = useState<MockGame | null>(null);

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const handleQuickMatch = useCallback(
    async (game: MockGame) => {
      const open = publicRooms.find((r) => r.gameId === game.id && r.status === 'waiting');
      if (open) {
        onJoin(open.roomCode);
        return;
      }
      try {
        const data = await apiFetch<{ rooms?: Array<{ roomCode: string }> }>(
          `/api/rooms?gameId=${encodeURIComponent(game.id)}`,
        );
        const rooms = data.rooms ?? [];
        if (rooms.length > 0) {
          onJoin(rooms[0].roomCode);
        } else {
          setHostGame(game);
        }
      } catch {
        setHostGame(game);
      }
    },
    [publicRooms, onJoin],
  );

  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';

  return (
    <div className="space-y-6">
      <Snes9xBanner />

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {SNES_GAMES.map((game) => (
          <SNESGameCard
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
          <Link to="/settings" className="underline" style={{ color: SNES_LIGHT }}>
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
    </div>
  );
}

// ---------------------------------------------------------------------------
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<SnesRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: SnesRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🕹️</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some SNES sessions to appear here!</p>
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
          style={{ background: 'rgba(124,58,237,0.06)', border: '1px solid rgba(124,58,237,0.15)' }}
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

const SNES_ACHIEVEMENT_GAMES = gamesWithAchievements().filter((id) => id.startsWith('snes-'));

function AchievementsPanel() {
  const raConnected = isRAConnected();
  const { username } = getRACredentials();
  const [selectedGame, setSelectedGame] = useState<string>(SNES_ACHIEVEMENT_GAMES[0] ?? '');
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
        style={{ background: 'rgba(124,58,237,0.05)', border: '1px solid rgba(124,58,237,0.18)' }}
      >
        <p className="text-3xl">🏅</p>
        <p className="font-semibold text-white">RetroAchievements not connected</p>
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <Link to="/settings" className="underline" style={{ color: SNES_LIGHT }}>
            Sign in to RetroAchievements
          </Link>{' '}
          to track your SNES progress.
        </p>
      </div>
    );
  }

  if (SNES_ACHIEVEMENT_GAMES.length === 0) {
    return (
      <div className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-3xl mb-2">🏅</p>
        <p className="font-semibold">No achievement data available for SNES yet.</p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      <div className="flex flex-wrap gap-2">
        {SNES_ACHIEVEMENT_GAMES.map((id) => (
          <button
            key={id}
            onClick={() => setSelectedGame(id)}
            className="px-3 py-1.5 rounded-xl text-xs font-semibold transition-all"
            style={
              selectedGame === id
                ? { background: 'rgba(124,58,237,0.18)', color: SNES_LIGHT, border: '1px solid rgba(124,58,237,0.45)' }
                : { background: 'rgba(255,255,255,0.04)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }
            }
          >
            {gameIdToTitle(id)}
          </button>
        ))}
      </div>

      {loading ? (
        <div className="flex items-center justify-center py-12">
          <div className="animate-spin text-3xl">🕹️</div>
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
                    background: unlocked ? 'rgba(124,58,237,0.1)' : 'rgba(255,255,255,0.02)',
                    border: unlocked ? '1px solid rgba(124,58,237,0.35)' : '1px solid rgba(255,255,255,0.06)',
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
                          background: unlocked ? 'rgba(124,58,237,0.25)' : 'rgba(255,255,255,0.06)',
                          color: unlocked ? SNES_LIGHT : 'var(--color-oasis-text-muted)',
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
// SNESPage
// ---------------------------------------------------------------------------

export default function SNESPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [joinCode, setJoinCode] = useState<string | null>(null);
  const { joinByCode, currentRoom } = useLobby();
  const navigate = useNavigate();

  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🕹️ Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'achievements', label: '🏅 Achievements' },
  ];

  const multiplayerCount = SNES_GAMES.filter((g) => g.maxPlayers > 1).length;

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: 'linear-gradient(135deg, rgba(124,58,237,0.2) 0%, rgba(124,58,237,0.06) 60%, transparent 100%)', border: '1px solid rgba(124,58,237,0.22)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: 'linear-gradient(135deg, #7c3aed, #3b1a8c)' }}
        >
          🕹️
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">
            Super Nintendo Entertainment System
          </h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Snes9x · 2–4 player online relay · bsnes accurate mode available
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <div className="flex items-center gap-2">
            <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {SNES_GAMES.length} games
            </span>
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(124,58,237,0.18)', color: SNES_LIGHT }}
            >
              {multiplayerCount} multiplayer
            </span>
          </div>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            4th Generation · 1990
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(124,58,237,0.18)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(124,58,237,0.12)', color: SNES_LIGHT, borderBottom: `2px solid ${SNES_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel onJoin={(code) => setJoinCode(code)} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'achievements' && <AchievementsPanel />}

      {joinCode && (
        <JoinRoomModal
          initialCode={joinCode}
          onConfirm={(code, name) => { joinByCode(code, name); setJoinCode(null); }}
          onClose={() => setJoinCode(null)}
        />
      )}
    </div>
  );
}
