/** Default base directory for saves (platform-specific in production) */
const DEFAULT_SAVE_BASE = './retro-oasis-saves';

/** Simple path joining utility (avoids Node.js 'path' module dependency). */
function joinPath(...parts: string[]): string {
  return parts
    .filter((p) => p.length > 0)
    .map((p, i) => (i === 0 ? p.replace(/\/+$/, '') : p.replace(/^\/+|\/+$/g, '')))
    .join('/');
}

/**
 * Build a save file path for a specific game.
 */
export function buildSavePath(
  gameId: string,
  system: string,
  baseDir: string = DEFAULT_SAVE_BASE
): string {
  return joinPath(baseDir, system, gameId);
}

/**
 * Get the save directory for a specific system.
 */
export function getSystemSaveDir(
  system: string,
  baseDir: string = DEFAULT_SAVE_BASE
): string {
  return joinPath(baseDir, system);
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
  return joinPath(baseDir, system, gameId, `state-${slotIndex}.sav`);
}

/**
 * Get the auto-save path for a specific game.
 */
export function getAutoSavePath(
  gameId: string,
  system: string,
  baseDir: string = DEFAULT_SAVE_BASE
): string {
  return joinPath(baseDir, system, gameId, 'auto.sav');
}
