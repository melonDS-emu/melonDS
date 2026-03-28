/**
 * Phase 23 — Retro Achievement Support
 *
 * Tests for:
 *  - RetroAchievementStore definitions (40 total, 10 games × 4 each)
 *  - Per-game definition filtering
 *  - Game summaries (counts + point totals)
 *  - Player progress initialization
 *  - unlock() — new unlock, duplicate prevention, unknown id
 *  - unlockMany() — batch unlock, partial duplicates
 *  - getEarned() — overall and per-game filtering
 *  - playerCount() tracking
 *  - No duplicate achievement IDs in catalog
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  RetroAchievementStore,
  RETRO_ACHIEVEMENT_DEFS,
} from '../retro-achievement-store';

// ─── helpers ─────────────────────────────────────────────────────────────────

const MK64_GAME_ID = 'n64-mario-kart-64';
const SSB_GAME_ID = 'n64-super-smash-bros';
const MKDS_GAME_ID = 'nds-mario-kart-ds';
const PKDP_GAME_ID = 'nds-pokemon-diamond';
const PKEM_GAME_ID = 'gba-pokemon-emerald';
const TETRIS_GAME_ID = 'gb-tetris';
const CONTRA_GAME_ID = 'nes-contra';
const SONIC2_GAME_ID = 'genesis-sonic-the-hedgehog-2';
const CRASH_GAME_ID = 'psx-crash-bandicoot';
const BOMBERMAN_GAME_ID = 'snes-super-bomberman';

// ─── Catalog structure ───────────────────────────────────────────────────────

describe('RETRO_ACHIEVEMENT_DEFS catalog', () => {
  it('contains at least 40 definitions', () => {
    expect(RETRO_ACHIEVEMENT_DEFS.length).toBeGreaterThanOrEqual(40);
  });

  it('has no duplicate achievement IDs', () => {
    const ids = RETRO_ACHIEVEMENT_DEFS.map((d) => d.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });

  it('covers at least 10 games', () => {
    const gameIds = new Set(RETRO_ACHIEVEMENT_DEFS.map((d) => d.gameId));
    expect(gameIds.size).toBeGreaterThanOrEqual(10);
  });

  it('every definition has required fields', () => {
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

  it('includes Mario Kart 64 achievements', () => {
    const mk64 = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === MK64_GAME_ID);
    expect(mk64.length).toBeGreaterThan(0);
    expect(mk64.map((d) => d.id)).toContain('mk64-first-race');
    expect(mk64.map((d) => d.id)).toContain('mk64-rainbow-road');
  });

  it('includes Sonic the Hedgehog 2 achievements', () => {
    const sonic = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === SONIC2_GAME_ID);
    expect(sonic.length).toBeGreaterThan(0);
    expect(sonic.map((d) => d.id)).toContain('sonic2-super-sonic');
  });

  it('includes Crash Bandicoot (PSX) achievements', () => {
    const crash = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === CRASH_GAME_ID);
    expect(crash.length).toBeGreaterThan(0);
    expect(crash.map((d) => d.id)).toContain('crash-no-death');
  });
});

// ─── RetroAchievementStore ───────────────────────────────────────────────────

describe('RetroAchievementStore', () => {
  let store: RetroAchievementStore;

  beforeEach(() => {
    store = new RetroAchievementStore();
  });

  // ── getDefinitions ─────────────────────────────────────────────────────────

  it('getDefinitions returns all defs', () => {
    expect(store.getDefinitions().length).toBeGreaterThanOrEqual(40);
  });

  it('getDefinitions returns the same array reference as catalog', () => {
    expect(store.getDefinitions()).toBe(RETRO_ACHIEVEMENT_DEFS);
  });

  // ── getGameDefinitions ─────────────────────────────────────────────────────

  it('getGameDefinitions filters by gameId', () => {
    const mk64 = store.getGameDefinitions(MK64_GAME_ID);
    expect(mk64).toHaveLength(4);
    expect(mk64.every((d) => d.gameId === MK64_GAME_ID)).toBe(true);
  });

  it('getGameDefinitions returns empty array for unknown game', () => {
    expect(store.getGameDefinitions('unknown-game')).toHaveLength(0);
  });

  // ── getGameIds ─────────────────────────────────────────────────────────────

  it('getGameIds returns at least 10 unique game IDs', () => {
    expect(store.getGameIds().length).toBeGreaterThanOrEqual(10);
  });

  it('getGameIds includes all expected systems', () => {
    const ids = store.getGameIds();
    expect(ids).toContain(MK64_GAME_ID);
    expect(ids).toContain(TETRIS_GAME_ID);
    expect(ids).toContain(CONTRA_GAME_ID);
    expect(ids).toContain(SONIC2_GAME_ID);
    expect(ids).toContain(CRASH_GAME_ID);
  });

  // ── getGameSummaries ───────────────────────────────────────────────────────

  it('getGameSummaries returns at least 10 entries', () => {
    expect(store.getGameSummaries().length).toBeGreaterThanOrEqual(10);
  });

  it('each game summary has correct totalAchievements', () => {
    for (const summary of store.getGameSummaries()) {
      expect(summary.totalAchievements).toBe(4);
    }
  });

  it('each game summary has correct totalPoints', () => {
    const mk64Summary = store.getGameSummaries().find((s) => s.gameId === MK64_GAME_ID);
    expect(mk64Summary).toBeDefined();
    const expectedPoints = RETRO_ACHIEVEMENT_DEFS
      .filter((d) => d.gameId === MK64_GAME_ID)
      .reduce((s, d) => s + d.points, 0);
    expect(mk64Summary!.totalPoints).toBe(expectedPoints);
  });

  // ── getProgress ────────────────────────────────────────────────────────────

  it('getProgress creates a new record for unknown player', () => {
    const prog = store.getProgress('p1');
    expect(prog.playerId).toBe('p1');
    expect(prog.earned).toEqual([]);
    expect(prog.totalPoints).toBe(0);
  });

  it('getProgress returns the same object on repeated calls', () => {
    const a = store.getProgress('p1');
    const b = store.getProgress('p1');
    expect(a).toBe(b);
  });

  // ── getEarned ──────────────────────────────────────────────────────────────

  it('getEarned returns empty array for unknown player', () => {
    expect(store.getEarned('no-one')).toEqual([]);
  });

  it('getEarned returns empty array when no achievements earned', () => {
    store.getProgress('p1');
    expect(store.getEarned('p1')).toEqual([]);
  });

  // ── unlock ─────────────────────────────────────────────────────────────────

  it('unlock returns the def on first unlock', () => {
    const def = store.unlock('p1', 'mk64-first-race');
    expect(def).not.toBeNull();
    expect(def!.id).toBe('mk64-first-race');
    expect(def!.gameId).toBe(MK64_GAME_ID);
  });

  it('unlock adds an entry to earned', () => {
    store.unlock('p1', 'mk64-first-race');
    const earned = store.getEarned('p1');
    expect(earned).toHaveLength(1);
    expect(earned[0].achievementId).toBe('mk64-first-race');
    expect(earned[0].gameId).toBe(MK64_GAME_ID);
    expect(earned[0].earnedAt).toBeTruthy();
  });

  it('unlock updates totalPoints', () => {
    store.unlock('p1', 'mk64-first-race'); // 5 pts
    expect(store.getProgress('p1').totalPoints).toBe(5);
    store.unlock('p1', 'mk64-podium');     // 10 pts
    expect(store.getProgress('p1').totalPoints).toBe(15);
  });

  it('unlock returns null for already-earned achievement', () => {
    store.unlock('p1', 'mk64-first-race');
    const dup = store.unlock('p1', 'mk64-first-race');
    expect(dup).toBeNull();
  });

  it('duplicate unlock does not double-add to earned list', () => {
    store.unlock('p1', 'mk64-first-race');
    store.unlock('p1', 'mk64-first-race');
    expect(store.getEarned('p1')).toHaveLength(1);
  });

  it('unlock returns null for unknown achievement ID', () => {
    expect(store.unlock('p1', 'nonexistent')).toBeNull();
  });

  it('unlock stores optional sessionId', () => {
    store.unlock('p1', 'mk64-first-race', 'sess-abc');
    expect(store.getEarned('p1')[0].sessionId).toBe('sess-abc');
  });

  it('unlock without sessionId stores no sessionId field', () => {
    store.unlock('p1', 'mk64-first-race');
    const earned = store.getEarned('p1')[0];
    expect(earned.sessionId).toBeUndefined();
  });

  // ── unlockMany ─────────────────────────────────────────────────────────────

  it('unlockMany returns all newly unlocked defs', () => {
    const newly = store.unlockMany('p1', ['mk64-first-race', 'mk64-podium']);
    expect(newly).toHaveLength(2);
    expect(newly.map((d) => d.id)).toContain('mk64-first-race');
    expect(newly.map((d) => d.id)).toContain('mk64-podium');
  });

  it('unlockMany skips already-earned entries', () => {
    store.unlock('p1', 'mk64-first-race');
    const newly = store.unlockMany('p1', ['mk64-first-race', 'mk64-podium']);
    expect(newly).toHaveLength(1);
    expect(newly[0].id).toBe('mk64-podium');
  });

  it('unlockMany skips unknown IDs without throwing', () => {
    const newly = store.unlockMany('p1', ['mk64-first-race', 'fake-id']);
    expect(newly).toHaveLength(1);
    expect(newly[0].id).toBe('mk64-first-race');
  });

  it('unlockMany with empty list returns empty array', () => {
    expect(store.unlockMany('p1', [])).toEqual([]);
  });

  // ── getEarned with gameId filter ───────────────────────────────────────────

  it('getEarned filtered by gameId returns only that game', () => {
    store.unlock('p1', 'mk64-first-race');
    store.unlock('p1', 'ssb64-first-ko');
    const mk64Earned = store.getEarned('p1', MK64_GAME_ID);
    expect(mk64Earned).toHaveLength(1);
    expect(mk64Earned[0].achievementId).toBe('mk64-first-race');
  });

  it('getEarned filtered by gameId returns empty for game with no unlocks', () => {
    store.unlock('p1', 'mk64-first-race');
    expect(store.getEarned('p1', TETRIS_GAME_ID)).toHaveLength(0);
  });

  // ── playerCount ────────────────────────────────────────────────────────────

  it('playerCount starts at 0', () => {
    expect(store.playerCount()).toBe(0);
  });

  it('playerCount increments when getProgress is called for new players', () => {
    store.getProgress('p1');
    expect(store.playerCount()).toBe(1);
    store.getProgress('p2');
    expect(store.playerCount()).toBe(2);
  });

  it('playerCount does not increment on repeated getProgress for same player', () => {
    store.getProgress('p1');
    store.getProgress('p1');
    expect(store.playerCount()).toBe(1);
  });

  it('unlock creates a player if they did not exist yet', () => {
    store.unlock('p99', 'mk64-first-race');
    expect(store.playerCount()).toBe(1);
  });

  // ── cross-player isolation ─────────────────────────────────────────────────

  it('achievements earned by one player do not appear for another', () => {
    store.unlock('alice', 'mk64-first-race');
    expect(store.getEarned('bob')).toHaveLength(0);
  });

  it('totalPoints are independent per player', () => {
    store.unlock('alice', 'mk64-first-race'); // 5 pts
    store.unlock('bob', 'mk64-rainbow-road');  // 50 pts
    expect(store.getProgress('alice').totalPoints).toBe(5);
    expect(store.getProgress('bob').totalPoints).toBe(50);
  });

  // ── specific game achievement IDs ──────────────────────────────────────────

  it('Pokémon Emerald achievements have correct IDs', () => {
    const ids = store.getGameDefinitions(PKEM_GAME_ID).map((d) => d.id);
    expect(ids).toContain('pkem-first-catch');
    expect(ids).toContain('pkem-link-battle');
    expect(ids).toContain('pkem-hoenn-champ');
    expect(ids).toContain('pkem-contest');
  });

  it('Tetris achievements have correct IDs', () => {
    const ids = store.getGameDefinitions(TETRIS_GAME_ID).map((d) => d.id);
    expect(ids).toContain('gb-tetris-10lines');
    expect(ids).toContain('gb-tetris-tetris');
    expect(ids).toContain('gb-tetris-level9');
    expect(ids).toContain('gb-tetris-vs-win');
  });

  it('Contra achievements include the hard no-life challenge', () => {
    const contra = store.getGameDefinitions(CONTRA_GAME_ID);
    const hardAch = contra.find((d) => d.id === 'nes-contra-nolose');
    expect(hardAch).toBeDefined();
    expect(hardAch!.points).toBe(100);
  });

  it('Mario Kart DS achievements are present', () => {
    const mkds = store.getGameDefinitions(MKDS_GAME_ID);
    expect(mkds.map((d) => d.id)).toContain('mkds-wifi-win');
    expect(mkds.map((d) => d.id)).toContain('mkds-mission');
  });

  it('Pokémon Diamond achievements are present', () => {
    const pkdp = store.getGameDefinitions(PKDP_GAME_ID);
    expect(pkdp.map((d) => d.id)).toContain('pkdp-elite-four');
    expect(pkdp.map((d) => d.id)).toContain('pkdp-trade');
  });

  it('Super Smash Bros N64 achievements are present', () => {
    const ssb = store.getGameDefinitions(SSB_GAME_ID);
    expect(ssb.map((d) => d.id)).toContain('ssb64-untouchable');
    expect(ssb.map((d) => d.id)).toContain('ssb64-comeback');
  });

  it('Super Bomberman (SNES) achievements are present', () => {
    const bomba = store.getGameDefinitions(BOMBERMAN_GAME_ID);
    expect(bomba).toHaveLength(4);
    expect(bomba.map((d) => d.id)).toContain('bomba-first-win');
    expect(bomba.map((d) => d.id)).toContain('bomba-no-miss');
  });
});
