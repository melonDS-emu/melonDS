/**
 * Phase 30 — QoL & "Wow" Layer
 *
 * GameMetadataStore — rich per-game metadata: genre, developer, year, onboarding
 * tips, netplay quality tips, recommended controller profiles, and quick-host
 * preset hints.  All data is static/seeded for now; a future phase can swap in a
 * database-backed version.
 */

export interface GameMetadata {
  gameId: string;
  /** Primary genre label (e.g. "Kart Racing", "Platform") */
  genre: string;
  /** Original developer studio */
  developer: string;
  /** Original release year */
  year: number;
  /** 1–3 short tips shown the first time a player opens the game detail page. */
  onboardingTips: string[];
  /** Tips specifically for improving online netplay quality. */
  netplayTips: string[];
  /** Recommended controller type for this game. */
  recommendedController: string;
  /** CSS-friendly hex color used as the artwork backdrop (matches cover art). */
  artworkColor: string;
  /** Quick-host preset label to pre-select in the host modal. */
  quickHostPreset?: '1v1' | '4-player' | 'full-party';
}

// ---------------------------------------------------------------------------
// Seed data — games across the major supported systems
// ---------------------------------------------------------------------------

const SEED_METADATA: GameMetadata[] = [
  // ── NES ────────────────────────────────────────────────────────────────────
  {
    gameId: 'nes-super-mario-bros',
    genre: 'Platform',
    developer: 'Nintendo',
    year: 1985,
    onboardingTips: [
      'Run with B held down for maximum speed.',
      'Collect every coin — they grant extra lives at 100.',
      'Warp zones are hidden in World 1-2 and 4-2.',
    ],
    netplayTips: [
      'NES netplay is very bandwidth-light — any stable connection works.',
      'If you see desyncs, make sure both ROMs have identical CRC checksums.',
    ],
    recommendedController: 'Any gamepad (D-pad + 2 buttons)',
    artworkColor: '#c84b11',
  },
  {
    gameId: 'nes-contra',
    genre: 'Run & Gun',
    developer: 'Konami',
    year: 1987,
    onboardingTips: [
      'Enter the Konami Code on the title screen for 30 lives.',
      'Spread gun (S) is widely considered the best weapon in co-op.',
      'Stay spread apart — a single hit ends both players if you overlap.',
    ],
    netplayTips: [
      'Sub-50 ms latency is ideal; above 80 ms you may notice missed shots.',
      'Both players should use identical ROMs (no NTSC/PAL mix).',
    ],
    recommendedController: 'NES-style pad or any 2-button gamepad',
    artworkColor: '#1a3a5c',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'nes-tecmo-super-bowl',
    genre: 'Sports (American Football)',
    developer: 'Tecmo',
    year: 1991,
    onboardingTips: [
      'Bo Jackson is unstoppable — pick the Raiders for a free win.',
      'Defense: spam B to dive-tackle ball carriers.',
      'Use Special Team plays to confuse opponents.',
    ],
    netplayTips: [
      'Tecmo Bowl is deterministic — low latency keeps plays in sync.',
      'Under 70 ms ping makes passing feel responsive.',
    ],
    recommendedController: 'D-pad gamepad',
    artworkColor: '#2d5a1b',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'nes-mega-man-2',
    genre: 'Action Platform',
    developer: 'Capcom',
    year: 1988,
    onboardingTips: [
      'Metal Man\'s weapon is effective against almost every boss.',
      'Get Metal Blade first — easiest stage is Metal Man.',
      'Crash Bomber destroys certain walls to reveal secrets.',
    ],
    netplayTips: [
      'Single-player focused; co-op spectate works well on any connection.',
    ],
    recommendedController: 'NES-style or any 2-button pad',
    artworkColor: '#0047ab',
  },
  {
    gameId: 'nes-punch-out',
    genre: 'Boxing',
    developer: 'Nintendo',
    year: 1987,
    onboardingTips: [
      'Watch opponents\' eyes — they flash just before an attack.',
      'Star punches (earned with body blows) do enormous damage.',
      'Glass Joe is the easiest fighter; Mike Tyson is the hardest.',
    ],
    netplayTips: [
      'Punch-Out!! is single-player; spectate with sub-100 ms for best experience.',
    ],
    recommendedController: 'D-pad gamepad recommended',
    artworkColor: '#8b0000',
  },

  // ── SNES ───────────────────────────────────────────────────────────────────
  {
    gameId: 'snes-super-mario-kart',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 1992,
    onboardingTips: [
      'Hop before turns to slide and maintain speed.',
      'Feather items let you jump over obstacles.',
      '150cc unlocks after completing 100cc cups.',
    ],
    netplayTips: [
      'Keep latency under 60 ms for reliable steering input.',
      'Ensure both players use the same region ROM.',
    ],
    recommendedController: 'SNES-style pad or Xbox/PlayStation controller',
    artworkColor: '#e8a000',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'snes-super-mario-world',
    genre: 'Platform',
    developer: 'Nintendo',
    year: 1990,
    onboardingTips: [
      'Yoshi can swallow enemies and gain special abilities from different shells.',
      'Hit a Cape Feather box to fly indefinitely with a running start.',
      'Star Road levels are shortcuts to the end.',
    ],
    netplayTips: [
      'SNES sync is very tolerant — most connections work fine.',
    ],
    recommendedController: 'SNES-style or modern gamepad',
    artworkColor: '#4a90d9',
  },
  {
    gameId: 'snes-street-fighter-ii-turbo',
    genre: 'Fighting',
    developer: 'Capcom',
    year: 1993,
    onboardingTips: [
      'Charge characters (Guile, Blanka) need directional holds before specials.',
      'Practice combos in Training Mode before heading online.',
      'Throws use forward/back + Y/B simultaneously.',
    ],
    netplayTips: [
      'Fighting games need under 40 ms latency for frame-perfect inputs.',
      'Consider using rollback netcode via RetroArch\'s core options.',
    ],
    recommendedController: 'SNES 6-button pad or USB arcade stick',
    artworkColor: '#b22222',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'snes-donkey-kong-country',
    genre: 'Platform',
    developer: 'Rare',
    year: 1994,
    onboardingTips: [
      'Diddy Kong is faster; Donkey Kong is stronger — swap based on the level.',
      'Rambi the Rhino can break hidden walls to find bonus rooms.',
      'Animal buddies grant special moves — protect them!',
    ],
    netplayTips: [
      'DKC co-op works well at moderate latency (< 80 ms).',
    ],
    recommendedController: 'SNES-style gamepad',
    artworkColor: '#3d2b1f',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'snes-f-zero',
    genre: 'Futuristic Racing',
    developer: 'Nintendo',
    year: 1990,
    onboardingTips: [
      'Use the side rails to gain extra boost (power slide into them briefly).',
      'The energy bar also acts as health — hitting walls depletes it.',
      'Mute City I is the best track to learn handling.',
    ],
    netplayTips: [
      'F-Zero is single-player; latency matters less for spectate.',
    ],
    recommendedController: 'SNES pad or any gamepad',
    artworkColor: '#0055a4',
  },

  // ── GB / GBC ────────────────────────────────────────────────────────────────
  {
    gameId: 'gb-tetris',
    genre: 'Puzzle',
    developer: 'Nintendo (port)',
    year: 1989,
    onboardingTips: [
      'Keep pieces low and flat until you build a well for tetrises.',
      'Rotating a piece in tight spaces requires precise timing.',
      'VS mode lets two players battle — clearing lines sends garbage.',
    ],
    netplayTips: [
      'Tetris link-cable netplay requires SameBoy or VBA-M backend.',
      'Latency tolerance is moderate — under 100 ms keeps VS smooth.',
    ],
    recommendedController: 'Any small gamepad or keyboard',
    artworkColor: '#0f3460',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'gb-pokemon-red',
    genre: 'RPG',
    developer: 'Game Freak',
    year: 1996,
    onboardingTips: [
      'Save often — battery saves can be lost on cartridges.',
      'Catch a Pokémon early to help with trading experiments.',
      'Status moves like Sleep and Paralyze make catching legendaries easier.',
    ],
    netplayTips: [
      'Pokémon Gen I trade/battle uses link cable emulation.',
      'Use SameBoy for the most accurate link cable behavior.',
      'Both players must be at a Pokémon Center or Cable Club.',
    ],
    recommendedController: 'Any 2-button gamepad or keyboard',
    artworkColor: '#cc0000',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'gbc-pokemon-crystal',
    genre: 'RPG',
    developer: 'Game Freak',
    year: 2000,
    onboardingTips: [
      'Crystal adds a Battle Tower — great for post-game competitive play.',
      'Suicune is the featured legendary; encounter it after Tin Tower events.',
      'Mobile adapter storyline is bypassed in emulation.',
    ],
    netplayTips: [
      'Use SameBoy backend for most faithful GBC link emulation.',
      'Trade evolution requires both players to complete the trade animation.',
    ],
    recommendedController: 'Any gamepad',
    artworkColor: '#2e86de',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'gbc-zelda-oracle-of-ages',
    genre: 'Action RPG',
    developer: 'Capcom / Nintendo',
    year: 2001,
    onboardingTips: [
      'Link it to Oracle of Seasons for the true ending.',
      'Nayru\'s Harp of Ages lets you solve puzzles across two timelines.',
      'Secret codes unlock items when starting the linked game.',
    ],
    netplayTips: [
      'Oracle games are single-player — spectate works great at any latency.',
    ],
    recommendedController: 'Any gamepad',
    artworkColor: '#1a3c6e',
  },

  // ── GBA ────────────────────────────────────────────────────────────────────
  {
    gameId: 'gba-pokemon-ruby',
    genre: 'RPG',
    developer: 'Game Freak',
    year: 2002,
    onboardingTips: [
      'Double Battles are new — use moves that target all opponents.',
      'Secret Bases let you trade battle team setups.',
      'Dive HM unlocks underwater routes after Mossdeep City.',
    ],
    netplayTips: [
      'GBA link cable netplay works with VBA-M backend.',
      'Wireless Adapter trade rooms require LAN mode in the emulator.',
    ],
    recommendedController: 'GBA-style or any gamepad',
    artworkColor: '#cc0000',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'gba-zelda-four-swords',
    genre: 'Action Adventure',
    developer: 'Capcom / Nintendo',
    year: 2002,
    onboardingTips: [
      'Coordination is key — collect rupees together and avoid hurting each other.',
      'Stage clear rewards scale with rupees collected as a team.',
      'Shadow Links appear randomly; defeat them for extra rupees.',
    ],
    netplayTips: [
      'Four Swords supports up to 4 players — a full party is the best experience.',
      'VBA-M link cable emulation handles 4-way connections.',
    ],
    recommendedController: 'GBA-style or Xbox/PlayStation gamepad',
    artworkColor: '#2ecc71',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'gba-mario-kart-super-circuit',
    genre: 'Kart Racing',
    developer: 'Nintendo / Intelligent Systems',
    year: 2001,
    onboardingTips: [
      'Landing coins on item boxes earns bonus points.',
      'The game includes all 20 SNES tracks as unlockable extra cups.',
      'Sliding around corners maintains speed on dirt surfaces.',
    ],
    netplayTips: [
      'Keep latency under 80 ms for smooth 4-player kart action.',
      'VBA-M 4-player link mode is supported with full-party setup.',
    ],
    recommendedController: 'GBA-style or any gamepad with D-pad',
    artworkColor: '#e8a000',
    quickHostPreset: 'full-party',
  },

  // ── N64 ────────────────────────────────────────────────────────────────────
  {
    gameId: 'n64-mario-kart-64',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 1996,
    onboardingTips: [
      'Toad and Yoshi are the fastest lightweight racers.',
      'Banana peels and shells can be held as shields by trailing them.',
      'Rainbow Road shortcut: hug the outside of the first turn.',
    ],
    netplayTips: [
      'N64 netplay benefits most from sub-60 ms latency.',
      'Use Xbox controller for the best analog stick mapping on N64.',
      'Mupen64Plus "balanced" preset is recommended for online play.',
    ],
    recommendedController: 'Xbox controller (analog stick maps perfectly)',
    artworkColor: '#e8a000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-super-smash-bros',
    genre: 'Fighting / Party',
    developer: 'HAL Laboratory',
    year: 1999,
    onboardingTips: [
      'Knock opponents off the stage — last stock standing wins.',
      'Each character has a unique recovery move — learn yours.',
      'Stock matches (3 lives each) are the standard format.',
    ],
    netplayTips: [
      'Smash 64 netplay works best under 40 ms for competitive play.',
      'For casual play, up to 80 ms is acceptable.',
      'Disable V-sync in Mupen64Plus for lower input latency.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#ff6600',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-mario-party-2',
    genre: 'Party',
    developer: 'Hudson Soft',
    year: 2000,
    onboardingTips: [
      'Saving your stars safely requires strategy — watch your star count.',
      'Bowser events shuffle stars randomly — don\'t bank on a lead!',
      'Mini-games are worth more coins than you think.',
    ],
    netplayTips: [
      'Party games tolerate up to 100 ms latency in minigame phases.',
      'Fill all 4 slots for the true Mario Party experience.',
    ],
    recommendedController: 'Xbox controller (N64 stick maps to left stick)',
    artworkColor: '#cc0000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-diddy-kong-racing',
    genre: 'Kart Racing / Adventure',
    developer: 'Rare',
    year: 1997,
    onboardingTips: [
      'Zipper boosts are hidden across every track — memorize their locations.',
      'Hovercraft and plane modes unlock in the adventure hub world.',
      'TT ghost races require beating every track first.',
    ],
    netplayTips: [
      'DKR supports up to 4 players — a full group is ideal.',
      'Sub-80 ms keeps vehicle handling responsive.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#8b0000',
    quickHostPreset: 'full-party',
  },

  // ── NDS ────────────────────────────────────────────────────────────────────
  {
    gameId: 'nds-mario-kart-ds',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 2005,
    onboardingTips: [
      'Snaking (rapid slide-boosting on straights) can increase speed significantly.',
      'Unlock all 32 tracks by winning every Cup on every CC.',
      'Mission Mode teaches advanced track techniques.',
    ],
    netplayTips: [
      'WFC mode uses Wiimmfi automatically — no DNS changes needed.',
      'Sub-70 ms latency is ideal for smooth MK:DS races.',
      'Avoid Wi-Fi interference — a wired connection is best on the host.',
    ],
    recommendedController: 'NDS-style pad or small USB gamepad',
    artworkColor: '#ff6600',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'nds-pokemon-diamond',
    genre: 'RPG',
    developer: 'Game Freak',
    year: 2006,
    onboardingTips: [
      'The Underground allows wireless fossil digging with friends.',
      'GTS lets you trade globally — but requires WFC setup.',
      'Infernape, Empoleon, or Torterra — each starter excels in different matchups.',
    ],
    netplayTips: [
      'Use the Global Terminal inside Pokémon Centers to access WFC features.',
      'Wiimmfi WFC is pre-configured in RetroOasis — just host a room.',
    ],
    recommendedController: 'Touch-friendly setup recommended',
    artworkColor: '#6666ff',
    quickHostPreset: '1v1',
  },

  // ── Wii ────────────────────────────────────────────────────────────────────
  {
    gameId: 'wii-mario-kart-wii',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 2008,
    onboardingTips: [
      'Bikes can wheelie on straightaways for a speed boost.',
      'Draft behind racers ahead for a slipstream boost.',
      '12-player online races are available through Wiimmfi.',
    ],
    netplayTips: [
      'MKWii uses Wiimmfi — zero-setup WFC via RetroOasis.',
      'Sub-80 ms latency keeps item timing accurate.',
      'Use wired LAN adapter on the host for best results.',
    ],
    recommendedController: 'Wii Wheel or Xbox/PS controller via Dolphin',
    artworkColor: '#e8a000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'wii-super-smash-bros-brawl',
    genre: 'Fighting / Party',
    developer: 'Sora Ltd.',
    year: 2008,
    onboardingTips: [
      'Tripping mechanic is random in regular matches — be cautious during runs.',
      'Final Smashes require breaking a Smash Ball — grab it quickly.',
      'Custom stages can be shared with friends.',
    ],
    netplayTips: [
      'Brawl netplay works best under 60 ms — tripping is harder to manage above.',
      'Use Dolphin\'s netplay lobby for host/join flow.',
    ],
    recommendedController: 'Xbox or PlayStation controller via Dolphin',
    artworkColor: '#1a3c6e',
    quickHostPreset: 'full-party',
  },

  // ── GameCube ────────────────────────────────────────────────────────────────
  {
    gameId: 'gc-mario-kart-double-dash',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 2003,
    onboardingTips: [
      'Each character pair has a unique special item — use them wisely.',
      'The driver in back can throw items — coordinate with your partner.',
      'Baby Park is great for chaotic 4-player races.',
    ],
    netplayTips: [
      'GC LAN mode supports up to 8 players over netplay.',
      'Dolphin netplay works best under 60 ms.',
    ],
    recommendedController: 'GameCube controller or Xbox controller via Dolphin',
    artworkColor: '#cc0000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'gc-super-smash-bros-melee',
    genre: 'Fighting',
    developer: 'HAL Laboratory',
    year: 2001,
    onboardingTips: [
      'L-cancelling halves aerial attack lag — master it for competitive play.',
      'Wavedashing is an advanced slide technique using air-dodge to ground.',
      'Fox and Falco are popular high-tier picks for online play.',
    ],
    netplayTips: [
      'Melee netplay with Slippi rollback code is possible via Dolphin.',
      'Under 30 ms is ideal for competitive Melee; casual play is fine at 60 ms.',
      'Use Wired connection — wireless adds jitter to rollback.',
    ],
    recommendedController: 'GameCube controller (highly recommended for Melee)',
    artworkColor: '#ff6600',
    quickHostPreset: '1v1',
  },

  // ── SEGA Genesis ───────────────────────────────────────────────────────────
  {
    gameId: 'genesis-sonic-the-hedgehog-2',
    genre: 'Platform',
    developer: 'Sonic Team / Sega',
    year: 1992,
    onboardingTips: [
      'Tails follows Player 1 — great for beginners.',
      'Spin dash (Down + B hold then release) gives massive speed bursts.',
      'Chemical Plant Zone Act 2 is notoriously difficult — don\'t rush.',
    ],
    netplayTips: [
      'Genesis netplay via RetroArch is very lightweight.',
      'Sub-80 ms keeps co-op movement in sync.',
    ],
    recommendedController: '6-button Genesis pad or modern gamepad',
    artworkColor: '#0070bd',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'genesis-streets-of-rage-2',
    genre: 'Beat \'em up',
    developer: 'Sega',
    year: 1992,
    onboardingTips: [
      'Axel\'s Grand Upper combo (B, B, C) is devastating in crowds.',
      'Blaze is the most technical character — great for veterans.',
      'Grab → throw enemies into each other for crowd control.',
    ],
    netplayTips: [
      'Co-op beat-em-up is tolerant of latency — fun at up to 100 ms.',
      'RetroArch Genesis Plus GX core is the recommended backend.',
    ],
    recommendedController: '6-button pad or Xbox/PlayStation controller',
    artworkColor: '#8b0000',
    quickHostPreset: '1v1',
  },

  // ── Dreamcast ──────────────────────────────────────────────────────────────
  {
    gameId: 'dc-sonic-adventure-2',
    genre: 'Action Platform',
    developer: 'Sonic Team',
    year: 2001,
    onboardingTips: [
      'Chao Garden lets you raise creatures and connect with other players.',
      'Treasure hunting stages — keep hunting, the radar updates as you move closer.',
      'Two campaigns (Hero / Dark) unlock the final story stage.',
    ],
    netplayTips: [
      'Flycast netplay supports rollback for VS stages.',
      'Sub-80 ms recommended for 2P racing stages.',
    ],
    recommendedController: 'Dreamcast-style or Xbox/PlayStation controller',
    artworkColor: '#003366',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-power-stone-2',
    genre: 'Fighting / Arena',
    developer: 'Capcom',
    year: 2000,
    onboardingTips: [
      'Collect all 3 Power Stones to transform into a super form.',
      'Use interactive stage elements (cannons, bombs) against opponents.',
      'Up to 4 players fight simultaneously — it\'s chaotic and fun.',
    ],
    netplayTips: [
      'Power Stone 2 supports 4-player — fill the lobby for best experience.',
      'Under 80 ms latency keeps transformation combos reliable.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#cc6600',
    quickHostPreset: 'full-party',
  },

  // ── PSX ────────────────────────────────────────────────────────────────────
  {
    gameId: 'psx-crash-team-racing',
    genre: 'Kart Racing',
    developer: 'Naughty Dog',
    year: 1999,
    onboardingTips: [
      'Boost management: three consecutive boosts from slides gives a super boost.',
      'Each hub boss rewards a Relic Key — collect all for the true ending.',
      'Oxide Time Trials are some of the hardest challenges in the game.',
    ],
    netplayTips: [
      'DuckStation netplay supports up to 4 players.',
      'Sub-60 ms keeps boost timing accurate.',
    ],
    recommendedController: 'PlayStation-style or Xbox controller',
    artworkColor: '#f7941d',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'psx-castlevania-symphony-of-the-night',
    genre: 'Action RPG / Metroidvania',
    developer: 'Konami',
    year: 1997,
    onboardingTips: [
      'The Reverse Castle opens after a key story moment — explore both halves.',
      'Familiars level up alongside Alucard — choose one that fits your style.',
      'Library cards let you instantly warp to the castle entrance.',
    ],
    netplayTips: [
      'SotN is single-player; spectate works at any latency.',
    ],
    recommendedController: 'PlayStation-style or any gamepad',
    artworkColor: '#3d0070',
  },

  // ── PS2 ────────────────────────────────────────────────────────────────────
  {
    gameId: 'ps2-guitar-hero-ii',
    genre: 'Rhythm / Music',
    developer: 'Harmonix',
    year: 2006,
    onboardingTips: [
      'Practice Mode lets you slow tracks down — perfect for learning solos.',
      'Hammer-on and pull-off notes don\'t need the strum bar.',
      'Co-op Campaign has unique arrangements not in solo mode.',
    ],
    netplayTips: [
      'Rhythm games need very low latency — under 30 ms is ideal for VS.',
      'PCSX2 netplay is experimental; consider local co-op via shared session.',
    ],
    recommendedController: 'Guitar Hero controller or mapped gamepad',
    artworkColor: '#660000',
    quickHostPreset: '1v1',
  },

  // ── PSP ────────────────────────────────────────────────────────────────────
  {
    gameId: 'psp-monster-hunter-freedom-unite',
    genre: 'Action RPG',
    developer: 'Capcom',
    year: 2008,
    onboardingTips: [
      'Bring Mega Potions and Whetstones on every hunt — you will need them.',
      'Target the monster\'s legs to trip it for safe follow-up attacks.',
      'Hammers and Great Swords deal the most raw damage per hit.',
    ],
    netplayTips: [
      'PPSSPP infrastructure mode enables 4-player co-op hunts.',
      'Under 100 ms keeps hit registration in sync during multiplayer hunts.',
      'All hunters must be in the same Village before entering a quest.',
    ],
    recommendedController: 'Xbox or PlayStation controller (dual analog required)',
    artworkColor: '#8b6914',
    quickHostPreset: 'full-party',
  },
];

// ---------------------------------------------------------------------------
// Store class
// ---------------------------------------------------------------------------

export class GameMetadataStore {
  private readonly map = new Map<string, GameMetadata>();

  constructor() {
    for (const m of SEED_METADATA) {
      this.map.set(m.gameId, m);
    }
  }

  /** Return metadata for a specific game, or undefined if not seeded. */
  get(gameId: string): GameMetadata | undefined {
    return this.map.get(gameId);
  }

  /** Return all seeded metadata entries. */
  getAll(): GameMetadata[] {
    return Array.from(this.map.values());
  }

  /**
   * Upsert a metadata record (useful for tests or admin tooling).
   * Returns the stored record.
   */
  set(record: GameMetadata): GameMetadata {
    this.map.set(record.gameId, record);
    return record;
  }

  /** Return all metadata for games in the given genre. */
  getByGenre(genre: string): GameMetadata[] {
    return this.getAll().filter(
      (m) => m.genre.toLowerCase() === genre.toLowerCase(),
    );
  }

  /** Return the count of seeded entries. */
  count(): number {
    return this.map.size;
  }
}
