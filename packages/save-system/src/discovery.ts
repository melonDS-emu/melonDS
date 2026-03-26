/**
 * Standardized save-file path discovery per emulator backend.
 *
 * Each backend stores its SRAM / save-state files in a different location.
 * This module provides a single source of truth so the UI and sync layer
 * can reliably locate save files regardless of which emulator is running.
 *
 * File-extension conventions per system:
 *   .sav  — battery-backed SRAM (most emulators)
 *   .srm  — Super Nintendo SRAM (SNES9x, higan/ares)
 *   .fcs  — FCEUX save state
 *   .state — generic save-state extension
 *   .ss0 – .ss9 — Snes9x save states
 *   .st0 – .st9 — FCEUX save states
 */

/** A discovered save file. */
export interface DiscoveredSave {
  /** Absolute path to the save file. */
  filePath: string;
  /** Emulator backend that wrote this file. */
  backendId: string;
  /** Game ROM basename (without extension). */
  gameBasename: string;
  /** 'sram' | 'state' — type of save data. */
  saveType: 'sram' | 'state';
  /** File extension (including the leading dot). */
  extension: string;
}

/**
 * Return the typical save-file extensions used by each emulator backend.
 *
 * sramExtensions  — battery-backed SRAM / cartridge save files
 * stateExtensions — save-state files (snapshot of emulator RAM)
 */
export function getBackendSaveExtensions(backendId: string): {
  sramExtensions: string[];
  stateExtensions: string[];
} {
  switch (backendId) {
    case 'fceux':
      return { sramExtensions: ['.sav'], stateExtensions: ['.fc0', '.fc1', '.fc2', '.fc3', '.fc4', '.fc5', '.fc6', '.fc7', '.fc8', '.fc9'] };
    case 'nestopia':
      return { sramExtensions: ['.sav'], stateExtensions: ['.nst'] };
    case 'snes9x':
      return { sramExtensions: ['.srm'], stateExtensions: ['.ss0', '.ss1', '.ss2', '.ss3', '.ss4', '.ss5', '.ss6', '.ss7', '.ss8', '.ss9'] };
    case 'bsnes':
    case 'higan':
    case 'ares':
      return { sramExtensions: ['.srm', '.sav'], stateExtensions: ['.bst'] };
    case 'mgba':
      return { sramExtensions: ['.sav'], stateExtensions: ['.ss0', '.ss1', '.ss2', '.ss3'] };
    case 'sameboy':
      return { sramExtensions: ['.sav'], stateExtensions: ['.sst'] };
    case 'vba-m':
    case 'visualboyadvance':
      return { sramExtensions: ['.sav', '.sgm'], stateExtensions: ['.sgm'] };
    case 'mupen64plus':
      return { sramExtensions: ['.sra', '.eep', '.mpk', '.fla'], stateExtensions: ['.st0', '.st1', '.st2', '.st3', '.st4', '.st5', '.st6', '.st7', '.st8', '.st9'] };
    case 'melonds':
      return { sramExtensions: ['.sav', '.dsv'], stateExtensions: ['.mln'] };
    case 'desmume':
      return { sramExtensions: ['.dsv', '.sav'], stateExtensions: ['.dst'] };
    case 'dolphin':
      return { sramExtensions: ['.gci', '.raw', '.sav'], stateExtensions: ['.sltmp', '.s00', '.s01', '.s02', '.s03', '.s04', '.s05', '.s06', '.s07', '.s08', '.s09'] };
    case 'citra':
    case 'lime3ds':
      return { sramExtensions: ['.sav'], stateExtensions: ['.cst'] };
    case 'flycast':
      return { sramExtensions: ['.bin', '.vms'], stateExtensions: ['.fcs'] };
    case 'duckstation':
      return { sramExtensions: ['.mcr', '.mcs', '.srm'], stateExtensions: ['.save'] };
    case 'pcsx2':
      return { sramExtensions: ['.ps2', '.mcd'], stateExtensions: ['.p2s'] };
    case 'ppsspp':
      return { sramExtensions: ['.bin', '.vmem'], stateExtensions: ['.ppst'] };
    case 'cemu':
      return { sramExtensions: ['.bin', '.sav'], stateExtensions: ['.cemu'] };
    case 'retroarch':
      // RetroArch uses core-specific extensions but .srm / .state are the defaults
      return { sramExtensions: ['.srm', '.sav'], stateExtensions: ['.state', '.state1', '.state2', '.state3', '.state4'] };
    default:
      return { sramExtensions: ['.sav', '.srm'], stateExtensions: ['.state'] };
  }
}

/**
 * Infer which backend wrote a save file based on its extension and a
 * hint about the system.
 *
 * Returns null when the extension is not uniquely attributable to a backend.
 */
export function inferBackendFromExtension(ext: string, system?: string): string | null {
  const e = ext.toLowerCase();
  if (['.srm'].includes(e)) {
    // srm is used by SNES9x / bsnes / higan primarily
    return system === 'snes' ? 'snes9x' : null;
  }
  if (['.gci', '.raw'].includes(e)) return 'dolphin';
  if (['.vms'].includes(e)) return 'flycast';
  if (['.dsv'].includes(e)) return 'desmume';
  if (['.eep', '.sra', '.fla', '.mpk'].includes(e)) return 'mupen64plus';
  if (['.mcd', '.ps2'].includes(e)) return 'pcsx2';
  if (['.mcr', '.mcs'].includes(e)) return 'duckstation';
  if (['.p2s'].includes(e)) return 'pcsx2';
  if (['.ppst'].includes(e)) return 'ppsspp';
  if (['.cst'].includes(e)) return 'citra';
  return null;
}

/**
 * Build the expected save path for a given game+backend combination.
 *
 * @param gameBasename  ROM filename without extension (e.g. "pokemon-red")
 * @param backendId     Emulator backend identifier
 * @param saveDir       Base save directory (e.g. "~/retro-oasis-saves")
 * @param saveType      'sram' for battery saves, 'state' for save states
 * @param slotIndex     Slot number for save states (ignored for sram)
 */
export function buildBackendSavePath(
  gameBasename: string,
  backendId: string,
  saveDir: string,
  saveType: 'sram' | 'state' = 'sram',
  slotIndex = 0
): string {
  const { sramExtensions, stateExtensions } = getBackendSaveExtensions(backendId);
  const ext = saveType === 'sram' ? sramExtensions[0] : (stateExtensions[slotIndex] ?? stateExtensions[0]);
  const base = saveDir.replace(/\/$/, '');
  return `${base}/${gameBasename}${ext}`;
}

/**
 * Discover all save files for a given game basename within a directory.
 *
 * This function is designed to be called with Node.js `fs` — callers must
 * inject `readdir` so this module remains compatible with browser environments
 * and test stubs.
 *
 * @param gameBasename  ROM filename without extension
 * @param saveDir       Directory to scan
 * @param backendId     Preferred backend (used for ordering candidates)
 * @param readdir       Async function returning filenames in `saveDir`
 */
export async function discoverSaveFiles(
  gameBasename: string,
  saveDir: string,
  backendId: string,
  readdir: (dir: string) => Promise<string[]>
): Promise<DiscoveredSave[]> {
  let entries: string[];
  try {
    entries = await readdir(saveDir);
  } catch {
    return [];
  }

  const results: DiscoveredSave[] = [];
  const lowerBasename = gameBasename.toLowerCase();

  for (const entry of entries) {
    const dotIndex = entry.lastIndexOf('.');
    if (dotIndex === -1) continue;

    const entryBasename = entry.slice(0, dotIndex).toLowerCase();
    const ext = entry.slice(dotIndex).toLowerCase();

    if (entryBasename !== lowerBasename) continue;

    const preferredExts = getBackendSaveExtensions(backendId);
    const isSram = preferredExts.sramExtensions.includes(ext);
    const isState = preferredExts.stateExtensions.includes(ext);

    if (isSram || isState) {
      results.push({
        filePath: `${saveDir.replace(/\/$/, '')}/${entry}`,
        backendId,
        gameBasename,
        saveType: isSram ? 'sram' : 'state',
        extension: ext,
      });
      continue;
    }

    // Also check non-preferred backends to surface all save files
    const inferredBackend = inferBackendFromExtension(ext);
    if (inferredBackend && inferredBackend !== backendId) {
      // Check if this ext is recognised by any backend as a save file
      const altExts = getBackendSaveExtensions(inferredBackend);
      const altSram = altExts.sramExtensions.includes(ext);
      const altState = altExts.stateExtensions.includes(ext);
      if (altSram || altState) {
        results.push({
          filePath: `${saveDir.replace(/\/$/, '')}/${entry}`,
          backendId: inferredBackend,
          gameBasename,
          saveType: altSram ? 'sram' : 'state',
          extension: ext,
        });
      }
    }
  }

  // Preferred backend SRAM first, then states, then other backends
  return results.sort((a, b) => {
    if (a.backendId === backendId && b.backendId !== backendId) return -1;
    if (a.backendId !== backendId && b.backendId === backendId) return 1;
    if (a.saveType === 'sram' && b.saveType !== 'sram') return -1;
    if (a.saveType !== 'sram' && b.saveType === 'sram') return 1;
    return a.filePath.localeCompare(b.filePath);
  });
}
