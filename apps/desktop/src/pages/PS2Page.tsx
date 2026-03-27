import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { MOCK_GAMES } from '../data/mock-games';
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

const SYSTEM_COLOR = '#00439C';
const SYSTEM_EMOJI = '💙';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

interface PS2Ranking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Game data — sourced from MOCK_GAMES
// ---------------------------------------------------------------------------

const PS2_GAMES = MOCK_GAMES.filter((g) => g.system === 'PS2');

type PS2Game = (typeof PS2_GAMES)[number];

// Games that require PCSX2 multitap for full player count
const MULTITAP_GAMES = new Set(['ps2-timesplitters-2']);

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

function PS2Banner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ backgroundColor: 'rgba(0,67,156,0.1)', border: '1px solid rgba(0,67,156,0.35)' }}
    >
      <span className="text-xl">{SYSTEM_EMOJI}</span>
      <div>
        <p className="text-sm font-bold" style={{ color: '#60a5fa' }}>
          Sony PlayStation 2 Relay Netplay via PCSX2
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          All PS2 sessions use RetroOasis TCP relay netplay with PCSX2. No port forwarding
          required. ROM files (.iso, .bin/.cue, .chd) are auto-detected from your library.
          Requires PS2 BIOS files for full compatibility.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function PS2GameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: PS2Game;
  onHost: (game: PS2Game) => void;
  onQuickMatch: (game: PS2Game) => void;
  openRooms: number;
}) {
  const isMultitap = MULTITAP_GAMES.has(game.id);
  const isCompetitive = game.tags.some((t) => t === 'Versus' || t === 'Competitive');
  const isCoop = game.tags.some((t) => t === 'Co-op');

  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: 'linear-gradient(135deg, rgba(0,67,156,0.12), rgba(0,26,92,0.06))',
        border: '1px solid rgba(0,67,156,0.2)',
      }}
    >
      <div className="flex items-start justify-between gap-2">
        <div className="flex items-center gap-2">
          <span className="text-2xl">{game.coverEmoji}</span>
          <div>
            <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.maxPlayers === 1 ? '1P' : game.maxPlayers === 2 ? '2P' : `Up to ${game.maxPlayers}P`}
              {isMultitap && ' · Multitap'}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1">
          {isCompetitive && (
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(251,146,60,0.15)', color: '#fb923c', border: '1px solid rgba(251,146,60,0.3)' }}
            >
              Competitive
            </span>
          )}
          {isCoop && (
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
              style={{ background: 'rgba(96,165,250,0.1)', color: '#60a5fa', border: '1px solid rgba(96,165,250,0.25)' }}
            >
              {openRooms} open
            </span>
          )}
        </div>
      </div>

      <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {game.description}
      </p>

      <div className="flex gap-2 mt-auto">
        <button
          onClick={() => onQuickMatch(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, #001a5c)`, color: '#fff' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: 'rgba(0,67,156,0.12)', border: '1px solid rgba(0,67,156,0.35)', color: '#60a5fa' }}
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
  const [rankings, setRankings] = useState<PS2Ranking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: PS2Ranking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
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
        <p className="text-sm mt-1">Play some PS2 sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(0,67,156,0.08)', border: '1px solid rgba(0,67,156,0.18)' }}
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
// Guide panel
// ---------------------------------------------------------------------------

function GuidePanel() {
  return (
    <div className="space-y-6 max-w-2xl">
      <section>
        <h3 className="text-base font-bold text-white mb-2">🖥️ Emulator Backends</h3>
        <div className="space-y-2 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <p><span className="font-semibold text-white">PCSX2</span> — Primary backend. High accuracy, Vulkan/OpenGL renderer, fast boot. Best for most titles.</p>
          <p><span className="font-semibold text-white">RetroArch (PCSX2 core)</span> — Fallback via RetroArch if standalone PCSX2 is unavailable.</p>
          <p>Download PCSX2: <span className="font-mono text-blue-400">pcsx2.net</span></p>
        </div>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">💾 BIOS Setup</h3>
        <div className="space-y-1 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <p>PS2 BIOS is required for full compatibility.</p>
          <p>Common filename: <span className="font-mono text-blue-400">SCPH-70012.bin</span> (NTSC-U)</p>
          <p>Place in PCSX2 bios directory (usually <span className="font-mono">~/.config/PCSX2/bios/</span>).</p>
        </div>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">📀 ROM Formats</h3>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Supported: <span className="font-mono text-white">.iso</span>, <span className="font-mono text-white">.bin/.cue</span>, <span className="font-mono text-white">.chd</span>
        </p>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">🎮 Multitap</h3>
        <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Games marked <span className="font-semibold text-white">Multitap</span> require the <span className="font-mono">--multitap</span> flag for 3–8 player sessions. RetroOasis passes this automatically.
        </p>
      </section>
      <section>
        <h3 className="text-base font-bold text-white mb-2">🌐 Netplay Tips</h3>
        <ul className="space-y-1 text-xs list-disc list-inside" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <li>PCSX2 relay sessions work best under 100 ms for action and fighting games.</li>
          <li>Enable Vulkan renderer for best performance on modern GPUs.</li>
          <li>Fast boot (<span className="font-mono">--fast-boot</span>) skips the PS2 BIOS splash — enabled by default.</li>
          <li>No port forwarding needed — RetroOasis TCP relay handles all connections.</li>
        </ul>
      </section>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({ games }: { games: PS2Game[] }) {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<PS2Game | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const handleQuickMatch = useCallback(
    async (game: PS2Game) => {
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
      <PS2Banner />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(0,67,156,0.1)', color: '#60a5fa', border: '1px solid rgba(0,67,156,0.3)' }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {games.map((game) => (
          <PS2GameCard
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
// PS2Page
// ---------------------------------------------------------------------------

export default function PS2Page() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide', label: '📖 Guide' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Header */}
      <div className="flex items-center gap-4">
        <span className="text-5xl">{SYSTEM_EMOJI}</span>
        <div>
          <h1 className="text-3xl font-black text-white">Sony PlayStation 2</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Online relay sessions via PCSX2 · Up to 8 players · No port forwarding needed
          </p>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(0,67,156,0.2)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(0,67,156,0.15)', color: '#60a5fa', borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby' && <LobbyPanel games={PS2_GAMES} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'guide' && <GuidePanel />}
    </div>
  );
}
