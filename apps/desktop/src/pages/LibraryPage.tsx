import { useState } from 'react';
import { Link } from 'react-router-dom';
import { useGames } from '../lib/use-games';
import { GameCard } from '../components/GameCard';
import { getRomDirectory } from '../lib/rom-settings';
import { tauriScanRoms } from '../lib/tauri-ipc';
import { setRomAssociation } from '../lib/rom-library';
import { fuzzyMatchGameId } from '../lib/rom-fuzzy-match';
import { getFavoriteIds } from '../lib/favorites-store';
import { getRecentlyPlayed } from '../lib/recently-played';

const SUCCESS_MESSAGE_DURATION_MS = 3000;
const SYSTEMS = ['All', 'NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS', 'GC', '3DS'];

/** Multiplayer mode filters with friendly labels + icons. */
const MODES: { tag: string; label: string; icon: string }[] = [
  { tag: 'All',     label: 'All Games', icon: '🎮' },
  { tag: 'Party',   label: 'Party',     icon: '🎉' },
  { tag: 'Co-op',   label: 'Co-op',     icon: '🤝' },
  { tag: 'Versus',  label: 'Versus',    icon: '⚔️' },
  { tag: 'Battle',  label: 'Battle',    icon: '💥' },
  { tag: 'Link',    label: 'Link',      icon: '🔗' },
  { tag: 'Trade',   label: 'Trade',     icon: '🔄' },
];

export function LibraryPage() {
  const [selectedSystem, setSelectedSystem] = useState('All');
  const [selectedTag, setSelectedTag] = useState('All');
  const [showBestWithFriends, setShowBestWithFriends] = useState(false);
  const [scanStatus, setScanStatus] = useState<'idle' | 'scanning' | 'done' | 'error'>('idle');
  const [scanCount, setScanCount] = useState<number | null>(null);
  const [scanError, setScanError] = useState<string | null>(null);
  const romDir = getRomDirectory();

  // Fetch all games for the fuzzy-match catalog (no filters applied here)
  const { data: allGames } = useGames();

  const { data: filtered, loading, error, refetch } = useGames({
    system: selectedSystem !== 'All' ? selectedSystem : undefined,
    tag: selectedTag !== 'All' ? selectedTag : undefined,
  });

  // "Best with Friends" — games with that badge, optionally intersected with active filters
  const bestWithFriendsGames = allGames.filter((g) =>
    g.badges.includes('Best with Friends') &&
    (selectedSystem === 'All' || g.system === selectedSystem) &&
    (selectedTag === 'All' || g.tags.includes(selectedTag)),
  );

  // Compute favorites + recently-played from localStorage
  const favoriteIds = getFavoriteIds();
  const recentIds = getRecentlyPlayed(8).map((e) => e.gameId);

  const favoriteGames = allGames.filter((g) => favoriteIds.includes(g.id));
  const recentGames = recentIds
    .map((id) => allGames.find((g) => g.id === id))
    .filter(Boolean) as typeof allGames;

  async function handleScan() {
    setScanStatus('scanning');
    setScanError(null);
    setScanCount(null);
    try {
      const roms = await tauriScanRoms(romDir, true);
      // Auto-associate each discovered ROM using fuzzy title matching against
      // the full game catalog.  Falls back gracefully when no confident match
      // is found (fuzzyMatchGameId returns null).
      for (const rom of roms) {
        const gameId = fuzzyMatchGameId(rom.system, rom.inferredTitle, allGames);
        if (gameId) setRomAssociation(gameId, rom.filePath);
      }
      setScanCount(roms.length);
      refetch();
      setScanStatus('done');
      setTimeout(() => {
        setScanStatus('idle');
        setScanCount(null);
      }, SUCCESS_MESSAGE_DURATION_MS);
    } catch (err) {
      setScanError(String(err));
      setScanStatus('error');
      setTimeout(() => setScanStatus('idle'), SUCCESS_MESSAGE_DURATION_MS);
    }
  }

  // Displayed games — when "Best with Friends" spotlight is active, replace the grid
  const displayGames = showBestWithFriends ? bestWithFriendsGames : filtered;

  return (
    <div className="max-w-5xl">
      {/* ── Page header ── */}
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-2xl font-black tracking-tight" style={{ color: '#fff' }}>
            Game Library
          </h1>
          <p className="text-xs font-semibold mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {displayGames.length} game{displayGames.length !== 1 ? 's' : ''} available
          </p>
        </div>

        {/* ROM scan */}
        {romDir ? (
          <button
            onClick={handleScan}
            disabled={scanStatus === 'scanning'}
            className="text-xs font-black px-4 py-2 rounded-full transition-all hover:brightness-110 active:scale-[0.97] disabled:opacity-50"
            style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
          >
            {scanStatus === 'scanning'
              ? '⏳ Scanning…'
              : scanStatus === 'done'
              ? `✓ Found ${scanCount ?? 0} ROM${scanCount !== 1 ? 's' : ''}`
              : scanStatus === 'error'
              ? '⚠️ Scan Failed'
              : '🔍 Scan ROMs'}
          </button>
        ) : (
          <Link
            to="/settings"
            className="text-xs font-black px-4 py-2 rounded-full"
            style={{
              backgroundColor: 'var(--color-oasis-card)',
              color: 'var(--color-oasis-text-muted)',
              border: '1px solid var(--n-border)',
            }}
          >
            ⚙️ Set ROM path
          </Link>
        )}
      </div>

      {/* ROM directory info */}
      {romDir && (
        <div
          className="flex items-center gap-2 mb-5 px-3 py-2 rounded-xl text-[11px] font-semibold"
          style={{
            backgroundColor: 'var(--color-oasis-card)',
            border: '1px solid var(--n-border)',
            color: 'var(--color-oasis-text-muted)',
          }}
        >
          <span>📂</span>
          <code className="font-mono truncate">{romDir}</code>
        </div>
      )}

      {/* Scan error banner */}
      {scanStatus === 'error' && scanError && (
        <div
          className="mb-4 px-3 py-2 rounded-xl text-[11px] font-semibold"
          style={{ backgroundColor: 'rgba(230,0,18,0.1)', border: '1px solid rgba(230,0,18,0.3)', color: '#f87171' }}
        >
          ⚠️ ROM scan failed: {scanError}
        </div>
      )}

      {/* ── Quick spotlight: Best with Friends ── */}
      {bestWithFriendsGames.length > 0 && (
        <button
          type="button"
          onClick={() => setShowBestWithFriends((v) => !v)}
          className="mb-5 w-full px-4 py-3 rounded-2xl text-left transition-all"
          style={{
            background: showBestWithFriends
              ? 'linear-gradient(90deg, rgba(230,0,18,0.18) 0%, rgba(230,0,18,0.08) 100%)'
              : 'linear-gradient(90deg, rgba(255,255,255,0.04) 0%, rgba(255,255,255,0.02) 100%)',
            border: `1px solid ${showBestWithFriends ? 'rgba(230,0,18,0.5)' : 'rgba(255,255,255,0.07)'}`,
          }}
        >
          <div className="flex items-center justify-between">
            <div>
              <span className="font-black text-sm" style={{ color: showBestWithFriends ? 'var(--color-oasis-accent-light)' : 'var(--color-oasis-text)' }}>
                🤝 Best with Friends
              </span>
              <p className="text-[11px] mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {bestWithFriendsGames.length} games picked for multiplayer fun
              </p>
            </div>
            <span className="text-xs font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>
              {showBestWithFriends ? 'Show all ✕' : 'Show →'}
            </span>
          </div>
        </button>
      )}

      {/* ── System filter ── */}
      <div className="mb-2">
        <p className="text-[10px] font-black uppercase tracking-widest mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
          System
        </p>
        <div className="flex flex-wrap gap-1.5">
          {SYSTEMS.map((sys) => (
            <button
              key={sys}
              onClick={() => setSelectedSystem(sys)}
              className="px-3.5 py-1.5 rounded-full text-xs font-black transition-all"
              style={{
                backgroundColor: selectedSystem === sys ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
                color: selectedSystem === sys ? 'white' : 'var(--color-oasis-text-muted)',
                border: selectedSystem === sys ? 'none' : '1px solid var(--n-border)',
              }}
            >
              {sys}
            </button>
          ))}
        </div>
      </div>

      {/* ── Multiplayer mode filter ── */}
      <div className="mb-7">
        <p className="text-[10px] font-black uppercase tracking-widest mb-2 mt-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Multiplayer Mode
        </p>
        <div className="flex flex-wrap gap-1.5">
          {MODES.map(({ tag, label, icon }) => (
            <button
              key={tag}
              onClick={() => { setSelectedTag(tag); setShowBestWithFriends(false); }}
              className="flex items-center gap-1 px-3.5 py-1.5 rounded-full text-xs font-black transition-all"
              style={{
                backgroundColor: selectedTag === tag && !showBestWithFriends ? 'var(--color-oasis-blue)' : 'var(--color-oasis-card)',
                color: selectedTag === tag && !showBestWithFriends ? 'white' : 'var(--color-oasis-text-muted)',
                border: selectedTag === tag && !showBestWithFriends ? 'none' : '1px solid var(--n-border)',
              }}
            >
              <span>{icon}</span>
              <span>{label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* ── Favorites section ── */}
      {favoriteGames.length > 0 && selectedSystem === 'All' && selectedTag === 'All' && !showBestWithFriends && (
        <section className="mb-8">
          <p
            className="text-[10px] font-black uppercase tracking-widest mb-3"
            style={{ color: 'var(--color-oasis-text-muted)' }}
          >
            ⭐ Favorites
          </p>
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
            {favoriteGames.map((game) => (
              <GameCard key={game.id} game={game} />
            ))}
          </div>
        </section>
      )}

      {/* ── Recently Played section ── */}
      {recentGames.length > 0 && selectedSystem === 'All' && selectedTag === 'All' && !showBestWithFriends && (
        <section className="mb-8">
          <p
            className="text-[10px] font-black uppercase tracking-widest mb-3"
            style={{ color: 'var(--color-oasis-text-muted)' }}
          >
            🕐 Recently Played
          </p>
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
            {recentGames.map((game) => (
              <GameCard key={game.id} game={game} />
            ))}
          </div>
        </section>
      )}

      {/* ── Game grid ── */}
      {showBestWithFriends && (
        <p className="text-[10px] font-black uppercase tracking-widest mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
          🤝 Best with Friends
        </p>
      )}
      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
        {displayGames.map((game) => (
          <GameCard key={game.id} game={game} />
        ))}
      </div>

      {loading && (
        <p className="text-center py-12 font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Loading…
        </p>
      )}
      {error && (
        <p className="text-center py-12 font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
          ⚠️ {error}
        </p>
      )}
      {!loading && !error && displayGames.length === 0 && (
        <div className="text-center py-16">
          <p className="text-4xl mb-3">🎮</p>
          <p className="text-sm font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No games match the current filters.
          </p>
        </div>
      )}
    </div>
  );
}
