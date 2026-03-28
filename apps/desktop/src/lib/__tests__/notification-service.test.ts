/**
 * Tests for the notification service.
 */
import { describe, it, expect, beforeEach, vi } from 'vitest';
import { NotificationService } from '../notification-service';

// Mock the browser Notification API
function setupNotificationMock(permission: NotificationPermission = 'granted') {
  const notifications: InstanceType<typeof Notification>[] = [];

  const MockNotification = vi.fn().mockImplementation(function (
    this: Notification,
    title: string,
    _opts?: NotificationOptions,
  ) {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (this as any).title = title;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (this as any).close = vi.fn();
    notifications.push(this as unknown as Notification);
  }) as unknown as typeof Notification;

  (MockNotification as unknown as Record<string, unknown>).permission = permission;
  (MockNotification as unknown as Record<string, unknown>).requestPermission = vi.fn().mockResolvedValue(permission);

  Object.defineProperty(globalThis, 'Notification', {
    value: MockNotification,
    writable: true,
    configurable: true,
  });

  return { notifications, MockNotification };
}

describe('NotificationService', () => {
  let svc: NotificationService;

  beforeEach(() => {
    svc = new NotificationService();
  });

  it('reports isSupported = true when Notification is defined', () => {
    setupNotificationMock('granted');
    expect(svc.isSupported).toBe(true);
  });

  it('reports the current permission', () => {
    setupNotificationMock('granted');
    expect(svc.permission).toBe('granted');
  });

  it('show() returns null when permission is denied', () => {
    setupNotificationMock('denied');
    const result = svc.show('Test');
    expect(result).toBeNull();
  });

  it('show() creates a Notification when permission is granted', () => {
    const { notifications } = setupNotificationMock('granted');
    svc.show('Hello RetroOasis');
    expect(notifications.length).toBe(1);
  });

  it('notifyFriendOnline creates a notification', () => {
    const { notifications } = setupNotificationMock('granted');
    svc.notifyFriendOnline('Alice');
    expect(notifications.length).toBe(1);
  });

  it('notifyFriendInLobby creates a notification', () => {
    const { notifications } = setupNotificationMock('granted');
    svc.notifyFriendInLobby('Bob', 'Mario Kart 64', 'ABC123');
    expect(notifications.length).toBe(1);
  });

  it('notifyFriendStartedGame creates a notification', () => {
    const { notifications } = setupNotificationMock('granted');
    svc.notifyFriendStartedGame('Charlie', 'GoldenEye 007');
    expect(notifications.length).toBe(1);
  });

  it('notifyRoomReady creates a notification', () => {
    const { notifications } = setupNotificationMock('granted');
    svc.notifyRoomReady('Mario Kart 64');
    expect(notifications.length).toBe(1);
  });

  it('notifyGameStarting creates a notification', () => {
    const { notifications } = setupNotificationMock('granted');
    svc.notifyGameStarting('Mario Kart 64');
    expect(notifications.length).toBe(1);
  });

  it('requestPermission returns the permission state', async () => {
    setupNotificationMock('granted');
    const result = await svc.requestPermission();
    expect(result).toBe('granted');
  });
});
