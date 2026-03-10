import * as http from 'http';
import { WebSocketServer } from 'ws';
import { LobbyManager } from './lobby-manager';
import { NetplayRelay } from './netplay-relay';
import { handleConnection } from './handler';
import { EmulatorBridge } from '@retro-oasis/emulator-bridge';

const PORT = parseInt(process.env.PORT ?? '8080', 10);
const lobbyManager = new LobbyManager();
const netplayRelay = new NetplayRelay();
const emulatorBridge = new EmulatorBridge();

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
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
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
    } = body as {
      romPath?: string;
      system?: string;
      backendId?: string;
      saveDirectory?: string;
      playerSlot?: number;
      netplayHost?: string;
      netplayPort?: number;
      fullscreen?: boolean;
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
    });

    json(res, result.success ? 200 : 500, result);
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
