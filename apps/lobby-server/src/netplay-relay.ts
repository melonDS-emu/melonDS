import * as net from 'net';
import { randomUUID } from 'crypto';

/**
 * NetplayRelay provides a lightweight TCP relay that forwards raw emulator
 * network packets between all players in a session.
 *
 * How it works:
 *  1. When a game starts the LobbyManager allocates a relay session.
 *  2. A unique TCP port (from the pool) is reserved for that session.
 *  3. Each player's emulator connects to the relay port.  The first 36 bytes
 *     sent are treated as the session token (UUID string, with hyphens).
 *  4. The relay forwards every subsequent packet to all other sockets in the
 *     same session — acting as a transparent packet bus.
 *
 * Supported emulator netplay protocols (all TCP-based):
 *  - mGBA  link-cable over TCP (default port 7888)
 *  - FCEUX NetPlay  (default port 4046)
 *  - Snes9x NetPlay (default port 6096)
 *  - Mupen64Plus netplay plugin (default port 6400)
 *  - melonDS local-wireless relay (default port 7000)
 */

interface RelaySession {
  roomId: string;
  playerIds: string[];
  /** Pre-assigned token per player: playerId -> relayToken (UUID) */
  playerTokens: Map<string, string>;
  /** Set of valid tokens for fast O(1) validation on connect */
  validTokens: Set<string>;
  sockets: Map<string, net.Socket>; // sessionToken -> socket
  port: number;
  server: net.Server;
  createdAt: Date;
}

const RELAY_BASE_PORT = 9000;
const RELAY_MAX_PORT = 9200;
/** UUID string with hyphens is exactly 36 bytes in ASCII/UTF-8. */
const SESSION_TOKEN_LENGTH = 36;

/**
 * Manages per-room TCP relay servers that bridge emulator netplay connections.
 */
export class NetplayRelay {
  private sessions: Map<string, RelaySession> = new Map(); // roomId -> session
  private usedPorts: Set<number> = new Set();

  private allocatePort(): number | null {
    for (let p = RELAY_BASE_PORT; p <= RELAY_MAX_PORT; p++) {
      if (!this.usedPorts.has(p)) {
        this.usedPorts.add(p);
        return p;
      }
    }
    return null;
  }

  /**
   * Allocate a relay session for the given room.
   * Generates a unique relay token per player so the server can authenticate
   * each emulator connection instead of accepting anonymous sockets.
   *
   * Returns `{ port, tokens }` where `tokens` maps playerId → relayToken,
   * or null if no ports are available.
   */
  allocateSession(
    roomId: string,
    playerIds: string[],
  ): { port: number; tokens: Map<string, string> } | null {
    if (this.sessions.has(roomId)) {
      const existing = this.sessions.get(roomId)!;
      return { port: existing.port, tokens: existing.playerTokens };
    }

    const port = this.allocatePort();
    if (port === null) return null;

    // Generate one pre-assigned token per player
    const playerTokens = new Map<string, string>();
    for (const pid of playerIds) {
      playerTokens.set(pid, randomUUID());
    }
    const validTokens = new Set(playerTokens.values());

    const sockets = new Map<string, net.Socket>();

    const server = net.createServer((socket) => {
      let sessionToken = '';
      /** Accumulated raw bytes before the token has been fully received. */
      const tokenChunks: Buffer[] = [];
      let tokenBytesReceived = 0;

      socket.on('data', (chunk: Buffer) => {
        if (sessionToken === '') {
          // Accumulate bytes until we have SESSION_TOKEN_LENGTH bytes for the token
          tokenChunks.push(chunk);
          tokenBytesReceived += chunk.length;

          if (tokenBytesReceived >= SESSION_TOKEN_LENGTH) {
            const combined = Buffer.concat(tokenChunks);
            const candidate = combined.subarray(0, SESSION_TOKEN_LENGTH).toString('ascii').trim();

            // Reject connections that don't present a pre-assigned token
            if (!validTokens.has(candidate)) {
              console.warn(`[relay] Rejected unauthorised token on room ${roomId}`);
              socket.destroy();
              return;
            }

            sessionToken = candidate;
            sockets.set(sessionToken, socket);

            // Forward any payload bytes that came after the token
            const remainder = combined.subarray(SESSION_TOKEN_LENGTH);
            if (remainder.length > 0) {
              forwardData(sessionToken, remainder);
            }
          }
          return;
        }

        forwardData(sessionToken, chunk);
      });

      socket.on('close', () => {
        if (sessionToken) sockets.delete(sessionToken);
      });

      socket.on('error', () => {
        if (sessionToken) sockets.delete(sessionToken);
      });
    });

    function forwardData(sourceToken: string, data: Buffer): void {
      for (const [token, sock] of sockets.entries()) {
        if (token === sourceToken) continue;
        if (!sock.destroyed) {
          sock.write(data);
        }
      }
    }

    server.on('error', (err: Error) => {
      console.error(`[relay] Session ${roomId} on port ${port} error:`, err.message);
      this.deallocateSession(roomId);
    });

    server.listen(port, '0.0.0.0', () => {
      console.log(`[relay] Session ${roomId} listening on TCP :${port}`);
    });

    const session: RelaySession = {
      roomId,
      playerIds,
      playerTokens,
      validTokens,
      sockets,
      port,
      server,
      createdAt: new Date(),
    };

    this.sessions.set(roomId, session);
    return { port, tokens: playerTokens };
  }

  /**
   * Get the pre-assigned relay token for a specific player in a room.
   * Returns null if the session or player is not found.
   */
  getPlayerToken(roomId: string, playerId: string): string | null {
    return this.sessions.get(roomId)?.playerTokens.get(playerId) ?? null;
  }

  /**
   * Close and remove a relay session.
   */
  deallocateSession(roomId: string): void {
    const session = this.sessions.get(roomId);
    if (!session) return;

    for (const sock of session.sockets.values()) {
      sock.destroy();
    }

    session.server.close();
    this.usedPorts.delete(session.port);
    this.sessions.delete(roomId);
    console.log(`[relay] Session ${roomId} closed (port ${session.port} freed)`);
  }

  /**
   * Get session info for a room.
   */
  getSession(roomId: string): { port: number; connectedPlayers: number } | null {
    const session = this.sessions.get(roomId);
    if (!session) return null;
    return { port: session.port, connectedPlayers: session.sockets.size };
  }

  /**
   * List all active sessions (for diagnostics).
   */
  listSessions(): Array<{ roomId: string; port: number; connectedPlayers: number; age: string }> {
    return Array.from(this.sessions.values()).map((s) => ({
      roomId: s.roomId,
      port: s.port,
      connectedPlayers: s.sockets.size,
      age: `${Math.floor((Date.now() - s.createdAt.getTime()) / 1000)}s`,
    }));
  }
}
