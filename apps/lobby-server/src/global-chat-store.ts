/**
 * Phase 17 — Global Lobby Chat Store
 *
 * In-memory ring-buffer for global chat messages shared across all connected
 * clients.  The store is intentionally simple: it has no persistence layer
 * (messages are ephemeral) and no per-user filtering.
 */

import { randomUUID } from 'crypto';

export interface GlobalChatMessage {
  id: string;
  playerId: string;
  displayName: string;
  text: string;
  timestamp: string;
}

export class GlobalChatStore {
  private messages: GlobalChatMessage[] = [];
  private readonly maxMessages: number;

  constructor(maxMessages = 500) {
    this.maxMessages = maxMessages;
  }

  /**
   * Post a new message.  When the ring buffer is full the oldest message is
   * evicted.
   */
  post(playerId: string, displayName: string, text: string): GlobalChatMessage {
    const msg: GlobalChatMessage = {
      id: randomUUID(),
      playerId,
      displayName,
      text,
      timestamp: new Date().toISOString(),
    };
    this.messages.push(msg);
    if (this.messages.length > this.maxMessages) {
      this.messages.shift();
    }
    return msg;
  }

  /** Return the most recent `limit` messages (default 100), newest last. */
  getRecent(limit = 100): GlobalChatMessage[] {
    return this.messages.slice(-Math.max(1, limit));
  }

  /** Clear all messages (useful in tests). */
  clear(): void {
    this.messages = [];
  }
}
