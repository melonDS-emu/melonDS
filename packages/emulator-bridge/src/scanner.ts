import * as fs from 'fs';
import * as path from 'path';

/**
 * ROM file extensions and the systems they correspond to.
 * Extensions are lowercased for comparison.
 */
export const ROM_EXTENSIONS: Record<string, string> = {
  '.nes': 'nes',
  '.fds': 'nes',      // Famicom Disk System
  '.unf': 'nes',
  '.sfc': 'snes',
  '.smc': 'snes',
  '.fig': 'snes',
  '.gd3': 'snes',
  '.gb':  'gb',
  '.sgb': 'gb',       // Super Game Boy
  '.gbc': 'gbc',
  '.cgb': 'gbc',
  '.gba': 'gba',
  '.agb': 'gba',
  '.z64': 'n64',
  '.v64': 'n64',
  '.n64': 'n64',
  '.nds': 'nds',
  '.dsi': 'nds',
  '.ids': 'nds',
  // Sega Master System / Game Gear
  '.sms': 'sms',    // Sega Master System ROM
  '.gg':  'sms',    // Game Gear (shares Genesis Plus GX core)
  '.sg':  'sms',    // SG-1000 (predecessor, same core)
  // SEGA Genesis / Mega Drive
  '.md':  'genesis',
  '.gen': 'genesis',
  '.bin': 'genesis',
  '.smd': 'genesis',
  // GameCube
  '.gcm': 'gc',     // raw GameCube disc image
  '.gcz': 'gc',     // Dolphin compressed GameCube image
  '.rvz': 'gc',     // Dolphin RVZ compressed format
  '.iso': 'gc',     // optical disc image — defaults to GC; Wii support not yet enabled
  '.ciso': 'gc',    // compact ISO
  // Nintendo 3DS
  '.3ds': '3ds',    // trimmed 3DS ROM (most common)
  '.cci': '3ds',    // raw 3DS card image
  '.cia': '3ds',    // 3DS installable content archive
  '.3dsx': '3ds',   // 3DS homebrew executable
};

export interface RomFileInfo {
  filePath: string;
  fileName: string;
  system: string;
  /** Inferred game title (file name without extension, with underscores→spaces). */
  inferredTitle: string;
  fileSizeBytes: number;
  lastModified: string;
}

/**
 * Scan a directory (non-recursively by default) for ROM files.
 * Returns a list of discovered ROM files sorted by inferred title.
 *
 * @param dirPath     Absolute path to the directory to scan
 * @param recursive   If true, scan subdirectories as well (max depth 3)
 */
export function scanRomDirectory(dirPath: string, recursive = false): RomFileInfo[] {
  const results: RomFileInfo[] = [];
  _scan(dirPath, recursive, 0, results);
  results.sort((a, b) => a.inferredTitle.localeCompare(b.inferredTitle));
  return results;
}

function _scan(dirPath: string, recursive: boolean, depth: number, results: RomFileInfo[]): void {
  if (depth > 3) return;

  let entries: fs.Dirent[];
  try {
    entries = fs.readdirSync(dirPath, { withFileTypes: true });
  } catch {
    // Directory not readable — skip silently
    return;
  }

  for (const entry of entries) {
    const fullPath = path.join(dirPath, entry.name);

    if (entry.isDirectory() && recursive) {
      _scan(fullPath, recursive, depth + 1, results);
      continue;
    }

    if (!entry.isFile()) continue;

    const ext = path.extname(entry.name).toLowerCase();
    const system = ROM_EXTENSIONS[ext];
    if (!system) continue;

    let stat: fs.Stats;
    try {
      stat = fs.statSync(fullPath);
    } catch {
      continue;
    }

    const baseName = path.basename(entry.name, ext);
    const inferredTitle = baseName
      .replace(/[_\-\.]+/g, ' ')    // separators → spaces
      .replace(/\s*\(.*?\)/g, '')   // strip (USA), (Rev 1), etc.
      .replace(/\s*\[.*?\]/g, '')   // strip [!], [T-Eng], etc.
      .trim()
      .replace(/\s+/g, ' ');

    results.push({
      filePath: fullPath,
      fileName: entry.name,
      system,
      inferredTitle: inferredTitle || baseName,
      fileSizeBytes: stat.size,
      lastModified: stat.mtime.toISOString(),
    });
  }
}

/**
 * Group scan results by system.
 */
export function groupBySystem(roms: RomFileInfo[]): Record<string, RomFileInfo[]> {
  const groups: Record<string, RomFileInfo[]> = {};
  for (const rom of roms) {
    if (!groups[rom.system]) groups[rom.system] = [];
    groups[rom.system].push(rom);
  }
  return groups;
}
