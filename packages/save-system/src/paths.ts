import * as path from 'path';

/** Default base directory for saves (platform-specific in production) */
const DEFAULT_SAVE_BASE = './retro-oasis-saves';

/**
 * Build a save file path for a specific game.
 */
export function buildSavePath(
  gameId: string,
  system: string,
  baseDir: string = DEFAULT_SAVE_BASE
): string {
  return path.join(baseDir, system, gameId);
}

/**
 * Get the save directory for a specific system.
 */
export function getSystemSaveDir(
  system: string,
  baseDir: string = DEFAULT_SAVE_BASE
): string {
  return path.join(baseDir, system);
}

/**
 * Get the save state directory for a specific game.
 */
export function getSaveStatePath(
  gameId: string,
  system: string,
  slotIndex: number,
  baseDir: string = DEFAULT_SAVE_BASE
): string {
  return path.join(baseDir, system, gameId, `state-${slotIndex}.sav`);
}

/**
 * Get the auto-save path for a specific game.
 */
export function getAutoSavePath(
  gameId: string,
  system: string,
  baseDir: string = DEFAULT_SAVE_BASE
): string {
  return path.join(baseDir, system, gameId, 'auto.sav');
}
