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

type ActiveTab = 'lobby' | 'guide' | 'leaderboard';

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
    <div className="bg-gray-800 rounded-lg p-5 mb-4">
      <h3 className="text-base font-semibold text-white mb-3">
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
        <ol className="list-decimal list-inside space-y-1 text-sm text-gray-300">
          <li>Download Ryujinx (RyuBing) from <span className="text-red-300">git.ryujinx.app/ryubing/ryujinx/releases</span></li>
          <li>Obtain <strong>prod.keys</strong> and <strong>title.keys</strong> from your own Nintendo Switch console</li>
          <li>Place key files in <code className="text-xs bg-gray-700 px-1 rounded">~/.config/Ryujinx/system/</code> (Linux/macOS) or <code className="text-xs bg-gray-700 px-1 rounded">%APPDATA%\Ryujinx\system\</code> (Windows)</li>
          <li>Dump firmware from your Switch using Hekate/Atmosphere — install it via Ryujinx Tools → Install Firmware</li>
          <li>Dump game cartridges to <code className="text-xs bg-gray-700 px-1 rounded">.xci</code> or eShop titles to <code className="text-xs bg-gray-700 px-1 rounded">.nsp</code> format using NXDumpTool on your console</li>
          <li>Enable PPTC (Persistent Profile-Guided Tiered Cache) in Ryujinx for faster shader compilation</li>
        </ol>
      </GuideSection>

      <GuideSection title="LDN Multiplayer (Local-Wireless Netplay)" emoji="📶">
        <p className="text-sm text-gray-300 mb-3">
          Ryujinx's LDN (Local Device Network) mode emulates the Switch's local-wireless multiplayer
          system. Players on the same LAN (or over a VPN/relay) can discover and join each other as if
          playing on real hardware in the same room.
        </p>
        <div className="space-y-2 text-sm">
          <div className="bg-gray-700 rounded p-3">
            <p className="text-white font-medium mb-1">Enable LDN</p>
            <ol className="list-decimal list-inside space-y-1 text-gray-300">
              <li>Open Ryujinx → Options → Settings → Multiplayer</li>
              <li>Set <strong>Mode</strong> to <em>LDN (Local Wireless)</em></li>
              <li>All players in the session must enable LDN with matching game versions</li>
            </ol>
          </div>
          <div className="bg-gray-700 rounded p-3">
            <p className="text-white font-medium mb-1">RetroOasis Relay</p>
            <p className="text-gray-300">For cross-internet sessions, RetroOasis routes TCP through its relay at the network level. Launch your game normally — the relay handles connectivity. All players need the room code from the host.</p>
          </div>
        </div>
      </GuideSection>

      <GuideSection title="ROM Formats" emoji="💾">
        <div className="grid grid-cols-2 gap-3 text-sm">
          {[
            { ext: '.xci', desc: 'Game Card Image — physical cartridge dump (preferred)' },
            { ext: '.nsp', desc: 'Nintendo Submission Package — eShop / installed title' },
            { ext: '.nca', desc: 'Nintendo Content Archive — raw encrypted content' },
            { ext: '.nso', desc: 'Nintendo Shared Object — homebrew executable' },
          ].map(({ ext, desc }) => (
            <div key={ext} className="bg-gray-700 rounded p-2">
              <code className="text-red-300 text-xs font-mono">{ext}</code>
              <p className="text-gray-400 text-xs mt-1">{desc}</p>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="Performance Tips" emoji="⚡">
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-red-300 font-medium">Vulkan backend:</span> Recommended for most GPUs; lower driver overhead than OpenGL</li>
          <li><span className="text-red-300 font-medium">PPTC cache:</span> Enable in Settings → System — dramatically reduces compile stutter after the first session</li>
          <li><span className="text-red-300 font-medium">Resolution scale:</span> 2× (1440p) is the recommended sweet spot; 3× (2160p) for high-end GPUs</li>
          <li><span className="text-red-300 font-medium">Shader cache:</span> Community caches available for popular titles — place in Ryujinx's shader cache directory</li>
          <li><span className="text-red-300 font-medium">Memory manager:</span> Host Unchecked mode gives the best performance on trusted game files</li>
          <li><span className="text-red-300 font-medium">DLC:</span> Install DLC NSPs in Ryujinx File → Install to NAND before loading the base game</li>
        </ul>
      </GuideSection>

      <GuideSection title="Controller Setup" emoji="🎮">
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-red-300 font-medium">Xbox/PS controllers:</span> Map to Joy-Con or Pro Controller in Ryujinx Input Settings — standard layout works out of the box</li>
          <li><span className="text-red-300 font-medium">Joy-Con pair:</span> Ryujinx can emulate a Joy-Con pair; left stick and D-Pad split is configured per player</li>
          <li><span className="text-red-300 font-medium">Pro Controller:</span> Most games recommend mapping to "Nintendo Switch Pro Controller" profile in Ryujinx</li>
          <li><span className="text-red-300 font-medium">Motion controls:</span> Gyro input supported for controllers with motion sensors (DualSense, Switch Joy-Con via Bluetooth)</li>
          <li><span className="text-red-300 font-medium">Amiibo:</span> Virtual Amiibo emulation via Ryujinx Tools → Manage Amiibo — no physical Amiibo required</li>
        </ul>
      </GuideSection>

      <GuideSection title="Best Multiplayer Games" emoji="🏆">
        <div className="space-y-2 text-sm text-gray-300">
          {[
            { title: 'Mario Kart 8 Deluxe', badge: '8P Racing', why: '96 courses, 200cc, battle mode — relay sessions stable under 80 ms via LDN' },
            { title: 'Super Smash Bros. Ultimate', badge: '8P Fighting', why: 'Every fighter ever — definitive platform fighter; LDN keeps inputs tight' },
            { title: 'Splatoon 3', badge: '4v4 + Co-op', why: 'Turf War and Salmon Run both work via LDN; best Switch online experience' },
            { title: 'Mario Party Superstars', badge: '4P Party', why: 'Classic N64 boards + 100 mini-games — fully online, very relay-tolerant' },
            { title: 'Animal Crossing: New Horizons', badge: '8P Chill', why: 'Island visits work at any latency; great for relaxed social sessions' },
            { title: 'Pokémon Scarlet / Violet', badge: '4P Open World', why: 'Union Circle 4P exploration and Tera Raid Battles — best Pokémon online yet' },
          ].map(({ title, badge, why }) => (
            <div key={title} className="flex gap-2 items-start">
              <span className="text-red-400 font-medium flex-none">▸</span>
              <div>
                <span className="text-white font-medium">{title}</span>
                <span
                  className="ml-2 text-xs px-1.5 py-0.5 rounded"
                  style={{ background: SYSTEM_COLOR + '33', color: SYSTEM_COLOR }}
                >
                  {badge}
                </span>
                <p className="text-gray-400 text-xs mt-0.5">{why}</p>
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
    { id: 'lobby', label: '🎮 Lobby' },
    { id: 'guide', label: '📖 Guide' },
    { id: 'leaderboard', label: '🏆 Leaderboard' },
  ];

  return (
    <div className="p-6 max-w-5xl mx-auto">
      {/* Header */}
      <div className="flex items-center gap-3 mb-6">
        <span className="text-4xl">🔴</span>
        <div>
          <h1 className="text-2xl font-bold" style={{ color: SYSTEM_COLOR }}>
            Nintendo Switch
          </h1>
          <p className="text-sm text-gray-400">
            Hybrid console gaming — Ryujinx (RyuBing) · LDN local-wireless netplay · Relay sessions
          </p>
        </div>
        <span
          className="ml-auto text-xs font-medium px-2 py-1 rounded"
          style={{ background: '#ff990033', color: '#ff9900', border: '1px solid #ff990066' }}
        >
          Early Support
        </span>
      </div>

      {/* Tabs */}
      <div className="flex gap-2 mb-6">
        {TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className={`px-4 py-2 rounded-lg text-sm font-medium transition-colors ${
              activeTab === tab.id
                ? 'text-white'
                : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
            }`}
            style={activeTab === tab.id ? { backgroundColor: SYSTEM_COLOR } : undefined}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Lobby tab */}
      {activeTab === 'lobby' && (
        <>
          {switchRooms.length > 0 && (
            <section className="mb-6">
              <h2 className="text-lg font-semibold text-white mb-3">🔴 Live Rooms</h2>
              <div className="space-y-2">
                {switchRooms.map((room) => (
                  <div
                    key={room.id}
                    className="bg-gray-800 rounded-lg p-4 flex items-center justify-between"
                  >
                    <div>
                      <span className="font-medium text-white">{room.name}</span>
                      <span className="ml-2 text-sm text-gray-400">{room.gameTitle}</span>
                    </div>
                    <div className="flex items-center gap-3 text-sm text-gray-400">
                      <span>{room.players.length}/{room.maxPlayers} players</span>
                      <button
                        onClick={() => setShowJoin(true)}
                        className="px-3 py-1 rounded text-white text-xs font-medium"
                        style={{ backgroundColor: SYSTEM_COLOR }}
                      >
                        Join
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            </section>
          )}

          <section>
            <h2 className="text-lg font-semibold text-white mb-3">🔴 Switch Games</h2>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              {SWITCH_GAMES.map((game) => (
                <div key={game.id} className="bg-gray-800 rounded-lg p-4 flex flex-col gap-3">
                  <div className="flex items-start gap-3">
                    <span className="text-3xl">{game.coverEmoji}</span>
                    <div className="flex-1 min-w-0">
                      <p className="font-medium text-white leading-tight">{game.title}</p>
                      <p className="text-xs text-gray-400 mt-0.5 line-clamp-2">{game.description}</p>
                      <div className="flex flex-wrap gap-1 mt-2">
                        {game.badges?.map((b) => (
                          <span key={b} className="text-xs px-1.5 py-0.5 rounded bg-gray-700 text-gray-300">{b}</span>
                        ))}
                        {LDN_IDS.has(game.id) && (
                          <span className="text-xs px-1.5 py-0.5 rounded bg-blue-900 text-blue-300">LDN Netplay</span>
                        )}
                        {COOP_IDS.has(game.id) && (
                          <span className="text-xs px-1.5 py-0.5 rounded bg-green-900 text-green-300">Co-op</span>
                        )}
                      </div>
                    </div>
                    <span
                      className="text-xs font-bold px-2 py-1 rounded shrink-0"
                      style={{ backgroundColor: SYSTEM_COLOR + '33', color: SYSTEM_COLOR }}
                    >
                      {game.maxPlayers}P
                    </span>
                  </div>
                  <div className="flex gap-2">
                    <button
                      onClick={() => handleQuickMatch(game.id)}
                      className="flex-1 px-3 py-2 rounded text-white text-sm font-medium hover:opacity-90"
                      style={{ backgroundColor: SYSTEM_COLOR }}
                    >
                      Quick Match
                    </button>
                    <button
                      onClick={() => handleHost(game.id)}
                      className="flex-1 px-3 py-2 rounded bg-gray-700 text-white text-sm font-medium hover:bg-gray-600"
                    >
                      Host Room
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </section>
        </>
      )}

      {/* Guide tab */}
      {activeTab === 'guide' && <SwitchGuide />}

      {/* Leaderboard tab */}
      {activeTab === 'leaderboard' && (
        <section>
          <h2 className="text-lg font-semibold text-white mb-3">🏆 Switch Leaderboard</h2>
          {rankingsError ? (
            <p className="text-red-400 text-sm">{rankingsError}</p>
          ) : rankings.length === 0 ? (
            <p className="text-gray-400 text-sm">No rankings yet — be the first to play!</p>
          ) : (
            <div className="space-y-2">
              {rankings.map((r, i) => (
                <div key={r.displayName} className="bg-gray-800 rounded-lg p-3 flex items-center gap-3">
                  <RankBadge rank={i + 1} />
                  <span className="flex-1 text-white font-medium">{r.displayName}</span>
                  <span className="text-gray-400 text-sm">{r.sessions} sessions</span>
                </div>
              ))}
            </div>
          )}
        </section>
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
