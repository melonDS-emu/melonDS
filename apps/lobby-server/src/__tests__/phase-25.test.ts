/**
 * Phase 25 — More Retro Achievements
 *
 * Tests for:
 *  - Expanded catalog (80 total, 20 games × 4 each)
 *  - New game entries: GBC, PSP, PSX (SotN + MGS), SNES (Secret of Mana)
 *  - RetroAchievementStore behaviour with new achievement IDs
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  RetroAchievementStore,
  RETRO_ACHIEVEMENT_DEFS,
} from '../retro-achievement-store';

// ─── constants ────────────────────────────────────────────────────────────────

const GBC_CRYSTAL_ID   = 'gbc-pokemon-crystal';
const PSP_MHFU_ID      = 'psp-monster-hunter-fu';
const PSX_SOTN_ID      = 'psx-castlevania-sotn';
const SNES_MANA_ID     = 'snes-secret-of-mana';
const PSX_MGS_ID       = 'psx-metal-gear-solid';

// ─── Expanded catalog ────────────────────────────────────────────────────────

describe('RETRO_ACHIEVEMENT_DEFS expanded catalog (Phase 25)', () => {
  it('contains exactly 80 definitions', () => {
    expect(RETRO_ACHIEVEMENT_DEFS).toHaveLength(80);
  });

  it('covers exactly 20 games', () => {
    const gameIds = new Set(RETRO_ACHIEVEMENT_DEFS.map((d) => d.gameId));
    expect(gameIds.size).toBe(20);
  });

  it('has no duplicate achievement IDs', () => {
    const ids = RETRO_ACHIEVEMENT_DEFS.map((d) => d.id);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('each game has exactly 4 achievements', () => {
    const gameIds = new Set(RETRO_ACHIEVEMENT_DEFS.map((d) => d.gameId));
    for (const gid of gameIds) {
      const count = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === gid).length;
      expect(count).toBe(4);
    }
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

  // ── New Phase 25 games ──────────────────────────────────────────────────

  it('includes Pokémon Crystal (GBC)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === GBC_CRYSTAL_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('gbc-pkcr-first-badge');
    expect(defs.map((d) => d.id)).toContain('gbc-pkcr-champion');
  });

  it('includes Monster Hunter Freedom Unite (PSP)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === PSP_MHFU_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('psp-mhfu-first-hunt');
    expect(defs.map((d) => d.id)).toContain('psp-mhfu-g-rank');
  });

  it('includes Castlevania: Symphony of the Night (PSX)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === PSX_SOTN_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('psx-sotn-first-area');
    expect(defs.map((d) => d.id)).toContain('psx-sotn-true-end');
  });

  it('includes Secret of Mana (SNES)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === SNES_MANA_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('snes-mana-first-weapon');
    expect(defs.map((d) => d.id)).toContain('snes-mana-mana-beast');
  });

  it('includes Metal Gear Solid (PSX)', () => {
    const defs = RETRO_ACHIEVEMENT_DEFS.filter((d) => d.gameId === PSX_MGS_ID);
    expect(defs).toHaveLength(4);
    expect(defs.map((d) => d.id)).toContain('psx-mgs-start');
    expect(defs.map((d) => d.id)).toContain('psx-mgs-big-boss');
  });
});

// ─── Store behaviour with Phase 25 achievements ───────────────────────────────

describe('RetroAchievementStore — Phase 25 new achievements', () => {
  let store: RetroAchievementStore;

  beforeEach(() => {
    store = new RetroAchievementStore();
  });

  it('can unlock a GBC achievement', () => {
    const def = store.unlock('alice', 'gbc-pkcr-first-badge');
    expect(def).not.toBeNull();
    expect(def!.gameId).toBe(GBC_CRYSTAL_ID);
    expect(def!.points).toBe(5);
  });

  it('can unlock a PSP achievement', () => {
    const def = store.unlock('bob', 'psp-mhfu-g-rank');
    expect(def).not.toBeNull();
    expect(def!.gameId).toBe(PSP_MHFU_ID);
    expect(def!.points).toBe(50);
  });

  it('can unlock SotN achievements', () => {
    store.unlock('carol', 'psx-sotn-first-area');
    store.unlock('carol', 'psx-sotn-inverted');
    store.unlock('carol', 'psx-sotn-true-end');
    expect(store.getEarned('carol', PSX_SOTN_ID)).toHaveLength(3);
  });

  it('can unlock Secret of Mana co-op achievement', () => {
    const def = store.unlock('dave', 'snes-mana-coop');
    expect(def).not.toBeNull();
    expect(def!.gameId).toBe(SNES_MANA_ID);
  });

  it('can unlock MGS big boss rank', () => {
    const def = store.unlock('eve', 'psx-mgs-big-boss');
    expect(def).not.toBeNull();
    expect(def!.points).toBe(100);
  });

  it('getGameDefinitions works for all new Phase 25 games', () => {
    for (const gameId of [GBC_CRYSTAL_ID, PSP_MHFU_ID, PSX_SOTN_ID, SNES_MANA_ID, PSX_MGS_ID]) {
      expect(store.getGameDefinitions(gameId)).toHaveLength(4);
    }
  });

  it('getGameSummaries includes all 20 games', () => {
    expect(store.getGameSummaries()).toHaveLength(20);
  });

  it('getGameIds returns 20 unique IDs', () => {
    expect(store.getGameIds()).toHaveLength(20);
  });

  it('leaderboard reflects points from Phase 25 achievements', () => {
    store.unlock('p1', 'gbc-pkcr-champion');   // 50pts
    store.unlock('p1', 'psp-mhfu-g-rank');     // 50pts  → 100pts total
    store.unlock('p2', 'psx-mgs-big-boss');    // 100pts
    store.unlock('p2', 'snes-mana-coop');      // 10pts  → 110pts total
    const lb = store.getLeaderboard();
    expect(lb[0].playerId).toBe('p2'); // 110pts
    expect(lb[0].totalPoints).toBe(110);
    expect(lb[1].playerId).toBe('p1'); // 100pts
    expect(lb[1].totalPoints).toBe(100);
  });

  it('unlockMany works across multiple Phase 25 games', () => {
    const unlocked = store.unlockMany('frank', [
      'gbc-pkcr-link-trade',
      'psp-mhfu-rathalos',
      'psx-sotn-familiar',
      'snes-mana-all-weapons',
      'psx-mgs-no-kill',
    ]);
    expect(unlocked).toHaveLength(5);
    const earnedPoints = unlocked.reduce((s, d) => s + d.points, 0);
    expect(earnedPoints).toBeGreaterThan(0);
  });
});
