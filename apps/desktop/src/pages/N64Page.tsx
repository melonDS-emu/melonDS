import { useState, useEffect, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { useLobby } from '../context/LobbyContext';
import type { MockGame } from '../data/mock-games';
import { MOCK_GAMES } from '../data/mock-games';
import { JoinRoomModal } from '../components/JoinRoomModal';

const N64_GAMES = MOCK_GAMES.filter((g) => g.system === 'N64');

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

const SERVER_URL =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

interface N64Ranking { displayName: string; sessions: number; }
type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

function RankBadge({ rank }: { rank: number }) {
  if (rank === 1) return <span style={{ fontSize: 18 }}>🥇</span>;
  if (rank === 2) return <span style={{ fontSize: 18 }}>🥈</span>;
  if (rank === 3) return <span style={{ fontSize: 18 }}>🥉</span>;
  return <span style={{ display: 'inline-flex', alignItems: 'center', justifyContent: 'center', width: 24, height: 24, borderRadius: '50%', background: '#333', color: '#ccc', fontSize: 12, fontWeight: 700 }}>{rank}</span>;
}

function N64Banner() {
  return (
    <div style={{ background: 'linear-gradient(135deg, #007500 0%, #004d00 100%)', borderRadius: 12, padding: '16px 20px', marginBottom: 20, color: '#fff', fontSize: 13 }}>
      <strong>🎮 N64 · Mupen64Plus Emulator</strong>
      <span style={{ margin: '0 8px', opacity: 0.6 }}>·</span>
      <span>2–4 player relay netplay</span>
      <span style={{ margin: '0 8px', opacity: 0.6 }}>·</span>
      <span>Automatic .z64 / .n64 / .v64 ROM detection</span>
      <p style={{ marginTop: 6, marginBottom: 4, opacity: 0.85, fontSize: 12 }}>
        <strong>Backends:</strong> Mupen64Plus (default, direct netplay) · ParaLLEl-N64 via RetroArch (high-accuracy Vulkan) · Project64 (Windows fallback)
      </p>
      <p style={{ marginTop: 0, marginBottom: 0, opacity: 0.85, fontSize: 12 }}>
        <strong>Performance:</strong> dynarec (<em>fast</em> preset) for best framerate · interpreter (<em>accurate</em> preset) for maximum compatibility
      </p>
    </div>
  );
}

function AccessoryBadge({ type }: { type: 'expansion-pak' | 'transfer-pak' }) {
  const labels: Record<string, { label: string; color: string; textColor: string }> = {
    'expansion-pak': { label: '📦 Expansion Pak', color: '#1a3a5c', textColor: '#8ab4d8' },
    'transfer-pak':  { label: '🔌 Transfer Pak',  color: '#2a1a5c', textColor: '#b4a0d8' },
  };
  const { label, color, textColor } = labels[type];
  return (
    <span style={{ background: color, borderRadius: 4, padding: '2px 6px', fontSize: 10, color: textColor, border: '1px solid rgba(100,160,220,0.3)', whiteSpace: 'nowrap' }}>
      {label}
    </span>
  );
}

function N64GameCard({ game, openRooms, onQuickMatch, onHostRoom }: { game: MockGame; openRooms: number; onQuickMatch: (g: MockGame) => void; onHostRoom: (g: MockGame) => void }) {
  const needsExpansionPak = EXPANSION_PAK_GAMES.has(game.id);
  const needsTransferPak  = TRANSFER_PAK_GAMES.has(game.id);

  return (
    <div style={{ background: '#1a1a1a', borderRadius: 10, padding: 16, display: 'flex', flexDirection: 'column', gap: 8, border: '1px solid #2a2a2a' }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
        <span style={{ fontSize: 32 }}>{game.coverEmoji}</span>
        <div style={{ flex: 1, minWidth: 0 }}>
          <div style={{ fontWeight: 700, fontSize: 14 }}>{game.title}</div>
          <div style={{ color: '#888', fontSize: 12 }}>{game.maxPlayers === 1 ? '1 Player' : `Up to ${game.maxPlayers} players`}</div>
        </div>
        {openRooms > 0 && (
          <span style={{ background: '#0a2a0a', borderRadius: 4, padding: '2px 8px', fontSize: 11, color: '#4caf50', flexShrink: 0 }}>{openRooms} open</span>
        )}
      </div>
      {(needsExpansionPak || needsTransferPak) && (
        <div style={{ display: 'flex', gap: 4, flexWrap: 'wrap' }}>
          {needsExpansionPak && <AccessoryBadge type="expansion-pak" />}
          {needsTransferPak  && <AccessoryBadge type="transfer-pak" />}
        </div>
      )}
      <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
        {game.tags.map((t) => (
          <span key={t} style={{ background: '#2a2a2a', borderRadius: 4, padding: '2px 6px', fontSize: 11, color: '#aaa' }}>{t}</span>
        ))}
      </div>
      <div style={{ color: '#888', fontSize: 12, lineHeight: 1.4 }}>{game.description}</div>
      {game.maxPlayers > 1 && (
        <div style={{ display: 'flex', gap: 8, marginTop: 4 }}>
          <button onClick={() => onQuickMatch(game)} style={{ flex: 1, padding: '8px 0', borderRadius: 6, border: 'none', background: 'linear-gradient(135deg, #007500, #004d00)', color: '#fff', fontWeight: 700, fontSize: 13, cursor: 'pointer' }}>⚡ Quick Match</button>
          <button onClick={() => onHostRoom(game)} style={{ flex: 1, padding: '8px 0', borderRadius: 6, border: '1px solid #009900', background: 'transparent', color: '#4caf50', fontWeight: 700, fontSize: 13, cursor: 'pointer' }}>🎮 Host Room</button>
        </div>
      )}
    </div>
  );
}

function LeaderboardPanel() {
  const [rankings, setRankings] = useState<N64Ranking[]>([]);
  useEffect(() => {
    fetch(`${SERVER_URL}/api/leaderboard?orderBy=sessions&limit=10`)
      .then((r) => r.json())
      .then((data: { leaderboard: N64Ranking[] }) => setRankings(data.leaderboard ?? []))
      .catch(() => {});
  }, []);
  return (
    <div style={{ maxWidth: 500 }}>
      <h3 style={{ margin: '0 0 16px', fontSize: 16, color: '#4caf50' }}>🏆 Top N64 Players</h3>
      {rankings.length === 0 && <p style={{ color: '#666', fontSize: 13 }}>No rankings yet. Be the first to play!</p>}
      {rankings.map((r, i) => (
        <div key={r.displayName} style={{ display: 'flex', alignItems: 'center', gap: 12, padding: '10px 0', borderBottom: '1px solid #1a1a1a' }}>
          <RankBadge rank={i + 1} />
          <span style={{ flex: 1, fontWeight: 600 }}>{r.displayName}</span>
          <span style={{ color: '#888', fontSize: 13 }}>{r.sessions} sessions</span>
        </div>
      ))}
    </div>
  );
}

function GuideSection({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div style={{ marginBottom: 28 }}>
      <h3 style={{ margin: '0 0 12px', fontSize: 15, color: '#4caf50', borderBottom: '1px solid #222', paddingBottom: 6 }}>{title}</h3>
      {children}
    </div>
  );
}

function GuideRow({ label, value, note }: { label: string; value: string; note?: string }) {
  return (
    <div style={{ display: 'flex', gap: 12, padding: '7px 0', borderBottom: '1px solid #1c1c1c', fontSize: 13 }}>
      <span style={{ color: '#888', minWidth: 140 }}>{label}</span>
      <div>
        <span style={{ color: '#e0e0e0', fontWeight: 600 }}>{value}</span>
        {note && <span style={{ color: '#666', marginLeft: 8, fontSize: 12 }}>{note}</span>}
      </div>
    </div>
  );
}

function GuidePanel() {
  return (
    <div style={{ maxWidth: 720 }}>
      <GuideSection title="🖥️ Emulator Backends">
        <GuideRow label="Mupen64Plus" value="Default — best netplay" note="Direct --netplay-host/port args; dynarec for speed" />
        <GuideRow label="ParaLLEl-N64" value="High-accuracy (Vulkan)" note="RetroArch libretro core; requires Vulkan GPU" />
        <GuideRow label="Project64" value="Windows fallback" note="No built-in netplay CLI; relay-only" />
        <p style={{ color: '#666', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          Select your backend in <strong style={{ color: '#aaa' }}>Settings → Emulator Paths</strong>.
          For most players Mupen64Plus gives the best experience. ParaLLEl-N64 is ideal for graphically demanding titles that need accurate cycle emulation.
        </p>
      </GuideSection>

      <GuideSection title="🎚️ Performance Presets">
        <GuideRow label="Fast (dynarec)" value="--emumode 2" note="Best for online play; some accuracy trade-off" />
        <GuideRow label="Balanced (default)" value="(no flag)" note="Mupen64Plus default; good for most titles" />
        <GuideRow label="Accurate (interpreter)" value="--emumode 0" note="Maximum compatibility; higher CPU usage" />
        <p style={{ color: '#666', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          Use <strong style={{ color: '#aaa' }}>Fast</strong> for party games like Mario Kart 64 and Bomberman 64.
          Switch to <strong style={{ color: '#aaa' }}>Accurate</strong> if you notice graphical glitches or game-breaking bugs.
        </p>
      </GuideSection>

      <GuideSection title="💾 ROM Formats">
        <GuideRow label=".z64" value="Big-endian (native)" note="Best compatibility — preferred format" />
        <GuideRow label=".n64" value="Little-endian (byte-swapped)" note="Works with Mupen64Plus and most emulators" />
        <GuideRow label=".v64" value="Byte-swapped (Backup Unit)" note="Byte-pair swapped; supported by all major emulators" />
        <p style={{ color: '#666', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          If you experience issues with .n64 or .v64 files, convert them to .z64 using a free ROM conversion tool.
          Mupen64Plus auto-detects the byte order, so all three formats should work transparently.
        </p>
      </GuideSection>

      <GuideSection title="🔌 N64 Accessories">
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(210px, 1fr))', gap: 12, marginBottom: 8 }}>
          {[
            { icon: '💾', name: 'Controller Pak', desc: 'Memory card for games without cartridge saves. Enable --controller-pak standard in Mupen64Plus.' },
            { icon: '📦', name: 'Expansion Pak', desc: 'Required by Perfect Dark (4P) and Star Fox 64 VS. Enable in Mupen64Plus: set "RAM = 8" (8 MB) in the [Memory] section of mupen64plus.cfg.' },
            { icon: '🔌', name: 'Transfer Pak', desc: 'Reads Game Boy save data for Pokémon Stadium. Enable --transfer-pak flag in Mupen64Plus.' },
            { icon: '📳', name: 'Rumble Pak', desc: 'Force feedback. Enable --controller-pak rumble in Mupen64Plus (replaces Controller Pak slot).' },
          ].map(({ icon, name, desc }) => (
            <div key={name} style={{ background: '#1a1a1a', borderRadius: 8, padding: '12px 14px', border: '1px solid #2a2a2a' }}>
              <div style={{ fontSize: 22, marginBottom: 6 }}>{icon}</div>
              <div style={{ fontWeight: 700, fontSize: 13, marginBottom: 4 }}>{name}</div>
              <div style={{ color: '#777', fontSize: 12, lineHeight: 1.5 }}>{desc}</div>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="🌐 Netplay Tips">
        <GuideRow label="Ideal latency" value="< 60 ms" note="Best for competitive titles (Smash 64, GoldenEye)" />
        <GuideRow label="Good for party" value="60–120 ms" note="Party games (MK64, Mario Party, Bomberman 64)" />
        <GuideRow label="Turn-based" value="up to 150 ms" note="Golf, Stadium — timing not twitch-dependent" />
        <p style={{ color: '#666', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          N64 emulation is CPU-intensive. Close background applications before hosting.
          Wired Ethernet is strongly recommended over Wi-Fi for the host.
          Use the <strong style={{ color: '#aaa' }}>Connection Diagnostics</strong> page to check your measured ping.
        </p>
      </GuideSection>

      <GuideSection title="🎮 Controller Setup">
        <GuideRow label="Recommended" value="Xbox controller" note="Left stick → N64 analog, bumpers → L/R triggers" />
        <GuideRow label="PlayStation" value="DualShock 3/4/5" note="Left stick → N64 analog, L1/R1 → L/R triggers" />
        <GuideRow label="Keyboard" value="WASD + arrow keys" note="Usable but analog precision is reduced" />
        <p style={{ color: '#666', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          The N64 C-buttons (C-Up/Down/Left/Right) map to the right analog stick on modern controllers.
          Mupen64Plus will auto-detect Xbox and PlayStation controllers — manually rebind in <strong style={{ color: '#aaa' }}>mupen64plus.cfg</strong> (<code style={{ color: '#888', fontSize: 11 }}>~/.config/mupen64plus/</code> on Linux, <code style={{ color: '#888', fontSize: 11 }}>%APPDATA%\Mupen64Plus\</code> on Windows) if needed.
        </p>
      </GuideSection>
    </div>
  );
}

function LobbyPanel({ onJoin }: { onJoin: (roomCode: string) => void }) {
  const { createRoom } = useLobby();
  const [openRoomsMap, setOpenRoomsMap] = useState<Record<string, number>>({});

  useEffect(() => {
    const fetchRoomCounts = async () => {
      const results: Record<string, number> = {};
      await Promise.all(
        N64_GAMES.map(async (game) => {
          try {
            const res = await fetch(`${SERVER_URL}/api/rooms?gameId=${encodeURIComponent(game.id)}`);
            const data = (await res.json()) as { rooms?: unknown[] };
            results[game.id] = data.rooms?.length ?? 0;
          } catch {
            results[game.id] = 0;
          }
        }),
      );
      setOpenRoomsMap(results);
    };
    void fetchRoomCounts();
  }, []);

  const handleQuickMatch = useCallback(async (game: MockGame) => {
    const displayName = localStorage.getItem('retro-oasis-display-name') ?? 'Player';
    try {
      const res = await fetch(`${SERVER_URL}/api/rooms?gameId=${encodeURIComponent(game.id)}`);
      const data = (await res.json()) as { rooms?: Array<{ roomCode: string }> };
      const rooms = data.rooms ?? [];
      if (rooms.length > 0) {
        onJoin(rooms[0].roomCode);
      } else {
        createRoom({ name: `${game.title} Room`, gameId: game.id, gameTitle: game.title, system: game.system, isPublic: true, maxPlayers: game.maxPlayers }, displayName);
      }
    } catch {
      createRoom({ name: `${game.title} Room`, gameId: game.id, gameTitle: game.title, system: game.system, isPublic: true, maxPlayers: game.maxPlayers }, displayName);
    }
  }, [createRoom, onJoin]);

  const handleHostRoom = useCallback((game: MockGame) => {
    const displayName = localStorage.getItem('retro-oasis-display-name') ?? 'Player';
    createRoom({ name: `${game.title} Room`, gameId: game.id, gameTitle: game.title, system: game.system, isPublic: true, maxPlayers: game.maxPlayers }, displayName);
  }, [createRoom]);

  return (
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(260px, 1fr))', gap: 16 }}>
      {N64_GAMES.map((game) => (
        <N64GameCard
          key={game.id}
          game={game}
          openRooms={openRoomsMap[game.id] ?? 0}
          onQuickMatch={handleQuickMatch}
          onHostRoom={handleHostRoom}
        />
      ))}
    </div>
  );
}

export default function N64Page() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [joinCode, setJoinCode] = useState<string | null>(null);
  const { joinByCode, currentRoom } = useLobby();
  const navigate = useNavigate();

  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  const tabs: Array<{ id: ActiveTab; label: string }> = [
    { id: 'lobby',       label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide',       label: '📖 Guide' },
  ];

  return (
    <div style={{ padding: 24, maxWidth: 960, margin: '0 auto' }}>
      <h1 style={{ margin: '0 0 4px', fontSize: 24, fontWeight: 800 }}>🎮 Nintendo 64</h1>
      <p style={{ margin: '0 0 20px', color: '#888', fontSize: 14 }}>Classic N64 games — up to 4 players via Mupen64Plus relay netplay</p>
      <N64Banner />
      <div style={{ display: 'flex', gap: 8, marginBottom: 20 }}>
        {tabs.map(({ id, label }) => (
          <button
            key={id}
            onClick={() => setActiveTab(id)}
            style={{ padding: '8px 18px', borderRadius: 6, border: activeTab === id ? '2px solid #009900' : '1px solid #333', background: activeTab === id ? '#009900' : 'transparent', color: activeTab === id ? '#fff' : '#aaa', fontWeight: 700, fontSize: 13, cursor: 'pointer' }}
          >
            {label}
          </button>
        ))}
      </div>
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
