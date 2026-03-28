/**
 * Phase 32 — Wiimmfi & WiiLink Online Services
 *
 * Tests covering:
 *
 *  1. WFC_PROVIDERS — Wiimmfi, WiiLink WFC, and AltWFC are all present with
 *     correct DNS IPs, URLs, and `supportsWii` flags.
 *
 *  2. WIILINK_CHANNELS — all 6 channel definitions have required fields and
 *     are marked active.
 *
 *  3. getWfcProvider() — returns the correct provider by id, or null for
 *     unknown IDs.
 *
 *  4. DEFAULT_WFC_PROVIDER_ID — still resolves to 'wiimmfi'.
 *
 *  5. REST endpoints:
 *     - GET /api/wfc/providers        → all 3 providers
 *     - GET /api/wfc/providers/:id    → single provider (200) / unknown (404)
 *     - GET /api/wfc/wiilink-channels → all 6 channels
 */

import { describe, it, expect } from 'vitest';
import {
  WFC_PROVIDERS,
  WIILINK_CHANNELS,
  getWfcProvider,
  DEFAULT_WFC_PROVIDER_ID,
} from '../wfc-config';

// ─── WFC_PROVIDERS unit tests ────────────────────────────────────────────────

describe('WFC_PROVIDERS — provider list (Phase 32)', () => {
  it('contains exactly 3 providers', () => {
    expect(WFC_PROVIDERS).toHaveLength(3);
  });

  it('first provider is Wiimmfi (default)', () => {
    expect(WFC_PROVIDERS[0].id).toBe('wiimmfi');
    expect(WFC_PROVIDERS[0].name).toBe('Wiimmfi');
  });

  it('second provider is WiiLink WFC', () => {
    expect(WFC_PROVIDERS[1].id).toBe('wiilink-wfc');
    expect(WFC_PROVIDERS[1].name).toBe('WiiLink WFC');
  });

  it('third provider is AltWFC', () => {
    expect(WFC_PROVIDERS[2].id).toBe('altwfc');
    expect(WFC_PROVIDERS[2].name).toBe('AltWFC');
  });

  it('every provider has required fields', () => {
    for (const p of WFC_PROVIDERS) {
      expect(typeof p.id).toBe('string');
      expect(typeof p.name).toBe('string');
      expect(typeof p.dnsServer).toBe('string');
      expect(typeof p.description).toBe('string');
      expect(typeof p.url).toBe('string');
      expect(typeof p.supportsWii).toBe('boolean');
      expect(Array.isArray(p.supportsGames)).toBe(true);
    }
  });
});

describe('Wiimmfi provider (Phase 32)', () => {
  const wiimmfi = WFC_PROVIDERS.find((p) => p.id === 'wiimmfi')!;

  it('has correct DNS IP', () => {
    expect(wiimmfi.dnsServer).toBe('178.62.43.212');
  });

  it('points to wiimmfi.de', () => {
    expect(wiimmfi.url).toContain('wiimmfi.de');
  });

  it('supportsWii is true', () => {
    expect(wiimmfi.supportsWii).toBe(true);
  });

  it('description mentions Wii and DS titles', () => {
    expect(wiimmfi.description.toLowerCase()).toMatch(/wii/);
    expect(wiimmfi.description.toLowerCase()).toMatch(/ds/);
  });

  it('description mentions 2014 shutdown', () => {
    expect(wiimmfi.description).toMatch(/2014/);
  });
});

describe('WiiLink WFC provider (Phase 32)', () => {
  const wiilink = WFC_PROVIDERS.find((p) => p.id === 'wiilink-wfc')!;

  it('exists in the providers list', () => {
    expect(wiilink).toBeDefined();
  });

  it('has correct DNS IP', () => {
    expect(wiilink.dnsServer).toBe('167.235.229.36');
  });

  it('points to wiilink24.com', () => {
    expect(wiilink.url).toContain('wiilink24.com');
  });

  it('supportsWii is true', () => {
    expect(wiilink.supportsWii).toBe(true);
  });

  it('description mentions WFC and channels', () => {
    expect(wiilink.description.toLowerCase()).toMatch(/wfc|wi-fi connection/i);
  });

  it('supportsGames defaults to empty array', () => {
    expect(wiilink.supportsGames).toEqual([]);
  });
});

describe('AltWFC provider (Phase 32)', () => {
  const altwfc = WFC_PROVIDERS.find((p) => p.id === 'altwfc')!;

  it('has correct DNS IP', () => {
    expect(altwfc.dnsServer).toBe('172.104.88.237');
  });

  it('supportsWii is false (DS-only fallback)', () => {
    expect(altwfc.supportsWii).toBe(false);
  });
});

// ─── getWfcProvider helper ───────────────────────────────────────────────────

describe('getWfcProvider() (Phase 32)', () => {
  it('returns Wiimmfi for id wiimmfi', () => {
    const p = getWfcProvider('wiimmfi');
    expect(p).not.toBeNull();
    expect(p!.id).toBe('wiimmfi');
  });

  it('returns WiiLink WFC for id wiilink-wfc', () => {
    const p = getWfcProvider('wiilink-wfc');
    expect(p).not.toBeNull();
    expect(p!.id).toBe('wiilink-wfc');
  });

  it('returns AltWFC for id altwfc', () => {
    const p = getWfcProvider('altwfc');
    expect(p).not.toBeNull();
    expect(p!.id).toBe('altwfc');
  });

  it('returns null for an unknown id', () => {
    expect(getWfcProvider('unknown-provider')).toBeNull();
  });
});

// ─── DEFAULT_WFC_PROVIDER_ID ─────────────────────────────────────────────────

describe('DEFAULT_WFC_PROVIDER_ID (Phase 32)', () => {
  it('is wiimmfi', () => {
    expect(DEFAULT_WFC_PROVIDER_ID).toBe('wiimmfi');
  });

  it('resolves to an actual provider', () => {
    const p = getWfcProvider(DEFAULT_WFC_PROVIDER_ID);
    expect(p).not.toBeNull();
  });
});

// ─── WIILINK_CHANNELS ────────────────────────────────────────────────────────

describe('WIILINK_CHANNELS (Phase 32)', () => {
  it('contains 6 channel definitions', () => {
    expect(WIILINK_CHANNELS).toHaveLength(6);
  });

  it('every channel has required fields', () => {
    for (const ch of WIILINK_CHANNELS) {
      expect(typeof ch.id).toBe('string');
      expect(typeof ch.name).toBe('string');
      expect(typeof ch.emoji).toBe('string');
      expect(typeof ch.description).toBe('string');
      expect(typeof ch.active).toBe('boolean');
    }
  });

  it('all channels are active', () => {
    for (const ch of WIILINK_CHANNELS) {
      expect(ch.active).toBe(true);
    }
  });

  it('includes Forecast Channel', () => {
    expect(WIILINK_CHANNELS.some((ch) => ch.id === 'forecast')).toBe(true);
  });

  it('includes News Channel', () => {
    expect(WIILINK_CHANNELS.some((ch) => ch.id === 'news')).toBe(true);
  });

  it('includes Everybody Votes Channel', () => {
    expect(WIILINK_CHANNELS.some((ch) => ch.id === 'everybody-votes')).toBe(true);
  });

  it('includes Check Mii Out Channel', () => {
    expect(WIILINK_CHANNELS.some((ch) => ch.id === 'check-mii-out')).toBe(true);
  });

  it('includes Nintendo Channel', () => {
    expect(WIILINK_CHANNELS.some((ch) => ch.id === 'nintendo')).toBe(true);
  });

  it('includes Demae Channel', () => {
    expect(WIILINK_CHANNELS.some((ch) => ch.id === 'demae')).toBe(true);
  });

  it('channel ids are unique', () => {
    const ids = WIILINK_CHANNELS.map((ch) => ch.id);
    const unique = new Set(ids);
    expect(unique.size).toBe(ids.length);
  });
});

// ─── REST endpoint tests ─────────────────────────────────────────────────────

import * as http from 'http';
import { AddressInfo } from 'net';
import * as urlMod from 'url';

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

function buildHandler(): (req: http.IncomingMessage, res: http.ServerResponse) => void {
  function json(res: http.ServerResponse, status: number, body: unknown) {
    const payload = JSON.stringify(body);
    res.writeHead(status, { 'Content-Type': 'application/json' });
    res.end(payload);
  }

  return (req, res) => {
    const parsed = urlMod.parse(req.url ?? '', true);
    const pathname = parsed.pathname ?? '';

    if (pathname === '/api/wfc/providers' && req.method === 'GET') {
      json(res, 200, WFC_PROVIDERS);
      return;
    }

    if (pathname === '/api/wfc/wiilink-channels' && req.method === 'GET') {
      json(res, 200, WIILINK_CHANNELS);
      return;
    }

    const providerMatch = pathname.match(/^\/api\/wfc\/providers\/([^/]+)$/);
    if (providerMatch && req.method === 'GET') {
      const providerId = decodeURIComponent(providerMatch[1]);
      const provider = WFC_PROVIDERS.find((p) => p.id === providerId);
      if (!provider) {
        json(res, 404, { error: 'WFC provider not found' });
        return;
      }
      json(res, 200, provider);
      return;
    }

    json(res, 404, { error: 'Not found' });
  };
}

describe('REST — GET /api/wfc/providers (Phase 32)', () => {
  it('returns all 3 providers', async () => {
    const handler = buildHandler();
    await withServer(handler, async (port) => {
      const { status, body } = await get(port, '/api/wfc/providers');
      expect(status).toBe(200);
      const list = body as Array<{ id: string }>;
      expect(list).toHaveLength(3);
      expect(list.map((p) => p.id)).toContain('wiimmfi');
      expect(list.map((p) => p.id)).toContain('wiilink-wfc');
      expect(list.map((p) => p.id)).toContain('altwfc');
    });
  });
});

describe('REST — GET /api/wfc/providers/:id (Phase 32)', () => {
  it('returns Wiimmfi for id wiimmfi', async () => {
    const handler = buildHandler();
    await withServer(handler, async (port) => {
      const { status, body } = await get(port, '/api/wfc/providers/wiimmfi');
      expect(status).toBe(200);
      expect((body as { id: string }).id).toBe('wiimmfi');
    });
  });

  it('returns WiiLink WFC for id wiilink-wfc', async () => {
    const handler = buildHandler();
    await withServer(handler, async (port) => {
      const { status, body } = await get(port, '/api/wfc/providers/wiilink-wfc');
      expect(status).toBe(200);
      expect((body as { id: string }).id).toBe('wiilink-wfc');
      expect((body as { dnsServer: string }).dnsServer).toBe('167.235.229.36');
    });
  });

  it('returns 404 for an unknown provider id', async () => {
    const handler = buildHandler();
    await withServer(handler, async (port) => {
      const { status } = await get(port, '/api/wfc/providers/unknown-service');
      expect(status).toBe(404);
    });
  });
});

describe('REST — GET /api/wfc/wiilink-channels (Phase 32)', () => {
  it('returns all 6 WiiLink channels', async () => {
    const handler = buildHandler();
    await withServer(handler, async (port) => {
      const { status, body } = await get(port, '/api/wfc/wiilink-channels');
      expect(status).toBe(200);
      const list = body as Array<{ id: string }>;
      expect(list).toHaveLength(6);
      expect(list.map((ch) => ch.id)).toContain('forecast');
      expect(list.map((ch) => ch.id)).toContain('everybody-votes');
    });
  });
});
