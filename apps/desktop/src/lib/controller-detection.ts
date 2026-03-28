/**
 * Controller detection service.
 *
 * Wraps the browser Gamepad API to provide:
 *  - A snapshot of currently-connected gamepads with type classification
 *  - Hot-plug detection (gamepadconnected / gamepaddisconnected events)
 *  - Controller-type identification from the gamepad ID string
 *
 * This module has no side effects when imported — callers explicitly
 * subscribe via `addControllerListener` / `removeControllerListener`.
 */

import type { ControllerType } from '@retro-oasis/multiplayer-profiles';

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

/** Snapshot of a single connected gamepad. */
export interface GamepadInfo {
  /** Browser-assigned index (0-based). */
  index: number;
  /** Raw ID string reported by the browser (e.g. "Xbox 360 Controller (XInput…)"). */
  id: string;
  /** Inferred controller family. */
  controllerType: ControllerType;
  /** Number of axes reported by the browser. */
  axisCount: number;
  /** Number of buttons reported by the browser. */
  buttonCount: number;
  /** Whether the gamepad is currently connected. */
  connected: boolean;
}

/** Callback invoked whenever the set of connected controllers changes. */
export type ControllerChangeCallback = (gamepads: GamepadInfo[]) => void;

// ---------------------------------------------------------------------------
// Controller-type detection
// ---------------------------------------------------------------------------

/**
 * Infer the controller family from the raw gamepad ID string.
 *
 * The Gamepad API exposes strings like:
 *  "Xbox 360 Controller (XInput STANDARD GAMEPAD)"
 *  "Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)"
 *  "USB Gamepad  (Vendor: 0079 Product: 0011)"
 */
export function detectGamepadType(id: string): ControllerType {
  const lower = id.toLowerCase();

  if (
    lower.includes('xbox') ||
    lower.includes('xinput') ||
    lower.includes('045e')  // Microsoft vendor ID
  ) {
    return 'xbox';
  }

  if (
    lower.includes('playstation') ||
    lower.includes('dualshock') ||
    lower.includes('dualsense') ||
    lower.includes('wireless controller') ||
    lower.includes('054c')  // Sony vendor ID
  ) {
    return 'playstation';
  }

  if (lower === 'keyboard' || lower.includes('keyboard')) {
    return 'keyboard';
  }

  if (lower.includes('gamepad') || lower.includes('joystick')) {
    return 'generic';
  }

  return 'unknown';
}

// ---------------------------------------------------------------------------
// Snapshot helper
// ---------------------------------------------------------------------------

/**
 * Return a snapshot of all currently-connected gamepads.
 * Safe to call in SSR/test environments where `navigator.getGamepads` may not
 * exist — returns an empty array in that case.
 */
export function getConnectedGamepads(): GamepadInfo[] {
  if (typeof navigator === 'undefined' || typeof navigator.getGamepads !== 'function') {
    return [];
  }

  const raw = navigator.getGamepads();
  const result: GamepadInfo[] = [];

  for (let i = 0; i < raw.length; i++) {
    const gp = raw[i];
    if (gp && gp.connected) {
      result.push({
        index: gp.index,
        id: gp.id,
        controllerType: detectGamepadType(gp.id),
        axisCount: gp.axes.length,
        buttonCount: gp.buttons.length,
        connected: true,
      });
    }
  }

  return result;
}

// ---------------------------------------------------------------------------
// Hot-plug subscription
// ---------------------------------------------------------------------------

const listeners = new Set<ControllerChangeCallback>();

function notifyAll(): void {
  const current = getConnectedGamepads();
  for (const cb of listeners) {
    cb(current);
  }
}

function onGamepadConnected(): void {
  notifyAll();
}

function onGamepadDisconnected(): void {
  notifyAll();
}

/**
 * Subscribe to controller connect / disconnect events.
 * The callback is invoked immediately with the current gamepad list and then
 * again whenever a gamepad is plugged in or removed.
 *
 * Returns an unsubscribe function.
 */
export function addControllerListener(cb: ControllerChangeCallback): () => void {
  listeners.add(cb);

  if (listeners.size === 1 && typeof window !== 'undefined') {
    window.addEventListener('gamepadconnected', onGamepadConnected);
    window.addEventListener('gamepaddisconnected', onGamepadDisconnected);
  }

  // Fire immediately with current state
  cb(getConnectedGamepads());

  return () => removeControllerListener(cb);
}

/**
 * Unsubscribe from controller events.
 */
export function removeControllerListener(cb: ControllerChangeCallback): void {
  listeners.delete(cb);

  if (listeners.size === 0 && typeof window !== 'undefined') {
    window.removeEventListener('gamepadconnected', onGamepadConnected);
    window.removeEventListener('gamepaddisconnected', onGamepadDisconnected);
  }
}
