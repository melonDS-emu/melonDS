import { useState } from 'react';
import { useGames } from '../lib/use-games';
import { GameCard } from '../components/GameCard';
import { useLobby } from '../context/LobbyContext';
import type { RomFileInfo } from '../services/lobby-types';

const SYSTEMS = ['All', 'NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS'];
const TAGS = ['All', 'Party', 'Co-op', 'Versus', 'Battle', 'Link', 'Trade'];

export function LibraryPage() {
  const [selectedSystem, setSelectedSystem] = useState('All');
  const [selectedTag, setSelectedTag] = useState('All');
  const [showRomSettings, setShowRomSettings] = useState(false);
  const [dirInput, setDirInput] = useState('');
  const { romDirectory, scannedRoms, scanning, scanRoms } = useLobby();

  // Initialise the text input from persisted value
  const [inputInitialised, setInputInitialised] = useState(false);
  if (!inputInitialised && romDirectory && !dirInput) {
    setDirInput(romDirectory);
    setInputInitialised(true);
  }

  const { data: filtered, loading, error } = useGames({
    system: selectedSystem !== 'All' ? selectedSystem : undefined,
    tag: selectedTag !== 'All' ? selectedTag : undefined,
  });

  function handleScan() {
    const dir = dirInput.trim();
    if (dir) scanRoms(dir);
  }

  return (
    <div className="max-w-5xl">
      <div className="flex items-center justify-between mb-4">
        <h1 className="text-2xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🎮 Multiplayer Library
        </h1>
        <button
          onClick={() => setShowRomSettings((v) => !v)}
          className="text-xs font-semibold px-3 py-1.5 rounded-lg transition-colors"
          style={{
            backgroundColor: showRomSettings ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
            color: showRomSettings ? 'white' : 'var(--color-oasis-text-muted)',
          }}
        >
          ⚙ ROM Settings
        </button>
      </div>

      {/* ROM directory picker */}
      {showRomSettings && (
        <div
          className="rounded-xl p-4 mb-5 space-y-3"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            ROM Library Directory
          </p>
          <div className="flex gap-2">
            <input
              type="text"
              value={dirInput}
              onChange={(e) => setDirInput(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleScan()}
              placeholder="/home/user/roms"
              className="flex-1 rounded-lg px-3 py-2 text-sm outline-none"
              style={{
                backgroundColor: 'var(--color-oasis-surface)',
                color: 'var(--color-oasis-text)',
                border: '1px solid rgba(255,255,255,0.1)',
              }}
            />
            <button
              onClick={handleScan}
              disabled={scanning || !dirInput.trim()}
              className="px-4 py-2 rounded-lg text-sm font-semibold disabled:opacity-50"
              style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
            >
              {scanning ? 'Scanning…' : 'Scan'}
            </button>
          </div>

          {scannedRoms.length > 0 && (
            <div>
              <p className="text-xs mb-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {scannedRoms.length} ROM{scannedRoms.length !== 1 ? 's' : ''} found in{' '}
                <span className="font-mono">{romDirectory}</span>
              </p>
              <div
                className="rounded-lg overflow-hidden max-h-60 overflow-y-auto"
                style={{ backgroundColor: 'var(--color-oasis-surface)' }}
              >
                {scannedRoms.map((rom) => (
                  <RomRow key={rom.filePath} rom={rom} />
                ))}
              </div>
            </div>
          )}

          {!scanning && romDirectory && scannedRoms.length === 0 && (
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              No ROMs found. Check the directory path and try again.
            </p>
          )}
        </div>
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

function RomRow({ rom }: { rom: RomFileInfo }) {
  const systemLabel = rom.system.toUpperCase();
  const sizeKb = Math.round(rom.fileSizeBytes / 1024);

  return (
    <div
      className="px-3 py-2 flex items-center gap-3 border-b border-white/5 last:border-0"
      title={rom.filePath}
    >
      <span
        className="text-[10px] font-bold px-1.5 py-0.5 rounded flex-shrink-0"
        style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
      >
        {systemLabel}
      </span>
      <span className="flex-1 text-xs truncate" style={{ color: 'var(--color-oasis-text)' }}>
        {rom.inferredTitle}
      </span>
      <span className="text-[10px] flex-shrink-0" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {sizeKb} KB
      </span>
    </div>
  );
}
