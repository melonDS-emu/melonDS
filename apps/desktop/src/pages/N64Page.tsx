import { useState, useEffect, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import type { MockGame } from '../data/mock-games';
import { MOCK_GAMES } from '../data/mock-games';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const N64_GAMES = MOCK_GAMES.filter((g) => g.system === 'N64');
const N64_COLOR = '#009900';
const N64_COLOR_DARK = '#004d00';

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

interface N64Ranking { displayName: string; sessions: number; }

/** Games that require or strongly benefit from the Expansion Pak */
const EXPANSION_PAK_GAMES = new Set([
  'n64-perfect-dark',
  'n64-star-fox-64',
  'n64-donkey-kong-64',
  'n64-majoras-mask',
]);

/** Games that support Transfer Pak (Game Boy cartridge reads) */
const TRANSFER_PAK_GAMES = new Set([
  'n64-pokemon-stadium',
  'n64-pokemon-stadium-2',
]);

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

function N64Banner() {
  return (
    <div
      className="rounded-2xl p-4 flex items-start gap-3"
      style={{ background: 'rgba(0,153,0,0.08)', border: '1px solid rgba(0,153,0,0.25)' }}
    >
      <span className="text-xl mt-0.5">🎮</span>
      <div>
        <p className="text-sm font-bold" style={{ color: N64_COLOR }}>
          Mupen64Plus · 2–4 Player Relay Netplay
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Automatic <span className="font-semibold text-white">.z64 / .n64 / .v64</span> ROM detection.
          Backends: Mupen64Plus (default, direct netplay) · ParaLLEl-N64 via RetroArch (Vulkan, high-accuracy) · Project64 (Windows fallback).
          Use <span className="font-semibold text-white">dynarec (fast)</span> preset for best online framerate.
        </p>
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Accessory badge
// ---------------------------------------------------------------------------

function AccessoryBadge({ type }: { type: 'expansion-pak' | 'transfer-pak' }) {
  const labels: Record<string, { label: string; bg: string; color: string; border: string }> = {
    'expansion-pak': { label: '📦 Expansion Pak', bg: 'rgba(0,153,0,0.1)', color: '#4ade80', border: '1px solid rgba(0,153,0,0.3)' },
    'transfer-pak':  { label: '🔌 Transfer Pak',  bg: 'rgba(99,102,241,0.1)', color: '#a5b4fc', border: '1px solid rgba(99,102,241,0.3)' },
  };
  const s = labels[type];
  return (
    <span
      className="text-xs px-2 py-0.5 rounded-full font-semibold"
      style={{ background: s.bg, color: s.color, border: s.border, whiteSpace: 'nowrap' }}
    >
      {s.label}
    </span>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function N64GameCard({
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
  const needsExpansionPak = EXPANSION_PAK_GAMES.has(game.id);
  const needsTransferPak  = TRANSFER_PAK_GAMES.has(game.id);
  const multiPlayer = game.maxPlayers > 1;

  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{ background: 'linear-gradient(135deg, rgba(0,153,0,0.07), rgba(0,153,0,0.02))', border: '1px solid rgba(0,153,0,0.15)' }}
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

      {/* Accessory badges */}
      {(needsExpansionPak || needsTransferPak) && (
        <div className="flex flex-wrap gap-1.5">
          {needsExpansionPak && <AccessoryBadge type="expansion-pak" />}
          {needsTransferPak  && <AccessoryBadge type="transfer-pak" />}
        </div>
      )}

      {/* Tags */}
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
            style={{ background: 'rgba(0,153,0,0.12)', color: '#4ade80', border: '1px solid rgba(0,153,0,0.25)' }}
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
            style={{ background: `linear-gradient(90deg, ${N64_COLOR}, ${N64_COLOR_DARK})`, color: '#fff' }}
          >
            ⚡ Quick Match
          </button>
          <button
            onClick={() => onHost(game)}
            className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
            style={{ background: 'rgba(0,153,0,0.08)', border: '1px solid rgba(0,153,0,0.35)', color: N64_COLOR }}
          >
            🎮 Host Room
          </button>
        </div>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel({ onJoin }: { onJoin: (roomCode: string) => void }) {
  const { publicRooms, createRoom } = useLobby();
  const [hostGame, setHostGame] = useState<MockGame | null>(null);
  const [openRoomsMap, setOpenRoomsMap] = useState<Record<string, number>>({});

  useEffect(() => {
    const fetchRoomCounts = async () => {
      const results: Record<string, number> = {};
      await Promise.all(
        N64_GAMES.map(async (game) => {
          try {
            const data = await apiFetch<{ rooms?: unknown[] }>(`/api/rooms?gameId=${encodeURIComponent(game.id)}`);
            results[game.id] = (data.rooms?.length ?? 0);
          } catch {
            results[game.id] = 0;
          }
        }),
      );
      setOpenRoomsMap(results);
    };
    void fetchRoomCounts();
  }, []);

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length || (openRoomsMap[gameId] ?? 0),
    [publicRooms, openRoomsMap],
  );

  const handleQuickMatch = useCallback(async (game: MockGame) => {
    const open = publicRooms.find((r) => r.gameId === game.id && r.status === 'waiting');
    if (open) { onJoin(open.roomCode); return; }
    try {
      const data = await apiFetch<{ rooms?: Array<{ roomCode: string }> }>(`/api/rooms?gameId=${encodeURIComponent(game.id)}`);
      const rooms = data.rooms ?? [];
      if (rooms.length > 0) {
        onJoin(rooms[0].roomCode);
      } else {
        setHostGame(game);
      }
    } catch {
      setHostGame(game);
    }
  }, [publicRooms, onJoin]);

  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';

  return (
    <div className="space-y-6">
      <N64Banner />

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {N64_GAMES.map((game) => (
          <N64GameCard
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
    </div>
  );
}

// ---------------------------------------------------------------------------
// Leaderboard panel
// ---------------------------------------------------------------------------

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<N64Ranking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: N64Ranking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
      .then((d) => setRankings(d.leaderboard ?? []))
      .catch(() => setRankings([]))
      .finally(() => setLoading(false));
  }, []);

  if (loading) {
    return (
      <div className="flex items-center justify-center py-16">
        <div className="animate-spin text-3xl">🎮</div>
      </div>
    );
  }

  if (rankings.length === 0) {
    return (
      <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <p className="text-4xl mb-3">🏆</p>
        <p className="font-semibold">No rankings yet</p>
        <p className="text-sm mt-1">Play some N64 sessions to appear here!</p>
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
          style={{ background: 'rgba(0,153,0,0.05)', border: '1px solid rgba(0,153,0,0.12)' }}
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

function GuideSection({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div
      className="rounded-2xl p-5 space-y-3"
      style={{ background: 'rgba(0,153,0,0.05)', border: '1px solid rgba(0,153,0,0.15)' }}
    >
      <h3 className="text-sm font-bold" style={{ color: N64_COLOR }}>{title}</h3>
      {children}
    </div>
  );
}

function GuideRow({ label, value, note }: { label: string; value: string; note?: string }) {
  return (
    <div className="flex gap-3 text-xs py-1.5" style={{ borderBottom: '1px solid rgba(0,153,0,0.08)' }}>
      <span className="min-w-[140px]" style={{ color: 'var(--color-oasis-text-muted)' }}>{label}</span>
      <div>
        <span className="font-semibold text-white">{value}</span>
        {note && <span className="ml-2" style={{ color: 'var(--color-oasis-text-muted)' }}>{note}</span>}
      </div>
    </div>
  );
}

function GuidePanel() {
  return (
    <div className="space-y-4 max-w-2xl">
      <GuideSection title="🖥️ Emulator Backends">
        <GuideRow label="Mupen64Plus"     value="Default — best netplay"       note="Direct --netplay-host/port args; dynarec for speed" />
        <GuideRow label="ParaLLEl-N64"    value="High-accuracy (Vulkan)"        note="RetroArch libretro core; requires Vulkan GPU" />
        <GuideRow label="Project64"       value="Windows fallback"              note="No built-in netplay CLI; relay-only" />
        <p className="text-xs pt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Select your backend in <strong className="text-white">Settings → Emulator Paths</strong>. Mupen64Plus gives the best experience for most players. ParaLLEl-N64 is ideal for graphically demanding titles.
        </p>
      </GuideSection>

      <GuideSection title="🎚️ Performance Presets">
        <GuideRow label="Fast (dynarec)"       value="--emumode 2"  note="Best for online play; some accuracy trade-off" />
        <GuideRow label="Balanced (default)"   value="(no flag)"    note="Mupen64Plus default; good for most titles" />
        <GuideRow label="Accurate (interpret)" value="--emumode 0"  note="Maximum compatibility; higher CPU usage" />
        <p className="text-xs pt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Use <strong className="text-white">Fast</strong> for party games (MK64, Bomberman 64). Switch to <strong className="text-white">Accurate</strong> if you see graphical glitches.
        </p>
      </GuideSection>

      <GuideSection title="💾 ROM Formats">
        <GuideRow label=".z64" value="Big-endian (native)"           note="Best compatibility — preferred format" />
        <GuideRow label=".n64" value="Little-endian (byte-swapped)"  note="Works with Mupen64Plus and most emulators" />
        <GuideRow label=".v64" value="Byte-swapped (Backup Unit)"    note="Supported by all major emulators" />
      </GuideSection>

      <GuideSection title="🔌 N64 Accessories">
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
          {[
            { icon: '💾', name: 'Controller Pak', desc: 'Memory card for saves. Enable --controller-pak standard in Mupen64Plus.' },
            { icon: '📦', name: 'Expansion Pak', desc: 'Required by Perfect Dark (4P) and Star Fox 64 VS. Set RAM = 8 in mupen64plus.cfg.' },
            { icon: '🔌', name: 'Transfer Pak', desc: 'Reads Game Boy saves for Pokémon Stadium. Enable --transfer-pak flag.' },
            { icon: '📳', name: 'Rumble Pak', desc: 'Force feedback. Enable --controller-pak rumble (replaces Controller Pak slot).' },
          ].map(({ icon, name, desc }) => (
            <div
              key={name}
              className="rounded-xl p-3"
              style={{ background: 'rgba(0,153,0,0.05)', border: '1px solid rgba(0,153,0,0.12)' }}
            >
              <p className="text-lg mb-1">{icon}</p>
              <p className="text-xs font-bold text-white mb-1">{name}</p>
              <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{desc}</p>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="🌐 Netplay Tips">
        <GuideRow label="Ideal latency"  value="< 60 ms"     note="Best for competitive titles (Smash 64, GoldenEye)" />
        <GuideRow label="Good for party" value="60–120 ms"   note="Party games (MK64, Mario Party, Bomberman 64)" />
        <GuideRow label="Turn-based"     value="up to 150 ms" note="Golf, Stadium — timing not twitch-dependent" />
        <p className="text-xs pt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          N64 emulation is CPU-intensive. Close background apps before hosting. Wired Ethernet is strongly recommended.
        </p>
      </GuideSection>

      <GuideSection title="🎮 Controller Setup">
        <GuideRow label="Recommended" value="Xbox controller"          note="Left stick → N64 analog, bumpers → L/R triggers" />
        <GuideRow label="PlayStation"  value="DualShock 3/4/5"          note="Left stick → N64 analog, L1/R1 → L/R triggers" />
        <GuideRow label="Keyboard"     value="WASD + arrow keys"         note="Usable but analog precision is reduced" />
        <p className="text-xs pt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          C-buttons map to the right analog stick. Mupen64Plus auto-detects Xbox and PlayStation controllers — manually rebind in <strong className="text-white">mupen64plus.cfg</strong> if needed.
        </p>
      </GuideSection>
    </div>
  );
}

// ---------------------------------------------------------------------------
// N64Page
// ---------------------------------------------------------------------------

export default function N64Page() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [joinCode, setJoinCode] = useState<string | null>(null);
  const { joinByCode, currentRoom } = useLobby();
  const navigate = useNavigate();

  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby',       label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide',       label: '📖 Guide' },
  ];

  const multiplayerCount = N64_GAMES.filter((g) => g.maxPlayers > 1).length;

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: 'linear-gradient(135deg, rgba(0,153,0,0.18) 0%, rgba(0,153,0,0.05) 60%, transparent 100%)', border: '1px solid rgba(0,153,0,0.2)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${N64_COLOR}, ${N64_COLOR_DARK})` }}
        >
          🎮
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Nintendo 64</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Mupen64Plus · Up to 4-player relay netplay · No port forwarding needed
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <div className="flex items-center gap-2">
            <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {N64_GAMES.length} games
            </span>
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: 'rgba(0,153,0,0.15)', color: N64_COLOR }}
            >
              {multiplayerCount} multiplayer
            </span>
          </div>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            5th Generation · 1996
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(0,153,0,0.15)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(0,153,0,0.1)', color: N64_COLOR, borderBottom: `2px solid ${N64_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'lobby'       && <LobbyPanel onJoin={(code) => setJoinCode(code)} />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'guide'       && <GuidePanel />}

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
