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

  // ── PS2 additional ────────────────────────────────────────────────────────
  {
    gameId: 'ps2-jak-and-daxter',
    genre: 'Platformer',
    developer: 'Naughty Dog',
    year: 2001,
    artworkColor: '#00439C',
    onboardingTips: [
      'No loading screens anywhere in Sandover Village -- the world loads seamlessly.',
      'Collect all Power Cells to unlock Dark Eco Silos.',
      'Jak cannot attack early in the game -- use Daxter to distract enemies.',
    ],
    netplayTips: [
      'Single-player only -- relay session for spectating and co-commentary.',
      'PCSX2 handles PS2 BIOS boot automatically if bios path is set.',
    ],
    recommendedController: 'DualShock 2',
  },
  {
    gameId: 'ps2-tekken-5',
    genre: 'Fighting',
    developer: 'Namco',
    year: 2004,
    artworkColor: '#00439C',
    onboardingTips: [
      'Tekken 5 has the full arcade history mode -- unlock characters via Story mode.',
      'Just Frame moves reward precise 1-frame inputs with bonus damage.',
      'Devil Within is a separate action mini-game unlocked after Story Mode.',
    ],
    netplayTips: [
      '2-player relay session via PCSX2 -- 80 ms latency target for fighting game timing.',
      'Use wired connection for lowest latency during 1v1 matches.',
      'Arcade Stick recommended for competitive play.',
    ],
    recommendedController: 'DualShock 2 or Arcade Stick',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'ps2-ssx3',
    genre: 'Sports / Racing',
    developer: 'EA Canada',
    year: 2003,
    artworkColor: '#00439C',
    onboardingTips: [
      'SSX 3 features one continuous mountain -- unlock peaks by earning medals.',
      'Ubertricks require a full boost meter -- hold the trick button to chain.',
      'Race and Slopestyle events each have separate leaderboards.',
    ],
    netplayTips: [
      '2-player race sessions work well at 100 ms -- deterministic physics engine.',
      'PCSX2 multitap not required for 2-player SSX3.',
    ],
    recommendedController: 'DualShock 2',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'ps2-timesplitters-2',
    genre: 'Shooter',
    developer: 'Free Radical Design',
    year: 2002,
    artworkColor: '#00439C',
    onboardingTips: [
      'TimeSplitters 2 supports 4-player split-screen locally.',
      'Arcade Custom mode lets you set custom rules, bots, and arenas.',
      'Story mode supports 2-player co-op on the same machine.',
    ],
    netplayTips: [
      '4-player relay session via PCSX2 multitap support.',
      'PCSX2 --multitap flag required for 3-4 player sessions.',
      'Deathmatch modes work best under 100 ms for responsive aiming.',
    ],
    recommendedController: 'DualShock 2',
    quickHostPreset: '4-player',
  },
  {
    gameId: 'ps2-devil-may-cry',
    genre: 'Action',
    developer: 'Capcom',
    year: 2001,
    artworkColor: '#00439C',
    onboardingTips: [
      'Style meter (S/A/B/C/D) grades your combo variety -- never repeat the same move.',
      'Dante can launch enemies with Launcher, then continue combos in the air.',
      'Secret missions reward Red Orbs and Blue Orbs for upgrading.',
    ],
    netplayTips: [
      'Single-player title -- relay session for spectating.',
      'PCSX2 runs DMC well with Vulkan renderer and fast boot enabled.',
    ],
    recommendedController: 'DualShock 2',
  },
  {
    gameId: 'ps2-ico',
    genre: 'Action Adventure',
    developer: 'Team ICO / SCE',
    year: 2001,
    artworkColor: '#00439C',
    onboardingTips: [
      "Hold Square to grip Yorda's hand -- never let go in dangerous areas.",
      'Light sources repel shadow creatures -- use them strategically.',
      'Pause often to appreciate the minimalist art direction.',
    ],
    netplayTips: [
      'Single-player title -- relay session for spectating or co-commentary.',
      'PCSX2 runs ICO well with software renderer for accurate lighting.',
    ],
    recommendedController: 'DualShock 2',
  },

  // ── PS3 metadata ──────────────────────────────────────────────────────────
  {
    gameId: 'ps3-street-fighter-iv',
    genre: 'Fighting',
    developer: 'Capcom',
    year: 2009,
    artworkColor: '#003087',
    onboardingTips: [
      'Focus Attacks absorb one hit and can be cancelled with backdash.',
      'Ultra Combos require a full Revenge Gauge -- use them after taking damage.',
      'EX moves cost one stock of Super meter for armored or enhanced versions.',
    ],
    netplayTips: [
      '1v1 relay session via RPCS3 RPCN -- 60 ms target for frame-perfect inputs.',
      'Wired connection strongly recommended for fighting games.',
      'RPCS3 RPCN account required -- free at rpcs3.net/rpcn.',
    ],
    recommendedController: 'DualShock 3 or Arcade Stick',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'ps3-tekken-6',
    genre: 'Fighting',
    developer: 'Namco Bandai',
    year: 2009,
    artworkColor: '#003087',
    onboardingTips: [
      'Bound combos extend juggles -- launch, bound, then continue air combo.',
      'Rage activates below 10% health -- extra damage window.',
      'Scenario Campaign unlocks customization items.',
    ],
    netplayTips: [
      '1v1 relay session via RPCS3 RPCN -- 80 ms latency target.',
      'Set RPCS3 to Vulkan renderer for best compatibility.',
      'RPCN account required for online relay.',
    ],
    recommendedController: 'DualShock 3 or Arcade Stick',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'ps3-little-big-planet',
    genre: 'Platformer',
    developer: 'Media Molecule',
    year: 2008,
    artworkColor: '#003087',
    onboardingTips: [
      'Collect all Prize Bubbles in a level to unlock bonus items.',
      'Create mode lets you build custom levels with the SackBoy toolkit.',
      'Some levels require 4 players to collect all items -- plan co-op runs.',
    ],
    netplayTips: [
      '4-player co-op relay via RPCS3 RPCN -- 120 ms max for casual platforming.',
      'RPCS3 RPCN account needed for all players.',
      'Asymmetric multiplayer: different players can take different paths.',
    ],
    recommendedController: 'DualShock 3',
    quickHostPreset: '4-player',
  },
  {
    gameId: 'ps3-castle-crashers',
    genre: 'Beat-em-up',
    developer: 'The Behemoth',
    year: 2010,
    artworkColor: '#003087',
    onboardingTips: [
      'Magic attacks scale with Magic stat -- build Magic or Agility for casters.',
      'Animal Orbs provide passive bonuses -- find them throughout the world.',
      'Juggle enemies in the air with up+Y for massive combo damage.',
    ],
    netplayTips: [
      '4-player co-op relay via RPCS3 RPCN -- 100 ms target for beat-em-up timing.',
      'Each player chooses a different colored knight for easy identification.',
    ],
    recommendedController: 'DualShock 3',
    quickHostPreset: '4-player',
  },
  {
    gameId: 'ps3-mortal-kombat-2011',
    genre: 'Fighting',
    developer: 'NetherRealm Studios',
    year: 2011,
    artworkColor: '#003087',
    onboardingTips: [
      'Enhanced Special moves (with meter) are stronger versions of normal specials.',
      'X-Ray attacks use 3 bars of meter for massive damage -- save for combos.',
      'Challenge Tower has 300 challenges to unlock bonus content.',
    ],
    netplayTips: [
      '1v1 relay via RPCS3 RPCN -- 80 ms target for kombat timing.',
      'Wired connection recommended for fatality inputs.',
      'RPCN account required for online sessions.',
    ],
    recommendedController: 'DualShock 3 or Arcade Stick',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'ps3-wipeout-hd',
    genre: 'Racing',
    developer: 'SCE Studio Liverpool',
    year: 2008,
    artworkColor: '#003087',
    onboardingTips: [
      'Airbraking around corners is essential at Phantom speed class.',
      'Use weapons defensively -- shield before corners to absorb hazards.',
      'Zone mode requires surviving increasingly fast speed zones.',
    ],
    netplayTips: [
      '8-player race relay via RPCS3 RPCN -- 80 ms target.',
      'Higher speed classes require lower latency for precise racing.',
    ],
    recommendedController: 'DualShock 3',
    quickHostPreset: '4-player',
  },
  {
    gameId: 'ps3-blazblue-calamity-trigger',
    genre: 'Fighting',
    developer: 'Arc System Works',
    year: 2009,
    artworkColor: '#003087',
    onboardingTips: [
      "Drive attacks are unique per character -- master your character's Drive.",
      'Barrier Burst creates space when cornered but leaves you staggered.',
      'Rapid Cancel costs 50 Heat to cancel recovery frames of any attack.',
    ],
    netplayTips: [
      '1v1 relay via RPCS3 RPCN -- 60 ms target for anime fighter frame data.',
      'RPCN rollback not available -- relay TCP recommended for low-latency sessions.',
    ],
    recommendedController: 'DualShock 3 or Arcade Stick',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'ps3-borderlands',
    genre: 'Shooter / RPG',
    developer: 'Gearbox Software',
    year: 2009,
    artworkColor: '#003087',
    onboardingTips: [
      'Build around Class Mods and Skill Trees -- each Vault Hunter has unique abilities.',
      'Slag-coated enemies take 3x damage from other elemental sources.',
      'Golden Keys from SHiFT codes unlock rare gear from the Golden Chest in Sanctuary.',
    ],
    netplayTips: [
      '4-player co-op via RPCS3 RPCN -- 120 ms target for looter-shooter sessions.',
      'RPCN account required for all co-op sessions.',
      'Loot is instanced -- all players get separate drops in co-op.',
    ],
    recommendedController: 'DualShock 3',
    quickHostPreset: '4-player',
  },

  // ── Sega CD metadata ──────────────────────────────────────────────────────
  {
    gameId: 'segacd-sonic-cd',
    genre: 'Platformer',
    developer: 'Sonic Team / Sega',
    year: 1993,
    artworkColor: '#4B5EFC',
    onboardingTips: [
      'Spin Dash through Past and Future signs to change time periods.',
      'Destroy all Robot Generators in the PAST to get a Good Future ending.',
      'Metal Sonic race in Stardust Speedway is the iconic boss of Sonic CD.',
    ],
    netplayTips: [
      'Single-player title -- relay session for spectating.',
      'Requires Sega CD BIOS (bios_CD_U.bin) in RetroArch system directory.',
    ],
    recommendedController: 'Genesis 3-Button or 6-Button',
  },
  {
    gameId: 'segacd-final-fight-cd',
    genre: 'Beat-em-up',
    developer: 'Capcom / Sega',
    year: 1993,
    artworkColor: '#4B5EFC',
    onboardingTips: [
      'All 3 characters (Cody, Haggar, Guy) are available unlike SNES port.',
      'CD audio tracks replace the original SNES sound -- vastly improved.',
      'Grab-and-throw enemies is faster than stand-alone attacks.',
    ],
    netplayTips: [
      '2-player co-op via RetroArch Genesis Plus GX relay.',
      'Sega CD BIOS required in RetroArch system directory.',
      'Best under 100 ms for beat-em-up responsiveness.',
    ],
    recommendedController: 'Genesis 6-Button',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'segacd-lunar-silver-star',
    genre: 'RPG',
    developer: 'Game Arts',
    year: 1992,
    artworkColor: '#4B5EFC',
    onboardingTips: [
      'Anime cutscenes and voice acting were groundbreaking for 1992.',
      'Magic Points are shared across the party -- manage MP carefully.',
      'Working Designs localization added humor and personality.',
    ],
    netplayTips: [
      'Single-player JRPG -- relay session for spectating or co-commentary.',
      'Requires Sega CD BIOS for proper CD audio playback.',
    ],
    recommendedController: 'Genesis 6-Button',
  },
  {
    gameId: 'segacd-nba-jam',
    genre: 'Sports',
    developer: 'Midway / Iguana',
    year: 1994,
    artworkColor: '#4B5EFC',
    onboardingTips: [
      "He's on fire! Three consecutive buckets lights a player up.",
      'Push, shove, and swat opponents -- no fouls in this game.',
      'Secret players unlock via specific initials at character select.',
    ],
    netplayTips: [
      '2-player relay via RetroArch Genesis Plus GX -- 80 ms target.',
      'Sega CD version has enhanced CD audio over the Genesis version.',
    ],
    recommendedController: 'Genesis 3-Button',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'segacd-snatcher',
    genre: 'Adventure',
    developer: 'Konami / Hideo Kojima',
    year: 1994,
    artworkColor: '#4B5EFC',
    onboardingTips: [
      'Examine everything -- Gillian Seed must inspect all clues for story progress.',
      'Junker HQ is your home base -- return often for new assignments.',
      'The Sega CD version includes Act 3, which the PC-88 original omitted.',
    ],
    netplayTips: [
      'Single-player visual novel -- relay session for co-commentary.',
      'Rare and highly valuable physical copy -- ROM required from personal backup.',
    ],
    recommendedController: 'Genesis 3-Button',
  },
  {
    gameId: 'segacd-robo-aleste',
    genre: 'Shoot-em-up',
    developer: 'Compile',
    year: 1992,
    artworkColor: '#4B5EFC',
    onboardingTips: [
      'Sub-weapons cycle with the C button -- mix primary and sub for boss fights.',
      'Shield upgrades absorb one hit -- collect them in later stages.',
      '2-player co-op: player 2 uses alternate mech with slightly different weapons.',
    ],
    netplayTips: [
      '2-player co-op relay via RetroArch Genesis Plus GX.',
      'Shmup sessions require low latency -- aim for under 100 ms.',
      'Sega CD BIOS required for CD audio.',
    ],
    recommendedController: 'Genesis 6-Button',
    quickHostPreset: '1v1',
  },

  // ── Sega 32X metadata ─────────────────────────────────────────────────────
  {
    gameId: 'sega32x-knuckles-chaotix',
    genre: 'Platformer',
    developer: 'Sega',
    year: 1995,
    artworkColor: '#CC0000',
    onboardingTips: [
      'The rubber band mechanic lets you slingshot your partner for speed boosts.',
      'Five Comix are the playable characters -- each has slightly different stats.',
      'Special Rings are hidden in each act -- find them all for a complete run.',
    ],
    netplayTips: [
      '2-player co-op via RetroArch PicoDrive relay.',
      'Sega 32X requires ROM in .32x format.',
    ],
    recommendedController: 'Genesis 6-Button',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'sega32x-cosmic-carnage',
    genre: 'Fighting',
    developer: 'Almanic / Sega',
    year: 1994,
    artworkColor: '#CC0000',
    onboardingTips: [
      'Each fighter has a unique weapon that changes their moveset.',
      'Rapid attacks stagger the opponent -- use them to set up throws.',
      'Cosmic Carnage is a 32X exclusive launch title.',
    ],
    netplayTips: [
      '1v1 relay via RetroArch PicoDrive -- 80 ms target for fighting game timing.',
      '.32x ROM format required for PicoDrive.',
    ],
    recommendedController: 'Genesis 6-Button',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'sega32x-star-wars-arcade',
    genre: 'Shooter',
    developer: 'Sega',
    year: 1994,
    artworkColor: '#CC0000',
    onboardingTips: [
      'Use proton torpedoes on the exhaust port for the trench run finale.',
      '2-player co-op: one pilots, one gunner in a turret view.',
      'Star Wars Arcade is a port of the System 32 arcade original.',
    ],
    netplayTips: [
      '2-player co-op relay via RetroArch PicoDrive.',
      'Best under 100 ms for 3D shooter responsiveness.',
    ],
    recommendedController: 'Genesis 6-Button',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'sega32x-nba-jam-te',
    genre: 'Sports',
    developer: 'Midway / Probe',
    year: 1994,
    artworkColor: '#CC0000',
    onboardingTips: [
      'Tournament Edition adds Bill Clinton and other hidden characters.',
      '32X version has enhanced color depth over the Genesis version.',
      "He's on fire still applies -- three consecutive shots.",
    ],
    netplayTips: [
      '2-player relay via RetroArch PicoDrive -- 80 ms target.',
      'Sports games work well under 120 ms for casual play.',
    ],
    recommendedController: 'Genesis 3-Button',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'sega32x-doom',
    genre: 'Shooter',
    developer: 'id Software / Sega',
    year: 1994,
    artworkColor: '#CC0000',
    onboardingTips: [
      'The 32X port includes all original DOOM levels at a higher frame rate than SNES.',
      'Secrets are hidden behind walls -- shoot or use suspicious walls.',
      'Plasma Rifle and BFG 9000 are the most powerful weapons.',
    ],
    netplayTips: [
      'Single-player -- relay session for spectating.',
      'PicoDrive handles the 32X hardware extensions for DOOM faithfully.',
    ],
    recommendedController: 'Genesis 6-Button',
  },

  // ── GB/GBC overhaul metadata ───────────────────────────────────────────────
  {
    gameId: 'gb-pokemon-yellow',
    genre: 'RPG',
    developer: 'Game Freak / Nintendo',
    year: 1998,
    artworkColor: '#8B956D',
    onboardingTips: [
      'Pikachu follows you on the overworld -- check its happiness in Cerulean City.',
      "Yellow version unlocks all three starter Pokemon through in-game trades.",
      'Surfing Pikachu mini-game unlocks if Pikachu knows Surf.',
    ],
    netplayTips: [
      'Link cable trading and battling via mGBA --link-host relay.',
      'Both players need the same mGBA version for stable link cable emulation.',
      'Pokemon Yellow is cross-compatible with Red and Blue via link cable.',
    ],
    recommendedController: 'Game Boy',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'gb-links-awakening',
    genre: 'Action Adventure',
    developer: 'Nintendo',
    year: 1993,
    artworkColor: '#8B956D',
    onboardingTips: [
      'The Wind Fish Egg is on Mount Tamaranch -- gather all 8 Instruments to enter.',
      'Trade sequence items with NPCs to unlock the Boomerang.',
      "Link's Awakening DX (GBC) adds a Color Dungeon with unique rewards.",
    ],
    netplayTips: [
      'Single-player title -- relay session for spectating or casual co-commentary.',
      'mGBA emulates the original GB version with high accuracy.',
    ],
    recommendedController: 'Game Boy',
  },
  {
    gameId: 'gb-wario-land',
    genre: 'Platformer',
    developer: 'Nintendo R&D1',
    year: 1994,
    artworkColor: '#8B956D',
    onboardingTips: [
      'Collect 10 coins in a level to earn a Heart Container.',
      "Wario's dash attack breaks brick walls and stuns enemies.",
      'Find all treasures for the best ending -- extra gold earns a bigger castle.',
    ],
    netplayTips: [
      'Single-player title -- relay session for spectating.',
      'mGBA runs Wario Land with accurate SGB border support.',
    ],
    recommendedController: 'Game Boy',
  },
  {
    gameId: 'gbc-zelda-oracle-of-seasons',
    genre: 'Action Adventure',
    developer: 'Capcom / Nintendo',
    year: 2001,
    artworkColor: '#6A5ACD',
    onboardingTips: [
      'Rod of Seasons changes the season -- use it to cross obstacles and solve puzzles.',
      'Linked Game: clear Oracle of Ages first for a Password that unlocks the true ending.',
      'Rings can be equipped for passive bonuses -- find them in dungeons and shops.',
    ],
    netplayTips: [
      'Single-player -- relay session for co-commentary.',
      'Pairs with Oracle of Ages for the linked game final boss.',
    ],
    recommendedController: 'Game Boy Color',
  },
  {
    gameId: 'gbc-zelda-oracle-of-ages',
    genre: 'Action Adventure',
    developer: 'Capcom / Nintendo',
    year: 2001,
    artworkColor: '#6A5ACD',
    onboardingTips: [
      'Harp of Ages travels between present and past -- both eras hide secrets.',
      'Trading sequence items across both eras unlocks powerful equipment.',
      'Linked Game: clear Oracle of Seasons first for a Password to reach the true ending.',
    ],
    netplayTips: [
      'Single-player -- relay session for co-commentary.',
      'Puzzle-focused companion to Oracle of Seasons.',
    ],
    recommendedController: 'Game Boy Color',
  },
  {
    gameId: 'gbc-wario-land-3',
    genre: 'Platformer',
    developer: 'Nintendo R&D1',
    year: 2000,
    artworkColor: '#6A5ACD',
    onboardingTips: [
      'Wario cannot die -- instead he transforms when hit (fire, frozen, etc.).',
      'Music boxes are the key items -- collect all to restore the music box world.',
      'Revisit earlier levels with new power-ups to access previously unreachable areas.',
    ],
    netplayTips: [
      'Single-player title -- relay session for spectating.',
      'mGBA emulates GBC color palette accurately for Wario Land 3.',
    ],
    recommendedController: 'Game Boy Color',
  },
  {
    gameId: 'wiiu-mario-kart-8',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 2014,
    onboardingTips: [
      'Anti-gravity sections let you collect coins on walls — aim for the blue arrows.',
      'Draft behind racers ahead of you for a slipstream boost; pull out to activate it.',
      '200cc mode requires brake-drifting through tight corners — hold the brake on sharp turns.',
    ],
    netplayTips: [
      'Relay sessions work well under 100 ms — item timing stays consistent.',
      'Use wired internet for best results; Wi-Fi adds ~15 ms jitter.',
      'Cemu\'s Vulkan backend is recommended — reduces frame-time spikes in hectic races.',
    ],
    recommendedController: 'Xbox or PlayStation controller (Pro Controller layout)',
    artworkColor: '#0099cc',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'wiiu-super-smash-bros-wiiu',
    genre: 'Fighting / Party',
    developer: 'Sora Ltd. / Bandai Namco',
    year: 2014,
    onboardingTips: [
      '8-Player Smash supports up to 8 players simultaneously on giant stages.',
      'GameCube controllers are natively supported via the official adapter — highly recommended.',
      'For Glory mode was the competitive ranked online mode on the real hardware.',
    ],
    netplayTips: [
      'Under 80 ms is the sweet spot for reliable smash inputs.',
      'Cemu relay works well for 4P; 8P sessions benefit from being on the same local network.',
      'Roll dodge spam is easier to punish than in later Smash games — learn the punishes.',
    ],
    recommendedController: 'Xbox (GC adapter layout) or PlayStation controller',
    artworkColor: '#cc3300',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'wiiu-splatoon',
    genre: 'Shooter / Action',
    developer: 'Nintendo',
    year: 2015,
    onboardingTips: [
      'Covering turf earns more points than splating opponents — paint walls and floors.',
      'Swim through your own ink to recharge ink tanks and move faster.',
      'Use bombs to flush opponents out of hiding and create space around objectives.',
    ],
    netplayTips: [
      'Turf War is the most stable mode for relay netplay — 4P sessions work well.',
      'Under 80 ms keeps squid movement and bomb detonations in sync.',
      'All 4 players should use the same resolution setting in Cemu for consistent performance.',
    ],
    recommendedController: 'Xbox or PlayStation controller (gyro aim not required in Cemu)',
    artworkColor: '#ff5500',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'wiiu-super-mario-3d-world',
    genre: 'Platformer',
    developer: 'Nintendo',
    year: 2013,
    onboardingTips: [
      'Cat Mario can climb walls and perform diving attacks — versatile for all skill levels.',
      'Characters have different stats: Rosalina is slowest but has a spin attack.',
      'Stars on the map require completing secret exits or Challenge mode.',
    ],
    netplayTips: [
      'Under 100 ms keeps player positions and power-up grabs in sync.',
      'Camera follows the group — slower players can get caught off-screen.',
      'Green Stars and stamps carry over between sessions via save sync.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#006644',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'wiiu-hyrule-warriors',
    genre: 'Hack and Slash',
    developer: 'Omega Force / Team Ninja',
    year: 2014,
    onboardingTips: [
      'Weak-point gauges appear when a commander is targeting you — deplete it for a big stun.',
      'Each character has a unique strong attack (ZR/RT in the keep square) — learn their ranges.',
      'Adventure Mode\'s map is a hidden treasure trove of unlockable characters and weapons.',
    ],
    netplayTips: [
      'Two-player co-op splits TV and GamePad screens — one player sees the map on GamePad.',
      'Under 120 ms keeps combos and enemy aggro in sync.',
      'Both players must have the same DLC installed to access DLC characters in co-op.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#5b3a00',
    quickHostPreset: '1v1',
  },
  {
    gameId: 'wiiu-xenoblade-chronicles-x',
    genre: 'Open World RPG',
    developer: 'Monolith Soft',
    year: 2015,
    onboardingTips: [
      'You can\'t fast-travel until you\'ve activated Landmarks on foot — explore early.',
      'Online Squad Missions allow 4-player co-op on mission objectives.',
      'Skell flight is unlocked mid-game and transforms map traversal entirely.',
    ],
    netplayTips: [
      'Online missions work via relay with 4 players — latency under 150 ms recommended.',
      'All players need matching DLC for online content access.',
      'Save frequently — the world is vast and progress is checkpoint-heavy.',
    ],
    recommendedController: 'Xbox or PlayStation controller (GamePad shows map in Cemu window)',
    artworkColor: '#003366',
    quickHostPreset: 'full-party',
  },

  // ── Nintendo 3DS ────────────────────────────────────────────────────────────
  {
    gameId: '3ds-mario-kart-7',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 2011,
    onboardingTips: [
      'Gliding sections allow tricks mid-air — hold up on the D-pad while gliding.',
      'Underwater sections slow you down slightly — find the underwater shortcuts.',
      'Customize your kart: body, tires, and glider all affect different stats.',
    ],
    netplayTips: [
      'Up to 8 players online — Lime3DS handles direct connect well.',
      'Under 80 ms keeps item timing and hit detection accurate.',
      'Use the Large Screen layout in Lime3DS for easier control visibility.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#cc0000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: '3ds-super-smash-bros-for-3ds',
    genre: 'Fighting / Party',
    developer: 'Sora Ltd. / Bandai Namco',
    year: 2014,
    onboardingTips: [
      'This is the first portable Smash with a full roster — great for on-the-go sessions.',
      'Smash Run mode is a unique single-player dungeon crawl before the final fight.',
      'No custom Miis allowed in For Glory — standard characters only.',
    ],
    netplayTips: [
      'For Glory mode (1v1 Final Destination) is the competitive benchmark.',
      'Under 80 ms is recommended for reliable frame-data interactions.',
      'Lime3DS Large Screen layout gives more visibility for off-stage edgeguarding.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#cc0000',
    quickHostPreset: 'full-party',
  },
  {
    gameId: '3ds-monster-hunter-4-ultimate',
    genre: 'Action RPG',
    developer: 'Capcom',
    year: 2014,
    onboardingTips: [
      'Mounting monsters is MH4U\'s signature mechanic — attack from high ledges.',
      'Bring a mix of Mega Potions and Lifepowders for multiplayer hunts.',
      'The Insect Glaive is beginner-friendly and excels at mounting.',
    ],
    netplayTips: [
      'Up to 4 hunters can join a quest — coordinate weapon types for synergy.',
      'Under 100 ms keeps mount damage and stagger windows accurate.',
      'Lime3DS online mode via direct connect is the most reliable for 4P hunts.',
    ],
    recommendedController: 'Xbox or PlayStation controller (analog sticks required)',
    artworkColor: '#8b4513',
    quickHostPreset: 'full-party',
  },
  {
    gameId: '3ds-luigis-mansion-dark-moon',
    genre: 'Puzzle / Action',
    developer: 'Next Level Games',
    year: 2013,
    onboardingTips: [
      'ScareScraper mode is separate from the main game — 4-player co-op tower floors.',
      'Pull in the opposite direction of the ghost\'s flee vector to drain HP faster.',
      'Dark-Light Device reveals invisible objects — use it on walls in later mansions.',
    ],
    netplayTips: [
      'ScareScraper 4P is the go-to multiplayer mode — 5 or 10 floor runs are ideal.',
      'Under 120 ms keeps ghost-stun timing and trap triggers in sync.',
      'All players need matched game versions for online ScareScraper.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#2d6a4f',
    quickHostPreset: 'full-party',
  },
  {
    gameId: '3ds-fire-emblem-awakening',
    genre: 'Tactical RPG',
    developer: 'Intelligent Systems',
    year: 2012,
    onboardingTips: [
      'Pair Up system lets two units fight as one — choose pairs with complementary stats.',
      'Lunatic difficulty disables Casual mode — permadeath applies to all units.',
      'Building support relationships unlocks powerful pair bonus stats.',
    ],
    netplayTips: [
      'SpotPass and StreetPass battles allow unit sharing between players.',
      'Under 150 ms is fine for turn-based gameplay — no real-time mechanics.',
      'Trade units via local wireless using Lime3DS LAN mode.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#990000',
    quickHostPreset: '1v1',
  },
  {
    gameId: '3ds-pokemon-x',
    genre: 'RPG / Monster Collector',
    developer: 'Game Freak',
    year: 2013,
    onboardingTips: [
      'Fairy type is new in Gen VI — strong against Dragon, Dark, and Fighting.',
      'Super Training allows EV training without battling — useful for competitive players.',
      'GTS lets you trade globally for any Pokémon you have seen in the Pokédex.',
    ],
    netplayTips: [
      'Battle via the PSS (Player Search System) — challenge friends directly.',
      'Under 150 ms is fine for turn-based battles.',
      'Lime3DS default layout works well — touch screen shows the menu.',
    ],
    recommendedController: 'Touch controls via Lime3DS mouse emulation',
    artworkColor: '#0066cc',
    quickHostPreset: '1v1',
  },

  // ── Nintendo Switch ─────────────────────────────────────────────────────────
  {
    gameId: 'switch-mario-kart-8-deluxe',
    genre: 'Kart Racing',
    developer: 'Nintendo',
    year: 2017,
    onboardingTips: [
      'Smart Steering and Auto-Accelerate can be toggled per session — great for beginners.',
      '200cc mode requires brake-drifting — hold the ZL/LB trigger on tight corners.',
      'Battle Mode has 6 unique game types including Balloon Battle and Coin Runners.',
    ],
    netplayTips: [
      'Ryujinx LDN mode enables local-wireless-style 12-player races over LAN.',
      'Under 80 ms recommended for item timing and drift boost accuracy.',
      'All players must use the same game version — check DLC wave compatibility.',
    ],
    recommendedController: 'Xbox or PlayStation controller (Joy-Con emulation not needed)',
    artworkColor: '#0099cc',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'switch-super-smash-bros-ultimate',
    genre: 'Fighting / Party',
    developer: 'Sora Ltd. / Bandai Namco',
    year: 2018,
    onboardingTips: [
      'Every fighter who has ever appeared in a Smash game is here — 89 characters total.',
      'World of Light single-player campaign is 30+ hours of content.',
      'Spirit battles on the adventure map work as modifier-based challenge fights.',
    ],
    netplayTips: [
      'Quickplay uses GSP (Global Smash Power) ranked matching — Preferred Rules is the "For Glory" equivalent.',
      'Under 80 ms is essential — Smash Ultimate\'s 1/60-second input window is unforgiving above 100 ms.',
      'Ryujinx LDN mode is the most reliable for private 8-player sessions.',
    ],
    recommendedController: 'Xbox or PlayStation controller (GameCube adapter also supported)',
    artworkColor: '#990022',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'switch-splatoon-3',
    genre: 'Shooter / Action',
    developer: 'Nintendo',
    year: 2022,
    onboardingTips: [
      'Salmon Run (co-op horde mode) is one of the best co-op modes in any shooter.',
      'Each main weapon has a Sub and Special — learn which combos suit your playstyle.',
      'Anarchy Battles (ranked) have unique objectives: Tower Control, Rainmaker, Clam Blitz.',
    ],
    netplayTips: [
      'Turf War and Anarchy Battle both work via Ryujinx LDN for 4v4 sessions.',
      'Under 80 ms is critical — ink physics and player hits desync at higher latency.',
      'Salmon Run co-op scales well up to 150 ms with careful play.',
    ],
    recommendedController: 'Xbox or PlayStation controller (gyro aim can be configured in Ryujinx)',
    artworkColor: '#ff6600',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'switch-pokemon-scarlet',
    genre: 'RPG / Monster Collector',
    developer: 'Game Freak',
    year: 2022,
    onboardingTips: [
      'Terastal Phenomenon changes a Pokémon\'s type mid-battle — use it strategically.',
      '7-star Tera Raid Battles are the most challenging content and reward rare Pokémon.',
      'The four-player Union Circle lets you explore the open world together in real time.',
    ],
    netplayTips: [
      'Union Circle requires 4 players with matching game versions — great for exploration sessions.',
      'Tera Raid Battles support 4-player co-op and work well under 150 ms.',
      'Trading and battling via Poke Portal is the core competitive experience.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#cc2200',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'switch-super-smash-bros-ultimate',
    genre: 'Fighting / Party',
    developer: 'Sora Ltd. / Bandai Namco',
    year: 2018,
    onboardingTips: [
      'Every fighter who has ever appeared in a Smash game is here — 89 characters total.',
      'World of Light single-player campaign is 30+ hours of content.',
    ],
    netplayTips: [
      'Under 80 ms is essential for frame-accurate smash inputs.',
      'Ryujinx LDN mode is the most reliable path for 8-player private sessions.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#990022',
    quickHostPreset: 'full-party',
  },
  {
    gameId: 'switch-mario-party-superstars',
    genre: 'Party / Board Game',
    developer: 'NDcube',
    year: 2021,
    onboardingTips: [
      'All 5 boards are classic N64 boards — Space Land and Woody Woods return.',
      'All 100 mini-games are from previous Mario Party games.',
      'Online play supports full 4-player board game sessions.',
    ],
    netplayTips: [
      'Fully playable online with 4 players — turn-based structure tolerates up to 150 ms.',
      'All players must have the same game version for online compatibility.',
      'Ryujinx LDN gives the most stable experience for private party sessions.',
    ],
    recommendedController: 'Xbox or PlayStation controller',
    artworkColor: '#ff9900',
    quickHostPreset: 'full-party',
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
