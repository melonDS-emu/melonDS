/**
 * In-memory direct-message store for Phase 14.
 *
 * Stores DM threads between players.  Replaced at runtime by
 * SqliteMessageStore when DB_PATH is set.
 */

import { randomUUID } from 'crypto';

export interface DirectMessage {
  id: string;
  fromPlayer: string;
  toPlayer: string;
  content: string;
  sentAt: string;
  readAt: string | null;
}

export interface ConversationSummary {
  peer: string;
  lastMessage: DirectMessage;
  unreadCount: number;
}

const MAX_CONTENT_LENGTH = 500;
const DEFAULT_CONVERSATION_LIMIT = 50;

export class MessageStore {
  protected messages: Map<string, DirectMessage> = new Map();

  // ---------------------------------------------------------------------------
  // Write
  // ---------------------------------------------------------------------------

  sendMessage(fromPlayer: string, toPlayer: string, content: string): DirectMessage {
    const msg: DirectMessage = {
      id: randomUUID(),
      fromPlayer,
      toPlayer,
      content: content.slice(0, MAX_CONTENT_LENGTH),
      sentAt: new Date().toISOString(),
      readAt: null,
    };
    this.messages.set(msg.id, msg);
    return msg;
  }

  markRead(fromPlayer: string, toPlayer: string): void {
    for (const msg of this.messages.values()) {
      if (msg.fromPlayer === fromPlayer && msg.toPlayer === toPlayer && msg.readAt === null) {
        msg.readAt = new Date().toISOString();
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Read
  // ---------------------------------------------------------------------------

  getConversation(player1: string, player2: string, limit = DEFAULT_CONVERSATION_LIMIT): DirectMessage[] {
    const msgs: DirectMessage[] = [];
    for (const msg of this.messages.values()) {
      if (
        (msg.fromPlayer === player1 && msg.toPlayer === player2) ||
        (msg.fromPlayer === player2 && msg.toPlayer === player1)
      ) {
        msgs.push(msg);
      }
    }
    return msgs.sort((a, b) => a.sentAt.localeCompare(b.sentAt)).slice(-limit);
  }

  getUnreadCount(toPlayer: string): number {
    let count = 0;
    for (const msg of this.messages.values()) {
      if (msg.toPlayer === toPlayer && msg.readAt === null) count++;
    }
    return count;
  }

  getRecentConversations(player: string): ConversationSummary[] {
    const peers = new Set<string>();
    for (const msg of this.messages.values()) {
      if (msg.fromPlayer === player) peers.add(msg.toPlayer);
      if (msg.toPlayer === player) peers.add(msg.fromPlayer);
    }

    const result: ConversationSummary[] = [];
    for (const peer of peers) {
      const conv = this.getConversation(player, peer, 1);
      const lastMessage = conv[conv.length - 1];
      if (lastMessage) {
        const unreadCount = Array.from(this.messages.values()).filter(
          (m) => m.fromPlayer === peer && m.toPlayer === player && m.readAt === null
        ).length;
        result.push({ peer, lastMessage, unreadCount });
      }
    }
    return result.sort((a, b) => b.lastMessage.sentAt.localeCompare(a.lastMessage.sentAt));
  }
}
