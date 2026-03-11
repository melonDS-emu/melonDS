import * as http from 'http';
import * as net from 'net';
import * as url from 'url';
import { WebSocketServer } from 'ws';
import { LobbyManager } from './lobby-manager';
import { NetplayRelay } from './netplay-relay';
import { handleConnection } from './handler';
import { EmulatorBridge, scanRomDirectory } from '@retro-oasis/emulator-bridge';
import { GameCatalog } from '@retro-oasis/game-db';
import { RateLimiter, getRemoteIp } from './rate-limiter';
import { SessionHistory } from './session-history';
import { SaveStore } from './save-store';

const PORT = parseInt(process.env.PORT ?? '8080', 10);
/** Public hostname/IP players use to reach the relay (surfaced in game-starting events). */
const RELAY_HOST = process.env.RELAY_HOST ?? 'localhost';
const lobbyManager = new LobbyManager();
const netplayRelay = new NetplayRelay();
const emulatorBridge = new EmulatorBridge();
const sessionHistory = new SessionHistory();
const saveStore = new SaveStore();
const gameCatalog = new GameCatalog();

// Rate limiter: max 20 connections/requests burst, 2 per second steady-state
const wsRateLimiter = new RateLimiter({ capacity: 20, refillRate: 2 });
const httpRateLimiter = new RateLimiter({ capacity: 60, refillRate: 10 });
// Cache the system list since it is derived from static seed data.
const CACHED_SYSTEMS: string[] = [...new Set(gameCatalog.getAll().map((g) => g.system))].sort();

/** Collect the full request body as a string. */
function readBody(req: http.IncomingMessage): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    req.on('data', (chunk: Buffer) => chunks.push(chunk));
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf-8')));
    req.on('error', reject);
  });
}

function setCors(res: http.ServerResponse): void {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
}

function json(res: http.ServerResponse, status: number, body: unknown): void {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    'Content-Type': 'application/json',
    'Content-Length': Buffer.byteLength(payload),
  });
  res.end(payload);
}

async function httpHandler(req: http.IncomingMessage, res: http.ServerResponse): Promise<void> {
  setCors(res);

  // Handle CORS pre-flight (not rate limited)
  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  // Per-IP rate limit for HTTP API calls
  const clientIp = getRemoteIp(req);
  if (!httpRateLimiter.allow(clientIp)) {
    json(res, 429, { error: 'Too many requests — please slow down.' });
    return;
  }

  if (req.method === 'POST' && req.url === '/api/launch') {
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { success: false, error: 'Invalid JSON body' });
      return;
    }

    const {
      romPath,
      system,
      backendId,
      saveDirectory,
      playerSlot,
      netplayHost,
      netplayPort,
      fullscreen,
      sessionToken,
    } = body as {
      romPath?: string;
      system?: string;
      backendId?: string;
      saveDirectory?: string;
      playerSlot?: number;
      netplayHost?: string;
      netplayPort?: number;
      fullscreen?: boolean;
      sessionToken?: string;
    };

    if (!romPath || !system || !backendId) {
      json(res, 400, { success: false, error: 'romPath, system, and backendId are required' });
      return;
    }

    const result = await emulatorBridge.launch({
      romPath,
      system,
      backendId,
      // Fall back to the ROM file's directory when no save directory is given
      saveDirectory: saveDirectory || romPath.replace(/[/\\][^/\\]+$/, '') || romPath,
      playerSlot: typeof playerSlot === 'number' ? playerSlot : 0,
      netplayHost: typeof netplayHost === 'string' ? netplayHost : undefined,
      netplayPort: typeof netplayPort === 'number' ? netplayPort : undefined,
      fullscreen: fullscreen ?? false,
      sessionToken: typeof sessionToken === 'string' ? sessionToken : undefined,
    });

    json(res, result.success ? 200 : 500, result);
    return;
  }

  // -------------------------------------------------------------------------
  // Catalog API  (GET /api/games, /api/games/:id, /api/systems)
  // -------------------------------------------------------------------------

  const parsedUrl = url.parse(req.url ?? '', true);
  const pathname = parsedUrl.pathname ?? '';

  if (req.method === 'GET' && pathname === '/api/systems') {
    json(res, 200, CACHED_SYSTEMS);
    return;
  }

  if (req.method === 'GET' && pathname === '/api/games') {
    const { system, tag, maxPlayers, search } = parsedUrl.query;
    const games = gameCatalog.query({
      system: typeof system === 'string' ? system : undefined,
      tags: typeof tag === 'string' ? [tag] : undefined,
      maxPlayers: typeof maxPlayers === 'string' ? parseInt(maxPlayers, 10) : undefined,
      searchText: typeof search === 'string' ? search : undefined,
    });
    json(res, 200, games);
    return;
  }

  if (req.method === 'GET' && pathname === '/api/games/search') {
    const { q } = parsedUrl.query;
    const games = gameCatalog.query({
      searchText: typeof q === 'string' ? q : undefined,
    });
    json(res, 200, games);
    return;
  }

  // /api/games/:id  — must come after /api/games/search
  const gameIdMatch = pathname.match(/^\/api\/games\/([^/]+)$/);
  if (req.method === 'GET' && gameIdMatch) {
    const gameId = decodeURIComponent(gameIdMatch[1]);
    const game = gameCatalog.getById(gameId);
    if (!game) {
      json(res, 404, { error: 'Game not found' });
    } else {
      json(res, 200, game);
    }
    return;
  }

  // -------------------------------------------------------------------------
  // ROM scan API  (GET /api/roms/scan?dir=<path>&recursive=<bool>)
  // -------------------------------------------------------------------------

  if (req.method === 'GET' && pathname === '/api/roms/scan') {
    const { dir, recursive } = parsedUrl.query;
    if (!dir || typeof dir !== 'string') {
      json(res, 400, { error: 'Query param "dir" is required' });
      return;
    }
    const isRecursive = recursive === 'true' || recursive === '1';
    try {
      const roms = scanRomDirectory(dir, isRecursive);
      json(res, 200, roms);
    } catch (err) {
      json(res, 500, { error: `Scan failed: ${String(err)}` });
    }
    return;
  }

  // -------------------------------------------------------------------------
  // Session history API  (GET /api/sessions)
  // -------------------------------------------------------------------------

  if (req.method === 'GET' && pathname === '/api/sessions') {
    const { completed } = parsedUrl.query;
    const sessions = completed === 'true' || completed === '1'
      ? sessionHistory.getCompleted()
      : sessionHistory.getAll();
    json(res, 200, sessions);
    return;
  }

  // GET /api/sessions/:roomId
  const sessionRoomMatch = pathname.match(/^\/api\/sessions\/([^/]+)$/);
  if (req.method === 'GET' && sessionRoomMatch) {
    const roomId = decodeURIComponent(sessionRoomMatch[1]);
    const record = sessionHistory.getByRoomId(roomId);
    if (!record) {
      json(res, 404, { error: 'Session not found' });
    } else {
      json(res, 200, record);
    }
    return;
  }

  // -------------------------------------------------------------------------
  // Save-sync API
  //   POST   /api/saves/:gameId         — upload/replace save data
  //   GET    /api/saves/:gameId         — list saves for a game
  //   GET    /api/saves/:gameId/:saveId — download a specific save
  //   DELETE /api/saves/:gameId/:saveId — delete a save
  // -------------------------------------------------------------------------

  const saveGameMatch = pathname.match(/^\/api\/saves\/([^/]+)$/);
  const saveSingleMatch = pathname.match(/^\/api\/saves\/([^/]+)\/([^/]+)$/);

  if (req.method === 'POST' && saveGameMatch) {
    const gameId = decodeURIComponent(saveGameMatch[1]);
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { name, data, mimeType } = body as { name?: string; data?: string; mimeType?: string };
    if (!name || !data) {
      json(res, 400, { error: '"name" and "data" (base64) are required' });
      return;
    }
    const record = saveStore.put(gameId, name, data, typeof mimeType === 'string' ? mimeType : 'application/octet-stream');
    json(res, 201, record);
    return;
  }

  if (req.method === 'GET' && saveGameMatch) {
    const gameId = decodeURIComponent(saveGameMatch[1]);
    json(res, 200, saveStore.listForGame(gameId));
    return;
  }

  if (req.method === 'GET' && saveSingleMatch) {
    const gameId = decodeURIComponent(saveSingleMatch[1]);
    const saveId = decodeURIComponent(saveSingleMatch[2]);
    const record = saveStore.get(saveId);
    if (!record || record.gameId !== gameId) {
      json(res, 404, { error: 'Save not found' });
    } else {
      json(res, 200, record);
    }
    return;
  }

  if (req.method === 'DELETE' && saveSingleMatch) {
    const gameId = decodeURIComponent(saveSingleMatch[1]);
    const saveId = decodeURIComponent(saveSingleMatch[2]);
    const ok = saveStore.delete(saveId, gameId);
    json(res, ok ? 200 : 404, ok ? { deleted: true } : { error: 'Save not found' });
    return;
  }

  json(res, 404, { error: 'Not found' });
}

// Attach WebSocket server to the HTTP server so both share the same port.
const server = http.createServer((req, res) => {
  httpHandler(req, res).catch((err: unknown) => {
    console.error('[http] Unhandled error:', err);
    if (!res.headersSent) {
      res.writeHead(500);
      res.end('Internal Server Error');
    }
  });
});

const wss = new WebSocketServer({ server });

wss.on('connection', (ws, req) => {
  // Per-IP rate limit for new WebSocket connections
  const clientIp = getRemoteIp(req);
  if (!wsRateLimiter.allow(clientIp)) {
    console.warn(`[rate-limit] WebSocket connection rejected from ${clientIp}`);
    ws.close(1008, 'Rate limit exceeded — too many connections from your IP.');
    return;
  }
  handleConnection(ws, lobbyManager, netplayRelay, RELAY_HOST, sessionHistory);
});

server.listen(PORT, () => {
  console.log(`RetroOasis Lobby Server running on ws://localhost:${PORT}`);
  console.log(`Launch API available at http://localhost:${PORT}/api/launch`);
  console.log(`Relay host: ${RELAY_HOST} | TCP ports ${process.env.RELAY_PORT_MIN ?? 9000}-${process.env.RELAY_PORT_MAX ?? 9200}`);
});
