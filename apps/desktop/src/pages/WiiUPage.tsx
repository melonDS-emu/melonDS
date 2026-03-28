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

const SYSTEM_COLOR = '#009AC7';
const SYSTEM_COLOR_DARK = '#005a75';

type ActiveTab = 'lobby' | 'leaderboard' | 'guide';

interface WiiURanking {
  displayName: string;
  sessions: number;
}

// ---------------------------------------------------------------------------
// Derived game list from shared mock catalog
// ---------------------------------------------------------------------------

const WIIU_GAMES = MOCK_GAMES.filter((g) => g.system === 'Wii U');

// Games that heavily rely on GamePad asymmetry — shown with a special badge
const ASYMMETRIC_IDS = new Set([
  'wiiu-nintendo-land',
  'wiiu-mario-party-10',
  'wiiu-captain-toad-treasure-tracker',
  'wiiu-star-fox-zero',
]);

// Games with prominent 2-player co-op
const COOP_IDS = new Set([
  'wiiu-new-super-mario-bros-u',
  'wiiu-donkey-kong-country-tf',
  'wiiu-bayonetta-2',
  'wiiu-hyrule-warriors',
  'wiiu-yoshis-woolly-world',
  'wiiu-super-mario-3d-world',
  'wiiu-pikmin-3',
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
      style={{ background: 'rgba(0,154,199,0.05)', border: '1px solid rgba(0,154,199,0.15)' }}
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

function WiiUGuide() {
  return (
    <div className="space-y-2">
      {/* Banner */}
      <div
        className="rounded-lg p-4 mb-4 text-sm text-white"
        style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}22, ${SYSTEM_COLOR}44)`, border: `1px solid ${SYSTEM_COLOR}55` }}
      >
        <p className="font-semibold mb-1">Powered by Cemu — open source since 2022</p>
        <p className="text-gray-300 text-xs">
          Cemu emulates the Wii U with high accuracy. Most of the Wii U library runs at full speed.
          Relay netplay bridges at the TCP level — no Cemu-specific netplay CLI flags are needed.
        </p>
      </div>

      <GuideSection title="Emulator Setup" emoji="⚙️">
        <ol className="list-decimal list-inside space-y-1 text-sm text-gray-300">
          <li>Download Cemu from <span className="text-cyan-400">cemu.info</span> or install via Flatpak (<code className="text-xs bg-gray-700 px-1 rounded">flatpak install flathub info.cemu.Cemu</code>)</li>
          <li>Obtain <strong>keys.txt</strong> (Wii U title keys) from your own console — place it in the Cemu <code className="text-xs bg-gray-700 px-1 rounded">mlc01/usr/title/</code> directory</li>
          <li>Dump game files from your own Wii U hardware using <strong>Dumpling</strong> homebrew app</li>
          <li>Supported formats: <code className="text-xs bg-gray-700 px-1 rounded">.wud</code> / <code className="text-xs bg-gray-700 px-1 rounded">.wux</code> / <code className="text-xs bg-gray-700 px-1 rounded">.rpx</code> / <code className="text-xs bg-gray-700 px-1 rounded">.wua</code> — WUA is the most convenient single-file format</li>
          <li>Launch a game once in Cemu to generate shader caches (first launch is slow — subsequent launches are fast)</li>
        </ol>
      </GuideSection>

      <GuideSection title="Graphics & Performance" emoji="🖥️">
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-cyan-300 font-medium">Backend:</span> Use Vulkan for best performance; OpenGL is the fallback for older GPUs</li>
          <li><span className="text-cyan-300 font-medium">Resolution:</span> 2× (1440p) is the sweet spot; 4K is possible on high-end GPUs</li>
          <li><span className="text-cyan-300 font-medium">Shader cache:</span> Enable async shader compilation to reduce in-game stutter on first play</li>
          <li><span className="text-cyan-300 font-medium">Graphics packs:</span> Community-made enhancements at ActualMandM/cemu_graphic_packs — resolution mods and anti-aliasing fixes</li>
          <li><span className="text-cyan-300 font-medium">CPU mode:</span> Tricore recompiler (default) is recommended; dual-core is faster but less compatible</li>
        </ul>
      </GuideSection>

      <GuideSection title="Controller Setup" emoji="🎮">
        <div className="space-y-3 text-sm text-gray-300">
          <div>
            <p className="font-medium text-white mb-1">Wii U Pro Controller (recommended for most games)</p>
            <p>Map an Xbox or PlayStation gamepad as a Wii U Pro Controller in Cemu Input Settings → Emulate Controller → Wii U Pro Controller. ABXY buttons are swapped relative to Xbox layout — configure to match your preference.</p>
          </div>
          <div>
            <p className="font-medium text-white mb-1">Wii U GamePad (asymmetric multiplayer)</p>
            <p>Cemu emulates the GamePad screen in a second window. The GamePad player controls the second window; the TV player uses the main Cemu window. Mouse controls touch input for the GamePad screen — or use a USB touchscreen.</p>
          </div>
          <div>
            <p className="font-medium text-white mb-1">Wiimote (Wii-mode games)</p>
            <p>Enable Wii Remote emulation in Input Settings for games that use Wii mode. MotionPlus emulation maps to your controller's gyroscope if supported.</p>
          </div>
        </div>
      </GuideSection>

      <GuideSection title="Netplay Setup" emoji="🌐">
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-cyan-300 font-medium">RetroOasis relay:</span> TCP relay bridges at network level — just host a room and launch Cemu normally</li>
          <li><span className="text-cyan-300 font-medium">Pretendo Network:</span> Community Nintendo Network revival — set DNS to <code className="text-xs bg-gray-700 px-1 rounded">94.23.2.42</code> for online services in supported titles</li>
          <li><span className="text-cyan-300 font-medium">Latency target:</span> Under 100 ms for kart/platformer; under 80 ms for fighting games (Smash 4)</li>
          <li><span className="text-cyan-300 font-medium">Wired connection:</span> Use Ethernet on the host for the most stable relay sessions</li>
          <li><span className="text-cyan-300 font-medium">Asymmetric games:</span> GamePad player requires a second monitor or window — configure Cemu to show GamePad on a secondary display</li>
        </ul>
      </GuideSection>

      <GuideSection title="ROM Formats" emoji="💾">
        <div className="grid grid-cols-2 gap-3 text-sm">
          {[
            { ext: '.wua', desc: 'All-in-one archive — best single-file format (Cemu 1.27+)' },
            { ext: '.wud', desc: 'Raw disc image — largest size, highest compatibility' },
            { ext: '.wux', desc: 'Compressed disc image — saves space, widely supported' },
            { ext: '.rpx', desc: 'Game executable — requires full game directory structure' },
          ].map(({ ext, desc }) => (
            <div key={ext} className="bg-gray-700 rounded p-2">
              <code className="text-cyan-300 text-xs font-mono">{ext}</code>
              <p className="text-gray-400 text-xs mt-1">{desc}</p>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="Best Multiplayer Games" emoji="🏆">
        <div className="space-y-2 text-sm text-gray-300">
          {[
            { title: 'Mario Kart 8', badge: '4P Racing', why: 'Anti-gravity tracks, 200cc mode, and relay-stable physics' },
            { title: 'Super Smash Bros. for Wii U', badge: '8P Fighting', why: '8-player Smash on giant stages — the biggest Smash ever' },
            { title: 'Splatoon', badge: '4P Shooter', why: 'Ink-based Turf War is uniquely satisfying and relay-friendly' },
            { title: 'Super Mario 3D World', badge: '4P Platformer', why: 'Cat Mario co-op chaos — one of the best co-op platformers' },
            { title: 'Nintendo Land', badge: '5P Asymmetric', why: 'GamePad vs 4 Wiimote players — the definitive GamePad showcase' },
            { title: 'Hyrule Warriors', badge: '2P Co-op', why: 'Split TV/GamePad co-op; a great Zelda crossover brawler' },
          ].map(({ title, badge, why }) => (
            <div key={title} className="flex gap-2 items-start">
              <span className="text-cyan-400 font-medium min-w-0 flex-none">▸</span>
              <div>
                <span className="text-white font-medium">{title}</span>
                <span className="ml-2 text-xs px-1.5 py-0.5 rounded" style={{ background: SYSTEM_COLOR + '33', color: SYSTEM_COLOR }}>{badge}</span>
                <p className="text-gray-400 text-xs mt-0.5">{why}</p>
              </div>
            </div>
          ))}
        </div>
      </GuideSection>

      <GuideSection title="Asymmetric GamePad Tips" emoji="📱">
        <p className="text-sm text-gray-300 mb-2">
          The Wii U GamePad was the console's signature feature — a second screen with touch input. In Cemu, the GamePad renders in a separate window.
        </p>
        <ul className="space-y-1 text-sm text-gray-300">
          <li><span className="text-yellow-300">Nintendo Land</span> — One player is the hunter on the GamePad (top-down map view); four players co-operate on the TV</li>
          <li><span className="text-yellow-300">Mario Party 10</span> — Bowser Party mode: GamePad player controls Bowser against four Wiimote players</li>
          <li><span className="text-yellow-300">Captain Toad</span> — GamePad player can stun enemies with blowdarts while the TV player controls Toad</li>
          <li><span className="text-yellow-300">Star Fox Zero</span> — TV shows third-person Arwing view; GamePad shows cockpit first-person aiming</li>
        </ul>
      </GuideSection>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Main component
// ---------------------------------------------------------------------------

export default function WiiUPage() {
  const { publicRooms, createRoom, joinByCode } = useLobby();
  const [activeTab, setActiveTab] = useState<ActiveTab>('lobby');
  const [selectedGameId, setSelectedGameId] = useState<string | null>(null);
  const [showHost, setShowHost] = useState(false);
  const [showJoin, setShowJoin] = useState(false);
  const [rankings, setRankings] = useState<WiiURanking[]>([]);
  const [rankingsError, setRankingsError] = useState<string | null>(null);

  useEffect(() => {
    if (activeTab !== 'leaderboard') return;
    apiFetch<{ rankings: WiiURanking[] }>('/api/rankings/wiiu')
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

  const wiiuRooms = publicRooms.filter((r) => r.system?.toLowerCase() === 'wii u');

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
        style={{ background: 'linear-gradient(135deg, rgba(0,154,199,0.18) 0%, rgba(0,154,199,0.05) 60%, transparent 100%)', border: '1px solid rgba(0,154,199,0.2)' }}
      >
        <div
          className="w-16 h-16 rounded-2xl flex items-center justify-center text-3xl shrink-0"
          style={{ background: `linear-gradient(135deg, ${SYSTEM_COLOR}, ${SYSTEM_COLOR_DARK})` }}
        >
          ⊞
        </div>
        <div className="flex-1 min-w-0">
          <h1 className="text-3xl font-black text-white leading-tight">Nintendo Wii U</h1>
          <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Cemu emulator · Vulkan · GamePad asymmetry · Pretendo Network
          </p>
        </div>
        <div className="hidden sm:flex flex-col items-end gap-1 shrink-0">
          <span className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {WIIU_GAMES.length} games
          </span>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            8th Generation · 2012
          </span>
        </div>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 border-b" style={{ borderColor: 'rgba(0,154,199,0.15)' }}>
        {TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="px-5 py-2.5 text-sm font-semibold rounded-t-xl transition-all"
            style={
              activeTab === tab.id
                ? { background: 'rgba(0,154,199,0.1)', color: SYSTEM_COLOR, borderBottom: `2px solid ${SYSTEM_COLOR}` }
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
          {wiiuRooms.length > 0 && (
            <div className="space-y-2">
              <p className="text-xs font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>🔵 Live Rooms</p>
              {wiiuRooms.map((room) => (
                <div
                  key={room.id}
                  className="rounded-xl p-4 flex items-center justify-between"
                  style={{ background: 'rgba(0,154,199,0.06)', border: '1px solid rgba(0,154,199,0.15)' }}
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
            {WIIU_GAMES.map((game) => (
              <div
                key={game.id}
                className="n-card rounded-2xl p-4 flex flex-col gap-3"
                style={{ background: 'linear-gradient(135deg, rgba(0,154,199,0.07), rgba(0,154,199,0.02))', border: '1px solid rgba(0,154,199,0.15)' }}
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
                    style={{ background: 'rgba(0,154,199,0.15)', color: SYSTEM_COLOR }}
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
                  {ASYMMETRIC_IDS.has(game.id) && (
                    <span
                      className="text-xs px-2 py-0.5 rounded-full font-semibold"
                      style={{ background: 'rgba(123,47,190,0.12)', color: '#c084fc', border: '1px solid rgba(123,47,190,0.3)' }}
                    >
                      📱 GamePad Asymmetric
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
                      style={{ background: 'rgba(0,154,199,0.08)', border: '1px solid rgba(0,154,199,0.35)', color: SYSTEM_COLOR }}
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
      {activeTab === 'guide' && <WiiUGuide />}

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
              <p className="text-sm mt-1">Play some Wii U sessions to appear here!</p>
            </div>
          ) : (
            rankings.map((r, i) => (
              <div
                key={r.displayName}
                className="flex items-center gap-3 rounded-xl px-4 py-3"
                style={{ background: 'rgba(0,154,199,0.05)', border: '1px solid rgba(0,154,199,0.12)' }}
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
