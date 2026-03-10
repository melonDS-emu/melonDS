import * as http from 'http';
import * as url from 'url';
import { WebSocketServer } from 'ws';
import { LobbyManager } from './lobby-manager';
import { NetplayRelay } from './netplay-relay';
import { handleConnection } from './handler';
import { EmulatorBridge, scanRomDirectory } from '@retro-oasis/emulator-bridge';
import { GameCatalog } from '@retro-oasis/game-db';

const PORT = parseInt(process.env.PORT ?? '8080', 10);
const lobbyManager = new LobbyManager();
const netplayRelay = new NetplayRelay();
const emulatorBridge = new EmulatorBridge();
const gameCatalog = new GameCatalog();
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

  // Handle CORS pre-flight
  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
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

wss.on('connection', (ws) => {
  handleConnection(ws, lobbyManager, netplayRelay);
});

server.listen(PORT, () => {
  console.log(`RetroOasis Lobby Server running on ws://localhost:${PORT}`);
  console.log(`Launch API available at http://localhost:${PORT}/api/launch`);
  console.log(`Netplay relay available on TCP ports 9000-9200`);
});
