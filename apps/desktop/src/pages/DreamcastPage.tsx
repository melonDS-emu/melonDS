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

const SYSTEM_COLOR = '#FF6600';
const SYSTEM_EMOJI = '🟠';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

const DC_GAMES = MOCK_GAMES.filter((g) => g.system === 'Dreamcast');

/** Games with prominent VMU (Visual Memory Unit) save features */
const VMU_GAMES = new Set([
  'dc-sonic-adventure-2',  // Chao Garden VMU mini-game
  'dc-virtua-tennis',      // save player profiles to VMU
  'dc-nba-2k2',            // career saves use VMU
  'dc-gauntlet-legends',   // character save data on VMU
]);

/** Games that play best in VGA mode (progressive scan) */
const VGA_GAMES = new Set([
  'dc-soul-calibur',
  'dc-dead-or-alive-2',
  'dc-ikaruga',
  'dc-marvel-vs-capcom-2',
  'dc-capcom-vs-snk',
  'dc-tech-romancer',
]);

interface DreamcastRanking {
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

function AccessoryBadge({ type }: { type: 'vmu' | 'vga' }) {
  const labels: Record<string, { label: string; bg: string; color: string; border: string }> = {
    vmu: { label: '💾 VMU', bg: 'rgba(251,146,60,0.15)', color: '#fdba74', border: '1px solid rgba(251,146,60,0.35)' },
    vga: { label: '🖥️ VGA', bg: 'rgba(74,222,128,0.1)',  color: '#4ade80', border: '1px solid rgba(74,222,128,0.25)' },
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

function DreamcastBanner() {
  return (
    <div
      className="rounded-2xl p-4 space-y-2"
      style={{ backgroundColor: 'rgba(255,102,0,0.1)', border: '1px solid rgba(255,102,0,0.35)' }}
    >
      <p className="text-sm font-bold" style={{ color: '#fb923c' }}>
        {SYSTEM_EMOJI} SEGA Dreamcast · RetroArch + Flycast · Standalone Flycast
      </p>
      <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
        <strong style={{ color: '#fdba74' }}>RetroArch + flycast_libretro</strong> is the recommended backend for relay netplay — no port
        forwarding required. <strong style={{ color: '#fdba74' }}>Standalone Flycast 2.x</strong> supports experimental rollback netcode
        for local or LAN sessions. ROM formats supported:{' '}
        <code>.gdi</code> (preferred), <code>.cdi</code>, <code>.chd</code>, <code>.bin/.cue</code>.
      </p>
      <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
        A Dreamcast BIOS (<code>dc_boot.bin</code>) is required. Place it in the RetroArch{' '}
        <code>system/</code> folder or the Flycast data directory. See the{' '}
        <strong style={{ color: '#fdba74' }}>Guide</strong> tab for full setup instructions.
      </p>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Game card
// ---------------------------------------------------------------------------

function DreamcastGameCard({
  game,
  onHost,
  onQuickMatch,
  openRooms,
}: {
  game: (typeof DC_GAMES)[number];
  onHost: (game: (typeof DC_GAMES)[number]) => void;
  onQuickMatch: (game: (typeof DC_GAMES)[number]) => void;
  openRooms: number;
}) {
  const hasVmu = VMU_GAMES.has(game.id);
  const hasVga = VGA_GAMES.has(game.id);

  return (
    <div
      className="n-card rounded-2xl p-4 flex flex-col gap-3"
      style={{
        background: 'linear-gradient(135deg, rgba(255,102,0,0.12), rgba(139,51,0,0.06))',
        border: '1px solid rgba(255,102,0,0.2)',
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
              style={{ background: 'rgba(253,186,116,0.1)', color: '#fdba74', border: '1px solid rgba(253,186,116,0.25)' }}
            >
              {openRooms} open
            </span>
          )}
        </div>
      </div>

      {(hasVmu || hasVga) && (
        <div className="flex gap-2 flex-wrap">
          {hasVmu && <AccessoryBadge type="vmu" />}
          {hasVga && <AccessoryBadge type="vga" />}
        </div>
      )}

      <div className="flex gap-2 mt-auto">
        <button
          onClick={() => onQuickMatch(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, #8b3300)`, color: '#fff' }}
        >
          ⚡ Quick Match
        </button>
        <button
          onClick={() => onHost(game)}
          className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
          style={{ background: 'rgba(255,102,0,0.12)', border: '1px solid rgba(255,102,0,0.35)', color: '#fdba74' }}
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
  const [rankings, setRankings] = useState<DreamcastRanking[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    apiFetch<{ leaderboard: DreamcastRanking[] }>('/api/leaderboard?orderBy=sessions&limit=10')
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
        <p className="text-sm mt-1">Play some Dreamcast sessions to appear here!</p>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      {rankings.map((r, i) => (
        <div
          key={r.displayName}
          className="flex items-center gap-3 rounded-xl px-4 py-3"
          style={{ background: 'rgba(255,102,0,0.08)', border: '1px solid rgba(255,102,0,0.18)' }}
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
      <h3 style={{ margin: '0 0 12px', fontSize: 15, color: '#fdba74', borderBottom: '1px solid rgba(255,102,0,0.2)', paddingBottom: 6 }}>
        {title}
      </h3>
      {children}
    </div>
  );
}

function GuideRow({ label, value, note }: { label: string; value: string; note?: string }) {
  return (
    <div style={{ display: 'flex', gap: 12, padding: '7px 0', borderBottom: '1px solid rgba(255,102,0,0.1)', fontSize: 13 }}>
      <span style={{ color: '#9ca3af', minWidth: 180 }}>{label}</span>
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
        <GuideRow label="RetroArch + flycast_libretro" value="Default — relay netplay" note="Install core: Main Menu → Download → flycast_libretro" />
        <GuideRow label="Standalone Flycast 2.x" value="Rollback netcode" note="Experimental DC/NAOMI rollback; best for local/LAN" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          Select your backend in <strong style={{ color: '#9ca3af' }}>Settings → Emulator Paths</strong>.
          For online relay sessions, configure <strong style={{ color: '#9ca3af' }}>RetroArch</strong> and download the
          <code style={{ color: '#fdba74', fontSize: 11, margin: '0 4px' }}>flycast_libretro</code> core via the RetroArch core downloader.
          Standalone Flycast supports rollback for local play.
        </p>
      </GuideSection>

      <GuideSection title="🔧 BIOS Setup">
        <GuideRow label="dc_boot.bin" value="Required" note="Dreamcast boot ROM — must match known-good MD5" />
        <GuideRow label="dc_flash.bin" value="Recommended" note="Flash ROM — stores regional settings and date/time" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          <strong style={{ color: '#9ca3af' }}>RetroArch:</strong> place both files in the RetroArch{' '}
          <code style={{ color: '#fdba74', fontSize: 11 }}>system/</code> directory.<br />
          <strong style={{ color: '#9ca3af' }}>Standalone Flycast (Linux/macOS):</strong>{' '}
          <code style={{ color: '#fdba74', fontSize: 11 }}>~/.local/share/flycast/data/</code><br />
          <strong style={{ color: '#9ca3af' }}>Standalone Flycast (Windows):</strong>{' '}
          <code style={{ color: '#fdba74', fontSize: 11 }}>%APPDATA%\flycast\data\</code><br />
          The app will alert you if a required BIOS is missing before launching.
        </p>
      </GuideSection>

      <GuideSection title="💿 ROM Formats">
        <GuideRow label=".gdi" value="GD-ROM image (preferred)" note="Full disc dump including audio tracks — highest accuracy" />
        <GuideRow label=".chd" value="Compressed Hard Disk" note="Best size/quality ratio; convert with chdman from .gdi" />
        <GuideRow label=".cdi" value="DiscJuggler" note="Common rip format; compatible but less accurate than .gdi" />
        <GuideRow label=".bin / .cue" value="CD image + cue sheet" note="Supported; use for data-only discs" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          Prefer <code style={{ color: '#fdba74', fontSize: 11 }}>.gdi</code> for accuracy or{' '}
          <code style={{ color: '#fdba74', fontSize: 11 }}>.chd</code> to save space.
          Convert GDI to CHD: <code style={{ color: '#fdba74', fontSize: 11 }}>chdman createcd -i game.gdi -o game.chd</code>
        </p>
      </GuideSection>

      <GuideSection title="💾 VMU & Save Data">
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: 12, marginBottom: 8 }}>
          {[
            { icon: '💾', name: 'VMU', desc: 'Flycast creates virtual VMU .bin files per controller port. Saves go to your configured save directory.' },
            { icon: '📸', name: 'Save States', desc: 'Use F2/F4 in RetroArch (slot 0), or Ctrl+1–9 in standalone Flycast. States stored as .state files.' },
            { icon: '🎮', name: 'VMU Games', desc: 'Sonic Adventure 2 Chao Garden and Virtua Tennis profiles use VMU — enable VMU emulation in Flycast settings.' },
            { icon: '🔄', name: 'Cloud Sync', desc: 'RetroOasis can sync .state and .vms files between session partners — enable in Settings → Save Sync.' },
          ].map(({ icon, name, desc }) => (
            <div key={name} style={{ background: 'rgba(255,102,0,0.08)', borderRadius: 8, padding: '12px 14px', border: '1px solid rgba(255,102,0,0.18)' }}>
              <div style={{ fontSize: 22, marginBottom: 6 }}>{icon}</div>
              <div style={{ fontWeight: 700, fontSize: 13, marginBottom: 4, color: '#fdba74' }}>{name}</div>
              <div style={{ color: '#6b7280', fontSize: 12, lineHeight: 1.5 }}>{desc}</div>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="🖥️ VGA Output">
        <GuideRow label="VGA-compatible games" value="480p progressive" note="Soul Calibur, DOA2, Ikaruga, MvC2, CvS, Tech Romancer" />
        <GuideRow label="Non-VGA games" value="480i interlaced" note="Crazy Taxi, Jet Grind Radio — use RGB or composite mode" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          VGA mode (<code style={{ color: '#fdba74', fontSize: 11 }}>--cable-type=vga</code>) gives the sharpest output for compatible games.
          The Dreamcast BIOS checks cable type at boot — non-VGA games will show a black screen in VGA mode.
          RetroArch: set the <code style={{ color: '#fdba74', fontSize: 11 }}>flycast_internal_resolution</code> core option to match your display.
        </p>
      </GuideSection>

      <GuideSection title="🌐 Netplay Tips">
        <GuideRow label="Fighters (MvC2, SC, DOA2)" value="≤ 60 ms" note="Frame-critical inputs; combo execution and counters" />
        <GuideRow label="Arena (Power Stone 2)" value="≤ 80 ms" note="Transformation combos and item use need solid sync" />
        <GuideRow label="Sports / Racing (VT, CT)" value="≤ 100 ms" note="Timing-tolerant for score comparison sessions" />
        <GuideRow label="Co-op shooters (Ikaruga)" value="≤ 100 ms" note="Bullet patterns fully deterministic; comfortable at 100 ms" />
        <GuideRow label="Co-op action (Gauntlet)" value="≤ 100 ms" note="AI pathing can drift above 100 ms in 4-player" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          Dreamcast emulation is lightweight — most modern hardware runs it at full speed.
          For best results, use wired Ethernet and ensure both players use the same ROM region and BIOS version.
          Check <strong style={{ color: '#9ca3af' }}>Connection Diagnostics</strong> to measure your relay ping.
        </p>
      </GuideSection>

      <GuideSection title="🎮 Controller Setup">
        <GuideRow label="Recommended" value="Xbox or PlayStation controller" note="Both map cleanly to DC A/B/X/Y layout" />
        <GuideRow label="Analog triggers" value="Required for L / R" note="DC triggers are analog — use LT/RT or L2/R2" />
        <GuideRow label="Keyboard" value="Usable fallback" note="WASD + XZAS for digital games; analog precision reduced" />
        <p style={{ color: '#6b7280', fontSize: 12, marginTop: 8, lineHeight: 1.6 }}>
          The Dreamcast controller has a single analog stick, analog L/R triggers, four face buttons, and a D-Pad.
          RetroArch auto-detects controllers and applies the flycast_libretro default mapping.
          For custom bindings open RetroArch <strong style={{ color: '#9ca3af' }}>Settings → Input → Port 1 Binds</strong>.
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
  const [hostGame, setHostGame] = useState<(typeof DC_GAMES)[number] | null>(null);
  const [showJoin, setShowJoin] = useState(false);
  const [joinCode, setJoinCode] = useState('');
  const [notification, setNotification] = useState('');

  const openRoomsFor = useCallback(
    (gameId: string) => publicRooms.filter((r) => r.gameId === gameId && r.status === 'waiting').length,
    [publicRooms],
  );

  const handleQuickMatch = useCallback(
    async (game: (typeof DC_GAMES)[number]) => {
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
      <DreamcastBanner />

      {notification && (
        <div
          className="rounded-xl px-4 py-3 text-sm font-semibold"
          style={{ background: 'rgba(255,102,0,0.1)', color: '#fdba74', border: '1px solid rgba(255,102,0,0.3)' }}
        >
          ℹ️ {notification}
        </div>
      )}

      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
        {DC_GAMES.map((game) => (
          <DreamcastGameCard
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
// DreamcastPage
// ---------------------------------------------------------------------------

export default function DreamcastPage() {
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
          <h1 className="text-3xl font-black text-white">SEGA Dreamcast</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            RetroArch + Flycast · Standalone Flycast · Up to 4 players · No port forwarding needed
          </p>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(255,102,0,0.2)' }}>
        {tabs.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(255,102,0,0.15)', color: '#fdba74', borderBottom: `2px solid ${SYSTEM_COLOR}` }
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
