import * as http from 'http';
import * as url from 'url';
import { WebSocketServer } from 'ws';
import { LobbyManager } from './lobby-manager';
import { SqliteLobbyManager } from './sqlite-lobby-manager';
import { NetplayRelay } from './netplay-relay';
import { handleConnection, pushRetroAchievementUnlocked } from './handler';
import { EmulatorBridge, scanRomDirectory } from '@retro-oasis/emulator-bridge';
import { GameCatalog } from '@retro-oasis/game-db';
import { RateLimiter, getRemoteIp } from './rate-limiter';
import { SessionHistory } from './session-history';
import { SqliteSessionHistory } from './sqlite-session-history';
import { SaveStore } from './save-store';
import { SqliteSaveStore } from './sqlite-save-store';
import { FriendStore } from './friend-store';
import { MatchmakingQueue } from './matchmaking';
import { PlayerIdentityStore } from './player-identity';
import { getDatabase } from './db';
import { normalizeDisplayName, validateDisplayName } from './auth';
import { AchievementStore } from './achievement-store';
import { SqliteAchievementStore } from './sqlite-achievement-store';
import { computePlayerStats, computeLeaderboard } from './player-stats';
import type { LeaderboardMetric } from './player-stats';
import { TournamentStore } from './tournament-store';
import { SqliteTournamentStore } from './sqlite-tournament-store';
import { WFC_PROVIDERS } from './wfc-config';
import { FriendCodeStore, validateFriendCode } from './friend-code-store';
import { SEASONAL_EVENTS, getActiveEvents, getNextEvent } from './seasonal-events';
import { getFeaturedGames } from './featured-games';
import { MessageStore } from './message-store';
import { SqliteMessageStore } from './sqlite-message-store';
import { GameRatingsStore, SqliteGameRatingsStore } from './game-ratings';
import { activityFeed, recordReviewSubmitted } from './activity-feed';
import { RankingStore, SqliteRankingStore } from './ranking-store';
import { GlobalChatStore } from './global-chat-store';
import { NotificationStore } from './notification-store';
import { RetroAchievementStore } from './retro-achievement-store';
import { SqliteRetroAchievementStore } from './sqlite-retro-achievement-store';
import { SaveBackupStore, type BackupReason } from './save-backup-store';
import { SqliteSaveBackupStore } from './sqlite-save-backup-store';

const PORT = parseInt(process.env.PORT ?? '8080', 10);
/** Public hostname/IP players use to reach the relay (surfaced in game-starting events). */
const RELAY_HOST = process.env.RELAY_HOST ?? 'localhost';

/**
 * When DB_PATH is set, use persistent SQLite-backed implementations.
 * Otherwise fall back to the in-memory implementations for zero-config usage.
 */
const USE_SQLITE = !!process.env.DB_PATH;

let lobbyManager: LobbyManager | SqliteLobbyManager;
let sessionHistory: SessionHistory | SqliteSessionHistory;
let saveStore: SaveStore | SqliteSaveStore;
let friendStore: FriendStore | undefined;
let matchmakingQueue: MatchmakingQueue | undefined;
let playerIdentityStore: PlayerIdentityStore | undefined;

if (USE_SQLITE) {
  const db = getDatabase();
  lobbyManager = new SqliteLobbyManager(db);
  sessionHistory = new SqliteSessionHistory(db);
  saveStore = new SqliteSaveStore(db);
  friendStore = new FriendStore(db);
  matchmakingQueue = new MatchmakingQueue(db);
  playerIdentityStore = new PlayerIdentityStore(db);
  console.log(`[db] SQLite persistence enabled — ${process.env.DB_PATH}`);
} else {
  lobbyManager = new LobbyManager();
  sessionHistory = new SessionHistory();
  saveStore = new SaveStore();
  console.log('[db] Using in-memory stores (set DB_PATH for persistence)');
}

const netplayRelay = new NetplayRelay();
const emulatorBridge = new EmulatorBridge();
const gameCatalog = new GameCatalog();
const friendCodeStore = new FriendCodeStore();
const achievementStore: AchievementStore | SqliteAchievementStore = USE_SQLITE
  ? new SqliteAchievementStore(getDatabase())
  : new AchievementStore();
const tournamentStore: TournamentStore | SqliteTournamentStore = USE_SQLITE
  ? new SqliteTournamentStore(getDatabase())
  : new TournamentStore();
const messageStore: MessageStore | SqliteMessageStore = USE_SQLITE
  ? (() => { const s = new SqliteMessageStore(getDatabase()); s.hydrate(); return s; })()
  : new MessageStore();
const gameRatingsStore: GameRatingsStore | SqliteGameRatingsStore = USE_SQLITE
  ? new SqliteGameRatingsStore(getDatabase())
  : new GameRatingsStore();
const rankingStore: RankingStore | SqliteRankingStore = USE_SQLITE
  ? new SqliteRankingStore(getDatabase())
  : new RankingStore();

const globalChatStore = new GlobalChatStore();
const notificationStore = new NotificationStore();
const retroAchievementStore: RetroAchievementStore | SqliteRetroAchievementStore = USE_SQLITE
  ? new SqliteRetroAchievementStore(getDatabase())
  : new RetroAchievementStore();
const saveBackupStore: SaveBackupStore | SqliteSaveBackupStore = USE_SQLITE
  ? new SqliteSaveBackupStore(getDatabase())
  : new SaveBackupStore();

/** Lazy reference to the WebSocketServer — set after server creation. */
let wss: WebSocketServer | undefined;

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
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, DELETE, OPTIONS');
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

  // GET /api/sessions/stats/most-played  — Phase 8 enhanced stats
  if (req.method === 'GET' && pathname === '/api/sessions/stats/most-played') {
    if (!(sessionHistory instanceof SqliteSessionHistory)) {
      json(res, 501, { error: 'Statistics require SQLite persistence. Set DB_PATH to enable.' });
      return;
    }
    const { limit } = parsedUrl.query;
    const games = sessionHistory.getMostPlayedGames(typeof limit === 'string' ? parseInt(limit, 10) : 10);
    json(res, 200, games);
    return;
  }

  // GET /api/sessions/stats/total-time  — Phase 8 enhanced stats
  if (req.method === 'GET' && pathname === '/api/sessions/stats/total-time') {
    if (!(sessionHistory instanceof SqliteSessionHistory)) {
      json(res, 501, { error: 'Statistics require SQLite persistence. Set DB_PATH to enable.' });
      return;
    }
    json(res, 200, { totalTimeSecs: sessionHistory.getTotalPlayTimeSecs() });
    return;
  }

  // GET /api/sessions/stats/player/:displayName  — Phase 8 player stats
  const playerStatsMatch = pathname.match(/^\/api\/sessions\/stats\/player\/([^/]+)$/);
  if (req.method === 'GET' && playerStatsMatch) {
    if (!(sessionHistory instanceof SqliteSessionHistory)) {
      json(res, 501, { error: 'Statistics require SQLite persistence. Set DB_PATH to enable.' });
      return;
    }
    const displayName = decodeURIComponent(playerStatsMatch[1]);
    json(res, 200, sessionHistory.getPlayerStats(displayName));
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

  // -------------------------------------------------------------------------
  // Phase 4: Save backup API
  //   POST   /api/saves/:gameId/backup                        — create backup
  //   GET    /api/saves/:gameId/backups                       — list backups
  //   DELETE /api/saves/:gameId/backups/:backupId             — delete backup
  //   POST   /api/saves/:gameId/backups/:backupId/restore     — restore backup → save slot
  //   POST   /api/saves/:gameId/backups/:backupId/mark-lkg    — mark last-known-good
  //   GET    /api/saves/:gameId/last-known-good?name=<slot>   — get last-known-good backup
  //   POST   /api/saves/:gameId/session-start                 — pre-session backup
  //   POST   /api/saves/:gameId/session-end                   — post-session sync
  // -------------------------------------------------------------------------

  const backupBaseMatch = pathname.match(/^\/api\/saves\/([^/]+)\/backup$/);
  const backupListMatch = pathname.match(/^\/api\/saves\/([^/]+)\/backups$/);
  const backupSingleMatch = pathname.match(/^\/api\/saves\/([^/]+)\/backups\/([^/]+)$/);
  const backupRestoreMatch = pathname.match(/^\/api\/saves\/([^/]+)\/backups\/([^/]+)\/restore$/);
  const backupLkgActionMatch = pathname.match(/^\/api\/saves\/([^/]+)\/backups\/([^/]+)\/mark-lkg$/);
  const lkgQueryMatch = pathname.match(/^\/api\/saves\/([^/]+)\/last-known-good$/);
  const sessionStartMatch = pathname.match(/^\/api\/saves\/([^/]+)\/session-start$/);
  const sessionEndMatch = pathname.match(/^\/api\/saves\/([^/]+)\/session-end$/);

  // POST /api/saves/:gameId/backup — create a manual backup
  if (req.method === 'POST' && backupBaseMatch) {
    const gameId = decodeURIComponent(backupBaseMatch[1]);
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { saveName, data, mimeType, reason, saveVersion } = body as {
      saveName?: string; data?: string; mimeType?: string; reason?: string; saveVersion?: number;
    };
    if (!saveName || !data) {
      json(res, 400, { error: '"saveName" and "data" (base64) are required' });
      return;
    }
    const backup = saveBackupStore.createBackup(gameId, saveName, data, {
      reason: (reason as BackupReason) ?? 'manual',
      mimeType: typeof mimeType === 'string' ? mimeType : undefined,
      saveVersion: typeof saveVersion === 'number' ? saveVersion : undefined,
    });
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    const { data: _d, ...meta } = backup;
    json(res, 201, meta);
    return;
  }

  // GET /api/saves/:gameId/backups — list backups for a game
  if (req.method === 'GET' && backupListMatch) {
    const gameId = decodeURIComponent(backupListMatch[1]);
    json(res, 200, saveBackupStore.listForGame(gameId));
    return;
  }

  // DELETE /api/saves/:gameId/backups/:backupId
  if (req.method === 'DELETE' && backupSingleMatch && !backupRestoreMatch && !backupLkgActionMatch) {
    const backupId = decodeURIComponent(backupSingleMatch[2]);
    const ok = saveBackupStore.delete(backupId);
    json(res, ok ? 200 : 404, ok ? { deleted: true } : { error: 'Backup not found' });
    return;
  }

  // POST /api/saves/:gameId/backups/:backupId/restore — restore backup into save slot
  if (req.method === 'POST' && backupRestoreMatch) {
    const gameId = decodeURIComponent(backupRestoreMatch[1]);
    const backupId = decodeURIComponent(backupRestoreMatch[2]);
    const backup = saveBackupStore.get(backupId);
    if (!backup || backup.gameId !== gameId) {
      json(res, 404, { error: 'Backup not found' });
      return;
    }
    // Write backup data back into the save store (this is the restore operation)
    const restored = saveStore.put(gameId, backup.saveName, backup.data, backup.mimeType);
    json(res, 200, restored);
    return;
  }

  // POST /api/saves/:gameId/backups/:backupId/mark-lkg — mark as last-known-good
  if (req.method === 'POST' && backupLkgActionMatch) {
    const gameId = decodeURIComponent(backupLkgActionMatch[1]);
    const backupId = decodeURIComponent(backupLkgActionMatch[2]);
    const backup = saveBackupStore.get(backupId);
    if (!backup || backup.gameId !== gameId) {
      json(res, 404, { error: 'Backup not found' });
      return;
    }
    const ok = saveBackupStore.markAsLastKnownGood(backupId);
    json(res, ok ? 200 : 500, ok ? { marked: true } : { error: 'Failed to mark backup' });
    return;
  }

  // GET /api/saves/:gameId/last-known-good?name=<saveName>
  if (req.method === 'GET' && lkgQueryMatch) {
    const gameId = decodeURIComponent(lkgQueryMatch[1]);
    const { name } = parsedUrl.query;
    if (!name || typeof name !== 'string') {
      json(res, 400, { error: 'Query param "name" (save slot name) is required' });
      return;
    }
    const lkg = saveBackupStore.getLastKnownGood(gameId, name);
    if (!lkg) {
      json(res, 404, { error: 'No last-known-good backup found' });
    } else {
      // eslint-disable-next-line @typescript-eslint/no-unused-vars
      const { data: _d, ...meta } = lkg;
      json(res, 200, meta);
    }
    return;
  }

  // POST /api/saves/:gameId/session-start — pre-session backup
  if (req.method === 'POST' && sessionStartMatch) {
    const gameId = decodeURIComponent(sessionStartMatch[1]);
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { slots } = body as {
      slots?: Array<{ saveName: string; data: string; mimeType?: string; version?: number }>;
    };
    if (!Array.isArray(slots) || slots.length === 0) {
      json(res, 400, { error: '"slots" array is required and must not be empty' });
      return;
    }
    const backups = saveBackupStore.preSessionBackup(gameId, slots);
    json(res, 201, { backups });
    return;
  }

  // POST /api/saves/:gameId/session-end — post-session sync
  if (req.method === 'POST' && sessionEndMatch) {
    const gameId = decodeURIComponent(sessionEndMatch[1]);
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { slots, cleanExit } = body as {
      slots?: Array<{ saveName: string; data: string; mimeType?: string; version?: number }>;
      cleanExit?: boolean;
    };
    if (!Array.isArray(slots) || slots.length === 0) {
      json(res, 400, { error: '"slots" array is required and must not be empty' });
      return;
    }
    // Also update the canonical save store with the latest data
    for (const slot of slots) {
      saveStore.put(gameId, slot.saveName, slot.data, slot.mimeType ?? 'application/octet-stream');
    }
    const backups = saveBackupStore.postSessionSync(gameId, slots, cleanExit === true);
    json(res, 200, { synced: true, backups });
    return;
  }

  // -------------------------------------------------------------------------
  // Phase 8: Friend list API
  //   GET    /api/friends?userId=<id>                   — list friends
  //   POST   /api/friends/add                           — add friend / send request
  //   DELETE /api/friends/:friendId?userId=<id>         — remove friend
  //   POST   /api/friends/requests/:requestId/accept    — accept request
  //   POST   /api/friends/requests/:requestId/decline   — decline request
  //   GET    /api/friends/requests?userId=<id>          — list incoming requests
  // -------------------------------------------------------------------------

  if (friendStore) {
    if (req.method === 'GET' && pathname === '/api/friends') {
      const { userId } = parsedUrl.query;
      if (!userId || typeof userId !== 'string') {
        json(res, 400, { error: 'Query param "userId" is required' });
        return;
      }
      json(res, 200, friendStore.getFriends(userId));
      return;
    }

    if (req.method === 'POST' && pathname === '/api/friends/add') {
      let body: Record<string, unknown>;
      try {
        const raw = await readBody(req);
        body = JSON.parse(raw) as Record<string, unknown>;
      } catch {
        json(res, 400, { error: 'Invalid JSON body' });
        return;
      }
      const { userId, friendId } = body as { userId?: string; friendId?: string };
      if (!userId || !friendId) {
        json(res, 400, { error: '"userId" and "friendId" are required' });
        return;
      }
      const request = friendStore.addRequest(userId, friendId);
      if (!request) {
        json(res, 409, { error: 'Already friends or request exists.' });
        return;
      }
      json(res, 201, request);
      return;
    }

    const friendDeleteMatch = pathname.match(/^\/api\/friends\/([^/]+)$/);
    if (req.method === 'DELETE' && friendDeleteMatch) {
      const friendId = decodeURIComponent(friendDeleteMatch[1]);
      const { userId } = parsedUrl.query;
      if (!userId || typeof userId !== 'string') {
        json(res, 400, { error: 'Query param "userId" is required' });
        return;
      }
      const ok = friendStore.removeFriend(userId, friendId);
      json(res, ok ? 200 : 404, ok ? { removed: true } : { error: 'Friendship not found' });
      return;
    }

    const requestActionMatch = pathname.match(/^\/api\/friends\/requests\/([^/]+)\/(accept|decline)$/);
    if (req.method === 'POST' && requestActionMatch) {
      const requestId = decodeURIComponent(requestActionMatch[1]);
      const action = requestActionMatch[2];
      let body: Record<string, unknown>;
      try {
        const raw = await readBody(req);
        body = JSON.parse(raw) as Record<string, unknown>;
      } catch {
        json(res, 400, { error: 'Invalid JSON body' });
        return;
      }
      const { userId } = body as { userId?: string };
      if (!userId) {
        json(res, 400, { error: '"userId" is required' });
        return;
      }
      const result = action === 'accept'
        ? friendStore.acceptRequest(requestId, userId)
        : friendStore.declineRequest(requestId, userId);
      if (!result) {
        json(res, 404, { error: 'Friend request not found or action not permitted.' });
        return;
      }
      json(res, 200, result);
      return;
    }

    if (req.method === 'GET' && pathname === '/api/friends/requests') {
      const { userId } = parsedUrl.query;
      if (!userId || typeof userId !== 'string') {
        json(res, 400, { error: 'Query param "userId" is required' });
        return;
      }
      json(res, 200, friendStore.getPendingRequests(userId));
      return;
    }
  }

  // -------------------------------------------------------------------------
  // Phase 8: Matchmaking API
  //   POST   /api/matchmaking/join   — join queue
  //   DELETE /api/matchmaking/leave  — leave queue
  //   GET    /api/matchmaking        — list queue (admin/debug)
  // -------------------------------------------------------------------------

  if (matchmakingQueue) {
    if (req.method === 'POST' && pathname === '/api/matchmaking/join') {
      let body: Record<string, unknown>;
      try {
        const raw = await readBody(req);
        body = JSON.parse(raw) as Record<string, unknown>;
      } catch {
        json(res, 400, { error: 'Invalid JSON body' });
        return;
      }
      const { playerId, displayName, gameId, gameTitle, system, maxPlayers } =
        body as {
          playerId?: string;
          displayName?: string;
          gameId?: string;
          gameTitle?: string;
          system?: string;
          maxPlayers?: number;
        };
      if (!playerId || !displayName || !gameId || !gameTitle || !system || !maxPlayers) {
        json(res, 400, { error: 'playerId, displayName, gameId, gameTitle, system, maxPlayers are required' });
        return;
      }
      const normalizedName = normalizeDisplayName(displayName);
      const nameError = validateDisplayName(normalizedName);
      if (nameError) {
        json(res, 400, { error: nameError });
        return;
      }
      const entry = matchmakingQueue.join({ playerId, displayName: normalizedName, gameId, gameTitle, system, maxPlayers });
      const all = matchmakingQueue.getAll();
      const position = all.findIndex((e) => e.playerId === playerId) + 1;
      json(res, 200, { entry, position });
      return;
    }

    if (req.method === 'DELETE' && pathname === '/api/matchmaking/leave') {
      const { playerId } = parsedUrl.query;
      if (!playerId || typeof playerId !== 'string') {
        json(res, 400, { error: 'Query param "playerId" is required' });
        return;
      }
      const ok = matchmakingQueue.leave(playerId);
      json(res, ok ? 200 : 404, ok ? { removed: true } : { error: 'Player not in queue' });
      return;
    }

    if (req.method === 'GET' && pathname === '/api/matchmaking') {
      json(res, 200, matchmakingQueue.getAll());
      return;
    }
  }

  // -------------------------------------------------------------------------
  // Phase 8: Player identity API
  //   POST /api/identity   — register or look up a persistent identity
  //   GET  /api/identity/:playerId  — get identity by stable player ID
  // -------------------------------------------------------------------------

  if (playerIdentityStore) {
    if (req.method === 'POST' && pathname === '/api/identity') {
      let body: Record<string, unknown>;
      try {
        const raw = await readBody(req);
        body = JSON.parse(raw) as Record<string, unknown>;
      } catch {
        json(res, 400, { error: 'Invalid JSON body' });
        return;
      }
      const { identityToken, displayName, updateName } =
        body as { identityToken?: string; displayName?: string; updateName?: boolean };
      if (!identityToken || !displayName) {
        json(res, 400, { error: '"identityToken" and "displayName" are required' });
        return;
      }
      const normalizedName = normalizeDisplayName(displayName);
      const nameError = validateDisplayName(normalizedName);
      if (nameError) {
        json(res, 400, { error: nameError });
        return;
      }
      const identity = playerIdentityStore.resolve(identityToken, normalizedName, updateName === true);
      json(res, 200, identity);
      return;
    }

    const identityByIdMatch = pathname.match(/^\/api\/identity\/([^/]+)$/);
    if (req.method === 'GET' && identityByIdMatch) {
      const playerId = decodeURIComponent(identityByIdMatch[1]);
      const identity = playerIdentityStore.getById(playerId);
      if (!identity) {
        json(res, 404, { error: 'Player not found' });
      } else {
        json(res, 200, identity);
      }
      return;
    }
  }

  // ─── Achievements API ────────────────────────────────────────────────────
  //   GET /api/achievements            — all definitions
  //   GET /api/achievements/:playerId  — earned achievements for a player
  // ─────────────────────────────────────────────────────────────────────────
  if (pathname === '/api/achievements' && req.method === 'GET') {
    json(res, 200, achievementStore.getDefinitions());
    return;
  }

  const achievementsPlayerMatch = pathname.match(/^\/api\/achievements\/([^/]+)$/);
  if (achievementsPlayerMatch && req.method === 'GET') {
    const playerId = decodeURIComponent(achievementsPlayerMatch[1]);
    const earned = achievementStore.getEarned(playerId);
    json(res, 200, { playerId, earned });
    return;
  }

  // POST /api/achievements/:playerId/refresh — re-evaluate achievements against live session history
  const achievementsRefreshMatch = pathname.match(/^\/api\/achievements\/([^/]+)\/refresh$/);
  if (achievementsRefreshMatch && req.method === 'POST') {
    const playerId = decodeURIComponent(achievementsRefreshMatch[1]);
    const allSessions = sessionHistory.getAll();
    const playerSessions = allSessions.filter((s) => s.players.includes(playerId));
    const newlyUnlocked = achievementStore.checkAndUnlock(playerId, playerId, playerSessions);
    const earned = achievementStore.getEarned(playerId);
    json(res, 200, { playerId, earned, newlyUnlocked: newlyUnlocked.map((d) => d.id) });
    return;
  }

  // ─── Player Stats API ────────────────────────────────────────────────────
  //   GET /api/stats/:playerId                — stats for a specific player
  //   GET /api/leaderboard?metric=sessions    — global leaderboard
  // ─────────────────────────────────────────────────────────────────────────
  const statsPlayerMatch = pathname.match(/^\/api\/stats\/([^/]+)$/);
  if (statsPlayerMatch && req.method === 'GET') {
    const displayName = decodeURIComponent(statsPlayerMatch[1]);
    const allSessions = sessionHistory.getAll();
    const playerSessions = allSessions.filter((s) =>
      s.players.includes(displayName)
    );
    const stats = computePlayerStats(displayName, displayName, playerSessions);
    json(res, 200, stats);
    return;
  }

  if (pathname === '/api/leaderboard' && req.method === 'GET') {
    const metric = (typeof parsedUrl.query.metric === 'string' ? parsedUrl.query.metric : 'sessions') as LeaderboardMetric;
    const limitRaw = typeof parsedUrl.query.limit === 'string' ? parsedUrl.query.limit : '10';
    const limit = Math.min(parseInt(limitRaw, 10), 50);
    const allSessions = sessionHistory.getAll();

    // Collect unique display names from session history
    const allNames = new Set<string>();
    for (const s of allSessions) {
      for (const name of s.players) allNames.add(name);
    }

    const allStats = [...allNames].map((name) => {
      const playerSessions = allSessions.filter((s) => s.players.includes(name));
      return computePlayerStats(name, name, playerSessions);
    });

    const leaderboard = computeLeaderboard(allStats, metric, limit);
    json(res, 200, leaderboard);
    return;
  }

  // ─── Tournaments API ─────────────────────────────────────────────────────
  //   GET  /api/tournaments              — list all tournaments
  //   POST /api/tournaments              — create a new tournament
  //   GET  /api/tournaments/:id          — get tournament details
  //   POST /api/tournaments/:id/matches/:matchId/result  — record match result
  // ─────────────────────────────────────────────────────────────────────────
  if (pathname === '/api/tournaments' && req.method === 'GET') {
    json(res, 200, tournamentStore.getAll());
    return;
  }

  if (pathname === '/api/tournaments' && req.method === 'POST') {
    const body = await readBody(req);
    let params: { name?: unknown; gameId?: unknown; gameTitle?: unknown; system?: unknown; players?: unknown };
    try {
      params = JSON.parse(body);
    } catch {
      json(res, 400, { error: 'Invalid JSON body.' });
      return;
    }
    if (
      typeof params.name !== 'string' ||
      typeof params.gameId !== 'string' ||
      typeof params.gameTitle !== 'string' ||
      typeof params.system !== 'string' ||
      !Array.isArray(params.players) ||
      (params.players as unknown[]).length < 2
    ) {
      json(res, 400, { error: 'Required: name, gameId, gameTitle, system, players (min 2).' });
      return;
    }
    try {
      const tournament = tournamentStore.create({
        name: params.name as string,
        gameId: params.gameId as string,
        gameTitle: params.gameTitle as string,
        system: params.system as string,
        players: (params.players as unknown[]).map(String),
      });
      json(res, 201, tournament);
    } catch (err: unknown) {
      json(res, 400, { error: (err as Error).message });
    }
    return;
  }

  const tournamentIdMatch = pathname.match(/^\/api\/tournaments\/([^/]+)$/);
  if (tournamentIdMatch && req.method === 'GET') {
    const tournament = tournamentStore.get(decodeURIComponent(tournamentIdMatch[1]));
    if (!tournament) {
      json(res, 404, { error: 'Tournament not found.' });
      return;
    }
    json(res, 200, tournament);
    return;
  }

  const tournamentResultMatch = pathname.match(
    /^\/api\/tournaments\/([^/]+)\/matches\/([^/]+)\/result$/
  );
  if (tournamentResultMatch && req.method === 'POST') {
    const tournamentId = decodeURIComponent(tournamentResultMatch[1]);
    const matchId = decodeURIComponent(tournamentResultMatch[2]);
    const body = await readBody(req);
    let payload: { winner?: unknown };
    try {
      payload = JSON.parse(body);
    } catch {
      json(res, 400, { error: 'Invalid JSON body.' });
      return;
    }
    if (typeof payload.winner !== 'string') {
      json(res, 400, { error: 'Required: winner (display name string).' });
      return;
    }
    try {
      const updated = tournamentStore.recordResult(tournamentId, matchId, payload.winner as string);

      // Broadcast tournament-updated to all connected WebSocket clients
      if (wss) {
        const msg = JSON.stringify({ type: 'tournament-updated', tournamentId });
        wss.clients.forEach((client) => {
          if (client.readyState === client.OPEN) client.send(msg);
        });
      }

      // Check tournament winner achievements
      if (updated && updated.status === 'completed' && updated.winner) {
        const winnerName = updated.winner;
        const allTournaments = tournamentStore.getAll();
        const wins = allTournaments.filter(
          (t) => t.status === 'completed' && t.winner === winnerName
        ).length;
        const matchWins = allTournaments
          .flatMap((t) => t.matches)
          .filter((m) => m.winner === winnerName).length;
        const newAchievements = achievementStore.checkTournamentAchievements(
          winnerName,
          winnerName,
          wins,
          matchWins
        );
        // Push achievement-unlocked WS messages to the winner if connected
        if (wss && newAchievements.length > 0) {
          for (const ach of newAchievements) {
            const achMsg = JSON.stringify({
              type: 'achievement-unlocked',
              achievementId: ach.id,
              name: ach.name,
              description: ach.description,
              icon: ach.icon,
            });
            // Best-effort — broadcast to all; the client filters by its own display name
            wss.clients.forEach((client) => {
              if (client.readyState === client.OPEN) client.send(achMsg);
            });
          }
        }
      }

      json(res, 200, updated);
    } catch (err: unknown) {
      json(res, 400, { error: (err as Error).message });
    }
    return;
  }

  // GET /api/tournaments/player/:displayName — player tournament history
  // (only available when using SqliteTournamentStore)
  const tournamentPlayerMatch = pathname.match(/^\/api\/tournaments\/player\/([^/]+)$/);
  if (tournamentPlayerMatch && req.method === 'GET') {
    const displayName = decodeURIComponent(tournamentPlayerMatch[1]);
    if (!USE_SQLITE || !('getPlayerTournaments' in tournamentStore)) {
      json(res, 501, { error: 'Tournament history requires SQLite persistence. Set DB_PATH to enable.' });
      return;
    }
    const sqliteStore = tournamentStore as import('./sqlite-tournament-store').SqliteTournamentStore;
    const tournaments = sqliteStore.getPlayerTournaments(displayName);
    const result = tournaments.map((t) => ({
      id: t.id,
      name: t.name,
      gameTitle: t.gameTitle,
      status: t.status,
      winner: t.winner,
      playerCount: t.players.length,
      createdAt: t.createdAt,
    }));
    json(res, 200, result);
    return;
  }

  // ─── WFC Provider API ────────────────────────────────────────────────────
  //   GET /api/wfc/providers        — list all WFC providers
  //   GET /api/wfc/providers/:id    — get a specific provider
  // ─────────────────────────────────────────────────────────────────────────
  if (pathname === '/api/wfc/providers' && req.method === 'GET') {
    json(res, 200, WFC_PROVIDERS);
    return;
  }

  const wfcProviderMatch = pathname.match(/^\/api\/wfc\/providers\/([^/]+)$/);
  if (wfcProviderMatch && req.method === 'GET') {
    const providerId = decodeURIComponent(wfcProviderMatch[1]);
    const provider = WFC_PROVIDERS.find((p) => p.id === providerId);
    if (!provider) {
      json(res, 404, { error: 'WFC provider not found' });
      return;
    }
    json(res, 200, provider);
    return;
  }

  // ─── Friend Codes API ─────────────────────────────────────────────────────
  //   GET    /api/friend-codes?gameId=<id>          — all codes for a game
  //   GET    /api/friend-codes?player=<name>        — all codes for a player
  //   POST   /api/friend-codes                      — register/update a code
  //   DELETE /api/friend-codes?player=<n>&gameId=<g> — remove a code
  // ─────────────────────────────────────────────────────────────────────────
  if (pathname === '/api/friend-codes') {
    if (req.method === 'GET') {
      const { gameId, player } = parsedUrl.query;
      if (typeof gameId === 'string') {
        json(res, 200, friendCodeStore.getByGame(gameId));
      } else if (typeof player === 'string') {
        json(res, 200, friendCodeStore.getByPlayer(player));
      } else {
        json(res, 200, friendCodeStore.getAll());
      }
      return;
    }

    if (req.method === 'POST') {
      let body: Record<string, unknown>;
      try {
        const raw = await readBody(req);
        body = JSON.parse(raw) as Record<string, unknown>;
      } catch {
        json(res, 400, { error: 'Invalid JSON body' });
        return;
      }
      const { displayName, gameId, gameTitle, code } = body as {
        displayName?: string;
        gameId?: string;
        gameTitle?: string;
        code?: string;
      };
      if (!displayName || !gameId || !gameTitle || !code) {
        json(res, 400, { error: '"displayName", "gameId", "gameTitle", and "code" are required' });
        return;
      }
      const codeError = validateFriendCode(code);
      if (codeError) {
        json(res, 400, { error: codeError });
        return;
      }
      const entry = friendCodeStore.put(displayName, gameId, gameTitle, code);
      json(res, 201, entry);
      return;
    }

    if (req.method === 'DELETE') {
      const { player, gameId } = parsedUrl.query;
      if (!player || typeof player !== 'string' || !gameId || typeof gameId !== 'string') {
        json(res, 400, { error: 'Query params "player" and "gameId" are required' });
        return;
      }
      const ok = friendCodeStore.delete(player, gameId);
      json(res, ok ? 200 : 404, ok ? { deleted: true } : { error: 'Friend code not found' });
      return;
    }
  }

  // ---------------------------------------------------------------------------
  // Seasonal Events & Featured Games API  (Phase 13)
  //   GET /api/events              — all seasonal events
  //   GET /api/events/current      — events active today
  //   GET /api/games/featured      — featured games for the current week
  // ---------------------------------------------------------------------------

  if (req.method === 'GET' && pathname === '/api/events') {
    const next = getNextEvent();
    json(res, 200, { events: SEASONAL_EVENTS, nextEvent: next ?? null });
    return;
  }

  if (req.method === 'GET' && pathname === '/api/events/current') {
    const active = getActiveEvents();
    const next = getNextEvent();
    json(res, 200, { active, nextEvent: next ?? null });
    return;
  }

  if (req.method === 'GET' && pathname === '/api/games/featured') {
    json(res, 200, { featured: getFeaturedGames() });
    return;
  }

  // ---------------------------------------------------------------------------
  // Direct Messages API  (Phase 14)
  //   GET  /api/messages/:player               — recent conversations for player
  //   GET  /api/messages/:player1/:player2     — conversation between two players
  //   POST /api/messages/send                  — send a DM
  //   POST /api/messages/read                  — mark conversation as read
  //   GET  /api/messages/:player/unread-count  — total unread count
  // ---------------------------------------------------------------------------

  const dmMatch = pathname.match(/^\/api\/messages\/([^/]+)(?:\/(.+))?$/);
  if (dmMatch) {
    const p1 = decodeURIComponent(dmMatch[1]);
    const p2Segment = dmMatch[2] ? decodeURIComponent(dmMatch[2]) : null;

    if (req.method === 'GET' && p2Segment === 'unread-count') {
      json(res, 200, { player: p1, unreadCount: messageStore.getUnreadCount(p1) });
      return;
    }

    if (req.method === 'GET' && p2Segment) {
      const msgs = messageStore.getConversation(p1, p2Segment);
      json(res, 200, { messages: msgs });
      return;
    }

    if (req.method === 'GET' && !p2Segment) {
      const convos = messageStore.getRecentConversations(p1);
      json(res, 200, { conversations: convos });
      return;
    }
  }

  if (req.method === 'POST' && pathname === '/api/messages/send') {
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { fromPlayer, toPlayer, content } = body as { fromPlayer?: string; toPlayer?: string; content?: string };
    if (!fromPlayer || !toPlayer || !content) {
      json(res, 400, { error: 'fromPlayer, toPlayer and content are required' });
      return;
    }
    const dm = messageStore.sendMessage(fromPlayer, toPlayer, content);
    json(res, 201, { message: dm });
    return;
  }

  if (req.method === 'POST' && pathname === '/api/messages/read') {
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { fromPlayer, toPlayer } = body as { fromPlayer?: string; toPlayer?: string };
    if (!fromPlayer || !toPlayer) {
      json(res, 400, { error: 'fromPlayer and toPlayer are required' });
      return;
    }
    messageStore.markRead(fromPlayer, toPlayer);
    json(res, 200, { ok: true });
    return;
  }

  // ---------------------------------------------------------------------------
  // Game Reviews & Ratings API  (Phase 15)
  //   GET    /api/reviews/:gameId            — reviews for a game
  //   POST   /api/reviews/:gameId            — submit/update a rating+review
  //   DELETE /api/reviews/:reviewId          — delete own review
  //   GET    /api/reviews/player/:playerId   — reviews by a player
  //   GET    /api/reviews/top                — top-rated games
  //   GET    /api/reviews/:gameId/summary    — aggregated summary for a game
  // ---------------------------------------------------------------------------

  if (pathname === '/api/reviews/top' && req.method === 'GET') {
    const limit = parseInt(parsedUrl.query.limit as string ?? '10', 10);
    json(res, 200, { games: gameRatingsStore.getTopRatedGames(limit) });
    return;
  }

  const reviewsPlayerMatch = pathname.match(/^\/api\/reviews\/player\/([^/]+)$/);
  if (reviewsPlayerMatch && req.method === 'GET') {
    const playerId = decodeURIComponent(reviewsPlayerMatch[1]);
    json(res, 200, { reviews: gameRatingsStore.getReviewsByPlayer(playerId) });
    return;
  }

  const reviewsGameSummaryMatch = pathname.match(/^\/api\/reviews\/([^/]+)\/summary$/);
  if (reviewsGameSummaryMatch && req.method === 'GET') {
    const gameId = decodeURIComponent(reviewsGameSummaryMatch[1]);
    const summary = gameRatingsStore.getSummaryForGame(gameId);
    if (!summary) { json(res, 404, { error: 'No reviews for this game' }); return; }
    json(res, 200, summary);
    return;
  }

  const reviewsDeleteMatch = pathname.match(/^\/api\/reviews\/([^/]+)$/) ;
  if (reviewsDeleteMatch && req.method === 'DELETE') {
    const reviewId = decodeURIComponent(reviewsDeleteMatch[1]);
    const raw = await readBody(req);
    const body = JSON.parse(raw.trim() || '{}') as { playerId?: string };
    if (!body.playerId) { json(res, 400, { error: 'playerId required' }); return; }
    const ok = gameRatingsStore.deleteReview(reviewId, body.playerId);
    json(res, ok ? 200 : 403, { ok });
    return;
  }

  const reviewsGameMatch = pathname.match(/^\/api\/reviews\/([^/]+)$/);
  if (reviewsGameMatch) {
    const gameId = decodeURIComponent(reviewsGameMatch[1]);
    if (req.method === 'GET') {
      json(res, 200, { reviews: gameRatingsStore.getReviewsForGame(gameId) });
      return;
    }
    if (req.method === 'POST') {
      const raw = await readBody(req);
      const body = JSON.parse(raw) as { gameTitle?: string; playerId?: string; playerName?: string; rating?: number; text?: string };
      const { gameTitle, playerId, playerName, rating, text } = body;
      if (!gameTitle || !playerId || !playerName || rating == null) {
        json(res, 400, { error: 'gameTitle, playerId, playerName and rating are required' });
        return;
      }
      try {
        const review = gameRatingsStore.upsertReview(gameId, gameTitle, playerId, playerName, rating, text);
        recordReviewSubmitted(playerName, gameTitle, rating);
        json(res, 201, { review });
      } catch (err: unknown) {
        json(res, 400, { error: err instanceof Error ? err.message : 'Invalid rating' });
      }
      return;
    }
  }

  // ---------------------------------------------------------------------------
  // Community Activity Feed API  (Phase 15)
  //   GET /api/activity              — recent community activity (last 50)
  //   GET /api/activity?type=X       — filter by event type
  //   GET /api/activity?player=X     — filter by player name
  //   GET /api/activity?limit=N      — override limit (max 200)
  // ---------------------------------------------------------------------------

  if (pathname === '/api/activity' && req.method === 'GET') {
    const limit = Math.min(parseInt(parsedUrl.query.limit as string ?? '50', 10), 200);
    const type = parsedUrl.query.type as string | undefined;
    const player = parsedUrl.query.player as string | undefined;
    const events = activityFeed.getRecent(limit, {
      type: type as NonNullable<Parameters<typeof activityFeed.getRecent>[1]>['type'],
      playerName: player,
    });
    json(res, 200, { events, total: activityFeed.count() });
    return;
  }

  // ---------------------------------------------------------------------------
  // Rankings API  (Phase 15)
  //   GET /api/rankings                       — global ELO leaderboard
  //   GET /api/rankings/:gameId               — per-game ELO leaderboard
  //   GET /api/rankings/player/:playerId      — global rank for a player
  //   GET /api/rankings/player/:playerId/:gameId — per-game rank for a player
  //   POST /api/rankings/match                — record a ranked match result
  // ---------------------------------------------------------------------------

  if (pathname === '/api/rankings' && req.method === 'GET') {
    const limit = parseInt(parsedUrl.query.limit as string ?? '20', 10);
    json(res, 200, { rankings: rankingStore.getGlobalLeaderboard(limit) });
    return;
  }

  if (req.method === 'POST' && pathname === '/api/rankings/match') {
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { playerAId, playerAName, playerBId, playerBName, gameId, gameTitle, outcome } = body as {
      playerAId?: string; playerAName?: string;
      playerBId?: string; playerBName?: string;
      gameId?: string; gameTitle?: string;
      outcome?: 1 | 0 | -1;
    };
    if (!playerAId || !playerAName || !playerBId || !playerBName || !gameId || !gameTitle || outcome == null) {
      json(res, 400, { error: 'playerAId, playerAName, playerBId, playerBName, gameId, gameTitle and outcome are required' });
      return;
    }
    rankingStore.recordMatch(playerAId, playerAName, playerBId, playerBName, gameId, gameTitle, outcome);
    json(res, 200, { ok: true });
    return;
  }

  const rankingsPlayerGameMatch = pathname.match(/^\/api\/rankings\/player\/([^/]+)\/([^/]+)$/);
  if (rankingsPlayerGameMatch && req.method === 'GET') {
    const playerId = decodeURIComponent(rankingsPlayerGameMatch[1]);
    const gameId = decodeURIComponent(rankingsPlayerGameMatch[2]);
    const rank = rankingStore.getPlayerGameRank(playerId, gameId);
    if (!rank) { json(res, 404, { error: 'No ranked games found for this player and game' }); return; }
    json(res, 200, rank);
    return;
  }

  const rankingsPlayerMatch = pathname.match(/^\/api\/rankings\/player\/([^/]+)$/);
  if (rankingsPlayerMatch && req.method === 'GET') {
    const playerId = decodeURIComponent(rankingsPlayerMatch[1]);
    const rank = rankingStore.getPlayerRank(playerId);
    if (!rank) { json(res, 404, { error: 'Player has no ranked games' }); return; }
    json(res, 200, rank);
    return;
  }

  const rankingsGameMatch = pathname.match(/^\/api\/rankings\/([^/]+)$/);
  if (rankingsGameMatch && req.method === 'GET') {
    const gameId = decodeURIComponent(rankingsGameMatch[1]);
    const limit = parseInt(parsedUrl.query.limit as string ?? '20', 10);
    json(res, 200, { gameId, rankings: rankingStore.getGameLeaderboard(gameId, limit) });
    return;
  }

  // -------------------------------------------------------------------------
  // Phase 17: global chat REST
  //   GET  /api/chat                          — recent messages
  // -------------------------------------------------------------------------
  if (pathname === '/api/chat' && req.method === 'GET') {
    const chatLimit = parseInt(parsedUrl.query.limit as string ?? '100', 10);
    json(res, 200, { messages: globalChatStore.getRecent(chatLimit) });
    return;
  }

  // -------------------------------------------------------------------------
  // Phase 17: notification REST
  //   GET  /api/notifications/:playerId               — list
  //   POST /api/notifications/:playerId/read          — mark all read
  //   POST /api/notifications/:playerId/read/:notificationId — mark one read
  // -------------------------------------------------------------------------
  const notifMatch = pathname.match(/^\/api\/notifications\/([^/]+)(\/read(?:\/([^/]+))?)?$/);
  if (notifMatch) {
    const notifPlayerId = decodeURIComponent(notifMatch[1]);
    if (req.method === 'GET' && !notifMatch[2]) {
      json(res, 200, { notifications: notificationStore.list(notifPlayerId) });
      return;
    }
    if (req.method === 'POST' && notifMatch[2] === '/read' && !notifMatch[3]) {
      const count = notificationStore.markAllRead(notifPlayerId);
      json(res, 200, { read: count });
      return;
    }
    if (req.method === 'POST' && notifMatch[3]) {
      const ok = notificationStore.markRead(notifMatch[3]);
      if (!ok) { json(res, 404, { error: 'Notification not found' }); return; }
      json(res, 200, { ok: true });
      return;
    }
  }

  // -------------------------------------------------------------------------
  // Phase 23 + 24: Retro Achievement REST
  //   GET  /api/retro-achievements                          — all defs
  //   GET  /api/retro-achievements/games                    — game summaries
  //   GET  /api/retro-achievements/games/:gameId            — defs for a game
  //   GET  /api/retro-achievements/player/:playerId         — earned (all games)
  //   GET  /api/retro-achievements/player/:playerId/game/:gameId — per-game progress
  //   POST /api/retro-achievements/player/:playerId/game/:gameId/unlock — unlock one
  //   GET  /api/retro-achievements/leaderboard              — top players by points
  // -------------------------------------------------------------------------

  if (pathname === '/api/retro-achievements' && req.method === 'GET') {
    json(res, 200, retroAchievementStore.getDefinitions());
    return;
  }

  if (pathname === '/api/retro-achievements/games' && req.method === 'GET') {
    json(res, 200, retroAchievementStore.getGameSummaries());
    return;
  }

  if (pathname === '/api/retro-achievements/leaderboard' && req.method === 'GET') {
    const limitParam = parsedUrl.query.limit;
    const limitNum = Array.isArray(limitParam) ? limitParam[0] : limitParam;
    const limit = limitNum ? Math.min(Math.max(parseInt(limitNum, 10) || 10, 1), 100) : 10;
    json(res, 200, retroAchievementStore.getLeaderboard(limit));
    return;
  }

  const retroGameMatch = pathname.match(/^\/api\/retro-achievements\/games\/([^/]+)$/);
  if (retroGameMatch && req.method === 'GET') {
    const gameId = decodeURIComponent(retroGameMatch[1]);
    const defs = retroAchievementStore.getGameDefinitions(gameId);
    if (defs.length === 0) {
      json(res, 404, { error: 'No achievements found for this game' });
      return;
    }
    json(res, 200, defs);
    return;
  }

  const retroPlayerAllMatch = pathname.match(/^\/api\/retro-achievements\/player\/([^/]+)$/);
  if (retroPlayerAllMatch && req.method === 'GET') {
    const pid = decodeURIComponent(retroPlayerAllMatch[1]);
    const prog = retroAchievementStore.getProgress(pid);
    json(res, 200, prog);
    return;
  }

  const retroPlayerGameMatch = pathname.match(
    /^\/api\/retro-achievements\/player\/([^/]+)\/game\/([^/]+)$/
  );
  if (retroPlayerGameMatch && req.method === 'GET') {
    const pid = decodeURIComponent(retroPlayerGameMatch[1]);
    const gid = decodeURIComponent(retroPlayerGameMatch[2]);
    const earned = retroAchievementStore.getEarned(pid, gid);
    const defs = retroAchievementStore.getGameDefinitions(gid);
    json(res, 200, {
      playerId: pid,
      gameId: gid,
      earned,
      totalEarned: earned.length,
      totalAvailable: defs.length,
    });
    return;
  }

  const retroUnlockMatch = pathname.match(
    /^\/api\/retro-achievements\/player\/([^/]+)\/game\/([^/]+)\/unlock$/
  );
  if (retroUnlockMatch && req.method === 'POST') {
    const pid = decodeURIComponent(retroUnlockMatch[1]);
    let body: Record<string, unknown>;
    try {
      const raw = await readBody(req);
      body = JSON.parse(raw) as Record<string, unknown>;
    } catch {
      json(res, 400, { error: 'Invalid JSON body' });
      return;
    }
    const { achievementId, sessionId } = body as { achievementId?: string; sessionId?: string };
    if (!achievementId || typeof achievementId !== 'string') {
      json(res, 400, { error: '"achievementId" is required' });
      return;
    }
    const unlocked = retroAchievementStore.unlock(
      pid,
      achievementId,
      typeof sessionId === 'string' ? sessionId : undefined
    );
    if (!unlocked) {
      json(res, 409, { error: 'Achievement already earned or unknown ID' });
      return;
    }
    // Push WS notification to the player if they are currently connected.
    pushRetroAchievementUnlocked(
      pid,
      unlocked.id,
      unlocked.title,
      unlocked.description,
      unlocked.badge,
      unlocked.points,
      notificationStore
    );
    json(res, 200, { unlocked });
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

wss = new WebSocketServer({ server });

wss.on('connection', (ws, req) => {
  // Per-IP rate limit for new WebSocket connections
  const clientIp = getRemoteIp(req);
  if (!wsRateLimiter.allow(clientIp)) {
    console.warn(`[rate-limit] WebSocket connection rejected from ${clientIp}`);
    ws.close(1008, 'Rate limit exceeded — too many connections from your IP.');
    return;
  }
  handleConnection(ws, lobbyManager, netplayRelay, RELAY_HOST, sessionHistory, {
    friends: friendStore,
    matchmaking: matchmakingQueue,
    playerIdentity: playerIdentityStore,
    achievements: achievementStore,
    messages: messageStore,
    globalChat: globalChatStore,
    notifications: notificationStore,
  });
});

server.listen(PORT, () => {
  console.log(`RetroOasis Lobby Server running on ws://localhost:${PORT}`);
  console.log(`Launch API available at http://localhost:${PORT}/api/launch`);
  console.log(`Relay host: ${RELAY_HOST} | TCP ports ${process.env.RELAY_PORT_MIN ?? 9000}-${process.env.RELAY_PORT_MAX ?? 9200}`);
});

process.on('uncaughtException', (err) => {
  console.error('[fatal] Uncaught exception:', err);
});

process.on('unhandledRejection', (reason) => {
  console.error('[fatal] Unhandled promise rejection:', reason);
});
