import { useState } from 'react';
import { MOCK_GAMES } from '../data/mock-games';
import { GameCard } from '../components/GameCard';

const SYSTEMS = ['All', 'NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS'];
const TAGS = ['All', 'Party', 'Co-op', 'Versus', 'Battle', 'Link', 'Trade'];

export function LibraryPage() {
  const [selectedSystem, setSelectedSystem] = useState('All');
  const [selectedTag, setSelectedTag] = useState('All');

  const filtered = MOCK_GAMES.filter((g) => {
    if (selectedSystem !== 'All' && g.system !== selectedSystem) return false;
    if (selectedTag !== 'All' && !g.tags.includes(selectedTag)) return false;
    return true;
  });

  return (
    <div className="max-w-5xl">
      <h1 className="text-2xl font-bold mb-4" style={{ color: 'var(--color-oasis-accent-light)' }}>
        🎮 Multiplayer Library
      </h1>

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

      {filtered.length === 0 && (
        <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No games match the current filters.
        </p>
      )}
    </div>
  );
}
