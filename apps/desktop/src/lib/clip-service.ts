/**
 * Clip service — browser-side clip capture using the MediaRecorder API.
 *
 * Clips are stored in IndexedDB (key = clip ID) so they survive page
 * reloads.  Metadata (id, gameId, gameTitle, capturedAt, duration) is also
 * kept in localStorage for fast listing without opening IndexedDB on every
 * page render.
 *
 * Usage:
 *   const handle = await startClipRecording(stream, { gameId, gameTitle });
 *   // ... wait for action ...
 *   const clip = await stopClipRecording(handle);
 *   // clip.url is a blob: URL valid for the current page session.
 */

const DB_NAME = 'retro-oasis-clips';
const STORE_NAME = 'clips';
const META_KEY = 'retro-oasis-clip-meta';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export interface ClipMeta {
  id: string;
  gameId: string;
  gameTitle: string;
  capturedAt: string; // ISO timestamp
  durationSecs: number;
}

export interface Clip extends ClipMeta {
  /** Revocable object URL pointing at the .webm blob. */
  url: string;
}

export interface RecordingHandle {
  recorder: MediaRecorder;
  gameId: string;
  gameTitle: string;
  startedAt: number; // Date.now()
  chunks: Blob[];
}

// ---------------------------------------------------------------------------
// IndexedDB helpers
// ---------------------------------------------------------------------------

function openDb(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, 1);
    req.onupgradeneeded = () => {
      req.result.createObjectStore(STORE_NAME);
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });
}

async function saveBlob(id: string, blob: Blob): Promise<void> {
  const db = await openDb();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, 'readwrite');
    tx.objectStore(STORE_NAME).put(blob, id);
    tx.oncomplete = () => { db.close(); resolve(); };
    tx.onerror = () => { db.close(); reject(tx.error); };
  });
}

async function loadBlob(id: string): Promise<Blob | undefined> {
  const db = await openDb();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, 'readonly');
    const req = tx.objectStore(STORE_NAME).get(id);
    req.onsuccess = () => { db.close(); resolve(req.result as Blob | undefined); };
    req.onerror = () => { db.close(); reject(req.error); };
  });
}

async function deleteBlob(id: string): Promise<void> {
  const db = await openDb();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(STORE_NAME, 'readwrite');
    tx.objectStore(STORE_NAME).delete(id);
    tx.oncomplete = () => { db.close(); resolve(); };
    tx.onerror = () => { db.close(); reject(tx.error); };
  });
}

// ---------------------------------------------------------------------------
// Metadata helpers (localStorage)
// ---------------------------------------------------------------------------

function loadMeta(): ClipMeta[] {
  try {
    const raw = localStorage.getItem(META_KEY);
    return raw ? (JSON.parse(raw) as ClipMeta[]) : [];
  } catch {
    return [];
  }
}

function saveMeta(metas: ClipMeta[]): void {
  localStorage.setItem(META_KEY, JSON.stringify(metas));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Start recording a MediaStream (e.g. from `canvas.captureStream()`).
 * Returns a handle that must be passed to `stopClipRecording`.
 */
export function startClipRecording(
  stream: MediaStream,
  opts: { gameId: string; gameTitle: string }
): RecordingHandle {
  const chunks: Blob[] = [];
  const recorder = new MediaRecorder(stream, { mimeType: 'video/webm;codecs=vp9,opus' });
  recorder.ondataavailable = (e) => {
    if (e.data.size > 0) chunks.push(e.data);
  };
  recorder.start(100); // collect data every 100 ms
  return { recorder, gameId: opts.gameId, gameTitle: opts.gameTitle, startedAt: Date.now(), chunks };
}

/**
 * Stop recording and persist the clip.  Resolves with the saved Clip metadata.
 */
export async function stopClipRecording(handle: RecordingHandle): Promise<ClipMeta> {
  return new Promise((resolve, reject) => {
    handle.recorder.onstop = async () => {
      try {
        const blob = new Blob(handle.chunks, { type: 'video/webm' });
        const id = `clip-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`;
        const durationSecs = Math.round((Date.now() - handle.startedAt) / 1000);
        const meta: ClipMeta = {
          id,
          gameId: handle.gameId,
          gameTitle: handle.gameTitle,
          capturedAt: new Date().toISOString(),
          durationSecs,
        };
        await saveBlob(id, blob);
        const metas = loadMeta();
        metas.unshift(meta);
        saveMeta(metas);
        resolve(meta);
      } catch (err) {
        reject(err);
      }
    };
    handle.recorder.stop();
  });
}

/** Load clip metadata for all clips, optionally filtered by gameId. */
export function listClipMeta(gameId?: string): ClipMeta[] {
  const all = loadMeta();
  return gameId ? all.filter((m) => m.gameId === gameId) : all;
}

/** Resolve a ClipMeta to a full Clip with a blob URL. */
export async function loadClip(meta: ClipMeta): Promise<Clip | null> {
  const blob = await loadBlob(meta.id);
  if (!blob) return null;
  return { ...meta, url: URL.createObjectURL(blob) };
}

/** Delete a clip (both blob and metadata). */
export async function deleteClip(id: string): Promise<void> {
  await deleteBlob(id);
  const metas = loadMeta().filter((m) => m.id !== id);
  saveMeta(metas);
}

/** Export a clip by triggering a browser download of the .webm file. */
export async function exportClip(meta: ClipMeta): Promise<void> {
  const blob = await loadBlob(meta.id);
  if (!blob) throw new Error('Clip data not found.');
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `${meta.gameTitle.replace(/[^a-z0-9]/gi, '-').toLowerCase()}-${meta.id}.webm`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  setTimeout(() => URL.revokeObjectURL(url), 5000);
}

/** Copy a local file URL string for the clip to the clipboard. */
export async function copyClipLink(meta: ClipMeta): Promise<void> {
  const blob = await loadBlob(meta.id);
  if (!blob) throw new Error('Clip data not found.');
  const url = URL.createObjectURL(blob);
  await navigator.clipboard.writeText(url);
}

/** Format seconds as mm:ss */
export function formatClipDuration(secs: number): string {
  const m = Math.floor(secs / 60);
  const s = secs % 60;
  return `${m}:${s.toString().padStart(2, '0')}`;
}
