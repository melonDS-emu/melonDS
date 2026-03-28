/**
 * Retro Achievement Store — per-game achievement definitions and player progress.
 *
 * Each game in the catalog has a set of in-game achievements (inspired by
 * RetroAchievements.org). Players earn them by completing in-game milestones
 * during netplay sessions. The store tracks earned achievements per player
 * in memory; a SQLite-backed variant (SqliteRetroAchievementStore) extends
 * this for persistence across server restarts.
 */

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export interface RetroGameAchievementDef {
  id: string;          // unique key, e.g. "mk64-first-race"
  gameId: string;      // matches game catalog ID
  title: string;       // display name
  description: string; // how to earn it
  points: number;      // difficulty weight (5–100)
  badge: string;       // emoji badge
}

export interface EarnedRetroAchievement {
  achievementId: string;
  gameId: string;
  earnedAt: string;   // ISO timestamp
  sessionId?: string; // optional: which session triggered the unlock
}

export interface RetroPlayerProgress {
  playerId: string;
  earned: EarnedRetroAchievement[];
  totalPoints: number;
}

export interface RetroGameSummary {
  gameId: string;
  totalAchievements: number;
  totalPoints: number;
}

export interface RetroLeaderboardEntry {
  playerId: string;
  totalPoints: number;
  earnedCount: number;
}

// ---------------------------------------------------------------------------
// Achievement catalog  (15 games × 4 achievements = 60 definitions)
// ---------------------------------------------------------------------------

export const RETRO_ACHIEVEMENT_DEFS: RetroGameAchievementDef[] = [
  // ── Mario Kart 64 (N64) ──────────────────────────────────────────────────
  {
    id: 'mk64-first-race',
    gameId: 'n64-mario-kart-64',
    title: 'First Race',
    description: 'Complete your first Mario Kart 64 race.',
    points: 5,
    badge: '🏁',
  },
  {
    id: 'mk64-podium',
    gameId: 'n64-mario-kart-64',
    title: 'Podium Finish',
    description: 'Finish in 1st, 2nd, or 3rd place.',
    points: 10,
    badge: '🏆',
  },
  {
    id: 'mk64-blue-shell',
    gameId: 'n64-mario-kart-64',
    title: 'Blue Shell Survivor',
    description: 'Win a race despite being hit by a blue shell.',
    points: 25,
    badge: '🐢',
  },
  {
    id: 'mk64-rainbow-road',
    gameId: 'n64-mario-kart-64',
    title: 'Rainbow Road',
    description: 'Complete Rainbow Road without falling off.',
    points: 50,
    badge: '🌈',
  },

  // ── Super Smash Bros (N64) ────────────────────────────────────────────────
  {
    id: 'ssb64-first-ko',
    gameId: 'n64-super-smash-bros',
    title: 'First KO',
    description: 'Score your first KO in a multiplayer match.',
    points: 5,
    badge: '💥',
  },
  {
    id: 'ssb64-untouchable',
    gameId: 'n64-super-smash-bros',
    title: 'Untouchable',
    description: 'Win a stock match without taking damage.',
    points: 50,
    badge: '🛡️',
  },
  {
    id: 'ssb64-last-stock',
    gameId: 'n64-super-smash-bros',
    title: 'Last Stock',
    description: 'Win a match by taking the final stock.',
    points: 10,
    badge: '⚡',
  },
  {
    id: 'ssb64-comeback',
    gameId: 'n64-super-smash-bros',
    title: 'Comeback Kid',
    description: 'Win a match after being at 150%+ damage.',
    points: 25,
    badge: '🔥',
  },

  // ── Mario Kart DS (NDS) ───────────────────────────────────────────────────
  {
    id: 'mkds-wifi-win',
    gameId: 'nds-mario-kart-ds',
    title: 'Wi-Fi Winner',
    description: 'Win a Wi-Fi multiplayer race.',
    points: 10,
    badge: '📶',
  },
  {
    id: 'mkds-snaking',
    gameId: 'nds-mario-kart-ds',
    title: 'Snake Charmer',
    description: 'Maintain a mini-turbo boost for 3 consecutive turns.',
    points: 25,
    badge: '🐍',
  },
  {
    id: 'mkds-150cc',
    gameId: 'nds-mario-kart-ds',
    title: '150cc Champion',
    description: 'Win a 150cc Grand Prix cup.',
    points: 25,
    badge: '🏅',
  },
  {
    id: 'mkds-mission',
    gameId: 'nds-mario-kart-ds',
    title: 'Mission Accomplished',
    description: 'Complete all Mission Mode stages.',
    points: 50,
    badge: '🎯',
  },

  // ── Pokémon Diamond (NDS) ─────────────────────────────────────────────────
  {
    id: 'pkdp-first-badge',
    gameId: 'nds-pokemon-diamond',
    title: 'First Badge',
    description: 'Earn your first gym badge.',
    points: 5,
    badge: '🔵',
  },
  {
    id: 'pkdp-trade',
    gameId: 'nds-pokemon-diamond',
    title: 'Trade Partner',
    description: 'Complete a trade via Wi-Fi.',
    points: 10,
    badge: '🔄',
  },
  {
    id: 'pkdp-elite-four',
    gameId: 'nds-pokemon-diamond',
    title: 'Elite Four',
    description: 'Defeat the Elite Four.',
    points: 50,
    badge: '👑',
  },
  {
    id: 'pkdp-pokedex-50',
    gameId: 'nds-pokemon-diamond',
    title: 'Pokédex Explorer',
    description: 'Register 50 Pokémon in the Pokédex.',
    points: 25,
    badge: '📖',
  },

  // ── Pokémon Emerald (GBA) ─────────────────────────────────────────────────
  {
    id: 'pkem-first-catch',
    gameId: 'gba-pokemon-emerald',
    title: 'First Catch',
    description: 'Catch your first wild Pokémon.',
    points: 5,
    badge: '⚾',
  },
  {
    id: 'pkem-link-battle',
    gameId: 'gba-pokemon-emerald',
    title: 'Link Battle',
    description: 'Win a link cable battle.',
    points: 10,
    badge: '⚔️',
  },
  {
    id: 'pkem-hoenn-champ',
    gameId: 'gba-pokemon-emerald',
    title: 'Hoenn Champion',
    description: 'Become the Hoenn Pokémon Champion.',
    points: 50,
    badge: '🏆',
  },
  {
    id: 'pkem-contest',
    gameId: 'gba-pokemon-emerald',
    title: 'Contest Star',
    description: 'Win a Pokémon Contest.',
    points: 25,
    badge: '🌟',
  },

  // ── Tetris (GB) ───────────────────────────────────────────────────────────
  {
    id: 'gb-tetris-10lines',
    gameId: 'gb-tetris',
    title: 'First Clear',
    description: 'Clear 10 lines in a single game.',
    points: 5,
    badge: '🧱',
  },
  {
    id: 'gb-tetris-tetris',
    gameId: 'gb-tetris',
    title: 'Tetris!',
    description: 'Clear 4 lines at once (a Tetris).',
    points: 25,
    badge: '✨',
  },
  {
    id: 'gb-tetris-level9',
    gameId: 'gb-tetris',
    title: 'Speed Demon',
    description: 'Reach level 9 in single-player mode.',
    points: 25,
    badge: '⚡',
  },
  {
    id: 'gb-tetris-vs-win',
    gameId: 'gb-tetris',
    title: 'VS Champion',
    description: 'Win a 2-player VS match.',
    points: 10,
    badge: '🥊',
  },

  // ── Contra (NES) ──────────────────────────────────────────────────────────
  {
    id: 'nes-contra-stage1',
    gameId: 'nes-contra',
    title: 'First Contact',
    description: 'Clear the first stage.',
    points: 5,
    badge: '🔫',
  },
  {
    id: 'nes-contra-spread',
    gameId: 'nes-contra',
    title: 'Spread Shot',
    description: 'Collect the Spread Gun.',
    points: 5,
    badge: '💥',
  },
  {
    id: 'nes-contra-coop',
    gameId: 'nes-contra',
    title: 'Brothers in Arms',
    description: 'Clear a stage in 2-player co-op.',
    points: 10,
    badge: '🤝',
  },
  {
    id: 'nes-contra-nolose',
    gameId: 'nes-contra',
    title: 'No Man Left Behind',
    description: 'Complete the game without losing a life.',
    points: 100,
    badge: '🎖️',
  },

  // ── Sonic the Hedgehog 2 (Genesis) ───────────────────────────────────────
  {
    id: 'sonic2-emerald-hill',
    gameId: 'genesis-sonic-the-hedgehog-2',
    title: 'Emerald Hill Clear',
    description: 'Clear Emerald Hill Zone Act 1.',
    points: 5,
    badge: '💎',
  },
  {
    id: 'sonic2-100rings',
    gameId: 'genesis-sonic-the-hedgehog-2',
    title: 'Ring Collector',
    description: 'Collect 100 rings in a single level.',
    points: 10,
    badge: '💍',
  },
  {
    id: 'sonic2-super-sonic',
    gameId: 'genesis-sonic-the-hedgehog-2',
    title: 'Super Sonic',
    description: 'Transform into Super Sonic.',
    points: 50,
    badge: '⭐',
  },
  {
    id: 'sonic2-2p-win',
    gameId: 'genesis-sonic-the-hedgehog-2',
    title: 'Fastest Hedgehog',
    description: 'Win a 2-player vs. race.',
    points: 25,
    badge: '🏃',
  },

  // ── Crash Bandicoot (PSX) ─────────────────────────────────────────────────
  {
    id: 'crash-first-box',
    gameId: 'psx-crash-bandicoot',
    title: 'Box Smasher',
    description: 'Smash your first crate.',
    points: 5,
    badge: '📦',
  },
  {
    id: 'crash-all-boxes',
    gameId: 'psx-crash-bandicoot',
    title: 'Box Maniac',
    description: 'Smash all boxes in a level.',
    points: 25,
    badge: '💯',
  },
  {
    id: 'crash-gem',
    gameId: 'psx-crash-bandicoot',
    title: 'Gem Hunter',
    description: 'Collect your first colored gem.',
    points: 10,
    badge: '💎',
  },
  {
    id: 'crash-no-death',
    gameId: 'psx-crash-bandicoot',
    title: 'Relentless Bandicoot',
    description: 'Complete a level without dying.',
    points: 25,
    badge: '🦔',
  },

  // ── Super Bomberman (SNES) ────────────────────────────────────────────────
  {
    id: 'bomba-first-win',
    gameId: 'snes-super-bomberman',
    title: 'Boom!',
    description: 'Win your first Battle Mode match.',
    points: 5,
    badge: '💣',
  },
  {
    id: 'bomba-chain',
    gameId: 'snes-super-bomberman',
    title: 'Chain Reaction',
    description: 'Blow up two opponents with a single bomb.',
    points: 25,
    badge: '🔗',
  },
  {
    id: 'bomba-survive',
    gameId: 'snes-super-bomberman',
    title: 'Last One Standing',
    description: 'Win a 4-player Battle Mode match.',
    points: 10,
    badge: '🏅',
  },
  {
    id: 'bomba-no-miss',
    gameId: 'snes-super-bomberman',
    title: 'Perfect Bomber',
    description: 'Clear a Normal Mode stage without dying.',
    points: 50,
    badge: '🎯',
  },

  // ── Mario Kart: Double Dash!! (GameCube) ─────────────────────────────────
  {
    id: 'mkdd-first-race',
    gameId: 'gc-mario-kart-double-dash',
    title: 'First Double Dash',
    description: 'Complete your first race in Mario Kart: Double Dash!!.',
    points: 5,
    badge: '🏎️',
  },
  {
    id: 'mkdd-team-finish',
    gameId: 'gc-mario-kart-double-dash',
    title: 'Perfect Partnership',
    description: 'Finish 1st in a race using a full item combination combo.',
    points: 25,
    badge: '🤜',
  },
  {
    id: 'mkdd-all-cups',
    gameId: 'gc-mario-kart-double-dash',
    title: 'Grand Prix Master',
    description: 'Win all four Grand Prix cups.',
    points: 50,
    badge: '🏆',
  },
  {
    id: 'mkdd-lan-win',
    gameId: 'gc-mario-kart-double-dash',
    title: 'LAN Party Champion',
    description: 'Win a 4-player LAN race.',
    points: 10,
    badge: '🌐',
  },

  // ── Mario Kart Wii (Wii) ──────────────────────────────────────────────────
  {
    id: 'mkwii-first-race',
    gameId: 'wii-mario-kart-wii',
    title: 'First Race Online',
    description: 'Complete your first Mario Kart Wii race online.',
    points: 5,
    badge: '🏁',
  },
  {
    id: 'mkwii-trick',
    gameId: 'wii-mario-kart-wii',
    title: 'Big Air',
    description: 'Land a trick off a ramp.',
    points: 5,
    badge: '🤸',
  },
  {
    id: 'mkwii-star-rank',
    gameId: 'wii-mario-kart-wii',
    title: 'Star Ranked',
    description: 'Earn a 1-star rank in any Grand Prix cup.',
    points: 25,
    badge: '⭐',
  },
  {
    id: 'mkwii-online-win',
    gameId: 'wii-mario-kart-wii',
    title: 'Worldwide Winner',
    description: 'Win a worldwide online race.',
    points: 50,
    badge: '🌍',
  },

  // ── Mario Kart 7 (3DS) ────────────────────────────────────────────────────
  {
    id: 'mk7-first-race',
    gameId: '3ds-mario-kart-7',
    title: 'First Race',
    description: 'Complete your first Mario Kart 7 race.',
    points: 5,
    badge: '🏁',
  },
  {
    id: 'mk7-glider',
    gameId: '3ds-mario-kart-7',
    title: 'Hang Glider',
    description: 'Fly the full length of a glider section.',
    points: 10,
    badge: '🪂',
  },
  {
    id: 'mk7-community',
    gameId: '3ds-mario-kart-7',
    title: 'Community Racer',
    description: 'Win a race in a Community room.',
    points: 25,
    badge: '👥',
  },
  {
    id: 'mk7-three-star',
    gameId: '3ds-mario-kart-7',
    title: 'Perfect Cup',
    description: 'Earn a 3-star rank in any Grand Prix cup.',
    points: 50,
    badge: '✨',
  },

  // ── Sonic Adventure 2 (Dreamcast) ─────────────────────────────────────────
  {
    id: 'sa2-first-stage',
    gameId: 'dc-sonic-adventure-2',
    title: 'City Escape',
    description: 'Clear the first stage as Sonic.',
    points: 5,
    badge: '🏙️',
  },
  {
    id: 'sa2-a-rank',
    gameId: 'dc-sonic-adventure-2',
    title: 'A-Rank Hero',
    description: 'Earn an A rank on any stage.',
    points: 25,
    badge: '🅰️',
  },
  {
    id: 'sa2-chao-garden',
    gameId: 'dc-sonic-adventure-2',
    title: 'Chao Keeper',
    description: 'Raise a Chao to level 10 in the Chao Garden.',
    points: 25,
    badge: '🐣',
  },
  {
    id: 'sa2-last-story',
    gameId: 'dc-sonic-adventure-2',
    title: 'Space Colony ARK',
    description: 'Complete the Last Story.',
    points: 50,
    badge: '🌌',
  },

  // ── GTA: San Andreas (PS2) ────────────────────────────────────────────────
  {
    id: 'gta-sa-first-mission',
    gameId: 'ps2-gta-san-andreas',
    title: 'Welcome to San Andreas',
    description: 'Complete the first mission.',
    points: 5,
    badge: '🌴',
  },
  {
    id: 'gta-sa-100k',
    gameId: 'ps2-gta-san-andreas',
    title: 'High Roller',
    description: 'Earn $100,000 in total.',
    points: 10,
    badge: '💰',
  },
  {
    id: 'gta-sa-grove-street',
    gameId: 'ps2-gta-san-andreas',
    title: 'Grove Street 4 Life',
    description: 'Reclaim all Grove Street territories.',
    points: 25,
    badge: '🟢',
  },
  {
    id: 'gta-sa-100pct',
    gameId: 'ps2-gta-san-andreas',
    title: '100% Complete',
    description: 'Achieve 100% game completion.',
    points: 100,
    badge: '🎖️',
  },

  // ── Pokémon Crystal (GBC) ─────────────────────────────────────────────────
  {
    id: 'gbc-pkcr-first-badge',
    gameId: 'gbc-pokemon-crystal',
    title: 'First Badge',
    description: 'Earn your first gym badge in Johto.',
    points: 5,
    badge: '🔵',
  },
  {
    id: 'gbc-pkcr-link-trade',
    gameId: 'gbc-pokemon-crystal',
    title: 'Link Trade',
    description: 'Complete a link cable trade with another player.',
    points: 10,
    badge: '🔄',
  },
  {
    id: 'gbc-pkcr-suicune',
    gameId: 'gbc-pokemon-crystal',
    title: 'Legendary Chase',
    description: 'Encounter the legendary Pokémon Suicune.',
    points: 25,
    badge: '💧',
  },
  {
    id: 'gbc-pkcr-champion',
    gameId: 'gbc-pokemon-crystal',
    title: 'Johto Champion',
    description: 'Defeat the Elite Four and become Johto Champion.',
    points: 50,
    badge: '👑',
  },

  // ── Monster Hunter Freedom Unite (PSP) ───────────────────────────────────
  {
    id: 'psp-mhfu-first-hunt',
    gameId: 'psp-monster-hunter-fu',
    title: 'First Hunt',
    description: 'Complete your first monster hunt.',
    points: 5,
    badge: '🗡️',
  },
  {
    id: 'psp-mhfu-adhoc',
    gameId: 'psp-monster-hunter-fu',
    title: 'Hunting Party',
    description: 'Complete a hunt with ad-hoc multiplayer.',
    points: 10,
    badge: '👥',
  },
  {
    id: 'psp-mhfu-rathalos',
    gameId: 'psp-monster-hunter-fu',
    title: 'Rathalos Slayer',
    description: 'Slay a Rathalos in a high-rank quest.',
    points: 25,
    badge: '🐉',
  },
  {
    id: 'psp-mhfu-g-rank',
    gameId: 'psp-monster-hunter-fu',
    title: 'G-Rank Hunter',
    description: 'Reach G-Rank and unlock the highest tier of hunts.',
    points: 50,
    badge: '⚔️',
  },

  // ── Castlevania: Symphony of the Night (PSX) ─────────────────────────────
  {
    id: 'psx-sotn-first-area',
    gameId: 'psx-castlevania-sotn',
    title: "Dracula's Castle",
    description: "Enter Dracula's Castle as Alucard.",
    points: 5,
    badge: '🏰',
  },
  {
    id: 'psx-sotn-familiar',
    gameId: 'psx-castlevania-sotn',
    title: 'Familiar Bond',
    description: 'Obtain and level up a familiar.',
    points: 10,
    badge: '🦇',
  },
  {
    id: 'psx-sotn-inverted',
    gameId: 'psx-castlevania-sotn',
    title: 'Inverted Castle',
    description: 'Discover the Inverted Castle.',
    points: 25,
    badge: '🔄',
  },
  {
    id: 'psx-sotn-true-end',
    gameId: 'psx-castlevania-sotn',
    title: 'True Ending',
    description: 'Defeat Shaft and achieve the true ending.',
    points: 50,
    badge: '⚡',
  },

  // ── Secret of Mana (SNES) ─────────────────────────────────────────────────
  {
    id: 'snes-mana-first-weapon',
    gameId: 'snes-secret-of-mana',
    title: 'First Weapon',
    description: 'Obtain your first elemental weapon orb.',
    points: 5,
    badge: '🗡️',
  },
  {
    id: 'snes-mana-coop',
    gameId: 'snes-secret-of-mana',
    title: 'Multiplayer Adventure',
    description: 'Play in co-op mode with another player.',
    points: 10,
    badge: '🤝',
  },
  {
    id: 'snes-mana-all-weapons',
    gameId: 'snes-secret-of-mana',
    title: 'Master of Arms',
    description: "Max out any weapon's experience level to Lv. 8.",
    points: 25,
    badge: '⚔️',
  },
  {
    id: 'snes-mana-mana-beast',
    gameId: 'snes-secret-of-mana',
    title: 'Mana Beast',
    description: 'Defeat the Mana Beast and restore the Mana Tree.',
    points: 50,
    badge: '🌿',
  },

  // ── Metal Gear Solid (PSX) ────────────────────────────────────────────────
  {
    id: 'psx-mgs-start',
    gameId: 'psx-metal-gear-solid',
    title: 'Sneaking Mission',
    description: 'Begin the Shadow Moses infiltration.',
    points: 5,
    badge: '🥷',
  },
  {
    id: 'psx-mgs-psycho-mantis',
    gameId: 'psx-metal-gear-solid',
    title: 'Mind Reader',
    description: 'Defeat Psycho Mantis.',
    points: 10,
    badge: '🧠',
  },
  {
    id: 'psx-mgs-no-kill',
    gameId: 'psx-metal-gear-solid',
    title: 'Pacifist',
    description: 'Complete the game without killing any enemy soldiers.',
    points: 50,
    badge: '☮️',
  },
  {
    id: 'psx-mgs-big-boss',
    gameId: 'psx-metal-gear-solid',
    title: 'Big Boss Rank',
    description: 'Achieve the highest Big Boss rank on completion.',
    points: 100,
    badge: '🎖️',
  },
];

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

export class RetroAchievementStore {
  private progress = new Map<string, RetroPlayerProgress>();

  /** All achievement definitions. */
  getDefinitions(): RetroGameAchievementDef[] {
    return RETRO_ACHIEVEMENT_DEFS;
  }

  /** Definitions for a specific game. */
  getGameDefinitions(gameId: string): RetroGameAchievementDef[] {
    return RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === gameId);
  }

  /** Unique game IDs that have at least one achievement. */
  getGameIds(): string[] {
    return [...new Set(RETRO_ACHIEVEMENT_DEFS.map((d) => d.gameId))];
  }

  /** Summary counts and point totals per game. */
  getGameSummaries(): RetroGameSummary[] {
    const map = new Map<string, RetroGameSummary>();
    for (const def of RETRO_ACHIEVEMENT_DEFS) {
      const existing = map.get(def.gameId);
      if (existing) {
        existing.totalAchievements++;
        existing.totalPoints += def.points;
      } else {
        map.set(def.gameId, {
          gameId: def.gameId,
          totalAchievements: 1,
          totalPoints: def.points,
        });
      }
    }
    return [...map.values()];
  }

  /** Return (or create) progress record for a player. */
  getProgress(playerId: string): RetroPlayerProgress {
    if (!this.progress.has(playerId)) {
      this.progress.set(playerId, {
        playerId,
        earned: [],
        totalPoints: 0,
      });
    }
    return this.progress.get(playerId)!;
  }

  /** Earned retro achievements for a player, optionally filtered by game. */
  getEarned(playerId: string, gameId?: string): EarnedRetroAchievement[] {
    const prog = this.progress.get(playerId);
    if (!prog) return [];
    if (gameId) return prog.earned.filter((e) => e.gameId === gameId);
    return prog.earned;
  }

  /**
   * Unlock a retro achievement for a player.
   * Returns the achievement def if it was newly unlocked, or null if it was
   * already earned or the id is unknown.
   */
  unlock(
    playerId: string,
    achievementId: string,
    sessionId?: string
  ): RetroGameAchievementDef | null {
    const def = RETRO_ACHIEVEMENT_DEFS.find((d) => d.id === achievementId);
    if (!def) return null;

    const prog = this.getProgress(playerId);
    const alreadyEarned = prog.earned.some((e) => e.achievementId === achievementId);
    if (alreadyEarned) return null;

    prog.earned.push({
      achievementId,
      gameId: def.gameId,
      earnedAt: new Date().toISOString(),
      ...(sessionId ? { sessionId } : {}),
    });
    prog.totalPoints += def.points;
    return def;
  }

  /**
   * Unlock multiple achievements for a player at once (e.g. after session end).
   * Returns the list of newly-unlocked defs.
   */
  unlockMany(
    playerId: string,
    achievementIds: string[],
    sessionId?: string
  ): RetroGameAchievementDef[] {
    return achievementIds
      .map((id) => this.unlock(playerId, id, sessionId))
      .filter((d): d is RetroGameAchievementDef => d !== null);
  }

  /** Total number of players tracked. */
  playerCount(): number {
    return this.progress.size;
  }

  /**
   * Top-N players sorted by totalPoints (descending).
   * Ties are broken by the number of earned achievements, then by playerId
   * alphabetically for deterministic ordering.
   */
  getLeaderboard(limit = 10): RetroLeaderboardEntry[] {
    const entries: RetroLeaderboardEntry[] = [];
    for (const prog of this.progress.values()) {
      if (prog.totalPoints > 0 || prog.earned.length > 0) {
        entries.push({
          playerId: prog.playerId,
          totalPoints: prog.totalPoints,
          earnedCount: prog.earned.length,
        });
      }
    }
    entries.sort((a, b) => {
      if (b.totalPoints !== a.totalPoints) return b.totalPoints - a.totalPoints;
      if (b.earnedCount !== a.earnedCount) return b.earnedCount - a.earnedCount;
      return a.playerId.localeCompare(b.playerId);
    });
    return entries.slice(0, limit);
  }
}
