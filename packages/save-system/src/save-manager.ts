import * as fs from 'fs/promises';
import * as fsSync from 'fs';
import { buildSavePath } from './paths';

export interface SaveSlotInfo {
  id: string;
  gameId: string;
  system: string;
  slotIndex: number;
  filePath: string;
  fileSize: number;
  createdAt: string;
  updatedAt: string;
  isSaveState: boolean;
  isAutoSave: boolean;
  label?: string;
}

/**
 * SaveManager handles local save file discovery, creation, and management.
 * Uses Node.js fs/promises for real file system operations.
 */
export class SaveManager {
  private saves: Map<string, SaveSlotInfo[]> = new Map();
  private baseDir: string;

  constructor(baseDir: string = './retro-oasis-saves') {
    this.baseDir = baseDir;
  }

  /**
   * Ensure the save directory for a game exists on disk.
   */
  private async ensureDir(gameId: string, system: string): Promise<string> {
    const dir = buildSavePath(gameId, system, this.baseDir);
    await fs.mkdir(dir, { recursive: true });
    return dir;
  }

  /**
   * List all saves for a game.
   * If the game directory exists, refreshes from disk first.
   */
  async listSaves(gameId: string, system?: string): Promise<SaveSlotInfo[]> {
    const sys = system ?? this.saves.get(gameId)?.[0]?.system;
    if (!sys) {
      // No system known yet — return in-memory list (may be empty)
      return this.saves.get(gameId) ?? [];
    }
    const dir = buildSavePath(gameId, sys, this.baseDir);
    if (fsSync.existsSync(dir)) {
      await this.refreshFromDisk(gameId, sys, dir);
    }
    return this.saves.get(gameId) ?? [];
  }

  private async refreshFromDisk(gameId: string, system: string, dir: string): Promise<void> {
    let entries: string[];
    try {
      entries = await fs.readdir(dir);
    } catch {
      return;
    }

    const existing = new Map((this.saves.get(gameId) ?? []).map((s) => [s.filePath, s]));
    const updated: SaveSlotInfo[] = [];

    for (const entry of entries) {
      const filePath = `${dir}/${entry}`;
      let stat: fsSync.Stats;
      try {
        stat = fsSync.statSync(filePath);
      } catch {
        continue;
      }

      if (existing.has(filePath)) {
        const s = existing.get(filePath)!;
        updated.push({ ...s, fileSize: stat.size, updatedAt: stat.mtime.toISOString() });
      } else {
        const slotIndex = updated.length;
        updated.push({
          id: `${gameId}-slot-${slotIndex}`,
          gameId,
          system,
          slotIndex,
          filePath,
          fileSize: stat.size,
          createdAt: stat.birthtime.toISOString(),
          updatedAt: stat.mtime.toISOString(),
          isSaveState: entry.includes('.state'),
          isAutoSave: entry.includes('auto'),
          label: entry,
        });
      }
    }

    this.saves.set(gameId, updated);
  }

  /**
   * Create a new save slot for a game.
   * Creates the directory structure on disk.
   */
  async createSaveSlot(
    gameId: string,
    system: string,
    options: { label?: string; isSaveState?: boolean; isAutoSave?: boolean } = {}
  ): Promise<SaveSlotInfo> {
    const dir = await this.ensureDir(gameId, system);
    const existing = this.saves.get(gameId) ?? [];
    const slotIndex = existing.length;
    const now = new Date().toISOString();

    const ext = options.isSaveState ? '.state' : '.sav';
    const autoTag = options.isAutoSave ? 'auto-' : '';
    const fileName = `${autoTag}slot-${slotIndex}${ext}`;
    const filePath = `${dir}/${fileName}`;

    // Create the save file placeholder on disk
    try {
      await fs.writeFile(filePath, Buffer.alloc(0), { flag: 'ax' });
    } catch {
      // File may already exist — that is fine
    }

    const save: SaveSlotInfo = {
      id: `${gameId}-slot-${slotIndex}`,
      gameId,
      system,
      slotIndex,
      filePath,
      fileSize: 0,
      createdAt: now,
      updatedAt: now,
      isSaveState: options.isSaveState ?? false,
      isAutoSave: options.isAutoSave ?? false,
      label: options.label ?? fileName,
    };

    existing.push(save);
    this.saves.set(gameId, existing);
    return save;
  }

  /**
   * Delete a save slot and its file from disk.
   */
  async deleteSave(gameId: string, slotIndex: number): Promise<boolean> {
    const existing = this.saves.get(gameId);
    if (!existing) return false;

    const index = existing.findIndex((s) => s.slotIndex === slotIndex);
    if (index === -1) return false;

    const save = existing[index];
    try {
      await fs.unlink(save.filePath);
    } catch {
      // File may not exist on disk — that is fine
    }

    existing.splice(index, 1);
    return true;
  }

  /**
   * Get the save directory for a game.
   */
  getSaveDirectory(gameId: string, system: string): string {
    return buildSavePath(gameId, system, this.baseDir);
  }

  /**
   * Import a save file from an external path by copying it into the save directory.
   */
  async importSave(
    gameId: string,
    system: string,
    externalPath: string,
    label?: string
  ): Promise<SaveSlotInfo> {
    const dir = await this.ensureDir(gameId, system);
    const existing = this.saves.get(gameId) ?? [];
    const slotIndex = existing.length;
    const ext = externalPath.slice(externalPath.lastIndexOf('.'));
    const destPath = `${dir}/import-slot-${slotIndex}${ext}`;

    await fs.copyFile(externalPath, destPath);
    const stat = await fs.stat(destPath);
    const now = new Date().toISOString();

    const save: SaveSlotInfo = {
      id: `${gameId}-slot-${slotIndex}`,
      gameId,
      system,
      slotIndex,
      filePath: destPath,
      fileSize: stat.size,
      createdAt: now,
      updatedAt: now,
      isSaveState: ext === '.state',
      isAutoSave: false,
      label: label ?? `Imported ${slotIndex}`,
    };

    existing.push(save);
    this.saves.set(gameId, existing);
    return save;
  }

  /**
   * Export a save to an external path by copying the file.
   */
  async exportSave(gameId: string, slotIndex: number, exportPath: string): Promise<boolean> {
    const existing = this.saves.get(gameId);
    if (!existing) return false;
    const save = existing.find((s) => s.slotIndex === slotIndex);
    if (!save) return false;

    try {
      await fs.copyFile(save.filePath, exportPath);
      return true;
    } catch {
      return false;
    }
  }
}
