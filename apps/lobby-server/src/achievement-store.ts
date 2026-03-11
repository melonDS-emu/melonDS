/**
 * Achievement engine — definitions, per-player progress, and unlock logic.
 *
 * Achievements are unlocked by inspecting a player's cumulative session history.
 * The store is in-memory by default; pass a SQLite DB connection to enable
 * persistence (same pattern as SqliteLobbyManager etc.).
 */

import type { SessionRecord } from './session-history';

// ---------------------------------------------------------------------------
// Achievement definitions
// ---------------------------------------------------------------------------

export type AchievementCategory =
  | 'sessions'
  | 'social'
  | 'systems'
  | 'games'
  | 'streaks';

export interface AchievementDef {
  id: string;
  name: string;
  description: string;
  icon: string;
  category: AchievementCategory;
  /** Threshold value used by the check function (e.g. number of sessions). */
  threshold?: number;
  /** Specific gameId required (for game-specific achievements). */
  requiredGameId?: string;
  /** Specific system required. */
  requiredSystem?: string;
}

export const ACHIEVEMENT_DEFS: AchievementDef[] = [
  // ── Sessions ──────────────────────────────────────────────────────────────
  {
    id: 'first-session',
    name: 'First Power-On',
    description: 'Complete your very first netplay session.',
    icon: '🎮',
    category: 'sessions',
    threshold: 1,
  },
  {
    id: 'sessions-5',
    name: 'Getting Warmed Up',
    description: 'Complete 5 netplay sessions.',
    icon: '🔥',
    category: 'sessions',
    threshold: 5,
  },
  {
    id: 'sessions-25',
    name: 'Regular',
    description: 'Complete 25 netplay sessions.',
    icon: '⭐',
    category: 'sessions',
    threshold: 25,
  },
  {
    id: 'sessions-100',
    name: 'Veteran',
    description: 'Complete 100 netplay sessions.',
    icon: '🏆',
    category: 'sessions',
    threshold: 100,
  },
  {
    id: 'marathon',
    name: 'Marathon Runner',
    description: 'Accumulate 10 hours of netplay time.',
    icon: '⏱️',
    category: 'sessions',
    threshold: 36000, // seconds
  },
  {
    id: 'long-session',
    name: 'Endurance Test',
    description: 'Play a single session lasting at least 60 minutes.',
    icon: '🕐',
    category: 'sessions',
    threshold: 3600,
  },

  // ── Social ────────────────────────────────────────────────────────────────
  {
    id: 'party-of-four',
    name: 'Party of Four',
    description: 'Play a 4-player session.',
    icon: '🎉',
    category: 'social',
    threshold: 4,
  },
  {
    id: 'social-butterfly',
    name: 'Social Butterfly',
    description: 'Play with 10 different players (by display name).',
    icon: '🦋',
    category: 'social',
    threshold: 10,
  },
  {
    id: 'host-master',
    name: 'Host Master',
    description: 'Host 10 netplay sessions.',
    icon: '👑',
    category: 'social',
    threshold: 10,
  },

  // ── Systems ───────────────────────────────────────────────────────────────
  {
    id: 'n64-debut',
    name: 'N64 Debut',
    description: 'Complete a Nintendo 64 netplay session.',
    icon: '🕹️',
    category: 'systems',
    requiredSystem: 'N64',
  },
  {
    id: 'nds-debut',
    name: 'DS Online',
    description: 'Complete a Nintendo DS netplay session.',
    icon: '📺',
    category: 'systems',
    requiredSystem: 'NDS',
  },
  {
    id: 'retro-purist',
    name: 'Retro Purist',
    description: 'Complete sessions on 4 different systems.',
    icon: '🕹️',
    category: 'systems',
    threshold: 4,
  },
  {
    id: 'system-collector',
    name: 'System Collector',
    description: 'Play on every supported system (NES, SNES, GB, GBC, GBA, N64, NDS).',
    icon: '🗂️',
    category: 'systems',
    threshold: 7,
  },

  // ── Specific games ────────────────────────────────────────────────────────
  {
    id: 'mario-kart-fan',
    name: 'Mario Kart Fan',
    description: 'Complete 5 Mario Kart sessions (any Mario Kart game).',
    icon: '🏎️',
    category: 'games',
    threshold: 5,
  },
  {
    id: 'pokemon-trainer',
    name: 'Pokémon Trainer',
    description: 'Complete 3 Pokémon link / WFC sessions (any Pokémon game).',
    icon: '⚡',
    category: 'games',
    threshold: 3,
  },
  {
    id: 'party-animal',
    name: 'Party Animal',
    description: 'Complete 10 party-game sessions.',
    icon: '🎊',
    category: 'games',
    threshold: 10,
  },

  // ── Streaks ───────────────────────────────────────────────────────────────
  {
    id: 'daily-player',
    name: 'Daily Player',
    description: 'Play sessions on 7 distinct calendar days.',
    icon: '📅',
    category: 'streaks',
    threshold: 7,
  },
  {
    id: 'comeback-king',
    name: 'Comeback King',
    description: 'Return after a break: play again after 7+ days of inactivity.',
    icon: '🔄',
    category: 'streaks',
  },
];

// ---------------------------------------------------------------------------
// Per-player record
// ---------------------------------------------------------------------------

export interface PlayerAchievement {
  achievementId: string;
  unlockedAt: string; // ISO timestamp
}

export interface PlayerAchievementState {
  playerId: string;
  displayName: string;
  earned: PlayerAchievement[];
  /** ISO timestamp of the most recent check. */
  lastCheckedAt: string;
}

// ---------------------------------------------------------------------------
// Store
// ---------------------------------------------------------------------------

const MARIO_KART_IDS = new Set(['mk64', 'super-mario-kart', 'mario-kart-ds']);
const POKEMON_KEYWORDS = ['pokemon', 'pokémon'];
const PARTY_KEYWORDS = ['party', 'mario party'];
const ALL_SYSTEMS = ['NES', 'SNES', 'GB', 'GBC', 'GBA', 'N64', 'NDS'];

export class AchievementStore {
  private states = new Map<string, PlayerAchievementState>();

  /** Return (or create) achievement state for a player. */
  getState(playerId: string, displayName: string): PlayerAchievementState {
    if (!this.states.has(playerId)) {
      this.states.set(playerId, {
        playerId,
        displayName,
        earned: [],
        lastCheckedAt: new Date().toISOString(),
      });
    }
    return this.states.get(playerId)!;
  }

  /** Return earned achievements for a player (empty array if unknown). */
  getEarned(playerId: string): PlayerAchievement[] {
    return this.states.get(playerId)?.earned ?? [];
  }

  /** All achievement definitions. */
  getDefinitions(): AchievementDef[] {
    return ACHIEVEMENT_DEFS;
  }

  /**
   * Check all achievements for a player given their complete session history.
   * Returns the list of newly unlocked achievement IDs (if any).
   */
  checkAndUnlock(
    playerId: string,
    displayName: string,
    sessions: SessionRecord[]
  ): AchievementDef[] {
    const state = this.getState(playerId, displayName);
    const earnedIds = new Set(state.earned.map((e) => e.achievementId));
    const newly: AchievementDef[] = [];

    const completed = sessions.filter((s) => s.endedAt !== null);
    const now = new Date().toISOString();

    for (const def of ACHIEVEMENT_DEFS) {
      if (earnedIds.has(def.id)) continue;
      if (this.isUnlocked(def, playerId, displayName, completed, sessions)) {
        state.earned.push({ achievementId: def.id, unlockedAt: now });
        earnedIds.add(def.id);
        newly.push(def);
      }
    }

    state.lastCheckedAt = now;
    return newly;
  }

  // ---------------------------------------------------------------------------
  // Unlock predicates
  // ---------------------------------------------------------------------------

  private isUnlocked(
    def: AchievementDef,
    playerId: string,
    displayName: string,
    completed: SessionRecord[],
    _all: SessionRecord[]
  ): boolean {
    switch (def.id) {
      // Sessions count
      case 'first-session':
        return completed.length >= 1;
      case 'sessions-5':
        return completed.length >= 5;
      case 'sessions-25':
        return completed.length >= 25;
      case 'sessions-100':
        return completed.length >= 100;

      // Total playtime
      case 'marathon': {
        const totalSecs = completed.reduce((sum, s) => sum + (s.durationSecs ?? 0), 0);
        return totalSecs >= (def.threshold ?? 36000);
      }

      // Longest single session
      case 'long-session': {
        const longest = Math.max(0, ...completed.map((s) => s.durationSecs ?? 0));
        return longest >= (def.threshold ?? 3600);
      }

      // Social: 4-player session
      case 'party-of-four':
        return completed.some((s) => s.playerCount >= 4);

      // Social: distinct players played with
      case 'social-butterfly': {
        const allNames = new Set<string>();
        for (const s of completed) {
          for (const name of s.players) {
            if (name !== displayName) allNames.add(name);
          }
        }
        return allNames.size >= (def.threshold ?? 10);
      }

      // Social: hosted sessions (player appears first in list as host)
      case 'host-master': {
        const hosted = completed.filter((s) => s.players[0] === displayName);
        return hosted.length >= (def.threshold ?? 10);
      }

      // System-specific
      case 'n64-debut':
        return completed.some((s) => s.system.toUpperCase() === 'N64');
      case 'nds-debut':
        return completed.some((s) => s.system.toUpperCase() === 'NDS');

      // Different systems played
      case 'retro-purist': {
        const systems = new Set(completed.map((s) => s.system.toUpperCase()));
        return systems.size >= (def.threshold ?? 4);
      }
      case 'system-collector': {
        const systems = new Set(completed.map((s) => s.system.toUpperCase()));
        return ALL_SYSTEMS.every((sys) => systems.has(sys));
      }

      // Game-specific
      case 'mario-kart-fan': {
        const mkSessions = completed.filter(
          (s) =>
            MARIO_KART_IDS.has(s.gameId) ||
            s.gameTitle.toLowerCase().includes('mario kart')
        );
        return mkSessions.length >= (def.threshold ?? 5);
      }
      case 'pokemon-trainer': {
        const pkSessions = completed.filter((s) =>
          POKEMON_KEYWORDS.some((kw) => s.gameTitle.toLowerCase().includes(kw))
        );
        return pkSessions.length >= (def.threshold ?? 3);
      }
      case 'party-animal': {
        const partySessions = completed.filter((s) =>
          PARTY_KEYWORDS.some((kw) => s.gameTitle.toLowerCase().includes(kw))
        );
        return partySessions.length >= (def.threshold ?? 10);
      }

      // Distinct calendar days played
      case 'daily-player': {
        const days = new Set(completed.map((s) => s.startedAt.slice(0, 10)));
        return days.size >= (def.threshold ?? 7);
      }

      // Comeback: gap of 7+ days between consecutive sessions
      case 'comeback-king': {
        const sorted = [...completed].sort((a, b) =>
          a.startedAt.localeCompare(b.startedAt)
        );
        for (let i = 1; i < sorted.length; i++) {
          const prev = new Date(sorted[i - 1].startedAt).getTime();
          const curr = new Date(sorted[i].startedAt).getTime();
          if (curr - prev >= 7 * 24 * 60 * 60 * 1000) return true;
        }
        return false;
      }

      default:
        return false;
    }
  }

  /** Total number of players tracked. */
  playerCount(): number {
    return this.states.size;
  }
}
