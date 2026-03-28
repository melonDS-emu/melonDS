/**
 * Browser push notification service for RetroOasis.
 *
 * Wraps the Notification API with permission management and convenience helpers.
 * Notifications are sent when friends come online, join a lobby, or start a game.
 *
 * Usage:
 *   import { NotificationService } from './notification-service';
 *   const svc = new NotificationService();
 *   await svc.requestPermission();
 *   svc.notifyFriendOnline('PlayerOne');
 */

export type NotificationPermissionState = 'default' | 'granted' | 'denied' | 'unsupported';

export interface NotificationOptions {
  body?: string;
  icon?: string;
  tag?: string;
  /** If true, replaces any existing notification with the same tag. Default: true */
  renotify?: boolean;
  /** If non-null, auto-close the notification after this many ms. */
  autoCloseMs?: number;
}

const DEFAULT_ICON = '/icon-192.png';

/** Type-safe accessor for the globalThis Notification constructor (if present). */
function getNotificationConstructor(): typeof Notification | null {
  const g = globalThis as unknown as Record<string, unknown>;
  return ('Notification' in g ? g['Notification'] : null) as typeof Notification | null;
}

export class NotificationService {
  /**
   * Whether the browser supports the Notification API.
   */
  get isSupported(): boolean {
    return getNotificationConstructor() !== null;
  }

  /**
   * Current permission state.
   */
  get permission(): NotificationPermissionState {
    const N = getNotificationConstructor();
    if (!N) return 'unsupported';
    return N.permission as NotificationPermissionState;
  }

  /**
   * Request notification permission from the user.
   * Returns the resulting permission state.
   */
  async requestPermission(): Promise<NotificationPermissionState> {
    const N = getNotificationConstructor();
    if (!N) return 'unsupported';
    if (N.permission === 'granted') return 'granted';
    const result = await N.requestPermission();
    return result as NotificationPermissionState;
  }

  /**
   * Show a notification. Returns the Notification instance or null if
   * permission is not granted or the API is unavailable.
   */
  show(title: string, options: NotificationOptions = {}): Notification | null {
    const N = getNotificationConstructor();
    if (!N || N.permission !== 'granted') return null;

    const n = new N(title, {
      body: options.body,
      icon: options.icon ?? DEFAULT_ICON,
      tag: options.tag,
      // `renotify` is supported by browsers but missing from older TS DOM types
      ...(options.renotify !== false && { renotify: true }),
      silent: false,
    } as NotificationOptions);

    if (options.autoCloseMs != null) {
      setTimeout(() => n.close(), options.autoCloseMs);
    }

    return n;
  }

  // ---------------------------------------------------------------------------
  // Convenience helpers
  // ---------------------------------------------------------------------------

  notifyFriendOnline(displayName: string): Notification | null {
    return this.show(`${displayName} is online`, {
      body: 'Your friend just connected to RetroOasis.',
      tag: `online-${displayName}`,
      autoCloseMs: 8_000,
    });
  }

  notifyFriendInLobby(displayName: string, gameTitle: string, roomCode: string): Notification | null {
    return this.show(`${displayName} opened a lobby`, {
      body: `Playing ${gameTitle} — join with code ${roomCode}`,
      tag: `lobby-${roomCode}`,
      autoCloseMs: 15_000,
    });
  }

  notifyFriendStartedGame(displayName: string, gameTitle: string): Notification | null {
    return this.show(`${displayName} started ${gameTitle}`, {
      body: 'Looks like the game is underway!',
      tag: `started-${displayName}`,
      autoCloseMs: 10_000,
    });
  }

  notifyRoomReady(gameTitle: string): Notification | null {
    return this.show('All players ready!', {
      body: `${gameTitle} — the host can now start the game.`,
      tag: 'room-ready',
      autoCloseMs: 10_000,
    });
  }

  notifyGameStarting(gameTitle: string): Notification | null {
    return this.show(`${gameTitle} is starting!`, {
      body: 'The emulator is launching — get ready.',
      tag: 'game-starting',
      autoCloseMs: 8_000,
    });
  }
}

/** Singleton instance for use throughout the app. */
export const notificationService = new NotificationService();
