export { SaveManager } from './save-manager';
export { CloudSyncService, computeFileHash } from './cloud-sync';
export type { CloudSaveMetadata, CloudStorageAdapter } from './cloud-sync';
export { buildSavePath, getSystemSaveDir } from './paths';
export {
  getBackendSaveExtensions,
  inferBackendFromExtension,
  buildBackendSavePath,
  discoverSaveFiles,
} from './discovery';
export type { DiscoveredSave } from './discovery';
