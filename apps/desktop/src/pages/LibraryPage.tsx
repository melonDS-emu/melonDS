import { useState } from 'react';
import { Link } from 'react-router-dom';
import { useGames } from '../lib/use-games';
import { GameCard } from '../components/GameCard';
import { getRomDirectory } from '../lib/rom-settings';
import { tauriScanRoms } from '../lib/tauri-ipc';
import { setRomAssociation } from '../lib/rom-library';

const SUCCESS_MESSAGE_DURATION_MS = 3000;
const SYSTEMS = ['All', 'NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS'];
const TAGS = ['All', 'Party', 'Co-op', 'Versus', 'Battle', 'Link', 'Trade'];

/** Attempt to map a scanned ROM's inferred title to a game ID using simple
 *  title normalisation. Returns null when no confident match can be made. */
function inferGameId(system: string, inferredTitle: string): string | null {
  const sys = system.toLowerCase();
  const slug = inferredTitle
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-|-$/g, '');
  return slug ? `${sys}-${slug}` : null;
}

export function LibraryPage() {
  const [selectedSystem, setSelectedSystem] = useState('All');
  const [selectedTag, setSelectedTag] = useState('All');
  const [scanStatus, setScanStatus] = useState<'idle' | 'scanning' | 'done' | 'error'>('idle');
  const [scanCount, setScanCount] = useState<number | null>(null);
  const [scanError, setScanError] = useState<string | null>(null);
  const romDir = getRomDirectory();

  const { data: filtered, loading, error, refetch } = useGames({
    system: selectedSystem !== 'All' ? selectedSystem : undefined,
    tag: selectedTag !== 'All' ? selectedTag : undefined,
  });

  async function handleScan() {
    setScanStatus('scanning');
    setScanError(null);
    setScanCount(null);
    try {
      const roms = await tauriScanRoms(romDir, true);
      // Auto-associate each discovered ROM to its inferred game ID so the
      // launch flow can resolve it automatically at session start.
      for (const rom of roms) {
        const gameId = inferGameId(rom.system, rom.inferredTitle);
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

  return (
    <div className="max-w-5xl">
      {/* ── Page header ── */}
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-2xl font-black tracking-tight" style={{ color: '#fff' }}>
            Game Library
          </h1>
          <p className="text-xs font-semibold mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
            {filtered.length} game{filtered.length !== 1 ? 's' : ''} available
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

      {/* ── Tag filter ── */}
      <div className="mb-7">
        <p className="text-[10px] font-black uppercase tracking-widest mb-2 mt-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Mode
        </p>
        <div className="flex flex-wrap gap-1.5">
          {TAGS.map((tag) => (
            <button
              key={tag}
              onClick={() => setSelectedTag(tag)}
              className="px-3.5 py-1.5 rounded-full text-xs font-black transition-all"
              style={{
                backgroundColor: selectedTag === tag ? 'var(--color-oasis-blue)' : 'var(--color-oasis-card)',
                color: selectedTag === tag ? 'white' : 'var(--color-oasis-text-muted)',
                border: selectedTag === tag ? 'none' : '1px solid var(--n-border)',
              }}
            >
              {tag}
            </button>
          ))}
        </div>
      </div>

      {/* ── Game grid ── */}
      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
        {filtered.map((game) => (
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
      {!loading && !error && filtered.length === 0 && (
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
