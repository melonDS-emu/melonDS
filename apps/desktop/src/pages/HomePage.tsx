import { Link } from 'react-router-dom';
import { MOCK_GAMES, MOCK_LOBBIES } from '../data/mock-games';
import { GameCard } from '../components/GameCard';
import { LobbyCard } from '../components/LobbyCard';

export function HomePage() {
  const partyPicks = MOCK_GAMES.filter((g) => g.tags.includes('Party'));
  const recentGames = MOCK_GAMES.slice(0, 4);

  return (
    <div className="space-y-8 max-w-5xl">
      {/* Hero actions */}
      <div className="flex gap-4">
        <button
          className="flex-1 py-4 rounded-2xl text-lg font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
          style={{ backgroundColor: 'var(--color-oasis-accent)', color: 'white' }}
        >
          🎮 Host a Room
        </button>
        <button
          className="flex-1 py-4 rounded-2xl text-lg font-bold transition-transform hover:scale-[1.02] active:scale-[0.98]"
          style={{ backgroundColor: 'var(--color-oasis-card)', color: 'var(--color-oasis-text)' }}
        >
          🚪 Join a Room
        </button>
      </div>

      {/* Joinable Lobbies */}
      <section>
        <h2 className="text-lg font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🔥 Joinable Lobbies
        </h2>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-3">
          {MOCK_LOBBIES.map((lobby) => (
            <LobbyCard key={lobby.id} lobby={lobby} />
          ))}
        </div>
      </section>

      {/* Quick Party Picks */}
      <section>
        <div className="flex items-center justify-between mb-3">
          <h2 className="text-lg font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
            🎉 Quick Party Picks
          </h2>
          <Link to="/library" className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            View All →
          </Link>
        </div>
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
          {partyPicks.map((game) => (
            <GameCard key={game.id} game={game} />
          ))}
        </div>
      </section>

      {/* Recently Played */}
      <section>
        <h2 className="text-lg font-bold mb-3" style={{ color: 'var(--color-oasis-accent-light)' }}>
          🕹️ Recently Played
        </h2>
        <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
          {recentGames.map((game) => (
            <GameCard key={game.id} game={game} />
          ))}
        </div>
      </section>
    </div>
  );
}
