/**
 * Smoke tests for GameApiClient (mock mode).
 *
 * These tests verify:
 *   - getGames()   returns a non-empty list of ApiGame objects
 *   - getGameById  resolves known/unknown IDs
 *   - searchGames  filters correctly
 *   - getSystems   returns a sorted list of system strings
 *   - query filters (system, tag, maxPlayers) work as expected
 */

import { describe, it, expect } from 'vitest';
import { GameApiClient } from '../api-client';

// Instantiate client in mock mode (no env vars needed)
const client = new GameApiClient('mock');

describe('GameApiClient – mock mode', () => {
  // -------------------------------------------------------------------------
  // getGames
  // -------------------------------------------------------------------------

  it('returns all games when no query is given', async () => {
    const games = await client.getGames();
    expect(games.length).toBeGreaterThan(0);
    // Every entry must satisfy the ApiGame contract
    for (const g of games) {
      expect(typeof g.id).toBe('string');
      expect(typeof g.title).toBe('string');
      expect(typeof g.system).toBe('string');
      expect(typeof g.maxPlayers).toBe('number');
      expect(Array.isArray(g.tags)).toBe(true);
      expect(Array.isArray(g.badges)).toBe(true);
    }
  });

  it('filters by system (case-insensitive)', async () => {
    const n64Games = await client.getGames({ system: 'n64' });
    expect(n64Games.length).toBeGreaterThan(0);
    for (const g of n64Games) {
      expect(g.system).toBe('N64');
    }
  });

  it('filters by tag', async () => {
    const partyGames = await client.getGames({ tag: 'Party' });
    expect(partyGames.length).toBeGreaterThan(0);
    for (const g of partyGames) {
      expect(g.tags).toContain('Party');
    }
  });

  it('filters by minimum player count', async () => {
    const fourPlusGames = await client.getGames({ maxPlayers: 4 });
    for (const g of fourPlusGames) {
      expect(g.maxPlayers).toBeGreaterThanOrEqual(4);
    }
  });

  it('returns empty array when no games match system filter', async () => {
    const games = await client.getGames({ system: 'ARCADE' });
    expect(games).toHaveLength(0);
  });

  // -------------------------------------------------------------------------
  // getGameById
  // -------------------------------------------------------------------------

  it('resolves a known game ID', async () => {
    const game = await client.getGameById('n64-mario-kart-64');
    expect(game).not.toBeNull();
    expect(game?.title).toBe('Mario Kart 64');
    expect(game?.system).toBe('N64');
  });

  it('returns null for an unknown ID', async () => {
    const game = await client.getGameById('non-existent-game');
    expect(game).toBeNull();
  });

  // -------------------------------------------------------------------------
  // searchGames
  // -------------------------------------------------------------------------

  it('finds games by title keyword', async () => {
    const results = await client.searchGames('mario');
    expect(results.length).toBeGreaterThan(0);
    const titles = results.map((g) => g.title.toLowerCase());
    expect(titles.some((t) => t.includes('mario'))).toBe(true);
  });

  it('finds games by description keyword', async () => {
    const results = await client.searchGames('co-op');
    expect(results.length).toBeGreaterThan(0);
  });

  it('returns empty array for no match', async () => {
    const results = await client.searchGames('xyzzy_no_match_1234');
    expect(results).toHaveLength(0);
  });

  // -------------------------------------------------------------------------
  // getSystems
  // -------------------------------------------------------------------------

  it('returns a sorted, non-empty list of systems', async () => {
    const systems = await client.getSystems();
    expect(systems.length).toBeGreaterThan(0);
    // All entries should be uppercase strings
    for (const s of systems) {
      expect(s).toBe(s.toUpperCase());
    }
    // Should include at least NES and N64
    expect(systems).toContain('NES');
    expect(systems).toContain('N64');
    // Should be sorted
    const sorted = [...systems].sort();
    expect(systems).toEqual(sorted);
  });

  // -------------------------------------------------------------------------
  // ApiGame shape invariants
  // -------------------------------------------------------------------------

  it('all games have a non-empty coverEmoji', async () => {
    const games = await client.getGames();
    for (const g of games) {
      expect(g.coverEmoji.length).toBeGreaterThan(0);
    }
  });

  it('all games have a valid systemColor hex string', async () => {
    const games = await client.getGames();
    for (const g of games) {
      expect(g.systemColor).toMatch(/^#[0-9A-Fa-f]{3,8}$/);
    }
  });

  it('all systems are uppercase abbreviations', async () => {
    const games = await client.getGames();
    for (const g of games) {
      expect(g.system).toBe(g.system.toUpperCase());
    }
  });
});
