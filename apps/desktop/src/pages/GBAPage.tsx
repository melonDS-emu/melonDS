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

const LINK_GAMES = MOCK_GAMES.filter(
  (g) => g.system === 'GB' || g.system === 'GBC' || g.system === 'GBA',
);

// Per-system accent colors
const SYSTEM_COLORS: Record<string, string> = {
  GB: '#4ade80',   // green for original Game Boy
  GBC: '#fb923c',  // orange for Game Boy Color
  GBA: '#38bdf8',  // sky blue for Game Boy Advance
};

const GBA_COLOR = '#0284c7';
const GBA_LIGHT = '#38bdf8';

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

type ActiveTab = 'lobby' | 'leaderboard' | 'achievements';
type SystemFilter = 'all' | 'GB' | 'GBC' | 'GBA';

interface GbaRanking {
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

function systemLabel(sys: string): string {
  if (sys === 'GB') return 'Game Boy';
  if (sys === 'GBC') return 'Game Boy Color';
  if (sys === 'GBA') return 'Game Boy Advance';
  return sys;
}

// ---------------------------------------------------------------------------
// Info banner
// ---------------------------------------------------------------------------

function MGBABanner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ background: 'rgba(2,132,199,0.08)', border: '1px solid rgba(2,132,199,0.25)' }}
    >
      <span className="text-xl mt-0.5">🔗</span>
      <div>
        <p className="text-sm font-bold" style={{ color: GBA_LIGHT }}>
          mGBA Emulator · Link Cable over TCP Relay
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          mGBA emulates the Game Boy link cable over TCP — trade Pokémon, battle friends, and play
          co-op. Automatic <span className="font-semibold text-white">.gb / .gbc / .gba</span> ROM
          detection. SameBoy (GB/GBC) and VisualBoyAdvance-M available as fallbacks.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// System filter pills
// ---------------------------------------------------------------------------

function SystemFilterBar({
  filter,
  onChange,
}: {
  filter: SystemFilter;
  onChange: (f: SystemFilter) => void;
}) {
  const options: { value: SystemFilter; label: string; color: string }[] = [
    { value: 'all', label: 'All Systems', color: GBA_COLOR },
    { value: 'GB', label: 'Game Boy', color: SYSTEM_COLORS.GB },
    { value: 'GBC', label: 'Game Boy Color', color: SYSTEM_COLORS.GBC },
    { value: 'GBA', label: 'Game Boy Advance', color: SYSTEM_COLORS.GBA },
  ];

  return (
    <div className="flex flex-wrap gap-2">
      {options.map((opt) => {
        const active = filter === opt.value;
        return (
          <button
            key={opt.value}
            onClick={() => onChange(opt.value)}
            className="px-3 py-1.5 rounded-xl text-xs font-semibold transition-all"
            style={
              active
                ? { background: `${opt.color}20`, color: opt.color, border: `1px solid ${opt.color}55` }
                : { background: 'rgba(255,255,255,0.04)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }
            }
          >
            {opt.label}
          </button>
        );
      })}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function GBAGameCard({
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
  const sysColor = SYSTEM_COLORS[game.system] ?? GBA_LIGHT;
  const multiPlayer = game.maxPlayers > 1;

  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{ background: 'linear-gradient(135deg, rgba(2,132,199,0.07), rgba(2,132,199,0.02))', border: '1px solid rgba(2,132,199,0.15)' }}
    >
      {/* Header row */}
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-3">
          <span className="text-3xl leading-none">{game.coverEmoji}</span>
          <div>
            <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
            <div className="flex items-center gap-1.5 mt-0.5">
              <span
                className="text-[10px] font-bold px-1.5 py-0.5 rounded-full"
                style={{ background: `${sysColor}22`, color: sysColor, border: `1px solid ${sysColor}44` }}
              >
                {game.system}
              </span>
              <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {game.maxPlayers === 1 ? 'Single Player' : `Up to ${game.maxPlayers}P`}
              </span>
            </div>
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
            style={{ background: 'rgba(2,132,199,0.15)', color: GBA_LIGHT, border: '1px solid rgba(2,132,199,0.3)' }}
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
            style={{ background: `linear-gradient(90deg, ${GBA_COLOR}, #0c3461)`, color: '#fff' }}
          >
            ⚡ Quick Match
          </button>
          <button
            onClick={() => onHost(game)}
            className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
            style={{ background: 'rgba(2,132,199,0.1)', border: `1px solid rgba(2,132,199,0.35)`, color: GBA_LIGHT }}
          >
            🔗 Host Room
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
  const [filter, setFilter] = useState<SystemFilter>('all');
  const [hostGame, setHostGame] = useState<MockGame | null>(null);

  const filtered = filter === 'all' ? LINK_GAMES : LINK_GAMES.filter((g) => g.system === filter);

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

  const gbCount = LINK_GAMES.filter((g) => g.system === 'GB').length;
  const gbcCount = LINK_GAMES.filter((g) => g.system === 'GBC').length;
  const gbaCount = LINK_GAMES.filter((g) => g.system === 'GBA').length;

  return (
    <div className="space-y-6">
      <MGBABanner />

      {/* System breakdown */}
      <div className="grid grid-cols-3 gap-3">
        {[
          { sys: 'GB', count: gbCount, label: 'Game Boy', emoji: '🟩' },
          { sys: 'GBC', count: gbcCount, label: 'GBC', emoji: '🟧' },
          { sys: 'GBA', count: gbaCount, label: 'GBA', emoji: '🟦' },
        ].map(({ sys, count, label, emoji }) => (
          <div
            key={sys}
            className="rounded-xl p-3 text-center cursor-pointer transition-all hover:scale-105"
            style={{
              background: filter === sys ? `${SYSTEM_COLORS[sys]}15` : 'rgba(255,255,255,0.03)',
              border: filter === sys ? `1px solid ${SYSTEM_COLORS[sys]}40` : '1px solid rgba(255,255,255,0.07)',
            }}
            onClick={() => setFilter(filter === sys ? 'all' : (sys as SystemFilter))}
          >
            <p className="text-xl mb-1">{emoji}</p>
            <p className="text-xs font-bold text-white">{label}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {count} game{count !== 1 ? 's' : ''}
            </p>
          </div>
        ))}
      </div>

      <SystemFilterBar filter={filter} onChange={setFilter} />

      {filtered.length === 0 ? (
        <div className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <p className="text-3xl mb-2">🎮</p>
          <p className="font-semibold">No {systemLabel(filter)} games in library yet.</p>
        </div>
      ) : (
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
          {filtered.map((game) => (
            <GBAGameCard
              key={game.id}
              game={game}
              openRooms={openRoomsFor(game.id)}
              onHost={setHostGame}
              onQuickMatch={handleQuickMatch}
            />
          ))}
        </div>
      )}

      {!displayName && (
        <p className="text-center text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Set a display name in{' '}
          <Link to="/settings" className="underline" style={{ color: GBA_LIGHT }}>
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
  const [rankings, setRankings] = useState<GbaRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: GbaRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🔗</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some GB/GBC/GBA sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2 max-w-lg">
      <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
        Top players ranked by session count across GB, GBC &amp; GBA
      </p>
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(2,132,199,0.06)', border: '1px solid rgba(2,132,199,0.15)' }}
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

const LINK_ACHIEVEMENT_GAMES = gamesWithAchievements().filter(
  (id) => id.startsWith('gba-') || id.startsWith('gbc-') || id.startsWith('gb-'),
);

function AchievementsPanel() {
  const raConnected = isRAConnected();
  const { username } = getRACredentials();
  const [selectedGame, setSelectedGame] = useState<string>(LINK_ACHIEVEMENT_GAMES[0] ?? '');
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
        style={{ background: 'rgba(2,132,199,0.05)', border: '1px solid rgba(2,132,199,0.2)' }}
      >
        <p className="text-3xl">🏅</p>
        <p className="font-semibold text-white">RetroAchievements not connected</p>
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <Link to="/settings" className="underline" style={{ color: GBA_LIGHT }}>
            Sign in to RetroAchievements
          </Link>{' '}
          to track your GB / GBC / GBA progress.
        </p>
      </div>
    );
  }

  if (LINK_ACHIEVEMENT_GAMES.length === 0) {
    return (
      <div className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-3xl mb-2">🏅</p>
        <p className="font-semibold">No achievement data available yet.</p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      <div className="flex flex-wrap gap-2">
        {LINK_ACHIEVEMENT_GAMES.map((id) => {
          const sys = id.startsWith('gba-') ? 'GBA' : id.startsWith('gbc-') ? 'GBC' : 'GB';
          const sysColor = SYSTEM_COLORS[sys] ?? GBA_LIGHT;
          return (
            <button
              key={id}
              onClick={() => setSelectedGame(id)}
              className="px-3 py-1.5 rounded-xl text-xs font-semibold transition-all"
              style={
                selectedGame === id
                  ? { background: `${sysColor}20`, color: sysColor, border: `1px solid ${sysColor}55` }
                  : { background: 'rgba(255,255,255,0.04)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }
              }
            >
              {gameIdToTitle(id)}
            </button>
          );
        })}
      </div>

      {loading ? (
        <div className="flex items-center justify-center py-12">
          <div className="animate-spin text-3xl">🔗</div>
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
                    background: unlocked ? 'rgba(2,132,199,0.1)' : 'rgba(255,255,255,0.02)',
                    border: unlocked ? '1px solid rgba(2,132,199,0.35)' : '1px solid rgba(255,255,255,0.06)',
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
                          background: unlocked ? 'rgba(2,132,199,0.25)' : 'rgba(255,255,255,0.06)',
                          color: unlocked ? GBA_LIGHT : 'var(--color-oasis-text-muted)',
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
// GBAPage
// ---------------------------------------------------------------------------

export default function GBAPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [joinCode, setJoinCode] = useState<string | null>(null);
  const { joinByCode, currentRoom } = useLobby();
  const navigate = useNavigate();

  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🔗 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'achievements', label: '🏅 Achievements' },
  ];

  const multiplayerCount = LINK_GAMES.filter((g) => g.maxPlayers > 1).length;

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: 'linear-gradient(135deg, rgba(2,132,199,0.18) 0%, rgba(2,132,199,0.05) 60%, transparent 100%)', border: '1px solid rgba(2,132,199,0.2)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: 'linear-gradient(135deg, #0284c7, #0c3461)' }}
        >
          🔗
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">
            Game Boy / GBC / GBA
          </h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            mGBA · Link cable over TCP relay · Trade, battle &amp; co-op
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <div className="flex items-center gap-2">
            <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {LINK_GAMES.length} games
            </span>
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(2,132,199,0.18)', color: GBA_LIGHT }}
            >
              {multiplayerCount} link cable
            </span>
          </div>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            4th–6th Gen · 1989–2001
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(2,132,199,0.18)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(2,132,199,0.12)', color: GBA_LIGHT, borderBottom: `2px solid ${GBA_COLOR}` }
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
