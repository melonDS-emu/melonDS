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
 * In production, file operations would use Tauri fs API or Node fs.
 */
export class SaveManager {
  private saves: Map<string, SaveSlotInfo[]> = new Map();
  private baseDir: string;

  constructor(baseDir: string = './retro-oasis-saves') {
    this.baseDir = baseDir;
  }

  /**
   * List all saves for a game.
   */
  listSaves(gameId: string): SaveSlotInfo[] {
    return this.saves.get(gameId) ?? [];
  }

  /**
   * Create a new save slot for a game.
   */
  createSaveSlot(
    gameId: string,
    system: string,
    options: { label?: string; isSaveState?: boolean; isAutoSave?: boolean } = {}
  ): SaveSlotInfo {
    const existing = this.saves.get(gameId) ?? [];
    const slotIndex = existing.length;
    const now = new Date().toISOString();

    const save: SaveSlotInfo = {
      id: `${gameId}-slot-${slotIndex}`,
      gameId,
      system,
      slotIndex,
      filePath: buildSavePath(gameId, system, this.baseDir),
      fileSize: 0,
      createdAt: now,
      updatedAt: now,
      isSaveState: options.isSaveState ?? false,
      isAutoSave: options.isAutoSave ?? false,
      label: options.label,
    };

    existing.push(save);
    this.saves.set(gameId, existing);

    // TODO: Actually create the file/directory on disk
    return save;
  }

  /**
   * Delete a save slot.
   */
  deleteSave(gameId: string, slotIndex: number): boolean {
    const existing = this.saves.get(gameId);
    if (!existing) return false;

    const index = existing.findIndex((s) => s.slotIndex === slotIndex);
    if (index === -1) return false;

    existing.splice(index, 1);
    // TODO: Actually delete the file from disk
    return true;
  }

  /**
   * Get the save directory for a game.
   */
  getSaveDirectory(gameId: string, system: string): string {
    return buildSavePath(gameId, system, this.baseDir);
  }

  /**
   * Import a save file from an external path.
   */
  async importSave(
    gameId: string,
    system: string,
    externalPath: string,
    label?: string
  ): Promise<SaveSlotInfo> {
    // TODO: Copy file from externalPath to save directory
    return this.createSaveSlot(gameId, system, { label: label ?? 'Imported Save' });
  }

  /**
   * Export a save to an external path.
   */
  async exportSave(_gameId: string, _slotIndex: number, _exportPath: string): Promise<boolean> {
    // TODO: Copy save file to exportPath
    return true;
  }
}
