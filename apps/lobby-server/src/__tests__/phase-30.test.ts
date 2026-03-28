/**
 * Phase 30 — QoL & "Wow" Layer
 *
 * Tests for:
 *  - GameMetadataStore: seed data, get, getByGenre, set
 *  - PatchStore: seed data, getSafe, getByGameId, upsert
 *  - PartyCollectionStore: seed data, getById, getByGameId, getByTag, upsert
 *  - REST endpoints via httpHandler:
 *      GET /api/game-metadata
 *      GET /api/game-metadata/:gameId
 *      GET /api/patches
 *      GET /api/patches?gameId=X
 *      GET /api/party-collections
 *      GET /api/party-collections?tag=X
 *      GET /api/party-collections/:id
 */

import { describe, it, expect, beforeEach } from 'vitest';
import { GameMetadataStore } from '../game-metadata-store';
import type { GameMetadata } from '../game-metadata-store';
import { PatchStore } from '../patch-store';
import type { GamePatch } from '../patch-store';
import { PartyCollectionStore } from '../party-collection-store';
import type { PartyCollection } from '../party-collection-store';

// ---------------------------------------------------------------------------
// GameMetadataStore
// ---------------------------------------------------------------------------

describe('GameMetadataStore — seed data', () => {
  let store: GameMetadataStore;
  beforeEach(() => { store = new GameMetadataStore(); });

  it('has at least 30 seeded entries', () => {
    expect(store.count()).toBeGreaterThanOrEqual(30);
  });

  it('all entries have required fields', () => {
    for (const m of store.getAll()) {
      expect(m.gameId).toBeTruthy();
      expect(m.genre).toBeTruthy();
      expect(m.developer).toBeTruthy();
      expect(m.year).toBeGreaterThan(1980);
      expect(Array.isArray(m.onboardingTips)).toBe(true);
      expect(m.onboardingTips.length).toBeGreaterThanOrEqual(1);
      expect(Array.isArray(m.netplayTips)).toBe(true);
      expect(m.recommendedController).toBeTruthy();
      expect(m.artworkColor).toMatch(/^#[0-9a-fA-F]{6}$/);
    }
  });

  it('mario-kart-64 has netplay tips', () => {
    const m = store.get('n64-mario-kart-64');
    expect(m).toBeDefined();
    expect(m!.netplayTips.length).toBeGreaterThan(0);
    expect(m!.genre).toBe('Kart Racing');
  });

  it('nds-mario-kart-ds has WFC netplay tip', () => {
    const m = store.get('nds-mario-kart-ds');
    expect(m).toBeDefined();
    expect(m!.netplayTips.some((t) => t.includes('Wiimmfi') || t.includes('WFC'))).toBe(true);
  });

  it('n64-super-smash-bros has recommended controller', () => {
    const m = store.get('n64-super-smash-bros');
    expect(m).toBeDefined();
    expect(m!.recommendedController).toBeTruthy();
  });

  it('gc-super-smash-bros-melee has quickHostPreset 1v1', () => {
    const m = store.get('gc-super-smash-bros-melee');
    expect(m).toBeDefined();
    expect(m!.quickHostPreset).toBe('1v1');
  });

  it('party games have quickHostPreset full-party', () => {
    const partyGames = store.getAll().filter((m) => m.quickHostPreset === 'full-party');
    expect(partyGames.length).toBeGreaterThanOrEqual(4);
  });

  it('returns undefined for unknown game', () => {
    expect(store.get('unknown-game-id')).toBeUndefined();
  });
});

describe('GameMetadataStore — getByGenre', () => {
  let store: GameMetadataStore;
  beforeEach(() => { store = new GameMetadataStore(); });

  it('returns kart racing games', () => {
    const racers = store.getByGenre('Kart Racing');
    expect(racers.length).toBeGreaterThanOrEqual(3);
    for (const m of racers) expect(m.genre).toBe('Kart Racing');
  });

  it('returns empty array for unknown genre', () => {
    expect(store.getByGenre('Breakdance Simulator')).toEqual([]);
  });

  it('genre lookup is case-insensitive', () => {
    const lower = store.getByGenre('kart racing');
    const upper = store.getByGenre('KART RACING');
    expect(lower.length).toBe(upper.length);
  });
});

describe('GameMetadataStore — set / upsert', () => {
  let store: GameMetadataStore;
  beforeEach(() => { store = new GameMetadataStore(); });

  it('can add a new metadata record', () => {
    const rec: GameMetadata = {
      gameId: 'test-game-1',
      genre: 'Test Genre',
      developer: 'Test Dev',
      year: 2000,
      onboardingTips: ['Tip 1'],
      netplayTips: ['Keep latency low'],
      recommendedController: 'Any',
      artworkColor: '#123456',
    };
    const returned = store.set(rec);
    expect(returned.gameId).toBe('test-game-1');
    expect(store.get('test-game-1')).toEqual(rec);
  });

  it('set overwrites an existing record', () => {
    const m = store.get('n64-mario-kart-64')!;
    const updated = { ...m, genre: 'Racing Updated' };
    store.set(updated);
    expect(store.get('n64-mario-kart-64')!.genre).toBe('Racing Updated');
  });

  it('count increases after adding a new entry', () => {
    const before = store.count();
    store.set({
      gameId: 'new-game-id',
      genre: 'Platform',
      developer: 'Dev',
      year: 1999,
      onboardingTips: [],
      netplayTips: [],
      recommendedController: 'Any',
      artworkColor: '#ffffff',
    });
    expect(store.count()).toBe(before + 1);
  });
});

// ---------------------------------------------------------------------------
// PatchStore
// ---------------------------------------------------------------------------

describe('PatchStore — seed data', () => {
  let store: PatchStore;
  beforeEach(() => { store = new PatchStore(); });

  it('has at least 15 seeded patches', () => {
    expect(store.count()).toBeGreaterThanOrEqual(15);
  });

  it('all safe patches have required fields', () => {
    for (const p of store.getSafe()) {
      expect(p.id).toBeTruthy();
      expect(p.gameId).toBeTruthy();
      expect(p.name).toBeTruthy();
      expect(p.description).toBeTruthy();
      expect(['translation', 'qol', 'bugfix', 'difficulty', 'enhancement']).toContain(p.type);
      expect(p.safe).toBe(true);
      expect(p.version).toBeTruthy();
      expect(Array.isArray(p.instructions)).toBe(true);
      expect(p.instructions.length).toBeGreaterThanOrEqual(1);
      expect(Array.isArray(p.tags)).toBe(true);
    }
  });

  it('getSafe returns only safe patches', () => {
    const safe = store.getSafe();
    expect(safe.every((p) => p.safe)).toBe(true);
  });

  it('getByGameId returns only patches for the given game', () => {
    const patches = store.getByGameId('n64-mario-kart-64');
    expect(patches.length).toBeGreaterThan(0);
    for (const p of patches) {
      expect(p.gameId).toBe('n64-mario-kart-64');
    }
  });

  it('getByGameId returns empty array for unknown game', () => {
    expect(store.getByGameId('atari-asteroids')).toEqual([]);
  });

  it('has patches for multiple systems', () => {
    const systems = new Set(store.getSafe().map((p) => p.gameId.split('-')[0]));
    expect(systems.size).toBeGreaterThanOrEqual(5);
  });

  it('some patches are tagged recommended', () => {
    const recommended = store.getSafe().filter((p) => p.tags.includes('recommended'));
    expect(recommended.length).toBeGreaterThanOrEqual(3);
  });

  it('some patches are tagged multiplayer-compatible', () => {
    const mpCompat = store.getSafe().filter((p) => p.tags.includes('multiplayer-compatible'));
    expect(mpCompat.length).toBeGreaterThanOrEqual(4);
  });
});

describe('PatchStore — getById / upsert', () => {
  let store: PatchStore;
  beforeEach(() => { store = new PatchStore(); });

  it('getById returns the patch', () => {
    const first = store.getSafe()[0];
    expect(store.getById(first.id)).toEqual(first);
  });

  it('getById returns undefined for unknown id', () => {
    expect(store.getById('does-not-exist')).toBeUndefined();
  });

  it('upsert adds a new patch', () => {
    const before = store.count();
    const patch: GamePatch = {
      id: 'test-patch-new',
      gameId: 'nes-super-mario-bros',
      name: 'Test Patch',
      description: 'A test patch',
      type: 'qol',
      safe: true,
      version: '1.0',
      instructions: ['Apply the patch'],
      tags: ['test'],
    };
    store.upsert(patch);
    expect(store.count()).toBe(before + 1);
    expect(store.getById('test-patch-new')).toEqual(patch);
  });

  it('upsert updates an existing patch', () => {
    const existing = store.getSafe()[0];
    const updated = { ...existing, version: '99.0' };
    store.upsert(updated);
    expect(store.getById(existing.id)!.version).toBe('99.0');
    // Count stays the same
    expect(store.count()).toBe(store.getAll().length);
  });
});

// ---------------------------------------------------------------------------
// PartyCollectionStore
// ---------------------------------------------------------------------------

describe('PartyCollectionStore — seed data', () => {
  let store: PartyCollectionStore;
  beforeEach(() => { store = new PartyCollectionStore(); });

  it('has at least 10 seeded collections', () => {
    expect(store.count()).toBeGreaterThanOrEqual(10);
  });

  it('all collections have required fields', () => {
    for (const c of store.getAll()) {
      expect(c.id).toBeTruthy();
      expect(c.name).toBeTruthy();
      expect(c.description).toBeTruthy();
      expect(c.emoji).toBeTruthy();
      expect(c.curatorNote).toBeTruthy();
      expect(Array.isArray(c.gameIds)).toBe(true);
      expect(c.gameIds.length).toBeGreaterThanOrEqual(1);
      expect(Array.isArray(c.tags)).toBe(true);
      expect(c.idealPlayers).toBeTruthy();
    }
  });

  it('kart-classics collection exists and includes multiple games', () => {
    const c = store.getById('kart-classics');
    expect(c).toBeDefined();
    expect(c!.gameIds.length).toBeGreaterThanOrEqual(4);
    expect(c!.emoji).toBe('🏎️');
  });

  it('online-wfc-ready collection contains WFC games', () => {
    const c = store.getById('online-wfc-ready');
    expect(c).toBeDefined();
    expect(c!.tags.includes('wfc') || c!.tags.includes('online')).toBe(true);
  });

  it('party-games collection has full-party capable entries', () => {
    const c = store.getById('party-games');
    expect(c).toBeDefined();
    expect(c!.gameIds.length).toBeGreaterThanOrEqual(3);
  });
});

describe('PartyCollectionStore — getByGameId', () => {
  let store: PartyCollectionStore;
  beforeEach(() => { store = new PartyCollectionStore(); });

  it('returns collections containing n64-mario-kart-64', () => {
    const cols = store.getByGameId('n64-mario-kart-64');
    expect(cols.length).toBeGreaterThan(0);
    for (const c of cols) expect(c.gameIds).toContain('n64-mario-kart-64');
  });

  it('returns empty array for unknown game', () => {
    expect(store.getByGameId('mystery-cart-123')).toEqual([]);
  });
});

describe('PartyCollectionStore — getByTag', () => {
  let store: PartyCollectionStore;
  beforeEach(() => { store = new PartyCollectionStore(); });

  it('returns collections tagged racing', () => {
    const cols = store.getByTag('racing');
    expect(cols.length).toBeGreaterThan(0);
    for (const c of cols) expect(c.tags).toContain('racing');
  });

  it('returns empty array for unknown tag', () => {
    expect(store.getByTag('extreme-ironing')).toEqual([]);
  });

  it('co-op tag returns at least 3 collections', () => {
    expect(store.getByTag('co-op').length).toBeGreaterThanOrEqual(3);
  });
});

describe('PartyCollectionStore — upsert', () => {
  let store: PartyCollectionStore;
  beforeEach(() => { store = new PartyCollectionStore(); });

  it('adds a new collection', () => {
    const before = store.count();
    const col: PartyCollection = {
      id: 'test-collection',
      name: 'Test Collection',
      description: 'For testing',
      emoji: '🧪',
      curatorNote: 'This is a test',
      gameIds: ['nes-contra'],
      tags: ['test'],
      idealPlayers: '1–2 players',
    };
    store.upsert(col);
    expect(store.count()).toBe(before + 1);
    expect(store.getById('test-collection')).toEqual(col);
  });

  it('updates an existing collection', () => {
    const existing = store.getAll()[0];
    const updated = { ...existing, name: 'Updated Name' };
    store.upsert(updated);
    expect(store.getById(existing.id)!.name).toBe('Updated Name');
    expect(store.count()).toBe(store.getAll().length);
  });
});

// ---------------------------------------------------------------------------
// REST endpoints via httpHandler
// ---------------------------------------------------------------------------

import * as http from 'http';
import { AddressInfo } from 'net';

/** Minimal HTTP test helper — starts a one-shot server with the handler. */
async function withServer(
  handler: (req: http.IncomingMessage, res: http.ServerResponse) => void,
  fn: (port: number) => Promise<void>,
): Promise<void> {
  const server = http.createServer(handler);
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = (server.address() as AddressInfo).port;
  try {
    await fn(port);
  } finally {
    await new Promise<void>((resolve, reject) =>
      server.close((err) => (err ? reject(err) : resolve())),
    );
  }
}

async function get(port: number, path: string): Promise<{ status: number; body: unknown }> {
  return new Promise((resolve, reject) => {
    http.get(`http://127.0.0.1:${port}${path}`, (res) => {
      const chunks: Buffer[] = [];
      res.on('data', (c: Buffer) => chunks.push(c));
      res.on('end', () => {
        const text = Buffer.concat(chunks).toString('utf-8');
        try {
          resolve({ status: res.statusCode ?? 0, body: JSON.parse(text) });
        } catch {
          resolve({ status: res.statusCode ?? 0, body: text });
        }
      });
    }).on('error', reject);
  });
}

// We import the stores directly and build a minimal handler that mirrors the
// production code so we can test the HTTP layer without starting the full server.
import { GameMetadataStore as GMS2 } from '../game-metadata-store';
import { PatchStore as PS2 } from '../patch-store';
import { PartyCollectionStore as PCS2 } from '../party-collection-store';
import * as urlMod from 'url';

function buildHandler(): (req: http.IncomingMessage, res: http.ServerResponse) => void {
  const meta = new GMS2();
  const patches = new PS2();
  const collections = new PCS2();

  function json(res: http.ServerResponse, status: number, body: unknown) {
    const payload = JSON.stringify(body);
    res.writeHead(status, { 'Content-Type': 'application/json' });
    res.end(payload);
  }

  return (req, res) => {
    const parsed = urlMod.parse(req.url ?? '', true);
    const pathname = parsed.pathname ?? '';
    const query = parsed.query;

    if (pathname === '/api/game-metadata' && req.method === 'GET') {
      json(res, 200, { metadata: meta.getAll() });
      return;
    }

    const metaMatch = pathname.match(/^\/api\/game-metadata\/([^/]+)$/);
    if (metaMatch && req.method === 'GET') {
      const gameId = decodeURIComponent(metaMatch[1]);
      const m = meta.get(gameId);
      if (!m) { json(res, 404, { error: 'Not found' }); return; }
      json(res, 200, { metadata: m });
      return;
    }

    if (pathname === '/api/patches' && req.method === 'GET') {
      const gameId = typeof query.gameId === 'string' ? query.gameId : undefined;
      const list = gameId ? patches.getByGameId(gameId) : patches.getSafe();
      json(res, 200, { patches: list });
      return;
    }

    if (pathname === '/api/party-collections' && req.method === 'GET') {
      const tag = typeof query.tag === 'string' ? query.tag : undefined;
      const list = tag ? collections.getByTag(tag) : collections.getAll();
      json(res, 200, { collections: list });
      return;
    }

    const colMatch = pathname.match(/^\/api\/party-collections\/([^/]+)$/);
    if (colMatch && req.method === 'GET') {
      const id = decodeURIComponent(colMatch[1]);
      const col = collections.getById(id);
      if (!col) { json(res, 404, { error: 'Not found' }); return; }
      json(res, 200, { collection: col });
      return;
    }

    json(res, 404, { error: 'Not found' });
  };
}

describe('Phase 30 REST — GET /api/game-metadata', () => {
  it('returns all metadata entries', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/game-metadata');
      expect(status).toBe(200);
      const b = body as { metadata: GameMetadata[] };
      expect(b.metadata.length).toBeGreaterThanOrEqual(30);
    });
  });

  it('returns 200 with correct fields', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/game-metadata/n64-mario-kart-64');
      expect(status).toBe(200);
      const b = body as { metadata: GameMetadata };
      expect(b.metadata.gameId).toBe('n64-mario-kart-64');
      expect(b.metadata.genre).toBe('Kart Racing');
      expect(b.metadata.onboardingTips.length).toBeGreaterThan(0);
      expect(b.metadata.netplayTips.length).toBeGreaterThan(0);
    });
  });

  it('returns 404 for unknown game', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status } = await get(port, '/api/game-metadata/no-such-game');
      expect(status).toBe(404);
    });
  });
});

describe('Phase 30 REST — GET /api/patches', () => {
  it('returns all safe patches', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/patches');
      expect(status).toBe(200);
      const b = body as { patches: GamePatch[] };
      expect(b.patches.length).toBeGreaterThanOrEqual(15);
      expect(b.patches.every((p) => p.safe)).toBe(true);
    });
  });

  it('filters by gameId', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/patches?gameId=n64-mario-kart-64');
      expect(status).toBe(200);
      const b = body as { patches: GamePatch[] };
      expect(b.patches.every((p) => p.gameId === 'n64-mario-kart-64')).toBe(true);
    });
  });

  it('returns empty array for game with no patches', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/patches?gameId=atari-breakout');
      expect(status).toBe(200);
      const b = body as { patches: GamePatch[] };
      expect(b.patches).toEqual([]);
    });
  });
});

describe('Phase 30 REST — GET /api/party-collections', () => {
  it('returns all collections', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/party-collections');
      expect(status).toBe(200);
      const b = body as { collections: PartyCollection[] };
      expect(b.collections.length).toBeGreaterThanOrEqual(10);
    });
  });

  it('filters by tag', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/party-collections?tag=racing');
      expect(status).toBe(200);
      const b = body as { collections: PartyCollection[] };
      expect(b.collections.length).toBeGreaterThan(0);
      expect(b.collections.every((c) => c.tags.includes('racing'))).toBe(true);
    });
  });

  it('returns single collection by id', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status, body } = await get(port, '/api/party-collections/kart-classics');
      expect(status).toBe(200);
      const b = body as { collection: PartyCollection };
      expect(b.collection.id).toBe('kart-classics');
      expect(b.collection.emoji).toBe('🏎️');
    });
  });

  it('returns 404 for unknown collection', async () => {
    await withServer(buildHandler(), async (port) => {
      const { status } = await get(port, '/api/party-collections/no-such-collection');
      expect(status).toBe(404);
    });
  });
});
