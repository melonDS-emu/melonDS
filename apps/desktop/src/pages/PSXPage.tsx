import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import { JoinRoomModal } from '../components/JoinRoomModal';
import { MOCK_GAMES } from '../data/mock-games';

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

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

const PSX_GAMES = MOCK_GAMES.filter((g) => g.system === 'PSX');

/** Games that require the Multitap accessory for full player-count support */
const MULTITAP_GAMES = new Set([
  'psx-crash-bash',
  'psx-crash-team-racing',
  'psx-worms-armageddon',
]);

/** Games that require DualShock analog mode */
const ANALOG_REQUIRED_GAMES = new Set([
  'psx-gran-turismo-2',
]);

interface PSXRanking {
  displayName: string;
  sessions: number;
}

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
// Accessory badge
// ---------------------------------------------------------------------------

function AccessoryBadge({ type }: { type: 'multitap' | 'analog' }) {
  const labels: Record<string, { label: string; bg: string; color: string; border: string }> = {
    multitap: { label: '🔌 Multitap', bg: 'rgba(99,102,241,0.15)', color: '#a5b4fc', border: '1px solid rgba(99,102,241,0.35)' },
    analog:   { label: '🕹️ Analog',   bg: 'rgba(234,179,8,0.12)',  color: '#fde68a', border: '1px solid rgba(234,179,8,0.3)' },
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
// Info banner
// ---------------------------------------------------------------------------

function PSXBanner() {
  return (
    <div
      className="rounded-2xl p-4 space-y-2"
      style={{ backgroundColor: 'rgba(128,128,128,0.1)', border: '1px solid rgba(128,128,128,0.35)' }}
    >
      <p className="text-sm font-bold" style={{ color: '#d1d5db' }}>
        {SYSTEM_EMOJI} Sony PlayStation · DuckStation · Mednafen PSX (Beetle HW) · RetroArch
      </p>
      <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <strong style={{ color: '#d1d5db' }}>DuckStation</strong> is the default launcher — fast, accurate, and great for local play.
        For highest accuracy and synced netplay use <strong style={{ color: '#d1d5db' }}>Mednafen PSX (Beetle PSX HW)</strong> via RetroArch,
        or any RetroArch PSX core. ROM formats supported: <code>.bin/.cue</code>, <code>.iso</code>, <code>.chd</code>, <code>.pbp</code>.
        A PS1 BIOS (<code>scph5501.bin</code> recommended) is required for best compatibility.
      </p>
      <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <strong style={{ color: '#d1d5db' }}>Multitap</strong> games support 3–4 players via DuckStation's multitap relay.
        <strong style={{ color: '#d1d5db' }}> DualShock analog</strong> mode is required for racing simulations.
      </p>
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
  game: (typeof PSX_GAMES)[number];
  onHost: (game: (typeof PSX_GAMES)[number]) => void;
  onQuickMatch: (game: (typeof PSX_GAMES)[number]) => void;
  openRooms: number;
}) {
  const needsMultitap = MULTITAP_GAMES.has(game.id);
  const needsAnalog   = ANALOG_REQUIRED_GAMES.has(game.id);

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
          <span className="text-2xl">{game.coverEmoji}</span>
          <div>
            <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
            <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.maxPlayers === 1 ? '1P' : game.maxPlayers === 2 ? '2P' : `Up to ${game.maxPlayers}P`}
              {game.tags.includes('Competitive') && ' · Competitive'}
              {game.tags.includes('Co-op') && ' · Co-op'}
            </p>
          </div>
        </div>
        <div className="flex flex-col items-end gap-1">
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

      {(needsMultitap || needsAnalog) && (
        <div className="flex gap-2 flex-wrap">
          {needsMultitap && <AccessoryBadge type="multitap" />}
          {needsAnalog   && <AccessoryBadge type="analog" />}
        </div>
      )}

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
// Guide panel
// ---------------------------------------------------------------------------

function GuideSection({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <div style={{ marginBottom: 28 }}>
      <h3 style={{ margin: '0 0 12px', fontSize: 15, color: '#d1d5db', borderBottom: '1px solid rgba(128,128,128,0.2)', paddingBottom: 6 }}>
        {title}
      </h3>
      {children}
    </div>
  );
}

function GuideRow({ label, value, note }: { label: string; value: string; note?: string }) {
  return (
    <div style={{ display: 'flex', gap: 12, padding: '7px 0', borderBottom: '1px solid rgba(128,128,128,0.1)', fontSize: 13 }}>
      <span style={{ color: '#9ca3af', minWidth: 160 }}>{label}</span>
      <div>
        <span style={{ color: '#e0e0e0', fontWeight: 600 }}>{value}</span>
        {note && <span style={{ color: '#6b7280', marginLeft: 8, fontSize: 12 }}>{note}</span>}
      </div>
    </div>
  );
}

function GuidePanel() {
  return (
    <div style={{ maxWidth: 720 }}>
      <GuideSection title="🖥️ Emulator Backends">
        <GuideRow label="DuckStation" value="Default — local + relay" note="Fast, accurate; --pgxp --fast-boot --multitap --analog flags" />
        <GuideRow label="Mednafen PSX (Beetle HW)" value="Highest accuracy" note="RetroArch libretro core; hardware renderer + PGXP" />
        <GuideRow label="RetroArch (PCSX-ReARMed)" value="Best netplay" note="Reliable synced online; use for competitive sessions" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          Select your backend in <strong style={{ color: '#9ca3af' }}>Settings → Emulator Paths</strong>.
          DuckStation excels for local play and quick sessions. Use <strong style={{ color: '#9ca3af' }}>Mednafen PSX HW</strong> or
          RetroArch for the most accurate and sync-stable online play.
        </p>
      </GuideSection>

      <GuideSection title="🔌 PSX Accessories">
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: 12, marginBottom: 8 }}>
          {[
            { icon: '🔌', name: 'Multitap', desc: 'Required for 3–4 player games (Crash Bash, CTR, Worms). Enable --multitap in DuckStation.' },
            { icon: '🕹️', name: 'DualShock Analog', desc: 'Required for Gran Turismo 2 and analog-only titles. Enable --analog in DuckStation.' },
            { icon: '💾', name: 'Memory Card', desc: 'DuckStation creates virtual memory cards automatically per game.' },
            { icon: '⚡', name: 'PGXP', desc: 'Precision Geometry Transform Pipeline — eliminates texture warping. Enable --pgxp in DuckStation.' },
          ].map(({ icon, name, desc }) => (
            <div key={name} style={{ background: 'rgba(128,128,128,0.08)', borderRadius: 8, padding: '12px 14px', border: '1px solid rgba(128,128,128,0.18)' }}>
              <div style={{ fontSize: 22, marginBottom: 6 }}>{icon}</div>
              <div style={{ fontWeight: 700, fontSize: 13, marginBottom: 4, color: '#d1d5db' }}>{name}</div>
              <div style={{ color: '#6b7280', fontSize: 12, lineHeight: 1.5 }}>{desc}</div>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="💾 ROM Formats">
        <GuideRow label=".bin / .cue" value="CD image + cue sheet" note="Most common rip format — preferred for multi-track games" />
        <GuideRow label=".chd" value="Compressed Hard Disk" note="Smaller file size; supported by DuckStation and RetroArch" />
        <GuideRow label=".iso" value="ISO 9660 disc image" note="Works for data-only discs; no audio track support" />
        <GuideRow label=".pbp" value="PlayStation Portable" note="PSP format; also supported by DuckStation" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          For multi-disc games (e.g. Final Fantasy VII) use <code style={{ color: '#9ca3af', fontSize: 11 }}>.chd</code> or a
          <code style={{ color: '#9ca3af', fontSize: 11 }}>.m3u</code> playlist pointing to each disc's <code style={{ color: '#9ca3af', fontSize: 11 }}>.cue</code> file.
        </p>
      </GuideSection>

      <GuideSection title="🔧 BIOS Setup">
        <GuideRow label="scph1001.bin" value="v2.2 USA (required)" note="Minimum required BIOS for DuckStation" />
        <GuideRow label="scph5501.bin" value="v3.0 USA (recommended)" note="Best compatibility; use for SOTN and netplay sessions" />
        <GuideRow label="scph7001.bin" value="v4.1 USA" note="Latest US BIOS; compatible with all US releases" />
        <GuideRow label="scph5502.bin" value="v3.0 Europe" note="Required for PAL-region games" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          Place BIOS files in DuckStation's BIOS directory or RetroArch's <code style={{ color: '#9ca3af', fontSize: 11 }}>system/</code> folder.
          The app will alert you if a required BIOS is missing before launching.
        </p>
      </GuideSection>

      <GuideSection title="🌐 Netplay Tips">
        <GuideRow label="Fighters (Tekken, SF)" value="< 80 ms" note="Input-driven; low latency critical for frame-perfect play" />
        <GuideRow label="Racing (CTR, GT2)" value="< 80 ms" note="Deterministic at low latency; CTR boost timing sensitive" />
        <GuideRow label="Party (Crash Bash)" value="80–120 ms" note="Minigame bursts tolerate moderate latency" />
        <GuideRow label="Turn-based (Worms, FF7)" value="up to 200 ms" note="Timing-insensitive; comfortable across continents" />
        <GuideRow label="Single-player co-view" value="any" note="Spectate/co-view sessions work at any latency" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          PSX emulation is relatively lightweight. For the smoothest hosting experience, use
          wired Ethernet and ensure your BIOS version matches your partner's.
          Check the <strong style={{ color: '#9ca3af' }}>Connection Diagnostics</strong> page for your measured ping.
        </p>
      </GuideSection>

      <GuideSection title="🎮 Controller Setup">
        <GuideRow label="Recommended" value="PlayStation DualShock" note="Best button mapping; analog required for GT2 / Ape Escape" />
        <GuideRow label="Xbox controller" value="Supported" note="Face buttons map correctly; triggers act as L2/R2" />
        <GuideRow label="Keyboard" value="Usable" note="D-Pad games work well; analog precision is reduced" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          DuckStation auto-detects connected controllers and applies a sensible default mapping.
          For custom bindings open DuckStation's <strong style={{ color: '#9ca3af' }}>Settings → Controllers</strong>.
          RetroArch bindings live under <strong style={{ color: '#9ca3af' }}>Settings → Input → Port 1 Binds</strong>.
        </p>
      </GuideSection>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Lobby panel
// ---------------------------------------------------------------------------

function LobbyPanel() {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const displayName = localStorage.getItem('retro-oasis-display-name') ?? '';
  const [hostGame, setHostGame] = useState<(typeof PSX_GAMES)[number] | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const handleQuickMatch = useCallback(
    async (game: (typeof PSX_GAMES)[number]) => {
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
        {PSX_GAMES.map((game) => (
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
// PSXPage
// ---------------------------------------------------------------------------

export default function PSXPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');

  const tabs: { id: ActiveTab; label: string }[] = [
    { id: 'lobby',       label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide',       label: '📖 Guide' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Header */}
      <div className="flex items-center gap-4">
        <span className="text-5xl">{SYSTEM_EMOJI}</span>
        <div>
          <h1 className="text-3xl font-black text-white">Sony PlayStation</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            DuckStation · Mednafen PSX · RetroArch · Up to 4 players · No port forwarding needed
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
      {activeTab === 'lobby'       && <LobbyPanel />}
      {activeTab === 'leaderboard' && <LeaderboardPanel />}
      {activeTab === 'guide'       && <GuidePanel />}
    </div>
  );
}

