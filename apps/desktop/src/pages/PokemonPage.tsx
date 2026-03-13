import { useState, useEffect, useCallback } from 'react';
import { useLobby } from '../context/LobbyContext';
import { HostRoomModal } from '../components/HostRoomModal';
import type { CreateRoomPayload } from '../services/lobby-types';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

interface WfcProvider {
  id: string;
  name: string;
  dnsServer: string;
  description: string;
  url: string;
}

interface FriendCodeEntry {
  id: string;
  displayName: string;
  gameId: string;
  gameTitle: string;
  code: string;
  createdAt: string;
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const API =
  typeof import.meta !== 'undefined'
    ? (import.meta as { env?: { VITE_SERVER_URL?: string } }).env?.VITE_SERVER_URL ??
      'http://localhost:8080'
    : 'http://localhost:8080';

/** Pokémon DS games that support WFC trades and battles. */
const POKEMON_GAMES: { id: string; title: string; gen: string; color: string }[] = [
  { id: 'nds-pokemon-diamond',  title: 'Pokémon Diamond',              gen: 'Gen IV', color: '#5B8DD9' },
  { id: 'nds-pokemon-pearl',   title: 'Pokémon Pearl',                 gen: 'Gen IV', color: '#A040A0' },
  { id: 'nds-pokemon-platinum', title: 'Pokémon Platinum',             gen: 'Gen IV', color: '#6B8FA8' },
  { id: 'nds-pokemon-hgss',    title: 'Pokémon HeartGold/SoulSilver',  gen: 'Gen IV', color: '#C0A030' },
  { id: 'nds-pokemon-black',   title: 'Pokémon Black',                 gen: 'Gen V',  color: '#444444' },
  { id: 'nds-pokemon-white',   title: 'Pokémon White',                 gen: 'Gen V',  color: '#BBBBBB' },
];

type ActiveTab = 'trade' | 'battle' | 'codes';

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

function WfcProviderBadge({ provider }: { provider: WfcProvider }) {
  return (
    <span
      className="inline-flex items-center gap-1 text-[11px] font-bold px-2 py-0.5 rounded-full"
      style={{ backgroundColor: 'rgba(74,222,128,0.15)', color: '#4ade80' }}
      title={`DNS: ${provider.dnsServer} — ${provider.description}`}
    >
      🌐 {provider.name}
    </span>
  );
}

function GameTag({ title, color }: { title: string; color: string }) {
  return (
    <span
      className="text-[10px] font-bold px-2 py-0.5 rounded-full"
      style={{ backgroundColor: `${color}33`, color, border: `1px solid ${color}55` }}
    >
      {title}
    </span>
  );
}

// ---------------------------------------------------------------------------
// Trade Lobby tab
// ---------------------------------------------------------------------------

function TradeLobby({
  providers,
  displayName,
}: {
  providers: WfcProvider[];
  displayName: string;
}) {
  const { publicRooms, createRoom } = useLobby();
  const [showHost, setShowHost] = useState(false);
  const [selectedGame, setSelectedGame] = useState(POKEMON_GAMES[0].id);
  const [selectedProvider, setSelectedProvider] = useState<string>(providers[0]?.id ?? 'wiimmfi');

  const tradeRooms = publicRooms.filter(
    (r) =>
      r.gameId?.startsWith('nds-pokemon') &&
      r.maxPlayers === 2 &&
      r.players.length < r.maxPlayers
  );

  const provider = providers.find((p) => p.id === selectedProvider);

  function handleHostConfirm(payload: Omit<CreateRoomPayload, 'displayName'>, dn: string) {
    createRoom(payload, dn);
    setShowHost(false);
  }

  return (
    <div className="space-y-6">
      {/* WFC auto-config banner */}
      <div
        className="rounded-2xl p-4 flex items-start gap-3"
        style={{ backgroundColor: 'rgba(74,222,128,0.08)', border: '1px solid rgba(74,222,128,0.2)' }}
      >
        <span className="text-2xl">✅</span>
        <div>
          <p className="text-sm font-bold" style={{ color: '#4ade80' }}>
            WFC auto-configured — no DNS setup needed
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            RetroOasis automatically points melonDS at the selected WFC server.
            Just load your ROM and click Play.
          </p>
        </div>
      </div>

      {/* Provider selector */}
      {providers.length > 0 && (
        <div className="flex items-center gap-3 flex-wrap">
          <span className="text-sm font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            WFC Server:
          </span>
          {providers.map((p) => (
            <button
              key={p.id}
              onClick={() => setSelectedProvider(p.id)}
              className="text-xs font-bold px-3 py-1.5 rounded-xl transition-all"
              style={{
                backgroundColor:
                  selectedProvider === p.id
                    ? 'var(--color-oasis-accent)'
                    : 'var(--color-oasis-card)',
                color: selectedProvider === p.id ? '#fff' : 'var(--color-oasis-text-muted)',
              }}
              title={`${p.name} — DNS ${p.dnsServer}\n${p.description}`}
            >
              {p.name}
            </button>
          ))}
          {provider && (
            <span className="text-[11px]" style={{ color: 'var(--color-oasis-text-muted)' }}>
              DNS: {provider.dnsServer}
            </span>
          )}
        </div>
      )}

      {/* Host a trade room */}
      <div
        className="rounded-2xl p-5"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <h3 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🤝 Host a Trade Room
        </h3>
        <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Create a 2-player WFC trade lobby. Your partner joins with a room code, then you both
          connect to the Union Room in-game to trade Pokémon.
        </p>

        {/* Game selector */}
        <div className="flex flex-wrap gap-2 mb-4">
          {POKEMON_GAMES.map((g) => (
            <button
              key={g.id}
              onClick={() => setSelectedGame(g.id)}
              className="text-xs font-bold px-3 py-1.5 rounded-xl transition-all"
              style={{
                backgroundColor:
                  selectedGame === g.id ? 'var(--color-oasis-accent)' : 'rgba(255,255,255,0.06)',
                color: selectedGame === g.id ? '#fff' : 'var(--color-oasis-text-muted)',
              }}
            >
              {g.title}
            </button>
          ))}
        </div>

        <button
          onClick={() => setShowHost(true)}
          disabled={!displayName}
          className="text-sm font-bold px-5 py-2 rounded-xl transition-all"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: displayName ? 1 : 0.5 }}
        >
          🎴 Host Trade Lobby
        </button>
        {!displayName && (
          <p className="text-xs mt-2" style={{ color: 'var(--color-oasis-accent)' }}>
            Set a display name in Settings first.
          </p>
        )}
      </div>

      {/* Open trade rooms */}
      <div>
        <h3 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
          📋 Open Trade Rooms
        </h3>
        {tradeRooms.length === 0 ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No trade rooms open right now. Host one above!
          </p>
        ) : (
          <div className="space-y-2">
            {tradeRooms.map((r) => {
              const host = r.players.find((p) => p.isHost);
              return (
                <div
                  key={r.id}
                  className="rounded-xl px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
                >
                  <div>
                    <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                      {r.gameTitle}
                    </p>
                    <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {host ? `Hosted by ${host.displayName}` : r.name} · {r.players.length}/{r.maxPlayers} players
                    </p>
                  </div>
                  <WfcProviderBadge provider={providers.find((p) => p.id === 'wiimmfi') ?? providers[0]} />
                </div>
              );
            })}
          </div>
        )}
      </div>

      {showHost && (
        <HostRoomModal
          preselectedGameId={selectedGame}
          onConfirm={handleHostConfirm}
          onClose={() => setShowHost(false)}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Battle Lobby tab
// ---------------------------------------------------------------------------

function BattleLobby({
  providers,
  displayName,
}: {
  providers: WfcProvider[];
  displayName: string;
}) {
  const { publicRooms, createRoom } = useLobby();
  const [showHost, setShowHost] = useState(false);
  const [selectedGame, setSelectedGame] = useState(POKEMON_GAMES[0].id);

  const battleRooms = publicRooms.filter(
    (r) => r.gameId?.startsWith('nds-pokemon') && r.players.length < r.maxPlayers
  );

  function handleHostConfirm(payload: Omit<CreateRoomPayload, 'displayName'>, dn: string) {
    createRoom(payload, dn);
    setShowHost(false);
  }

  return (
    <div className="space-y-6">
      {/* Info banner */}
      <div
        className="rounded-2xl p-4 flex items-start gap-3"
        style={{ backgroundColor: 'rgba(230,0,18,0.08)', border: '1px solid rgba(230,0,18,0.2)' }}
      >
        <span className="text-2xl">⚔️</span>
        <div>
          <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-accent)' }}>
            Pokémon Battle via WFC
          </p>
          <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Host a Wi-Fi battle lobby. Both players connect via Wiimmfi, then meet in the
            Battle Tower / Wi-Fi Club to start a battle.
          </p>
        </div>
      </div>

      {/* Host */}
      <div
        className="rounded-2xl p-5"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <h3 className="text-base font-bold mb-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
          ⚔️ Host a Battle Room
        </h3>
        <p className="text-xs mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Create a 2-player Wi-Fi battle lobby. Share the room code, wait for your opponent,
          then challenge each other inside the game.
        </p>
        <div className="flex flex-wrap gap-2 mb-4">
          {POKEMON_GAMES.map((g) => (
            <button
              key={g.id}
              onClick={() => setSelectedGame(g.id)}
              className="text-xs font-bold px-3 py-1.5 rounded-xl transition-all"
              style={{
                backgroundColor:
                  selectedGame === g.id ? 'var(--color-oasis-accent)' : 'rgba(255,255,255,0.06)',
                color: selectedGame === g.id ? '#fff' : 'var(--color-oasis-text-muted)',
              }}
            >
              {g.title}
            </button>
          ))}
        </div>
        <button
          onClick={() => setShowHost(true)}
          disabled={!displayName}
          className="text-sm font-bold px-5 py-2 rounded-xl"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: '#fff', opacity: displayName ? 1 : 0.5 }}
        >
          ⚔️ Host Battle Lobby
        </button>
      </div>

      {/* Open battle rooms */}
      <div>
        <h3 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
          📋 Open Battle Rooms
        </h3>
        {battleRooms.length === 0 ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No battle rooms open right now. Challenge someone!
          </p>
        ) : (
          <div className="space-y-2">
            {battleRooms.map((r) => {
              const host = r.players.find((p) => p.isHost);
              return (
                <div
                  key={r.id}
                  className="rounded-xl px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
                >
                  <div>
                    <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text)' }}>
                      {r.gameTitle}
                    </p>
                    <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {host ? `Hosted by ${host.displayName}` : r.name} · {r.players.length}/{r.maxPlayers} players
                    </p>
                  </div>
                  <WfcProviderBadge provider={providers[0]} />
                </div>
              );
            })}
          </div>
        )}
      </div>

      {showHost && (
        <HostRoomModal
          preselectedGameId={selectedGame}
          onConfirm={handleHostConfirm}
          onClose={() => setShowHost(false)}
        />
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Friend Codes tab
// ---------------------------------------------------------------------------

function FriendCodes({ displayName }: { displayName: string }) {
  const [gameId, setGameId] = useState(POKEMON_GAMES[0].id);
  const [gameTitle, setGameTitle] = useState(POKEMON_GAMES[0].title);
  const [code, setCode] = useState('');
  const [myCodes, setMyCodes] = useState<FriendCodeEntry[]>([]);
  const [gameCodes, setGameCodes] = useState<FriendCodeEntry[]>([]);
  const [status, setStatus] = useState('');
  const [codeError, setCodeError] = useState('');

  const fetchMyCodes = useCallback(async () => {
    if (!displayName) return;
    try {
      const data = await apiFetch<FriendCodeEntry[]>(
        `/api/friend-codes?player=${encodeURIComponent(displayName)}`
      );
      setMyCodes(data);
    } catch {
      // silently ignore — server may be offline
    }
  }, [displayName]);

  const fetchGameCodes = useCallback(async () => {
    try {
      const data = await apiFetch<FriendCodeEntry[]>(
        `/api/friend-codes?gameId=${encodeURIComponent(gameId)}`
      );
      setGameCodes(data);
    } catch {
      // silently ignore
    }
  }, [gameId]);

  useEffect(() => {
    void fetchMyCodes();
  }, [fetchMyCodes]);

  useEffect(() => {
    void fetchGameCodes();
  }, [fetchGameCodes]);

  function handleCodeInput(value: string) {
    // Auto-format: insert dashes after 4 and 8 digits
    const digits = value.replace(/\D/g, '').slice(0, 12);
    const parts: string[] = [];
    if (digits.length > 0) parts.push(digits.slice(0, 4));
    if (digits.length > 4) parts.push(digits.slice(4, 8));
    if (digits.length > 8) parts.push(digits.slice(8, 12));
    setCode(parts.join('-'));
    setCodeError('');
  }

  async function handleRegister() {
    if (!/^\d{4}-\d{4}-\d{4}$/.test(code)) {
      setCodeError('Enter a 12-digit friend code, e.g. 1234-5678-9012');
      return;
    }
    try {
      await apiFetch('/api/friend-codes', {
        method: 'POST',
        body: JSON.stringify({ displayName, gameId, gameTitle, code }),
      });
      setStatus('✅ Friend code registered!');
      setCode('');
      setTimeout(() => setStatus(''), 3000);
      void fetchMyCodes();
      void fetchGameCodes();
    } catch (err) {
      setStatus(`❌ ${(err as Error).message}`);
    }
  }

  async function handleDelete(gId: string) {
    try {
      await apiFetch(
        `/api/friend-codes?player=${encodeURIComponent(displayName)}&gameId=${encodeURIComponent(gId)}`,
        { method: 'DELETE' }
      );
      void fetchMyCodes();
      void fetchGameCodes();
    } catch {
      // ignore
    }
  }

  function handleGameChange(id: string) {
    setGameId(id);
    setGameTitle(POKEMON_GAMES.find((g) => g.id === id)?.title ?? id);
  }

  return (
    <div className="space-y-6">
      {/* Explainer */}
      <div
        className="rounded-2xl p-4"
        style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
      >
        <p className="text-sm font-bold mb-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🎮 About DS Friend Codes
        </p>
        <p className="text-xs leading-relaxed" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Each DS game assigns you a unique 12-digit friend code. Register yours here so other
          RetroOasis players can add you in-game without searching Discord or forums.
          Your code only works for one specific game — you will have a different one for each title.
        </p>
      </div>

      {/* Register code */}
      {displayName ? (
        <div
          className="rounded-2xl p-5"
          style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
        >
          <h3 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
            ➕ Register Your Code
          </h3>

          {/* Game picker */}
          <div className="flex flex-wrap gap-2 mb-4">
            {POKEMON_GAMES.map((g) => (
              <button
                key={g.id}
                onClick={() => handleGameChange(g.id)}
                className="text-xs font-bold px-3 py-1.5 rounded-xl transition-all"
                style={{
                  backgroundColor:
                    gameId === g.id ? 'var(--color-oasis-accent)' : 'rgba(255,255,255,0.06)',
                  color: gameId === g.id ? '#fff' : 'var(--color-oasis-text-muted)',
                }}
              >
                {g.title}
              </button>
            ))}
          </div>

          {/* Code input */}
          <div className="flex gap-2 items-start">
            <div className="flex-1 max-w-xs">
              <input
                value={code}
                onChange={(e) => handleCodeInput(e.target.value)}
                placeholder="0000-0000-0000"
                className="w-full rounded-xl px-3 py-2 text-sm font-mono border-0 focus:outline-none"
                style={{
                  backgroundColor: 'rgba(255,255,255,0.07)',
                  color: 'var(--color-oasis-text)',
                  letterSpacing: '0.05em',
                }}
                maxLength={14}
              />
              {codeError && (
                <p className="text-[11px] mt-1" style={{ color: 'var(--color-oasis-accent)' }}>
                  {codeError}
                </p>
              )}
            </div>
            <button
              onClick={() => void handleRegister()}
              disabled={code.length < 14}
              className="text-sm font-bold px-4 py-2 rounded-xl transition-opacity"
              style={{
                backgroundColor: 'var(--color-oasis-accent)',
                color: '#fff',
                opacity: code.length >= 14 ? 1 : 0.5,
              }}
            >
              Register
            </button>
          </div>
          {status && (
            <p className="text-xs mt-2" style={{ color: '#4ade80' }}>
              {status}
            </p>
          )}
        </div>
      ) : (
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Set a display name in Settings to register friend codes.
        </p>
      )}

      {/* My codes */}
      {myCodes.length > 0 && (
        <div>
          <h3 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
            📖 My Codes
          </h3>
          <div className="space-y-2">
            {myCodes.map((entry) => (
              <div
                key={entry.id}
                className="rounded-xl px-4 py-3 flex items-center justify-between"
                style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
              >
                <div>
                  <p className="text-sm font-bold font-mono" style={{ color: 'var(--color-oasis-text)' }}>
                    {entry.code}
                  </p>
                  <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {entry.gameTitle}
                  </p>
                </div>
                <button
                  onClick={() => void handleDelete(entry.gameId)}
                  className="text-xs px-2 py-1 rounded-lg"
                  style={{ color: 'var(--color-oasis-accent)', opacity: 0.7 }}
                  title="Remove"
                >
                  ✕
                </button>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Community codes for selected game */}
      <div>
        <div className="flex items-center gap-2 mb-3">
          <h3 className="text-base font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🌍 Community Codes
          </h3>
          <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
            for {POKEMON_GAMES.find((g) => g.id === gameId)?.title}
          </span>
        </div>
        {gameCodes.filter((c) => c.displayName !== displayName).length === 0 ? (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No community codes registered for this game yet.
          </p>
        ) : (
          <div className="space-y-2">
            {gameCodes
              .filter((c) => c.displayName !== displayName)
              .map((entry) => (
                <div
                  key={entry.id}
                  className="rounded-xl px-4 py-3 flex items-center justify-between"
                  style={{ backgroundColor: 'var(--color-oasis-card)', border: '1px solid var(--n-border)' }}
                >
                  <div>
                    <p className="text-sm font-bold font-mono" style={{ color: 'var(--color-oasis-text)' }}>
                      {entry.code}
                    </p>
                    <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {entry.displayName}
                    </p>
                  </div>
                  <button
                    onClick={() => { void navigator.clipboard.writeText(entry.code); }}
                    className="text-[10px] font-bold px-2 py-1 rounded-lg"
                    style={{ backgroundColor: 'rgba(255,255,255,0.06)', color: 'var(--color-oasis-text-muted)' }}
                    title="Copy code"
                  >
                    Copy
                  </button>
                </div>
              ))}
          </div>
        )}
      </div>
    </div>
  );
}

// ---------------------------------------------------------------------------
// Page
// ---------------------------------------------------------------------------

export function PokemonPage() {
  const [activeTab, setActiveTab] = useState<ActiveTab>('trade');
  const [providers, setProviders] = useState<WfcProvider[]>([]);
  const [displayName, setDisplayName] = useState('');

  useEffect(() => {
    // Load display name from settings
    const stored = localStorage.getItem('retro-oasis-display-name');
    if (stored) setDisplayName(stored);
  }, []);

  useEffect(() => {
    apiFetch<WfcProvider[]>('/api/wfc/providers')
      .then(setProviders)
      .catch(() => {
        // Fallback providers when server is offline
        setProviders([
          {
            id: 'wiimmfi',
            name: 'Wiimmfi',
            dnsServer: '178.62.43.212',
            description: 'The largest Nintendo WFC replacement server.',
            url: 'https://wiimmfi.de',
          },
          {
            id: 'altwfc',
            name: 'AltWFC',
            dnsServer: '172.104.88.237',
            description: 'Alternative WFC server.',
            url: 'https://altwfc.com',
          },
        ]);
      });
  }, []);

  const TABS: { id: ActiveTab; label: string; icon: string }[] = [
    { id: 'trade',  label: 'Trade Lobby',  icon: '🤝' },
    { id: 'battle', label: 'Battle Lobby', icon: '⚔️' },
    { id: 'codes',  label: 'Friend Codes', icon: '🎮' },
  ];

  return (
    <div className="space-y-6 max-w-3xl">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold flex items-center gap-2" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🔴 Pokémon Online
        </h1>
        <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Trade and battle via Wi-Fi — WFC auto-configured, no DNS setup needed.
        </p>

        {/* Supported games row */}
        <div className="flex flex-wrap gap-1.5 mt-3">
          {POKEMON_GAMES.map((g) => (
            <GameTag key={g.id} title={g.title} color={g.color} />
          ))}
        </div>
      </div>

      {/* Tab bar */}
      <div
        className="flex rounded-2xl p-1 gap-1"
        style={{ backgroundColor: 'var(--color-oasis-card)' }}
      >
        {TABS.map((tab) => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            className="flex-1 flex items-center justify-center gap-1.5 py-2 text-sm font-bold rounded-xl transition-all"
            style={{
              backgroundColor:
                activeTab === tab.id ? 'var(--color-oasis-accent)' : 'transparent',
              color:
                activeTab === tab.id ? '#fff' : 'var(--color-oasis-text-muted)',
            }}
          >
            <span>{tab.icon}</span>
            <span>{tab.label}</span>
          </button>
        ))}
      </div>

      {/* Tab content */}
      {activeTab === 'trade'  && <TradeLobby  providers={providers} displayName={displayName} />}
      {activeTab === 'battle' && <BattleLobby providers={providers} displayName={displayName} />}
      {activeTab === 'codes'  && <FriendCodes  displayName={displayName} />}
    </div>
  );
}


