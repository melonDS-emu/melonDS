# Phase 4 — Save System Productization

**Status:** ✅ Complete  
**Date:** 2026-03-10

---

## Goals

Turn save management into a trustworthy, understandable feature for normal users:

- Local save visibility
- Cloud sync visibility
- Import / export polish
- Restore flow
- Conflict resolution UX with clear safe defaults
- Last sync / source indicators

---

## What Was Built

### `packages/save-system` (existing, unchanged)

The Node.js-backed `SaveManager` and scaffold `CloudSyncService` were already in place. They handle real file-system operations and are intended for the Electron/desktop host layer.

### `apps/desktop/src/lib/save-service.ts` (new)

Browser-compatible save service that powers the management UI. Uses `localStorage` as its persistence layer (suitable for Vite/React dev; would be replaced by Electron IPC calls in production).

Key exports:

| Export | Purpose |
|--------|---------|
| `listAllSaves()` | Returns all save records |
| `listSavesForGame(gameId)` | Filtered list per game |
| `deleteSave(id)` | Remove a save record |
| `renameSave(id, label)` | Update display label |
| `resolveConflict(id, resolution)` | Resolve local vs cloud conflict |
| `completePendingSync(id)` | Mark a pending sync as done |
| `importSaveFromFile(file, …)` | Add an imported file as a new record |
| `exportSave(record)` | Download save data via browser link |
| `getSyncStatusDisplay(status)` | User-facing label, colour, description |
| `groupByGame(saves)` | Group records by `gameId` |
| `formatFileSize(bytes)` | Human-readable size |
| `formatSaveDate(iso)` | Human-readable date/time |

The service is seeded with representative saves showing all sync states:

- **Synced** — Super Bomberman slot 0
- **Local Only** — Super Bomberman auto-save
- **Conflict** — Mario Kart 64 (local and cloud differ)
- **Pending Upload** — Super Smash Bros. (local change not yet synced)
- **Pending Download / Update Available** — Pokémon Red (cloud is newer)
- **Imported** — Tetris (imported from an external `.sav` file)

### `apps/desktop/src/pages/SavesPage.tsx` (new)

Main save management page at `/saves`. Features:

- **Summary stat cards** — total saves, synced count, conflict count (red when > 0)
- **Conflict alert banner** — appears when any save has `syncStatus === 'conflict'`; tells users what to do
- **Search bar** — filters by game title, save label, or system
- **Game-grouped list** — saves grouped by game with system badge; conflict indicator on group heading
- **SaveRow** — per-save card showing label, type badges (Auto / Save State), sync status badge, source badge, last saved date, size, last sync date, and cloud version date on conflicts
- **Action buttons**:
  - `Resolve` (conflicts only) → opens ConflictResolutionModal
  - `Restore` (pending-download only) → opens RestoreConfirmModal with warning
  - `⬇` Export → downloads save via `exportSave()`
  - `🗑` Delete → shows inline confirmation banner (safe: Cancel is visually prominent)
- **Status legend** — at the bottom of the page, explains all five status types in plain language
- **Toast notifications** — brief confirmation after actions
- **Empty state** — friendly message with icon when no saves exist
- **Import Save button** → opens file picker for `.sav`, `.state`, `.srm` files → ImportOverlay gathers game/label info before adding the record

### `apps/desktop/src/components/ConflictResolutionModal.tsx` (new)

Dedicated conflict resolution modal:

- Shows both local and cloud version metadata side-by-side (date, size)
- Clear explanation of what will happen to the losing version
- **Safe default is Cancel** — the "Cancel — Decide Later" button is the most prominent action (accent colour, full width)
- Destructive actions (Keep Local, Use Cloud) are smaller buttons in the card background colour

### `apps/desktop/src/components/Layout.tsx` (updated)

Added **💾 Saves** nav item to the sidebar, between Library and the Friends panel.

### `apps/desktop/src/App.tsx` (updated)

Added `/saves` route pointing to `SavesPage`.

---

## Acceptance Criteria

| Criterion | Status |
|-----------|--------|
| Users can tell whether saves are local, synced, or conflicted | ✅ Status badges and colour coding on every save row |
| Restore / import / export is understandable | ✅ Each flow has a labelled button, a confirmation step, and plain-language descriptions |
| Conflict resolution has a clear safe path | ✅ Cancel is the default prominent action; destructive options are secondary |

---

## Known Limitations / Future Work

- The browser-layer `save-service.ts` uses `localStorage` and stub export data. Production integration requires Electron IPC calls that delegate to the Node.js `SaveManager` in `packages/save-system`.
- `CloudSyncService` in `packages/save-system` is a scaffold. Actual cloud upload/download (S3 or equivalent) must be wired up in a future phase.
- Save file binary content is not transferred during export (JSON metadata only) until the Electron bridge is in place.
- "Restore" and conflict resolution currently only update `syncStatus` in `localStorage`. Real sync operations will be driven by the cloud backend.
