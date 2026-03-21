/**
 * Phase 30 — QoL & "Wow" Layer
 *
 * PartyCollectionStore — curated, named collections of games that work
 * especially well in party / group settings.  Each collection has a title,
 * description, emoji icon, curator note, and an ordered list of game IDs.
 *
 * Collections are static/seeded.  Future phases can add DB-backed persistence.
 */

export interface PartyCollection {
  id: string;
  name: string;
  description: string;
  /** Display emoji for the collection card. */
  emoji: string;
  /** Short curator note explaining the selection. */
  curatorNote: string;
  /** Ordered game IDs included in this collection. */
  gameIds: string[];
  /** Tags like 'competitive', 'casual', 'co-op', 'racing'. */
  tags: string[];
  /** Ideal player count description (e.g. "2–4 players"). */
  idealPlayers: string;
}

// ---------------------------------------------------------------------------
// Seed data — 12 curated collections
// ---------------------------------------------------------------------------

const SEED_COLLECTIONS: PartyCollection[] = [
  {
    id: 'kart-classics',
    name: 'Kart Racing Classics',
    description: 'The best kart racers across every era — perfect for competitive groups.',
    emoji: '🏎️',
    curatorNote:
      'Every game here supports at least 4 players. Run a round-robin tournament across generations!',
    gameIds: [
      'snes-super-mario-kart',
      'n64-mario-kart-64',
      'gba-mario-kart-super-circuit',
      'gc-mario-kart-double-dash',
      'nds-mario-kart-ds',
      'wii-mario-kart-wii',
      'psx-crash-team-racing',
    ],
    tags: ['racing', 'competitive', 'all-ages'],
    idealPlayers: '2–4 players',
  },
  {
    id: 'couch-fighters',
    name: 'Couch Fighters',
    description: 'Head-to-head fighting games that ignite friendly rivalries.',
    emoji: '🥊',
    curatorNote:
      'Street Fighter II Turbo for SNES classics, Melee for technical depth, and Smash Bros 64 for pure chaos.',
    gameIds: [
      'snes-street-fighter-ii-turbo',
      'n64-super-smash-bros',
      'gc-super-smash-bros-melee',
      'wii-super-smash-bros-brawl',
    ],
    tags: ['fighting', 'competitive', '1v1'],
    idealPlayers: '2 players (1v1)',
  },
  {
    id: 'party-games',
    name: 'Ultimate Party Night',
    description: 'Games specifically designed to be played in groups with plenty of laughs.',
    emoji: '🎉',
    curatorNote:
      'Mario Party 2 is the gold standard. Power Stone 2 adds arena chaos. Four Swords is pure co-op madness.',
    gameIds: [
      'n64-mario-party-2',
      'gba-zelda-four-swords',
      'dc-power-stone-2',
      'n64-diddy-kong-racing',
    ],
    tags: ['party', 'casual', 'co-op'],
    idealPlayers: '3–4 players',
  },
  {
    id: 'pokemon-multiverse',
    name: 'Pokémon Multiverse',
    description: 'Collect, train, and battle across every Pokémon generation available on RetroOasis.',
    emoji: '🔴',
    curatorNote:
      'Trade Pokémon from Red up to Crystal and Diamond to fill your Pokédex across generations.',
    gameIds: [
      'gb-pokemon-red',
      'gbc-pokemon-crystal',
      'gba-pokemon-ruby',
      'nds-pokemon-diamond',
    ],
    tags: ['rpg', 'co-op', 'trading'],
    idealPlayers: '2 players',
  },
  {
    id: 'beat-em-ups',
    name: 'Streets Are Dangerous',
    description: 'Classic co-op beat-\'em-ups where teamwork is the only way to survive.',
    emoji: '👊',
    curatorNote:
      'Streets of Rage 2 is the undisputed king. Contra is relentless. Both reward tight co-op coordination.',
    gameIds: [
      'genesis-streets-of-rage-2',
      'nes-contra',
    ],
    tags: ['action', 'co-op', 'arcade'],
    idealPlayers: '2 players',
  },
  {
    id: 'speedrun-ready',
    name: 'Speedrun Ready',
    description: 'Tight, skill-based games where shaving seconds off your time keeps you coming back.',
    emoji: '⏱️',
    curatorNote:
      'Race your friends on any of these classics. Every one has an active speedrunning community on speedrun.com.',
    gameIds: [
      'nes-super-mario-bros',
      'snes-super-mario-world',
      'gb-pokemon-red',
      'snes-donkey-kong-country',
      'nes-mega-man-2',
    ],
    tags: ['speedrun', 'competitive', 'solo'],
    idealPlayers: '1–2 players (race format)',
  },
  {
    id: 'monster-hunters',
    name: 'Hunt Together',
    description: 'Co-op action RPGs that reward grinding, teamwork, and long sessions.',
    emoji: '🐉',
    curatorNote:
      'MHFU is the deepest multiplayer RPG on the PSP. Bring snacks — hunts can go for hours.',
    gameIds: [
      'psp-monster-hunter-freedom-unite',
    ],
    tags: ['rpg', 'co-op', 'grinding'],
    idealPlayers: '2–4 players',
  },
  {
    id: 'retro-platformers',
    name: 'Retro Platformer Marathon',
    description: 'The greatest platform games from NES through N64 — a tour through gaming history.',
    emoji: '🍄',
    curatorNote:
      'Start with SMB on NES, work up through DKC on SNES, and finish with the N64 classics.',
    gameIds: [
      'nes-super-mario-bros',
      'snes-super-mario-world',
      'snes-donkey-kong-country',
      'genesis-sonic-the-hedgehog-2',
      'dc-sonic-adventure-2',
    ],
    tags: ['platform', 'all-ages', 'casual'],
    idealPlayers: '1–2 players',
  },
  {
    id: 'puzzle-and-strategy',
    name: 'Big Brain Energy',
    description: 'Puzzle and strategy games that test your thinking as much as your reflexes.',
    emoji: '🧠',
    curatorNote:
      'Tetris head-to-head is timeless. F-Zero requires careful speed management. Both are approachable.',
    gameIds: [
      'gb-tetris',
      'snes-f-zero',
    ],
    tags: ['puzzle', 'strategy', 'competitive'],
    idealPlayers: '1–2 players',
  },
  {
    id: 'zelda-adventures',
    name: 'Zelda Chronicles',
    description: 'Link\'s greatest multiplayer adventures — solo exploration and co-op puzzle solving.',
    emoji: '🗡️',
    curatorNote:
      'Oracle of Ages for solo exploration, Four Swords for group fun. Both are classics.',
    gameIds: [
      'gbc-zelda-oracle-of-ages',
      'gba-zelda-four-swords',
    ],
    tags: ['adventure', 'co-op', 'casual'],
    idealPlayers: '1–4 players',
  },
  {
    id: 'sega-classics',
    name: 'SEGA Golden Age',
    description: 'SEGA\'s finest from the Genesis and Dreamcast eras.',
    emoji: '🔵',
    curatorNote:
      'Streets of Rage 2 and Sonic 2 defined the Genesis. SA2 and Power Stone 2 show what the Dreamcast could do.',
    gameIds: [
      'genesis-sonic-the-hedgehog-2',
      'genesis-streets-of-rage-2',
      'dc-sonic-adventure-2',
      'dc-power-stone-2',
    ],
    tags: ['action', 'all-ages', 'arcade'],
    idealPlayers: '1–4 players',
  },
  {
    id: 'online-wfc-ready',
    name: 'Online WFC Ready',
    description: 'Games with full Wiimmfi/WFC online support — no extra configuration needed.',
    emoji: '🌐',
    curatorNote:
      'Every game here connects automatically through RetroOasis\'s built-in WFC auto-config. Just start a room!',
    gameIds: [
      'nds-mario-kart-ds',
      'nds-pokemon-diamond',
      'wii-mario-kart-wii',
    ],
    tags: ['online', 'wfc', 'recommended'],
    idealPlayers: '2–12 players',
  },
];

// ---------------------------------------------------------------------------
// Store class
// ---------------------------------------------------------------------------

export class PartyCollectionStore {
  private readonly collections: PartyCollection[] = [...SEED_COLLECTIONS];

  /** All seeded collections. */
  getAll(): PartyCollection[] {
    return this.collections.slice();
  }

  /** Single collection by ID, or undefined. */
  getById(id: string): PartyCollection | undefined {
    return this.collections.find((c) => c.id === id);
  }

  /** Collections that include a given game ID. */
  getByGameId(gameId: string): PartyCollection[] {
    return this.collections.filter((c) => c.gameIds.includes(gameId));
  }

  /** Collections that have a specific tag. */
  getByTag(tag: string): PartyCollection[] {
    return this.collections.filter((c) => c.tags.includes(tag));
  }

  /** Add or update a collection. Returns the stored record. */
  upsert(collection: PartyCollection): PartyCollection {
    const idx = this.collections.findIndex((c) => c.id === collection.id);
    if (idx >= 0) {
      this.collections[idx] = collection;
    } else {
      this.collections.push(collection);
    }
    return collection;
  }

  /** Total collection count. */
  count(): number {
    return this.collections.length;
  }
}
