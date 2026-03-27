import { useState, useEffect } from 'react';
import { useParams, Link, useNavigate } from 'react-router-dom';
import { useGame } from '../lib/use-games';
import { HostRoomModal } from '../components/HostRoomModal';
import { useLobby } from '../context/LobbyContext';
import { getRomAssociation, setRomAssociation, clearRomAssociation } from '../lib/rom-library';
import { tauriPickFile, tauriLaunchLocal, isTauri } from '../lib/tauri-ipc';
import { isFavorite, toggleFavorite } from '../lib/favorites-store';
import { recordRecentlyPlayed } from '../lib/recently-played';
import { getEmulatorPathForSystem, getBackendIdForSystem, EMULATOR_NAMES } from '../lib/emulator-settings';
import { getSaveDirectory } from '../lib/rom-settings';
import { SystemBadge } from '../components/SystemBadge';
import { getAchievementCapability, systemSupportsAchievements } from '../lib/achievement-capability';
import { fetchRetroGameSummary, gamesWithAchievements } from '../lib/retro-achievement-service';
import { isRAConnected } from '../lib/retro-achievements-settings';
import { resolveGameCapability, onlineSupportBadgeColor, onlineSupportIcon } from '../lib/game-capability';

// ---------------------------------------------------------------------------
// Phase 30 — rich metadata fetched from the API
// ---------------------------------------------------------------------------

interface GameMetadata {
  gameId: string;
  genre: string;
  developer: string;
  year: number;
  onboardingTips: string[];
  netplayTips: string[];
  recommendedController: string;
  artworkColor: string;
  quickHostPreset?: '1v1' | '4-player' | 'full-party';
}

interface GamePatch {
  id: string;
  gameId: string;
  name: string;
  description: string;
  type: string;
  safe: boolean;
  version: string;
  infoUrl?: string;
  instructions: string[];
  tags: string[];
}

const API_BASE: string =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

async function fetchGameMetadata(gameId: string): Promise<GameMetadata | null> {
  try {
    const res = await fetch(`${API_BASE}/api/game-metadata/${encodeURIComponent(gameId)}`);
    if (!res.ok) return null;
    const data = await res.json() as { metadata: GameMetadata };
    return data.metadata;
  } catch {
    return null;
  }
}

async function fetchGamePatches(gameId: string): Promise<GamePatch[]> {
  try {
    const res = await fetch(`${API_BASE}/api/patches?gameId=${encodeURIComponent(gameId)}`);
    if (!res.ok) return [];
    const data = await res.json() as { patches: GamePatch[] };
    return data.patches;
  } catch {
    return [];
  }
}

function useGameMetadata(gameId: string | undefined) {
  const [metadata, setMetadata] = useState<GameMetadata | null>(null);
  const [patches, setPatches] = useState<GamePatch[]>([]);
  useEffect(() => {
    if (!gameId) return;
    fetchGameMetadata(gameId).then(setMetadata);
    fetchGamePatches(gameId).then(setPatches);
  }, [gameId]);
  return { metadata, patches };
}

/** Validate that a string is a safe CSS hex color (#rrggbb) before injecting into style. */
function safeArtworkColor(color: string | undefined): string {
  if (color && /^#[0-9a-fA-F]{6}$/.test(color)) return color;
  return 'var(--color-oasis-surface)';
}

/** ROM file extensions used in the native file picker filter. */
const ROM_EXTENSIONS = [
  // Nintendo
  'nes', 'sfc', 'smc', 'gb', 'gbc', 'gba', 'n64', 'z64', 'v64', 'nds',
  // Sega
  'sms', 'gg', 'sg', 'md', 'gen', 'bin',
  // Sony
  'iso', 'cue', 'chd', 'pbp', 'cso',
  // GameCube / Wii / Wii U
  'gcm', 'wbfs', 'wud', 'wux',
  // 3DS
  '3ds', 'cia',
];

/** System-specific quick-start hints shown on the game detail page. */
function getPartyHint(game: { system: string; maxPlayers: number; tags: string[] }): string | null {
  if (game.system === 'N64' && game.maxPlayers >= 4) {
    if (game.tags.includes('Party')) {
      return '🎉 Perfect for a full party of 4 — grab your friends and launch a room!';
    }
    return '👾 Best with 4 players — fill those slots for the full N64 experience.';
  }
  if (game.system === 'NDS' && game.maxPlayers >= 4) {
    if (game.tags.includes('Party')) {
      return '🎉 Party game for up to 4 — touch screen mini-games await!';
    }
    return '🎮 Best with 4 players — share your room code and race to fill the slots.';
  }
  if (game.system === 'NDS' && game.tags.includes('WFC')) {
    return '🌐 WFC-enabled — connects through Wiimmfi automatically. Just start a room!';
  }
  if (game.maxPlayers >= 4) {
    return `🎮 Supports up to ${game.maxPlayers} players — more is merrier!`;
  }
  return null;
}

/** Controller tip shown on N64 game detail pages. */
const N64_CONTROLLER_TIP =
  '🕹️ Controller tip: Use an Xbox or PlayStation controller — the left stick maps to the N64 analog stick automatically. A gamepad is strongly recommended for analog-heavy games.';

/** Dual-screen tip shown on NDS game detail pages. */
const NDS_DUAL_SCREEN_TIP =
  '📱 Dual-screen tip: melonDS shows the top screen above and the bottom (touch) screen below. Move your mouse to the bottom screen area and click to interact with touch controls. Use F11 to swap screens if needed.';

/** WFC tip shown on NDS games that support Pokémon Wi-Fi Connection. */
const NDS_WFC_TIP =
  '🌐 Wi-Fi Connection: This game uses Wiimmfi — a free replacement for Nintendo\'s discontinued online service. RetroOasis sets it up automatically when you host a room. No extra configuration needed!';

/** DSiWare tip shown on DSi-mode games. */
const NDS_DSI_TIP =
  '🟦 DSi Mode: This is a DSiWare title and requires DSi BIOS files (bios7i.bin, bios9i.bin, nand.bin) in your melonDS config directory. RetroOasis launches melonDS with --dsi-mode automatically when you host a room for a DSiWare game.';

/** Sega Master System tip. */
const SMS_TIP =
  '🕹️ Master System: This game runs via RetroArch + Genesis Plus GX — the same core as Sega Genesis titles. Standard 2-button (or D-pad-only) layout applies. Most modern gamepads work out of the box.';

/** Sega Genesis tip. */
const GENESIS_TIP =
  '🕹️ Genesis: Configure your gamepad to use the 3-button (A, B, C) or 6-button (A, B, C, X, Y, Z) layout for the best experience. RetroArch Genesis Plus GX supports both automatically.';

// ---------------------------------------------------------------------------
// Phase 34 — Achievement summary card
// ---------------------------------------------------------------------------

function AchievementSummaryCard({
  gameId,
  system,
  retroSummaryCount,
}: {
  gameId: string;
  system: string;
  retroSummaryCount: number | null;
}) {
  const systemKey = system.toLowerCase();
  const cap = getAchievementCapability(systemKey);
  const hasDefs = gamesWithAchievements().includes(gameId);
  const raConnected = isRAConnected();

  if (!systemSupportsAchievements(systemKey)) {
    return (
      <div
        className="mt-3 px-4 py-2 rounded-xl flex items-center gap-2 text-xs"
        style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
      >
        <span>🏅</span>
        <span>Achievements not supported for this system.</span>
      </div>
    );
  }

  if (!raConnected) {
    return (
      <div
        className="mt-3 px-4 py-2 rounded-xl flex items-center gap-2 text-xs"
        style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
      >
        <span>🏅</span>
        <span>
          <Link
            to="/settings"
            className="underline"
            style={{ color: 'var(--color-oasis-accent-light)' }}
          >
            Sign in to RetroAchievements
          </Link>{' '}
          to track progress — {cap.notes}.
        </span>
      </div>
    );
  }

  const count = retroSummaryCount ?? (hasDefs ? '?' : null);
  if (count === null) return null;

  return (
    <div
      className="mt-3 px-4 py-2 rounded-xl flex items-center gap-2 text-xs"
      style={{ backgroundColor: 'rgba(234,179,8,0.1)', color: '#fbbf24', border: '1px solid rgba(234,179,8,0.25)' }}
    >
      <span>🏅</span>
      <span className="font-semibold">{count} achievements available</span>
      <Link
        to="/retro-achievements"
        className="ml-auto text-[10px] underline"
        style={{ color: 'var(--color-oasis-accent-light)' }}
      >
        View →
      </Link>
    </div>
  );
}

export function GameDetailsPage() {
  const { gameId } = useParams<{ gameId: string }>();
  const navigate = useNavigate();
  const { createRoom, currentRoom } = useLobby();
  const [showHost, setShowHost] = useState(false);
  const [romAssoc, setRomAssoc] = useState(() =>
    gameId ? getRomAssociation(gameId) : null,
  );
  const [favorited, setFavorited] = useState(() =>
    gameId ? isFavorite(gameId) : false,
  );
  const [launchStatus, setLaunchStatus] = useState<'idle' | 'launching' | 'success' | 'error'>('idle');
  const [launchError, setLaunchError] = useState<string | null>(null);
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [showPatches, setShowPatches] = useState(false);
  const [retroSummaryCount, setRetroSummaryCount] = useState<number | null>(null);

  const { data: game, loading, error } = useGame(gameId);
  const { metadata, patches } = useGameMetadata(gameId);

  // Fetch achievement summary for this game
  useEffect(() => {
    if (!gameId) return;
    fetchRetroGameSummary(gameId)
      .then((s) => {
        setRetroSummaryCount(s ? s.totalAchievements : null);
      })
      .catch(() => {
        // Non-fatal: leave retroSummaryCount as null if the server is unavailable.
        setRetroSummaryCount(null);
      });
  }, [gameId]);

  // Navigate to lobby after room is created
  useEffect(() => {
    if (currentRoom) navigate(`/lobby/${currentRoom.id}`);
  }, [currentRoom, navigate]);

  async function handlePickRom() {
    if (!gameId) return;
    const result = await tauriPickFile([
      { name: 'ROM Files', extensions: ROM_EXTENSIONS },
    ]);
    if (result.path) {
      setRomAssociation(gameId, result.path);
      setRomAssoc(getRomAssociation(gameId));
    }
  }

  function handleClearRom() {
    if (!gameId) return;
    clearRomAssociation(gameId);
    setRomAssoc(null);
  }

  function handleToggleFavorite() {
    if (!gameId) return;
    const next = toggleFavorite(gameId);
    setFavorited(next);
  }

  async function handleLaunchLocal() {
    if (!gameId || !game) return;

    const romPath = romAssoc?.romPath ?? null;
    if (!romPath) {
      setLaunchError('No ROM file set. Use "Set ROM File" to associate a ROM with this game.');
      setLaunchStatus('error');
      return;
    }

    const system = game.system.toLowerCase();
    const backendId = getBackendIdForSystem(system);
    const emulatorPath = getEmulatorPathForSystem(system);

    if (!emulatorPath) {
      const name = backendId ? (EMULATOR_NAMES[backendId] ?? backendId) : game.system;
      setLaunchError(
        `No emulator path configured for ${name}. Go to Settings → Emulator Paths to set the executable location.`,
      );
      setLaunchStatus('error');
      return;
    }

    setLaunchStatus('launching');
    setLaunchError(null);

    try {
      const result = await tauriLaunchLocal({
        emulatorPath,
        romPath,
        system,
        backendId: backendId ?? system,
        saveDirectory: getSaveDirectory() ?? undefined,
      });

      if (result.success) {
        recordRecentlyPlayed(gameId);
        setLaunchStatus('success');
        setTimeout(() => setLaunchStatus('idle'), 3000);
      } else {
        setLaunchError(result.error ?? 'Emulator failed to start. Check the path in Settings.');
        setLaunchStatus('error');
      }
    } catch (err) {
      setLaunchError(String(err));
      setLaunchStatus('error');
    }
  }

  if (loading) {
    return (
      <div className="text-center py-12">
        <p style={{ color: 'var(--color-oasis-text-muted)' }}>Loading…</p>
      </div>
    );
  }

  if (error || !game) {
    return (
      <div className="text-center py-12">
        <p style={{ color: 'var(--color-oasis-text-muted)' }}>Game not found.</p>
        <Link to="/" className="underline text-sm" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ← Back to Home
        </Link>
      </div>
    );
  }

  const partyHint = getPartyHint(game);
  const isN64 = game.system === 'N64';
  const isNDS = game.system === 'NDS';
  const isNdsWfc = isNDS && game.tags.includes('WFC');
  const isDsiWare = isNDS && (game.isDsiWare === true || game.badges.includes('DSi Mode'));
  const isSMS = game.system === 'SMS';
  const isGenesis = game.system === 'Genesis';

  // Resolve capability state from central model
  const systemKey = game.system.toLowerCase();
  const emulatorPath = getEmulatorPathForSystem(systemKey);
  const backendId = getBackendIdForSystem(systemKey);
  const capability = resolveGameCapability(systemKey, {
    emulatorPath,
    backendId,
    romPath: romAssoc?.romPath,
    raConnected: isRAConnected(),
    hasAchievementDefs: retroSummaryCount !== null,
  });
  const onlineBadge = onlineSupportBadgeColor(capability.onlineSupportLevel);

  return (
    <div className="max-w-3xl">
      <Link to="/" className="text-sm mb-4 inline-block" style={{ color: 'var(--color-oasis-text-muted)' }}>
        ← Back
      </Link>

      <div className="rounded-2xl p-6" style={{ backgroundColor: 'var(--color-oasis-card)' }}>
        {/* Header */}
        <div className="flex items-start gap-6">
          <div
            className="w-28 h-28 rounded-xl flex items-center justify-center text-5xl flex-shrink-0"
            style={{ backgroundColor: safeArtworkColor(metadata?.artworkColor) }}
          >
            {game.coverEmoji}
          </div>
          <div className="flex-1">
            <div className="flex items-center gap-2 mb-1 flex-wrap">
              <SystemBadge system={game.system} color={game.systemColor} />
              <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                Up to {game.maxPlayers} players
              </span>
              {isNDS && !isDsiWare && (
                <span
                  className="px-2 py-0.5 rounded text-[10px] font-bold"
                  style={{ backgroundColor: '#E87722', color: 'white' }}
                >
                  Dual Screen
                </span>
              )}
              {isDsiWare && (
                <span
                  className="px-2 py-0.5 rounded text-[10px] font-bold"
                  style={{ backgroundColor: '#0369a1', color: 'white' }}
                >
                  DSiWare
                </span>
              )}
              {isNdsWfc && (
                <span
                  className="px-2 py-0.5 rounded text-[10px] font-bold"
                  style={{ backgroundColor: '#2563eb', color: 'white' }}
                >
                  WFC Online
                </span>
              )}
              {/* Online support badge derived from capability model */}
              <span
                className="px-2 py-0.5 rounded text-[10px] font-bold"
                style={{ backgroundColor: onlineBadge.bg, color: onlineBadge.text }}
                title={capability.onlineSupportNote}
              >
                {onlineSupportIcon(capability.onlineSupportLevel)}{' '}
                {capability.onlineSupportLabel}
              </span>
            </div>
            <div className="flex items-center gap-2">
              <h1 className="text-2xl font-bold mb-2 flex-1">{game.title}</h1>
              <button
                type="button"
                onClick={handleToggleFavorite}
                title={favorited ? 'Remove from favorites' : 'Add to favorites'}
                className="text-xl pb-2 transition-transform hover:scale-110 active:scale-95 flex-shrink-0"
                aria-label={favorited ? 'Remove from favorites' : 'Add to favorites'}
              >
                {favorited ? '⭐' : '☆'}
              </button>
            </div>
            <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {game.description}
            </p>
          </div>
        </div>

        {/* Party hint banner */}
        {partyHint && (
          <div
            className="mt-4 px-4 py-3 rounded-xl text-sm font-semibold"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-accent-light)' }}
          >
            {partyHint}
          </div>
        )}

        {/* N64 analog stick note */}
        {isN64 && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            {N64_CONTROLLER_TIP}
          </div>
        )}

        {/* NDS dual-screen tip */}
        {isNDS && !isDsiWare && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            {NDS_DUAL_SCREEN_TIP}
          </div>
        )}

        {/* DSiWare tip */}
        {isDsiWare && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'rgba(3,105,161,0.15)', color: '#7dd3fc' }}
          >
            {NDS_DSI_TIP}
          </div>
        )}

        {/* NDS WFC tip */}
        {isNdsWfc && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'rgba(37,99,235,0.15)', color: '#93c5fd' }}
          >
            {NDS_WFC_TIP}
          </div>
        )}

        {/* SMS controller tip */}
        {isSMS && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            {SMS_TIP}
          </div>
        )}

        {/* Genesis controller tip */}
        {isGenesis && (
          <div
            className="mt-2 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            {GENESIS_TIP}
          </div>
        )}

        {/* Phase 30 — Rich metadata: genre/developer/year pill row */}
        {metadata && (
          <div className="flex flex-wrap gap-2 mt-4">
            <span
              className="px-2.5 py-1 rounded-full text-xs font-semibold"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              🎭 {metadata.genre}
            </span>
            <span
              className="px-2.5 py-1 rounded-full text-xs font-semibold"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              🏢 {metadata.developer}
            </span>
            <span
              className="px-2.5 py-1 rounded-full text-xs font-semibold"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              📅 {metadata.year}
            </span>
          </div>
        )}

        {/* Phase 30 — Onboarding tips */}
        {metadata && metadata.onboardingTips.length > 0 && (
          <div
            className="mt-3 px-4 py-3 rounded-xl"
            style={{ backgroundColor: 'var(--color-oasis-surface)' }}
          >
            <p className="text-xs font-bold mb-2" style={{ color: 'var(--color-oasis-accent-light)' }}>
              💡 Getting Started
            </p>
            <ul className="list-disc list-inside space-y-1">
              {metadata.onboardingTips.map((tip, i) => (
                <li key={`tip-${i}-${tip}`} className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {tip}
                </li>
              ))}
            </ul>
          </div>
        )}

        {/* Phase 30 — Netplay tips */}
        {metadata && metadata.netplayTips.length > 0 && (
          <div
            className="mt-2 px-4 py-3 rounded-xl"
            style={{ backgroundColor: 'rgba(37,99,235,0.08)', border: '1px solid rgba(37,99,235,0.2)' }}
          >
            <p className="text-xs font-bold mb-2" style={{ color: '#93c5fd' }}>
              🌐 Netplay Tips
            </p>
            <ul className="list-disc list-inside space-y-1">
              {metadata.netplayTips.map((tip, i) => (
                <li key={`ntip-${i}-${tip}`} className="text-xs" style={{ color: '#93c5fd', opacity: 0.85 }}>
                  {tip}
                </li>
              ))}
            </ul>
          </div>
        )}

        {/* Phase 30 — Recommended controller */}
        {metadata && (
          <div
            className="mt-2 px-4 py-2 rounded-xl flex items-center gap-2 text-xs"
            style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
          >
            <span className="flex-shrink-0">🎮</span>
            <span><strong>Recommended controller:</strong> {metadata.recommendedController}</span>
          </div>
        )}

        {/* Phase 30 — Safe patches */}
        {patches.length > 0 && (
          <div className="mt-3">
            <button
              type="button"
              onClick={() => setShowPatches((v) => !v)}
              className="flex items-center gap-1.5 text-xs font-semibold transition-opacity hover:opacity-80"
              style={{ color: 'var(--color-oasis-text-muted)' }}
            >
              <span>{showPatches ? '▾' : '▸'}</span>
              <span>🩹 Available Patches ({patches.length})</span>
            </button>
            {showPatches && (
              <div className="mt-2 space-y-2">
                {patches.map((patch) => (
                  <div
                    key={patch.id}
                    className="px-4 py-3 rounded-xl"
                    style={{ backgroundColor: 'var(--color-oasis-surface)' }}
                  >
                    <div className="flex items-center gap-2 flex-wrap mb-1">
                      <span className="text-xs font-bold">{patch.name}</span>
                      <span
                        className="text-[10px] px-2 py-0.5 rounded-full font-semibold"
                        style={{ backgroundColor: 'rgba(22,163,74,0.15)', color: '#4ade80' }}
                      >
                        {patch.type}
                      </span>
                      <span
                        className="text-[10px] px-2 py-0.5 rounded-full font-semibold"
                        style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)' }}
                      >
                        v{patch.version}
                      </span>
                    </div>
                    <p className="text-xs mb-1.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {patch.description}
                    </p>
                    {patch.infoUrl && (
                      <a
                        href={patch.infoUrl}
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-[10px] underline"
                        style={{ color: 'var(--color-oasis-accent-light)' }}
                      >
                        More info ↗
                      </a>
                    )}
                  </div>
                ))}
              </div>
            )}
          </div>
        )}

        {/* Phase 34 — Achievements summary */}
        {game && (
          <AchievementSummaryCard
            gameId={game.id}
            system={game.system}
            retroSummaryCount={retroSummaryCount}
          />
        )}

        {/* Tags */}
        <div className="flex flex-wrap gap-2 mt-4">
          {game.tags.map((tag) => (
            <span
              key={tag}
              className="px-2.5 py-1 rounded-full text-xs font-semibold"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-accent-light)' }}
            >
              {tag}
            </span>
          ))}
        </div>

        {/* Badges */}
        <div className="flex flex-wrap gap-2 mt-3">
          {game.badges.map((badge) => (
            <span
              key={badge}
              className="px-2.5 py-1 rounded-full text-xs font-semibold"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              {badge}
            </span>
          ))}
        </div>

        {/* ROM file association */}
        <div className="mt-4">
          <button
            type="button"
            onClick={() => setShowAdvanced((v) => !v)}
            className="flex items-center gap-1.5 text-xs font-semibold transition-opacity hover:opacity-80"
            style={{ color: 'var(--color-oasis-text-muted)' }}
          >
            <span>{showAdvanced ? '▾' : '▸'}</span>
            <span>⚙ Advanced Settings</span>
          </button>
          {showAdvanced && (
            <div
              className="mt-2 px-4 py-3 rounded-xl"
              style={{ backgroundColor: 'var(--color-oasis-surface)' }}
            >
              <p className="text-xs font-bold mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
                🗂️ ROM File
              </p>
              {romAssoc ? (
                <div className="flex items-center gap-2 flex-wrap">
                  <code
                    className="text-[11px] font-mono flex-1 truncate"
                    style={{ color: 'var(--color-oasis-text)' }}
                    title={romAssoc.romPath}
                  >
                    {romAssoc.romPath}
                  </code>
                  {isTauri() && (
                    <button
                      type="button"
                      onClick={handlePickRom}
                      className="text-xs px-2 py-1 rounded-lg font-semibold transition-opacity hover:opacity-80 flex-shrink-0"
                      style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text-muted)', border: '1px solid rgba(255,255,255,0.1)' }}
                    >
                      📂 Change
                    </button>
                  )}
                  <button
                    type="button"
                    onClick={handleClearRom}
                    className="text-xs px-2 py-1 rounded-lg font-semibold transition-opacity hover:opacity-80 flex-shrink-0"
                    style={{ backgroundColor: 'rgba(230,0,18,0.1)', color: '#f87171', border: '1px solid rgba(230,0,18,0.2)' }}
                  >
                    ✕ Clear
                  </button>
                </div>
              ) : (
                <div className="flex items-center gap-2">
                  <span className="text-[11px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    No ROM file set — will use ROM directory as fallback.
                  </span>
                  {isTauri() && (
                    <button
                      type="button"
                      onClick={handlePickRom}
                      className="text-xs px-3 py-1.5 rounded-lg font-semibold transition-opacity hover:opacity-80 flex-shrink-0"
                      style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
                    >
                      📂 Set ROM File
                    </button>
                  )}
                </div>
              )}
              <p className="text-[10px] mt-1.5" style={{ color: 'var(--color-oasis-text-muted)', opacity: 0.6 }}>
                {isTauri()
                  ? 'This path is auto-used when your session starts. Use the Browse button to pick the exact ROM file.'
                  : 'Run the app in Tauri desktop mode to use the native file picker. You can also set a ROM directory in Settings.'}
              </p>
            </div>
          )}
        </div>

        {/* Launch error banner */}
        {launchStatus === 'error' && launchError && (
          <div
            className="mt-4 px-4 py-3 rounded-xl text-sm font-semibold"
            style={{ backgroundColor: 'rgba(230,0,18,0.1)', border: '1px solid rgba(230,0,18,0.3)', color: '#f87171' }}
          >
            ⚠️ {launchError}
          </div>
        )}

        {/* Capability notice — emulator not configured */}
        {!capability.emulatorConfigured && launchStatus === 'idle' && (
          <div
            className="mt-4 px-4 py-3 rounded-xl text-xs flex items-center gap-3"
            style={{ backgroundColor: 'rgba(245,158,11,0.1)', border: '1px solid rgba(245,158,11,0.3)', color: '#fbbf24' }}
          >
            <span>⚙️</span>
            <span className="flex-1">
              Emulator not configured for {game.system}.{' '}
              <Link to="/settings" className="underline font-semibold" style={{ color: '#fbbf24' }}>
                Set up in Settings → Emulator Paths
              </Link>
            </span>
          </div>
        )}

        {/* Capability notice — online support context */}
        {capability.onlineSupportLevel !== 'supported' && (
          <div
            className="mt-3 px-4 py-2 rounded-xl text-xs"
            style={{ backgroundColor: onlineBadge.bg, color: onlineBadge.text }}
          >
            {onlineSupportIcon(capability.onlineSupportLevel)}{' '}
            {capability.onlineSupportNote}
          </div>
        )}

        {/* Actions — play with friends is the primary CTA */}
        <div className="flex flex-col gap-2 mt-6">
          <button
            onClick={() => setShowHost(true)}
            className="w-full py-4 rounded-2xl font-black text-base transition-transform hover:scale-[1.01] active:scale-[0.99] btn-glow-red shimmer-overlay"
            style={{ color: 'white' }}
          >
            🎮 Play with Friends
          </button>
          <div className="flex gap-2">
            {/* Play Solo */}
            <button
              onClick={handleLaunchLocal}
              disabled={launchStatus === 'launching'}
              className="flex-1 py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98] disabled:opacity-50 text-sm"
              style={{
                backgroundColor: launchStatus === 'success'
                  ? '#16a34a'
                  : launchStatus === 'error'
                  ? 'rgba(230,0,18,0.7)'
                  : 'var(--color-oasis-surface)',
                color: 'white',
                border: '1px solid rgba(255,255,255,0.1)',
              }}
            >
              {launchStatus === 'launching'
                ? '⏳ Launching…'
                : launchStatus === 'success'
                ? '✓ Launched!'
                : launchStatus === 'error'
                ? '⚠️ Launch Failed'
                : '▶ Play Solo'}
            </button>
            <Link
              to="/friends"
              className="flex-1 py-3 rounded-xl font-bold transition-transform hover:scale-[1.02] active:scale-[0.98] flex items-center justify-center text-sm"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text)', border: '1px solid rgba(255,255,255,0.1)' }}
            >
              👥 Invite Friends
            </Link>
          </div>
        </div>
      </div>

      {showHost && (
        <HostRoomModal
          preselectedGameId={game.id}
          onConfirm={(payload, displayName) => {
            createRoom(payload, displayName);
            setShowHost(false);
          }}
          onClose={() => setShowHost(false)}
        />
      )}
    </div>
  );
}
