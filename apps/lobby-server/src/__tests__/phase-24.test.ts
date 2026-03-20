/**
 * Phase 24 — Further Retro Achievement Support
 *
 * Tests for:
 *  - Expanded catalog (60 total, 15 games × 4 each)
 *  - New game entries: GC, Wii, 3DS, Dreamcast, PS2
 *  - getLeaderboard() — in-memory RetroAchievementStore
 *  - RetroLeaderboardEntry type
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  RetroAchievementStore,
  RETRO_ACHIEVEMENT_DEFS,
  type RetroLeaderboardEntry,
} from '../retro-achievement-store';

// ─── helpers ─────────────────────────────────────────────────────────────────

const GC_MKDD_ID = 'gc-mario-kart-double-dash';
const WII_MKW_ID = 'wii-mario-kart-wii';
const MK7_ID = '3ds-mario-kart-7';
const SA2_ID = 'dc-sonic-adventure-2';
const GTA_SA_ID = 'ps2-gta-san-andreas';

// ─── Expanded catalog ────────────────────────────────────────────────────────

describe('RETRO_ACHIEVEMENT_DEFS expanded catalog (Phase 24)', () => {
  it('contains exactly 60 definitions', () => {
    expect(RETRO_ACHIEVEMENT_DEFS).toHaveLength(60);
  });

  it('covers exactly 15 games', () => {
    const gameIds = new Set(RETRO_ACHIEVEMENT_DEFS.map((d) => d.gameId));
    expect(gameIds.size).toBe(15);
  });

  it('has no duplicate achievement IDs', () => {
    const ids = RETRO_ACHIEVEMENT_DEFS.map((d) => d.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });

  it('includes Mario Kart: Double Dash!! (GC)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === GC_MKDD_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('mkdd-first-race');
    expect(defs.map((d) => d.id)).toContain('mkdd-all-cups');
  });

  it('includes Mario Kart Wii (Wii)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === WII_MKW_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('mkwii-first-race');
    expect(defs.map((d) => d.id)).toContain('mkwii-online-win');
  });

  it('includes Mario Kart 7 (3DS)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === MK7_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('mk7-first-race');
    expect(defs.map((d) => d.id)).toContain('mk7-three-star');
  });

  it('includes Sonic Adventure 2 (Dreamcast)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === SA2_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('sa2-first-stage');
    expect(defs.map((d) => d.id)).toContain('sa2-last-story');
  });

  it('includes GTA: San Andreas (PS2)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === GTA_SA_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('gta-sa-first-mission');
    expect(defs.map((d) => d.id)).toContain('gta-sa-100pct');
  });

  it('every definition has required fields with positive points', () => {
    for (const def of RETRO_ACHIEVEMENT_DEFS) {
      expect(def.id).toBeTruthy();
      expect(def.gameId).toBeTruthy();
      expect(def.title).toBeTruthy();
      expect(def.description).toBeTruthy();
      expect(def.badge).toBeTruthy();
      expect(def.points).toBeGreaterThan(0);
    }
  });

  it('each game has exactly 4 achievements', () => {
    const gameIds = new Set(RETRO_ACHIEVEMENT_DEFS.map((d) => d.gameId));
    for (const gid of gameIds) {
      const count = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === gid).length;
      expect(count).toBe(4);
    }
  });
});

// ─── getLeaderboard — in-memory store ────────────────────────────────────────

describe('RetroAchievementStore.getLeaderboard', () => {
  let store: RetroAchievementStore;

  beforeEach(() => {
    store = new RetroAchievementStore();
  });

  it('returns empty array when no players have unlocked anything', () => {
    expect(store.getLeaderboard()).toEqual([]);
  });

  it('returns empty array for players with no earned achievements', () => {
    store.getProgress('p1'); // creates entry but earns nothing
    expect(store.getLeaderboard()).toHaveLength(0);
  });

  it('returns one entry after a player unlocks an achievement', () => {
    store.unlock('p1', 'mk64-first-race'); // 5pts
    const lb = store.getLeaderboard();
    expect(lb).toHaveLength(1);
    expect(lb[0].playerId).toBe('p1');
    expect(lb[0].totalPoints).toBe(5);
    expect(lb[0].earnedCount).toBe(1);
  });

  it('sorts by totalPoints descending', () => {
    store.unlock('p1', 'mk64-first-race'); // 5pts
    store.unlock('p2', 'mk64-rainbow-road'); // 50pts
    const lb = store.getLeaderboard();
    expect(lb[0].playerId).toBe('p2');
    expect(lb[1].playerId).toBe('p1');
  });

  it('respects the limit parameter', () => {
    for (let i = 1; i <= 15; i++) {
      store.unlock(`player-${i}`, 'mk64-first-race');
    }
    expect(store.getLeaderboard(5)).toHaveLength(5);
    expect(store.getLeaderboard(1)).toHaveLength(1);
  });

  it('defaults to limit 10', () => {
    for (let i = 1; i <= 20; i++) {
      store.unlock(`player-${i}`, 'mk64-first-race');
    }
    expect(store.getLeaderboard()).toHaveLength(10);
  });

  it('returns RetroLeaderboardEntry shaped objects', () => {
    store.unlock('alice', 'mk64-first-race');
    const entry: RetroLeaderboardEntry = store.getLeaderboard()[0];
    expect(typeof entry.playerId).toBe('string');
    expect(typeof entry.totalPoints).toBe('number');
    expect(typeof entry.earnedCount).toBe('number');
  });

  it('leaderboard includes new Phase 24 game achievements', () => {
    store.unlock('p1', 'mkdd-first-race');   // 5pts
    store.unlock('p1', 'mkwii-online-win');  // 50pts
    store.unlock('p2', 'sa2-last-story');    // 50pts
    store.unlock('p2', 'gta-sa-100pct');     // 100pts
    const lb = store.getLeaderboard();
    expect(lb[0].playerId).toBe('p2'); // 150pts
    expect(lb[0].totalPoints).toBe(150);
    expect(lb[1].playerId).toBe('p1'); // 55pts
  });
});

// ─── getGameSummaries with expanded catalog ───────────────────────────────────

describe('RetroAchievementStore (expanded catalog) — summaries and game IDs', () => {
  let store: RetroAchievementStore;

  beforeEach(() => {
    store = new RetroAchievementStore();
  });

  it('returns 15 game summaries', () => {
    expect(store.getGameSummaries()).toHaveLength(15);
  });

  it('getGameIds returns 15 unique IDs', () => {
    expect(store.getGameIds()).toHaveLength(15);
  });

  it('GC MK:DD summary has 4 achievements', () => {
    const summary = store.getGameSummaries().find((s) => s.gameId === GC_MKDD_ID);
    expect(summary).toBeDefined();
    expect(summary!.totalAchievements).toBe(4);
  });

  it('DC Sonic Adventure 2 summary has correct totalPoints', () => {
    const summary = store.getGameSummaries().find((s) => s.gameId === SA2_ID);
    expect(summary).toBeDefined();
    const expected = RETRO_ACHIEVEMENT_DEFS
      .filter((d) => d.gameId === SA2_ID)
      .reduce((s, d) => s + d.points, 0);
    expect(summary!.totalPoints).toBe(expected);
  });
});

