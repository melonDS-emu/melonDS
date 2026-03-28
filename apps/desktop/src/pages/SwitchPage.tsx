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

const SYSTEM_COLOR = '#E4003A';
const SYSTEM_COLOR_DARK = '#8a0021';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

interface SwitchRanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Derived game list from shared mock catalog
// ---------------------------------------------------------------------------

const SWITCH_GAMES = MOCK_GAMES.filter((g) => g.system === 'Switch');

// Games that support LDN (local-wireless) multiplayer via Ryujinx
const LDN_IDS = new Set([
  'switch-mario-kart-8-deluxe',
  'switch-super-smash-bros-ultimate',
  'switch-splatoon-3',
  'switch-mario-party-superstars',
  'switch-animal-crossing-new-horizons',
  'switch-luigi-mansion-3',
]);

// Games with online co-op
const COOP_IDS = new Set([
  'switch-animal-crossing-new-horizons',
  'switch-luigi-mansion-3',
  'switch-kirby-forgotten-land',
  'switch-pikmin-4',
  'switch-splatoon-3',
]);

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
// Sub-components
// ---------------------------------------------------------------------------

function RankBadge({ rank }: { rank: number }) {
  const medals: Record<number, string> = { 1: '🥇', 2: '🥈', 3: '🥉' };
  return (
    <span className="text-lg w-8 text-center" title={`Rank #${rank}`}>
      {medals[rank] ?? `#${rank}`}
    </span>
  );
}

function GuideSection({
  title,
  emoji,
  children,
}: {
  title: string;
  emoji: string;
  children: React.ReactNode;
}) {
  return (
    <div
      className="rounded-2xl p-5 mb-4"
      style={{ background: 'rgba(228,0,58,0.05)', border: '1px solid rgba(228,0,58,0.15)' }}
    >
      <h3 className="text-sm font-bold mb-3" style={{ color: SYSTEM_COLOR }}>
        {emoji} {title}
      </h3>
      {children}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Guide Tab
// ---------------------------------------------------------------------------

function SwitchGuide() {
  return (
    <div className="space-y-2">
      {/* Alpha banner */}
      <div
        className="rounded-lg p-4 mb-4 text-sm"
        style={{ background: '#ff990033', border: '1px solid #ff990066' }}
      >
        <p className="font-semibold text-yellow-300 mb-1">⚠️ Early Support — Active Development</p>
        <p className="text-gray-300 text-xs">
          Nintendo Switch emulation via Ryujinx (RyuBing community fork) is functional for many titles
          but still maturing. Expect occasional crashes and version-specific compatibility. Always keep
          Ryujinx updated to the latest nightly build.
        </p>
      </div>

      {/* Emulator banner */}
      <div
        className="rounded-lg p-4 mb-4 text-sm text-white"
        style={{
          background: `linear-gradient(135deg, ${SYSTEM_COLOR}22, ${SYSTEM_COLOR}44)`,
          border: `1px solid ${SYSTEM_COLOR}55`,
        }}
      >
        <p className="font-semibold mb-1">Powered by Ryujinx — RyuBing community fork</p>
        <p className="text-gray-300 text-xs">
          The original Ryujinx project was discontinued in October 2024. The RyuBing community fork
          continues active development at git.ryujinx.app/ryubing/ryujinx and is the recommended
          Switch emulator for RetroOasis sessions.
        </p>
      </div>

      <GuideSection title="Emulator Setup" emoji="⚙️">
        <ol className="list-decimal list-inside space-y-1 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <li>Download Ryujinx (RyuBing) from <span style={{ color: SYSTEM_COLOR }}>git.ryujinx.app/ryubing/ryujinx/releases</span></li>
          <li>Obtain <strong className="text-white">prod.keys</strong> and <strong className="text-white">title.keys</strong> from your own Nintendo Switch console</li>
          <li>Place key files in <code className="text-xs px-1 rounded" style={{ background: 'rgba(228,0,58,0.1)', color: SYSTEM_COLOR }}>~/.config/Ryujinx/system/</code> (Linux/macOS) or <code className="text-xs px-1 rounded" style={{ background: 'rgba(228,0,58,0.1)', color: SYSTEM_COLOR }}>%APPDATA%\Ryujinx\system\</code> (Windows)</li>
          <li>Dump firmware from your Switch using Hekate/Atmosphere — install it via Ryujinx Tools → Install Firmware</li>
          <li>Dump game cartridges to <code className="text-xs px-1 rounded" style={{ background: 'rgba(228,0,58,0.1)', color: SYSTEM_COLOR }}>.xci</code> or eShop titles to <code className="text-xs px-1 rounded" style={{ background: 'rgba(228,0,58,0.1)', color: SYSTEM_COLOR }}>.nsp</code> format using NXDumpTool on your console</li>
          <li>Enable PPTC (Persistent Profile-Guided Tiered Cache) in Ryujinx for faster shader compilation</li>
        </ol>
      </GuideSection>

      <GuideSection title="LDN Multiplayer (Local-Wireless Netplay)" emoji="📶">
        <p className="text-xs mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Ryujinx's LDN mode emulates the Switch's local-wireless multiplayer. Players can discover and join each other as if playing in the same room.
        </p>
        <div className="space-y-2">
          <div className="rounded-xl p-3" style={{ background: 'rgba(228,0,58,0.05)', border: '1px solid rgba(228,0,58,0.12)' }}>
            <p className="text-white font-medium text-xs mb-1">Enable LDN</p>
            <ol className="list-decimal list-inside space-y-1 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              <li>Open Ryujinx → Options → Settings → Multiplayer</li>
              <li>Set <strong className="text-white">Mode</strong> to <em>LDN (Local Wireless)</em></li>
              <li>All players must enable LDN with matching game versions</li>
            </ol>
          </div>
          <div className="rounded-xl p-3" style={{ background: 'rgba(228,0,58,0.05)', border: '1px solid rgba(228,0,58,0.12)' }}>
            <p className="text-white font-medium text-xs mb-1">RetroOasis Relay</p>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>For cross-internet sessions, RetroOasis routes TCP through its relay. Launch your game normally — the relay handles connectivity. All players need the room code from the host.</p>
          </div>
        </div>
      </GuideSection>

      <GuideSection title="ROM Formats" emoji="💾">
        <div className="grid grid-cols-2 gap-3">
          {[
            { ext: '.xci', desc: 'Game Card Image — physical cartridge dump (preferred)' },
            { ext: '.nsp', desc: 'Nintendo Submission Package — eShop / installed title' },
            { ext: '.nca', desc: 'Nintendo Content Archive — raw encrypted content' },
            { ext: '.nso', desc: 'Nintendo Shared Object — homebrew executable' },
          ].map(({ ext, desc }) => (
            <div key={ext} className="rounded-xl p-2" style={{ background: 'rgba(228,0,58,0.05)', border: '1px solid rgba(228,0,58,0.12)' }}>
              <code className="text-xs font-mono font-bold" style={{ color: SYSTEM_COLOR }}>{ext}</code>
              <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>{desc}</p>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="Performance Tips" emoji="⚡">
        <ul className="space-y-1 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Vulkan backend:</span> Recommended for most GPUs; lower driver overhead than OpenGL</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>PPTC cache:</span> Enable in Settings → System — dramatically reduces compile stutter after the first session</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Resolution scale:</span> 2× (1440p) is the recommended sweet spot; 3× (2160p) for high-end GPUs</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Shader cache:</span> Community caches available for popular titles — place in Ryujinx's shader cache directory</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Memory manager:</span> Host Unchecked mode gives the best performance on trusted game files</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>DLC:</span> Install DLC NSPs in Ryujinx File → Install to NAND before loading the base game</li>
        </ul>
      </GuideSection>

      <GuideSection title="Controller Setup" emoji="🎮">
        <ul className="space-y-1 text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Xbox/PS controllers:</span> Map to Joy-Con or Pro Controller in Ryujinx Input Settings — standard layout works out of the box</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Joy-Con pair:</span> Ryujinx can emulate a Joy-Con pair; left stick and D-Pad split is configured per player</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Pro Controller:</span> Most games recommend mapping to "Nintendo Switch Pro Controller" profile</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Motion controls:</span> Gyro input supported for controllers with motion sensors (DualSense, Switch Joy-Con via Bluetooth)</li>
          <li><span className="font-medium" style={{ color: SYSTEM_COLOR }}>Amiibo:</span> Virtual Amiibo emulation via Ryujinx Tools → Manage Amiibo — no physical Amiibo required</li>
        </ul>
      </GuideSection>

      <GuideSection title="Best Multiplayer Games" emoji="🏆">
        <div className="space-y-2">
          {[
            { title: 'Mario Kart 8 Deluxe', badge: '8P Racing', why: '96 courses, 200cc, battle mode — relay sessions stable under 80 ms via LDN' },
            { title: 'Super Smash Bros. Ultimate', badge: '8P Fighting', why: 'Every fighter ever — definitive platform fighter; LDN keeps inputs tight' },
            { title: 'Splatoon 3', badge: '4v4 + Co-op', why: 'Turf War and Salmon Run both work via LDN; best Switch online experience' },
            { title: 'Mario Party Superstars', badge: '4P Party', why: 'Classic N64 boards + 100 mini-games — fully online, very relay-tolerant' },
            { title: 'Animal Crossing: New Horizons', badge: '8P Chill', why: 'Island visits work at any latency; great for relaxed social sessions' },
            { title: 'Pokémon Scarlet / Violet', badge: '4P Open World', why: 'Union Circle 4P exploration and Tera Raid Battles — best Pokémon online yet' },
          ].map(({ title, badge, why }) => (
            <div key={title} className="flex gap-2 items-start">
              <span className="font-medium flex-none" style={{ color: SYSTEM_COLOR }}>▸</span>
              <div>
                <span className="text-white text-xs font-medium">{title}</span>
                <span
                  className="ml-2 text-xs px-1.5 py-0.5 rounded-full"
                  style={{ background: 'rgba(228,0,58,0.15)', color: SYSTEM_COLOR }}
                >
                  {badge}
                </span>
                <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>{why}</p>
              </div>
            </div>
          ))}
        </div>
      </GuideSection>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main component
// ---------------------------------------------------------------------------

export default function SwitchPage() {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [selectedGameId, setSelectedGameId] = useState<string | null>(null);
  const [showHost, setShowHost] = useState(false);
  const [showJoin, setShowJoin] = useState(false);
  const [rankings, setRankings] = useState<SwitchRanking[]>([]);
  const [rankingsError, setRankingsError] = useState<string | null>(null);

  useEffect(() => {
    if (activeTab !== 'leaderboard') return;
    apiFetch<{ rankings: SwitchRanking[] }>('/api/rankings/switch')
      .then((data) => setRankings(data.rankings ?? []))
      .catch((err: unknown) =>
        setRankingsError(err instanceof Error ? err.message : 'Failed to load leaderboard'),
      );
  }, [activeTab]);

  const handleQuickMatch = useCallback((gameId: string) => {
    setSelectedGameId(gameId);
    setShowJoin(true);
  }, []);

  const handleHost = useCallback((gameId: string) => {
    setSelectedGameId(gameId);
    setShowHost(true);
  }, []);

  const switchRooms = publicRooms.filter(
    (r) => r.system?.toLowerCase() === 'switch',
  );

  const TABS: { id: ActiveTab; label: string }[] = [
    { id: 'lobby',       label: '🎮 Lobby' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
    { id: 'guide',       label: '📖 Guide' },
  ];

  return (
    <div className="max-w-5xl mx-auto p-6 space-y-8">
      {/* Hero header */}
      <div
        className="rounded-3xl p-6 flex items-center gap-5"
        style={{ background: 'linear-gradient(135deg, rgba(228,0,58,0.18) 0%, rgba(228,0,58,0.05) 60%, transparent 100%)', border: '1px solid rgba(228,0,58,0.2)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          🔴
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Nintendo Switch</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Ryujinx (RyuBing) · LDN local-wireless netplay · Relay sessions
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <div className="flex items-center gap-2">
            <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {SWITCH_GAMES.length} games
            </span>
            <span
              className="text-xs px-2 py-0.5 rounded-full font-semibold"
              style={{ background: '#ff990033', color: '#ff9900', border: '1px solid #ff990066' }}
            >
              Early Support
            </span>
          </div>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            8th Generation · 2017
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(228,0,58,0.15)' }}>
        {TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(228,0,58,0.1)', color: SYSTEM_COLOR, borderBottom: `2px solid ${SYSTEM_COLOR}` }
                : { color: 'var(--color-oasis-text-muted)' }
            }
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Lobby tab */}
      {activeTab === 'lobby' && (
        <div className="space-y-6">
          {switchRooms.length > 0 && (
            <div className="space-y-2">
              <p className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>🔴 Live Rooms</p>
              {switchRooms.map((room) => (
                <div
                  key={room.id}
                  className="rounded-xl p-4 flex items-center justify-between"
                  style={{ background: 'rgba(228,0,58,0.06)', border: '1px solid rgba(228,0,58,0.15)' }}
                >
                  <div>
                    <span className="font-medium text-white">{room.name}</span>
                    <span className="ml-2 text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>{room.gameTitle}</span>
                  </div>
                  <div className="flex items-center gap-3 text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    <span>{room.players.length}/{room.maxPlayers} players</span>
                    <button
                      onClick={() => setShowJoin(true)}
                      className="px-3 py-1.5 rounded-xl text-white text-xs font-bold transition-all hover:scale-105"
                      style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
                    >
                      Join
                    </button>
                  </div>
                </div>
              ))}
            </div>
          )}

          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {SWITCH_GAMES.map((game) => (
              <div
                key={game.id}
                className="n-card rounded-2xl p-4 flex flex-col gap-3"
                style={{ background: 'linear-gradient(135deg, rgba(228,0,58,0.07), rgba(228,0,58,0.02))', border: '1px solid rgba(228,0,58,0.15)' }}
              >
                <div className="flex items-start justify-between gap-2">
                  <div className="flex items-center gap-2">
                    <span className="text-3xl leading-none">{game.coverEmoji}</span>
                    <div className="min-w-0">
                      <p className="text-sm font-bold text-white leading-tight">{game.title}</p>
                      <p className="text-xs mt-0.5 line-clamp-2" style={{ color: 'var(--color-oasis-text-muted)' }}>{game.description}</p>
                    </div>
                  </div>
                  <span
                    className="text-xs font-bold px-2 py-0.5 rounded-full shrink-0"
                    style={{ background: 'rgba(228,0,58,0.15)', color: SYSTEM_COLOR }}
                  >
                    {game.maxPlayers}P
                  </span>
                </div>
                <div className="flex flex-wrap gap-1.5">
                  {game.badges?.map((b) => (
                    <span
                      key={b}
                      className="text-xs px-2 py-0.5 rounded-full"
                      style={{ background: 'rgba(255,255,255,0.06)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.08)' }}
                    >
                      {b}
                    </span>
                  ))}
                  {LDN_IDS.has(game.id) && (
                    <span
                      className="text-xs px-2 py-0.5 rounded-full font-semibold"
                      style={{ background: 'rgba(99,102,241,0.12)', color: '#a5b4fc', border: '1px solid rgba(99,102,241,0.3)' }}
                    >
                      📶 LDN Netplay
                    </span>
                  )}
                  {COOP_IDS.has(game.id) && (
                    <span
                      className="text-xs px-2 py-0.5 rounded-full font-semibold"
                      style={{ background: 'rgba(74,222,128,0.1)', color: '#4ade80', border: '1px solid rgba(74,222,128,0.25)' }}
                    >
                      Co-op
                    </span>
                  )}
                </div>
                {game.maxPlayers > 1 && (
                  <div className="flex gap-2 mt-auto">
                    <button
                      onClick={() => handleQuickMatch(game.id)}
                      className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
                      style={{ background: `linear-gradient(90deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})`, color: '#fff' }}
                    >
                      ⚡ Quick Match
                    </button>
                    <button
                      onClick={() => handleHost(game.id)}
                      className="flex-1 px-3 py-1.5 rounded-xl text-xs font-bold transition-all hover:scale-105"
                      style={{ background: 'rgba(228,0,58,0.08)', border: '1px solid rgba(228,0,58,0.35)', color: SYSTEM_COLOR }}
                    >
                      🎮 Host Room
                    </button>
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Guide tab */}
      {activeTab === 'guide' && <SwitchGuide />}

      {/* Leaderboard tab */}
      {activeTab === 'leaderboard' && (
        <div className="space-y-2 max-w-lg">
          <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Top players ranked by session count
          </p>
          {rankingsError ? (
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>{rankingsError}</p>
          ) : rankings.length === 0 ? (
            <div className="text-center py-16" style={{ color: 'var(--color-oasis-text-muted)' }}>
              <p className="text-4xl mb-3">🏆</p>
              <p className="font-semibold">No rankings yet</p>
              <p className="text-sm mt-1">Play some Switch sessions to appear here!</p>
            </div>
          ) : (
            rankings.map((r, i) => (
              <div
                key={r.displayName}
                className="flex items-center gap-3 rounded-xl px-4 py-3"
                style={{ background: 'rgba(228,0,58,0.05)', border: '1px solid rgba(228,0,58,0.12)' }}
              >
                <RankBadge rank={i + 1} />
                <span className="flex-1 text-sm font-semibold text-white">{r.displayName}</span>
                <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{r.sessions} sessions</span>
              </div>
            ))
          )}
        </div>
      )}

      {showHost && selectedGameId && (
        <HostRoomModal
          preselectedGameId={selectedGameId}
          onConfirm={(payload, dn) => { createRoom(payload, dn); setShowHost(false); }}
          onClose={() => setShowHost(false)}
        />
      )}
      {showJoin && (
        <JoinRoomModal
          onConfirm={(code, dn) => { joinByCode(code, dn); setShowJoin(false); }}
          onClose={() => setShowJoin(false)}
        />
      )}
    </div>
  );
}
