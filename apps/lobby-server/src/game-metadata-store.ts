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
  {
    gameId: 'n64-goldeneye-007',
    genre: 'First-Person Shooter',
    developer: 'Rare',
    year: 1997,
    onboardingTips: [
      'Use the License to Kill mode with Slappers Only for chaotic fun.',
      'Proximity mines at doorways are the classic camping strategy.',
      '2-player split-screen is more stable than 4-player for netplay.',
    ],
    netplayTips: [
      'Limit to 2 players online — 4P split-screen is CPU-intensive and may desync.',
      'Use Mupen64Plus "accurate" preset for best compatibility.',
      'Sub-60 ms strongly recommended; above 80 ms hit detection becomes unreliable.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#d4af37',
    quickHostPreset: 'standard',
  },
  {
    gameId: 'n64-mario-tennis',
    genre: 'Sports',
    developer: 'Camelot',
    year: 2000,
    onboardingTips: [
      'Each character has a unique play style — power, speed, or all-around.',
      'Timed power shots charge up during rallies for higher-speed returns.',
      'The tournament mode teaches all court types before you go online.',
    ],
    netplayTips: [
      'Tennis is very timing-sensitive — aim for sub-70 ms for smooth rallies.',
      'Doubles (2v2) is the recommended online format for 4 players.',
      'Mupen64Plus "balanced" preset works well for this title.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#1a6600',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-mario-golf',
    genre: 'Sports',
    developer: 'Camelot',
    year: 1999,
    onboardingTips: [
      'Match the power gauge peak timing precisely to get full distance.',
      'Wind direction shown on the course map — factor it into every shot.',
      'Each character has different max power and shot curve.',
    ],
    netplayTips: [
      'Golf is turn-based — tolerates up to 150 ms latency comfortably.',
      'A 4-round game with 4 players takes roughly 90 minutes online.',
      'Stroke Play is the smoothest mode for online sessions.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#2d6a00',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-pokemon-stadium',
    genre: 'Strategy / Battle',
    developer: 'HAL Laboratory',
    year: 1999,
    onboardingTips: [
      'Use the Transfer Pak with a Game Boy cartridge to battle with your own Pokémon.',
      'Rental Pokémon are available but have limited move sets.',
      'The Prime Cup requires Lv. 50 and Lv. 100 brackets separately.',
    ],
    netplayTips: [
      'Battle animations can cause timing drift at >80 ms — keep speed set to Fast.',
      'Transfer Pak emulation requires enabling the --transfer-pak flag in Mupen64Plus.',
      'All 4 players need to agree on rental vs. own-Pokémon before starting.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#cc0000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-pokemon-stadium-2',
    genre: 'Strategy / Battle',
    developer: 'HAL Laboratory',
    year: 2000,
    onboardingTips: [
      'Gold/Silver Pokémon are available — bring your Gen 2 team via Transfer Pak.',
      'Little Cup (Lv. 5 unevolved) is a fan favourite competitive format.',
      'The Gym Leader Castle requires beating all 16 Kanto + Johto leaders.',
    ],
    netplayTips: [
      'Same Transfer Pak rules as Stadium 1 — enable --transfer-pak in Mupen64Plus.',
      'Set battle speed to Fastest and animations to Skip for minimal lag.',
      'Sub-80 ms is comfortable; above that, turn-start animations may become desynced.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#d4a000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-wave-race-64',
    genre: 'Jet Ski Racing',
    developer: 'Nintendo',
    year: 1996,
    onboardingTips: [
      'Lean into buoys correctly — hitting them on the right side boosts your turbo meter.',
      'Stunt Mode rewards aerial tricks between buoys for bonus points.',
      'Weight distribution (lean forward/back) affects speed and handling in waves.',
    ],
    netplayTips: [
      'Wave Race is 2-player only — perfect for a low-latency head-to-head race.',
      'Sub-80 ms ensures the wave physics simulation stays in sync.',
      'Mupen64Plus "balanced" preset is ideal; "accurate" if wave glitches appear.',
    ],
    recommendedController: 'Xbox controller (triggers map well to accelerate/brake)',
    artworkColor: '#005fa3',
    quickHostPreset: 'standard',
  },
  {
    gameId: 'n64-star-fox-64',
    genre: 'Rail Shooter / Versus',
    developer: 'Nintendo',
    year: 1997,
    onboardingTips: [
      'Lock-on laser (hold Z) is critical for tough bosses and homing shots.',
      'Perform U-turns in All-Range Mode to shake pursuing enemies.',
      'The VS battle mode supports 4 players and is the best multiplayer mode.',
    ],
    netplayTips: [
      'VS mode is fully deterministic — netplay is stable up to ~120 ms.',
      'Requires the Expansion Pak for 4-player VS mode.',
      'Enable Mupen64Plus "fast" preset (dynarec) for best framerate in VS.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#663300',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-f-zero-x',
    genre: 'Anti-Gravity Racing',
    developer: 'Nintendo',
    year: 1998,
    onboardingTips: [
      'Hold A + B simultaneously for a boost that drains energy — use near track end.',
      'Side-attack (tap left/right + Z) to spin-smash rivals off the track.',
      'Captain Falcon in the Blue Falcon is a well-rounded beginner pick.',
    ],
    netplayTips: [
      'F-Zero X has fully deterministic physics — one of the best N64 netplay titles.',
      'Works well at up to 120 ms latency with Mupen64Plus.',
      'Use "balanced" or "fast" preset; "accurate" is unnecessary for this title.',
    ],
    recommendedController: 'Xbox controller (bumpers for side-attack)',
    artworkColor: '#00008b',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-perfect-dark',
    genre: 'First-Person Shooter',
    developer: 'Rare',
    year: 2000,
    onboardingTips: [
      'Combat Simulator bots (simulants) can fill slots for 1–4 player sessions.',
      'FarSight XR-20 can target enemies through walls — controversial online.',
      'The Laptop Gun secondary function deploys as an auto-turret.',
    ],
    netplayTips: [
      'Perfect Dark requires Expansion Pak for full 4-player VS.',
      'Sub-80 ms ensures reliable hit detection in fast-paced combat.',
      'Use Mupen64Plus "fast" preset (dynarec) to maintain 30 fps under load.',
    ],
    recommendedController: 'Xbox controller (dual-stick layout matches PD\'s dual-control scheme)',
    artworkColor: '#4b0082',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-mario-party',
    genre: 'Party',
    developer: 'Hudson Soft',
    year: 1998,
    onboardingTips: [
      'The original Mario Party infamously required rotating the analog stick rapidly — use button controls instead.',
      'Koopa Troopa spaces send you backward, so plan your star route carefully.',
      'Mini-game coins add up fast — win consistently to dominate the board.',
    ],
    netplayTips: [
      'Mario Party 1 tolerates up to 120 ms latency during mini-games.',
      'Fill all 4 slots for the full chaotic party experience.',
      'Note: joystick-rotation mini-games use button substitutes in emulation.',
    ],
    recommendedController: 'Xbox controller (no analog rotation needed in emulated form)',
    artworkColor: '#cc3300',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'n64-bomberman-64',
    genre: 'Action / Battle',
    developer: 'Hudson Soft',
    year: 1997,
    onboardingTips: [
      'Pump bombs to increase their blast radius — essential for arena control.',
      'Kicking a pumped bomb can surprise opponents across the arena.',
      'The 3D arenas have multiple levels; use the height advantage.',
    ],
    netplayTips: [
      'Bomberman 64 battle mode is fully deterministic — great for netplay.',
      'Sub-80 ms is ideal; the fast-paced explosions need tight sync.',
      'Use "balanced" preset — the title is well-optimised in Mupen64Plus.',
    ],
    recommendedController: 'Xbox controller',
    artworkColor: '#ff4400',
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
  {
    gameId: 'dc-marvel-vs-capcom-2',
    genre: 'Fighting / Tag',
    developer: 'Capcom',
    year: 2000,
    onboardingTips: [
      'Build a team of 3 — mix rushdown, keepaway, and assist characters.',
      'Learn to DHC (Delayed Hyper Combo) between supers for massive damage.',
      'Snapback forces out a specific opponent character — great for targeting low-health backline.',
    ],
    netplayTips: [
      'One of the most netplay-tested DC titles — fully deterministic.',
      'Sub-60 ms one-way strongly recommended for hi-lo mix combos.',
      'Both players must use the same ROM region for identical frame data.',
    ],
    recommendedController: 'PlayStation-style controller (6-button layout preferred)',
    artworkColor: '#8b0000',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-soul-calibur',
    genre: 'Weapons Fighting',
    developer: 'Namco',
    year: 1999,
    onboardingTips: [
      'Vertical and horizontal attacks each have different range — mix them up.',
      'Guard Impacts (GI) parry and counter in one motion — timing is everything.',
      'Soul Charge briefly enhances your character\'s attacks; use it after a knockdown.',
    ],
    netplayTips: [
      'Fully deterministic 3D fighter; excellent relay stability.',
      'Counter-hit timing requires ≤ 60 ms one-way for competitive matches.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#1a1a5e',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-crazy-taxi',
    genre: 'Arcade Racing',
    developer: 'SEGA / Hitmaker',
    year: 1999,
    onboardingTips: [
      'CRAZY Dash at start (hold gas + brake, then full throttle) for a speed boost.',
      'Crazy Through, Crazy Drift, and Crazy Stop combos multiply your tip earnings.',
      'Learn shortcut paths — the quickest routes aren\'t always on the road.',
    ],
    netplayTips: [
      'Score-attack sessions are latency-tolerant up to ~100 ms.',
      'Both players should use the same ROM region for identical passenger spawn patterns.',
    ],
    recommendedController: 'Xbox or PlayStation controller (analog triggers recommended)',
    artworkColor: '#cc8800',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-virtua-tennis',
    genre: 'Sports / Tennis',
    developer: 'SEGA / Hitmaker',
    year: 1999,
    onboardingTips: [
      'Time your swing to the bounce — early returns go cross-court, late shots go down the line.',
      'Doubles mode pairs two human players on the same side; communicate before serving.',
      'World Tour mini-games (ball machine, vs. robots) also work in relay sessions.',
    ],
    netplayTips: [
      'Deterministic physics; 4-player doubles stable on relay at ≤ 100 ms.',
      'Confirm all players are on the same ROM version before starting doubles.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#006600',
    quickHostPreset: '2v2',
  },
  {
    gameId: 'dc-project-justice',
    genre: 'Fighting / Team',
    developer: 'Capcom',
    year: 2000,
    onboardingTips: [
      'Team-up attacks require specific character pairings — learn your school\'s combo.',
      'Brutal KO attacks deal massive damage but leave you open if blocked.',
      'Counter-burst (Rival Break) can reverse a losing match — use it wisely.',
    ],
    netplayTips: [
      'Deterministic 3D fighter; tested on flycast_libretro relay.',
      'Sub-80 ms recommended for Rival Break counter timing.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#4a0055',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-jet-grind-radio',
    genre: 'Action / Style',
    developer: 'SEGA / Smilebit',
    year: 2000,
    onboardingTips: [
      'String grinds and jumps together for a Style multiplier on your tags.',
      'Tag-battle mode: tag all rival gang territories before time runs out.',
      'Memorise spray tag inputs early — complex tags (hold + follow shapes) score higher.',
    ],
    netplayTips: [
      'Tag-battle mode is the 2P competitive mode; aim for ≤ 80 ms.',
      'Both players need matching ROM versions for identical stage layouts.',
    ],
    recommendedController: 'Xbox or PlayStation controller (analog stick for grinding)',
    artworkColor: '#cc4400',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-dead-or-alive-2',
    genre: 'Fighting / 3D',
    developer: 'Tecmo / Team Ninja',
    year: 2000,
    onboardingTips: [
      'Holds (counters) are the core mechanic — high, mid, or low hold at the right moment.',
      'Triangle system: Strikes beat Throws, Throws beat Holds, Holds beat Strikes.',
      'Use stage walls and multi-tier drops to extend combos — the Dreamcast version has the most stages.',
    ],
    netplayTips: [
      'Deterministic 3D fighter; hold timing is frame-sensitive — aim for ≤ 60 ms.',
      'The Dreamcast version is considered the definitive release; ensure both players use the same build.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#1a004a',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-capcom-vs-snk',
    genre: 'Fighting / 2D',
    developer: 'Capcom',
    year: 2000,
    onboardingTips: [
      'The Ratio system assigns each character a value 1–4; balance your team\'s total Ratio.',
      'SNK-groove and Capcom-groove styles play very differently — learn both.',
      'Ratio 4 characters (Geese, Rugal) are high-risk powerhouses; protect your big slot.',
    ],
    netplayTips: [
      'Fully deterministic 2-D crossover fighter; relay sessions excellent at ≤ 80 ms.',
      'Confirm groove settings match before online ranked sessions.',
    ],
    recommendedController: 'PlayStation-style controller (6-button layout preferred)',
    artworkColor: '#800000',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-ikaruga',
    genre: 'Vertical Shooter',
    developer: 'Treasure',
    year: 2001,
    onboardingTips: [
      'Absorb bullets matching your colour to charge your homing laser.',
      'Chain 3 enemies of the same colour for score multipliers — S-ranking requires perfect chains.',
      'In 2-player co-op, coordinate colour switches to cover each other\'s weak side.',
    ],
    netplayTips: [
      'Fully deterministic bullet patterns; 2P co-op excellent at ≤ 100 ms.',
      'High-chain co-op runs benefit from voice chat to coordinate polarity switches.',
    ],
    recommendedController: 'Xbox or PlayStation controller (D-Pad for precise dodging)',
    artworkColor: '#000033',
    quickHostPreset: '2p-coop',
  },
  {
    gameId: 'dc-cannon-spike',
    genre: 'Action Shooter',
    developer: 'Capcom / Psikyo',
    year: 2000,
    onboardingTips: [
      'Each Street Fighter character has unique shots and a powerful cannon spike special.',
      'Lock-on system auto-targets the nearest enemy; switch targets manually with the second button.',
      'Co-op: focus one player on crowd clearing, the other on boss damage.',
    ],
    netplayTips: [
      'Arcade-style deterministic co-op; stable on relay at ≤ 100 ms.',
      'Boss health is shared in 2P — coordinate special use to avoid wasting ammo.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#006633',
    quickHostPreset: '2p-coop',
  },
  {
    gameId: 'dc-tech-romancer',
    genre: 'Fighting / Mech',
    developer: 'Capcom',
    year: 1998,
    onboardingTips: [
      'Weapon gauge fills as you take damage — save it for super-armoured weapon attacks.',
      'Each mech has a unique Hyper Mode triggered at low health; bait your opponent in.',
      'Wall bounces and ground slams extend air combos significantly.',
    ],
    netplayTips: [
      'Deterministic 3D mech fighter; relay tested with flycast_libretro.',
      'Recommend ≤ 80 ms for consistent guard cancel and Hyper Mode timing.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#003355',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'dc-toy-racer',
    genre: 'Racing / Kart',
    developer: 'SEGA',
    year: 2000,
    onboardingTips: [
      'Toy Racer was originally an online-only Dreamcast game — designed for relay from day one.',
      'Collect power-up crates mid-race for temporary speed boosts and weapons.',
      'Smaller toys have tighter handling; larger ones have better straight-line speed.',
    ],
    netplayTips: [
      'Originally designed for Dreamcast online — deterministic physics ideal for relay.',
      '4-player sessions stable at ≤ 100 ms; low bandwidth requirement.',
    ],
    recommendedController: 'Xbox or PlayStation controller (analog triggers for throttle)',
    artworkColor: '#cc0033',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'dc-gauntlet-legends',
    genre: 'Action RPG',
    developer: 'Midway',
    year: 2000,
    onboardingTips: [
      'Shoot food when low on health and enemies are near — don\'t waste it on a full bar.',
      'Each character class excels at different encounter types: Warrior tanks, Valkyrie deflects, Wizard nukes.',
      'Collect Runestones in each world to unlock the final area of each realm.',
    ],
    netplayTips: [
      'Co-op AI paths introduce occasional desync above 100 ms — keep sessions under that.',
      'Save your relay slot for 3–4 player sessions; 2-player is also solid.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#5a2d00',
    quickHostPreset: 'full-party',
  },

  // ── PSX ────────────────────────────────────────────────────────────────────
  {
    gameId: 'psx-tekken-3',
    genre: 'Fighting',
    developer: 'Namco',
    year: 1998,
    onboardingTips: [
      'Learn each character\'s signature 10-hit combo — they deal massive damage.',
      'Side-stepping is key; Tekken 3 introduced true 3-D side movement.',
      'Grapple breaks require matching the attack button used by the opponent.',
    ],
    netplayTips: [
      'Input-driven fighter; very low desync rate. Sub-80 ms is ideal.',
      'Set RetroArch input delay to 2–3 frames for comfortable online play.',
    ],
    recommendedController: 'PlayStation-style controller (D-Pad preferred)',
    artworkColor: '#1a1a4e',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'psx-street-fighter-alpha-3',
    genre: 'Fighting',
    developer: 'Capcom',
    year: 1998,
    onboardingTips: [
      'Choose your ISM (V-ISM, A-ISM, X-ISM) carefully — each changes your super system.',
      'A-ISM custom combos can deal devastating damage if practiced.',
      'World Tour mode unlocks new moves for every character.',
    ],
    netplayTips: [
      'Deterministic 2-D fighter; reliable at ≤ 100 ms one-way latency.',
      'Disable audio interpolation in RetroArch for tighter timing.',
    ],
    recommendedController: 'PlayStation-style controller (6-button layout preferred)',
    artworkColor: '#7a0000',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'psx-tony-hawks-pro-skater-2',
    genre: 'Extreme Sports',
    developer: 'Neversoft',
    year: 2000,
    onboardingTips: [
      'Manual into trick combos dramatically multiplies your score.',
      'Hold Grind to extend rail grinds and earn more points.',
      'Complete all S-K-A-T-E letters in every level for score bonuses.',
    ],
    netplayTips: [
      'Score-attack is deterministic; online sessions work well at ≤ 100 ms.',
      'Both players need the same ROM region for score parity.',
    ],
    recommendedController: 'Any gamepad — analog sticks not required',
    artworkColor: '#1a3a00',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'psx-twisted-metal-2',
    genre: 'Combat Racing',
    developer: 'SingleTrac',
    year: 1996,
    onboardingTips: [
      'Learn the special attack codes (e.g. ↑↓↓← for Sweet Tooth\'s fire clown).',
      'Homing missiles can be shaken with tight turns and speed bursts.',
      'Freeze + Napalm is one of the deadliest combos in the game.',
    ],
    netplayTips: [
      'Physics are deterministic in 1v1; occasional desyncs in 4-player at > 120 ms.',
      'Keep sessions under 3 hours to avoid memory accumulation drift.',
    ],
    recommendedController: 'Any gamepad',
    artworkColor: '#3a0000',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'psx-crash-bash',
    genre: 'Party',
    developer: 'Eurocom',
    year: 1999,
    onboardingTips: [
      'Requires the Multitap accessory for 3 or 4 players in a single session.',
      'Different minigame types favour different playstyles — try them all.',
      'Adventure mode unlocks gems that boost your party-game stats.',
    ],
    netplayTips: [
      'Up to 4 players via DuckStation multitap relay. Keep latency under 100 ms.',
      'Minigames are turn-based or short-burst; latency has minimal gameplay impact.',
    ],
    recommendedController: 'PlayStation-style controller',
    artworkColor: '#5a3a00',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'psx-worms-armageddon',
    genre: 'Strategy / Turn-Based',
    developer: 'Team17',
    year: 1999,
    onboardingTips: [
      'Wind direction shown in the top-right affects all projectiles — check before firing.',
      'The Holy Hand Grenade has a 3-second fuse; count carefully.',
      'Ninja rope momentum combined with a Shotgun is the most versatile move.',
    ],
    netplayTips: [
      'Turn-based game; very latency-tolerant — playable at up to 200 ms.',
      'All four players can be on opposite sides of the world without gameplay issues.',
    ],
    recommendedController: 'Any gamepad or keyboard',
    artworkColor: '#1a4a00',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'psx-crash-bandicoot',
    genre: 'Platformer',
    developer: 'Naughty Dog',
    year: 1996,
    onboardingTips: [
      'Spin attack while running through Nitro crates — don\'t jump into them.',
      'Clear boxes in every level for the Gem — needed for 100 % completion.',
      'Cortex bonus rounds are timed; learn the layout to get all tokens quickly.',
    ],
    netplayTips: [
      'Single-player; online session is for synchronized co-viewing or race challenges.',
      'Spectate-friendly at any latency.',
    ],
    recommendedController: 'PlayStation-style controller',
    artworkColor: '#e07820',
  },
  {
    gameId: 'psx-metal-gear-solid',
    genre: 'Stealth / Action',
    developer: 'Konami',
    year: 1998,
    onboardingTips: [
      'Crawl under objects to avoid enemy line-of-sight in tight corridors.',
      'The Codec frequency for Meryl\'s contact is on the CD case (or your memory card in-universe).',
      'Many bosses have unconventional defeat conditions — read all Codec tips.',
    ],
    netplayTips: [
      'FMV-heavy; streaming decompression can drift at > 80 ms latency.',
      'Keep co-viewing sessions under 4 hours to avoid accumulation drift.',
    ],
    recommendedController: 'PlayStation-style controller',
    artworkColor: '#1c2b1c',
  },
  {
    gameId: 'psx-final-fantasy-vii',
    genre: 'RPG',
    developer: 'Square',
    year: 1997,
    onboardingTips: [
      'Materia combinations (e.g. All + Magic or Mime + Knights of the Round) are game-breaking in the best way.',
      'The Chocobo breeding sidequest unlocks Knights of the Round — the most powerful summon.',
      'Save to multiple slots before key story moments.',
    ],
    netplayTips: [
      'Turn-based combat tolerates latency well; safe to play at up to 150 ms.',
      'Random-encounter RNG can desync under certain emulator configs — use the same BIOS.',
    ],
    recommendedController: 'Any gamepad',
    artworkColor: '#1a2a4a',
  },
  {
    gameId: 'psx-gran-turismo-2',
    genre: 'Racing Simulation',
    developer: 'Polyphony Digital',
    year: 1999,
    onboardingTips: [
      'Requires an analog DualShock controller — digital pad lacks steering precision.',
      'Earn license badges to unlock higher-tier races.',
      'Tuning the suspension and gear ratios makes a bigger difference than horsepower alone.',
    ],
    netplayTips: [
      'Physics simulation is partially non-deterministic; occasional desyncs at > 80 ms.',
      'Enable analog mode (--analog flag) in DuckStation before starting a race session.',
    ],
    recommendedController: 'DualShock (analog required)',
    artworkColor: '#002244',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'psx-diablo',
    genre: 'Action RPG',
    developer: 'Blizzard Entertainment',
    year: 1997,
    onboardingTips: [
      'The PS1 port adds a unique Warrior class variant not in the PC version.',
      'Shrines respawn each new dungeon — use them liberally for buffs.',
      'Mana Shield (Sorcerer) effectively doubles your total hit points.',
    ],
    netplayTips: [
      'Single-player dungeon crawler; online session is co-viewing only.',
      'Spectate-friendly at any latency — no multiplayer mode on PS1.',
    ],
    recommendedController: 'PlayStation-style controller',
    artworkColor: '#2a0000',
  },

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
