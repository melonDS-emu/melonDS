import { useState } from 'react';
import { Link } from 'react-router-dom';
import { useGames } from '../lib/use-games';
import { GameCard } from '../components/GameCard';
import { getRomDirectory } from '../lib/rom-settings';

const SCAN_SIMULATION_MS = 800;
const SUCCESS_MESSAGE_DURATION_MS = 3000;
const SYSTEMS = ['All', 'NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS'];
const TAGS = ['All', 'Party', 'Co-op', 'Versus', 'Battle', 'Link', 'Trade'];

export function LibraryPage() {
  const [selectedSystem, setSelectedSystem] = useState('All');
  const [selectedTag, setSelectedTag] = useState('All');
  const [scanStatus, setScanStatus] = useState<'idle' | 'scanning' | 'done'>('idle');
  const romDir = getRomDirectory();

  const { data: filtered, loading, error, refetch } = useGames({
    system: selectedSystem !== 'All' ? selectedSystem : undefined,
    tag: selectedTag !== 'All' ? selectedTag : undefined,
  });

  function handleScan() {
    setScanStatus('scanning');
    // Simulate ROM scan (real scan would run through Tauri IPC in a native build)
    setTimeout(() => {
      refetch();
      setScanStatus('done');
      setTimeout(() => setScanStatus('idle'), SUCCESS_MESSAGE_DURATION_MS);
    }, SCAN_SIMULATION_MS);
  }

  return (
    <div className="max-w-5xl">
      <div className="flex items-start justify-between mb-4">
        <h1 className="text-2xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🎮 Multiplayer Library
        </h1>

        {/* ROM scan control */}
        <div className="flex items-center gap-2">
          {romDir ? (
            <button
              onClick={handleScan}
              disabled={scanStatus === 'scanning'}
              className="text-xs font-semibold px-3 py-1.5 rounded-lg transition-opacity disabled:opacity-50"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              {scanStatus === 'scanning' ? '⏳ Scanning…' : scanStatus === 'done' ? '✓ Refreshed' : '🔍 Scan ROMs'}
            </button>
          ) : (
            <Link
              to="/settings"
              className="text-xs font-semibold px-3 py-1.5 rounded-lg"
              style={{ backgroundColor: 'var(--color-oasis-surface)', color: 'var(--color-oasis-text-muted)' }}
            >
              ⚙️ Set ROM path
            </Link>
          )}
        </div>
      </div>

      {/* ROM directory info */}
      {romDir && (
        <p className="text-[11px] mb-4" style={{ color: 'var(--color-oasis-text-muted)' }}>
          ROM directory:{' '}
          <code
            className="px-1.5 py-0.5 rounded text-[10px] font-mono"
            style={{ backgroundColor: 'var(--color-oasis-surface)' }}
          >
            {romDir}
          </code>
        </p>
      )}

      {/* System filter */}
      <div className="flex flex-wrap gap-2 mb-3">
        {SYSTEMS.map((sys) => (
          <button
            key={sys}
            onClick={() => setSelectedSystem(sys)}
            className="px-3 py-1 rounded-full text-xs font-semibold transition-colors"
            style={{
              backgroundColor: selectedSystem === sys ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
              color: selectedSystem === sys ? 'white' : 'var(--color-oasis-text-muted)',
            }}
          >
            {sys}
          </button>
        ))}
      </div>

      {/* Tag filter */}
      <div className="flex flex-wrap gap-2 mb-6">
        {TAGS.map((tag) => (
          <button
            key={tag}
            onClick={() => setSelectedTag(tag)}
            className="px-3 py-1 rounded-full text-xs font-semibold transition-colors"
            style={{
              backgroundColor: selectedTag === tag ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
              color: selectedTag === tag ? 'white' : 'var(--color-oasis-text-muted)',
            }}
          >
            {tag}
          </button>
        ))}
      </div>

      <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
        {filtered.map((game) => (
          <GameCard key={game.id} game={game} />
        ))}
      </div>

      {loading && (
        <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Loading…
        </p>
      )}
      {error && (
        <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          ⚠️ {error}
        </p>
      )}
      {!loading && !error && filtered.length === 0 && (
        <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No games match the current filters.
        </p>
      )}
    </div>
  );
}
