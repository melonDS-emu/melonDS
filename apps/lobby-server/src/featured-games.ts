/**
 * Featured Games — Phase 13
 *
 * Returns a deterministic list of featured games for the current ISO week.
 * Each week a different curated subset is promoted on the home page.
 * Games are drawn from the full catalog pool so no stale data is needed here.
 */

export interface FeaturedGame {
  gameId: string;
  gameTitle: string;
  system: string;
  reason: string;
  emoji: string;
}

/**
 * Static pool of featured-game entries.  The weekly rotation selects a
 * sliding window of `FEATURED_COUNT` entries, wrapping around the pool.
 */
const FEATURED_POOL: FeaturedGame[] = [
  // Racing
  { gameId: 'nds-mario-kart-ds',            gameTitle: 'Mario Kart DS',            system: 'nds', reason: 'Community Favorite',  emoji: '🏎️' },
  { gameId: 'n64-mario-kart-64',            gameTitle: 'Mario Kart 64',            system: 'n64', reason: 'Timeless Classic',     emoji: '🏎️' },
  { gameId: 'gba-mario-kart-super-circuit', gameTitle: 'Mario Kart: Super Circuit', system: 'gba', reason: 'Retro Pick',           emoji: '🏎️' },
  // Fighting
  { gameId: 'n64-super-smash-bros',         gameTitle: 'Super Smash Bros.',         system: 'n64', reason: 'Fan Favorite',        emoji: '🥊' },
  { gameId: 'snes-street-fighter-ii-turbo', gameTitle: 'Street Fighter II Turbo',   system: 'snes', reason: 'All-Time Classic',   emoji: '👊' },
  // Party
  { gameId: 'n64-mario-party-3',            gameTitle: 'Mario Party 3',             system: 'n64', reason: 'Party Pick',          emoji: '🎲' },
  { gameId: 'n64-mario-party-2',            gameTitle: 'Mario Party 2',             system: 'n64', reason: 'Staff Pick',          emoji: '🎲' },
  { gameId: 'snes-super-bomberman',         gameTitle: 'Super Bomberman',           system: 'snes', reason: 'Party Bomb',         emoji: '💣' },
  // Pokémon
  { gameId: 'nds-pokemon-diamond',          gameTitle: 'Pokémon Diamond',           system: 'nds', reason: 'Trending',            emoji: '💎' },
  { gameId: 'gba-pokemon-emerald',          gameTitle: 'Pokémon Emerald',           system: 'gba', reason: 'Trending',            emoji: '💚' },
  { gameId: 'gb-pokemon-red',               gameTitle: 'Pokémon Red',               system: 'gb',  reason: 'Where It All Started', emoji: '🔴' },
  // Puzzle
  { gameId: 'nds-tetris-ds',               gameTitle: 'Tetris DS',                  system: 'nds', reason: 'Quick Sessions',     emoji: '🧱' },
  { gameId: 'gb-tetris',                   gameTitle: 'Tetris',                     system: 'gb',  reason: 'The OG',             emoji: '🟦' },
  // Adventure
  { gameId: 'nds-animal-crossing-wild-world', gameTitle: 'Animal Crossing: Wild World', system: 'nds', reason: 'Chill Vibes',   emoji: '🌿' },
  { gameId: 'snes-secret-of-mana',         gameTitle: 'Secret of Mana',            system: 'snes', reason: 'Co-op Gem',         emoji: '⚔️' },
  { gameId: 'gba-zelda-four-swords',       gameTitle: 'Zelda: Four Swords',         system: 'gba', reason: 'Co-op Classic',     emoji: '🗡️' },
  // Action
  { gameId: 'n64-goldeneye-007',           gameTitle: 'GoldenEye 007',              system: 'n64', reason: 'Legendary Multiplayer', emoji: '🔫' },
  { gameId: 'n64-diddy-kong-racing',       gameTitle: 'Diddy Kong Racing',          system: 'n64', reason: 'Hidden Gem',        emoji: '🐵' },
  { gameId: 'nes-contra',                  gameTitle: 'Contra',                     system: 'nes', reason: 'Co-op Legend',      emoji: '🔥' },
  { gameId: 'nds-new-super-mario-bros',    gameTitle: 'New Super Mario Bros.',      system: 'nds', reason: 'Must-Play',         emoji: '⭐' },
];

/** Number of games shown as featured at any one time. */
export const FEATURED_COUNT = 6;

/**
 * Get the ISO week number (1–53) for a given date.
 * Implements ISO 8601 week numbering: week 1 is the week containing the first
 * Thursday of the year. The formula `d.getUTCDate() + 4 - dayNum` shifts the
 * reference date to the nearest Thursday, then computes week index from Jan 1.
 */
function isoWeek(date: Date): number {
  const d = new Date(Date.UTC(date.getFullYear(), date.getMonth(), date.getDate()));
  const dayNum = d.getUTCDay() || 7;
  d.setUTCDate(d.getUTCDate() + 4 - dayNum);
  const yearStart = new Date(Date.UTC(d.getUTCFullYear(), 0, 1));
  return Math.ceil((((d.getTime() - yearStart.getTime()) / 86400000) + 1) / 7);
}

/**
 * Return the featured games for the given date (defaults to today).
 * The selection rotates weekly; the same week always returns the same games.
 */
export function getFeaturedGames(date: Date = new Date()): FeaturedGame[] {
  const week = isoWeek(date);
  const poolLen = FEATURED_POOL.length;
  const start = (week * FEATURED_COUNT) % poolLen;
  const result: FeaturedGame[] = [];
  for (let i = 0; i < FEATURED_COUNT; i++) {
    result.push(FEATURED_POOL[(start + i) % poolLen]);
  }
  return result;
}
