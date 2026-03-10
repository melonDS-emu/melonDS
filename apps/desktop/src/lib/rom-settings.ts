const ROM_DIR_KEY = 'retro-oasis-rom-directory';
const SAVE_DIR_KEY = 'retro-oasis-save-directory';

/** Get the configured ROM path (file or directory), or empty string if not set. */
export function getRomDirectory(): string {
  return localStorage.getItem(ROM_DIR_KEY) ?? '';
}

/** Persist the ROM path. */
export function setRomDirectory(path: string): void {
  localStorage.setItem(ROM_DIR_KEY, path.trim());
}

/**
 * Get the configured save directory path.
 * Returns empty string if not set; callers should fall back to the ROM path directory.
 */
export function getSaveDirectory(): string {
  return localStorage.getItem(SAVE_DIR_KEY) ?? '';
}

/** Persist the save directory path. */
export function setSaveDirectory(path: string): void {
  localStorage.setItem(SAVE_DIR_KEY, path.trim());
}

/**
 * Build the full path to a ROM file given its filename.
 * If no directory is configured, returns the filename as-is.
 */
export function buildRomPath(filename: string): string {
  const dir = getRomDirectory();
  if (!dir) return filename;
  const sep = dir.endsWith('/') || dir.endsWith('\\') ? '' : '/';
  return `${dir}${sep}${filename}`;
}
