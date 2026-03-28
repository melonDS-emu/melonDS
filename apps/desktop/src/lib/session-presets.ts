/**
 * Quick-start session presets for the "Play with Friends" host flow.
 *
 * These lightweight presets help players pick a player-count configuration
 * without touching advanced settings.  The logic is pure data — no React
 * imports — so it can be unit-tested directly.
 */

/** A quick-start session preset shown before the player customises the room. */
export interface SessionPreset {
  id: string;
  label: string;
  description: string;
  playerCount: number;
  emoji: string;
}

/** Tags that trigger a "Full Party" preset (fill all player slots). */
const FULL_PARTY_TAGS = ['Party', 'Battle'];

/** Generate quick-start presets for a given game based on its properties. */
export function getGamePresets(game: {
  maxPlayers: number;
  tags: string[];
  badges: string[];
}): SessionPreset[] {
  const presets: SessionPreset[] = [];

  if (game.maxPlayers >= 4 && game.tags.some((t) => FULL_PARTY_TAGS.includes(t))) {
    presets.push({
      id: 'full-party',
      label: 'Full Party',
      description: `${game.maxPlayers} players — fill every slot`,
      playerCount: game.maxPlayers,
      emoji: '🎉',
    });
  }

  if (game.maxPlayers >= 4) {
    presets.push({
      id: 'four-player',
      label: '4-Player Match',
      description: 'Classic 4-player session',
      playerCount: 4,
      emoji: '🕹️',
    });
  }

  if (game.maxPlayers >= 2) {
    presets.push({
      id: 'two-player',
      label: '1-on-1',
      description: 'Focused 2-player battle',
      playerCount: 2,
      emoji: '⚔️',
    });
  }

  return presets;
}
