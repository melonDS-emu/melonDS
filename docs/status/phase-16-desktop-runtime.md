# Phase 16 — Desktop Runtime & Core Integration

**Status:** ✅ Complete  
**Date:** 2026-03-18

---

## Summary

Phase 16 hardened the RetroOasis desktop app from a browser prototype into a
native-desktop-ready runtime. The key deliverables were:

1. Tauri IPC wired into the launch flow
2. Real native ROM discovery (replacing the simulated scan)
3. Per-game ROM selection persisted in localStorage
4. melonDS IPC bridge integration spike
5. Environment-driven config hardening
6. End-to-end launch validation notes per system family

---

## Milestones

### ✅ 1. Tauri desktop shell integration

`apps/desktop/src/lib/tauri-ipc.ts` defines the full typed command surface:

| Command              | Tauri handler (planned)   | Non-Tauri fallback          |
|----------------------|---------------------------|-----------------------------|
| `scan_rom_directory` | Native filesystem walk    | `GET /api/roms/scan`        |
| `pick_file`          | `tauri-plugin-dialog`     | Returns `{ path: null }`    |
| `pick_directory`     | `tauri-plugin-dialog`     | Returns `{ path: null }`    |
| `launch_emulator`    | `std::process::Command`   | `POST /api/launch`          |

`LobbyContext` now calls `tauriLaunchEmulator()` instead of `fetch('/api/launch')`,
so native builds get the Rust-side launch path automatically while the browser
dev build continues to use the HTTP fallback.

**Remaining Rust work** (tracked separately):
- Scaffold `src-tauri/` (`tauri.conf.json`, `Cargo.toml`, `src/main.rs`)
- Implement Rust handlers for the four commands above
- Wire `npm run build:tauri` into the workspace

---

### ✅ 2. Native ROM discovery wired into library

`LibraryPage` "Scan ROMs" now calls `tauriScanRoms(romDir, recursive=true)`.
In the Tauri native build this performs a real filesystem walk; in browser/dev
mode it falls back to `GET /api/roms/scan` on the lobby server.

After a successful scan:
- Each discovered ROM is auto-associated to its inferred game ID via
  `setRomAssociation()`.
- The scan result count is shown on the button (`✓ Found N ROMs`).
- Scan errors surface a red banner rather than silently failing.

---

### ✅ 3. Per-game ROM selection

`apps/desktop/src/lib/rom-library.ts` persists a `gameId → romPath` map in
`localStorage`.

`GameDetailsPage` now shows:
- The current associated ROM file path (if set).
- A **📂 Set ROM File** button (Tauri only) that opens the native file picker.
- A **✕ Clear** button to remove the association.
- A **📂 Change** button to replace an existing association.

The `resolveGameRomPath()` function (used in the launch path) resolves:
1. Explicit per-game association → used directly.
2. ROM directory fallback → the configured ROM directory.
3. No path → `null`; emulator auto-launch is skipped.

`SettingsPage` also gained **📂 Browse** buttons (Tauri only) next to both the
ROM directory and Save directory inputs so users don't have to type absolute
paths manually.

---

### ✅ 4. melonDS IPC bridge — initial integration spike

`apps/lobby-server/src/melonds-ipc.ts` documents the planned IPC protocol
(Unix domain socket / named Windows pipe) and provides a `MelonDsIpcStub`
that logs all commands and returns predictable placeholder responses.

The stub is exercised by the Phase 16 unit tests (`phase-16.test.ts`) so the
integration contract is validated in CI even before the real C++ bridge is
implemented.

**Remaining C++ work** (tracked separately):
- Integrate `CMakeLists.txt` into the npm workspace build.
- Implement the IPC server in C++ (bind to
  `/tmp/retro-oasis-melonds-<sessionId>.sock`).
- Replace `MelonDsIpcStub` with the real socket client.
- Wire Tauri sidecar / child_process launch into the bridge.

---

### ✅ 5. Network/runtime config hardening

`apps/desktop/.env.example` now documents **all** environment variables:

| Variable             | Default                    | Description                             |
|----------------------|----------------------------|-----------------------------------------|
| `VITE_API_MODE`      | `mock`                     | `mock` / `backend` / `hybrid`           |
| `VITE_BACKEND_URL`   | `http://localhost:8080`    | Backend HTTP base URL                   |
| `VITE_WS_URL`        | `ws://localhost:8080`      | Lobby WebSocket URL                     |
| `VITE_LAUNCH_API_URL`| `http://localhost:8080`    | Launch API fallback base URL            |
| `VITE_SERVER_URL`    | `http://localhost:8080`    | Community / social API URL              |
| `VITE_RELAY_HOST`    | *(server-provided)*        | Override relay hostname in UI banner    |

`VITE_RELAY_HOST` is new in Phase 16. When set, it overrides the `relayHost`
field from the server's `game-starting` event, enabling public deployments to
show the correct relay address in the session banner without modifying the
server config.

---

### ✅ 6. End-to-end launch validation — system family status

| System Family    | Emulator Backend | Netplay Path        | ROM Discovery  | Status              |
|------------------|-----------------|---------------------|----------------|---------------------|
| NES / SNES       | FCEUX / Snes9x  | HTTP API fallback   | Scan + assoc.  | ✅ Flow wired       |
| GB / GBC / GBA   | SameBoy / mGBA  | p2p link cable      | Scan + assoc.  | ✅ Flow wired       |
| N64              | Mupen64Plus     | NetPlay relay TCP   | Scan + assoc.  | ✅ Flow wired       |
| NDS              | melonDS         | WFC / relay         | Scan + assoc.  | ✅ IPC stub wired   |

> **Note:** "Flow wired" means `tauriLaunchEmulator` is called with the correct
> `backendId`, `netplayHost`, `netplayPort`, and `sessionToken`. The Rust-side
> process spawning and the C++ melonDS IPC are still stubs / not yet compiled.
> Full end-to-end validation requires the `src-tauri/` package and the native
> binary.

---

## Tests added

- `apps/desktop/src/lib/__tests__/rom-library.test.ts` — 13 tests covering
  `setRomAssociation`, `getRomAssociation`, `clearRomAssociation`,
  `getAllAssociations`, and `resolveGameRomPath`.
- `apps/lobby-server/src/__tests__/phase-16.test.ts` — 15 tests covering
  `MelonDsIpcStub` (all command variants, connect/disconnect lifecycle) and
  the `defaultBackendId` mapping contract.

---

## Known limitations

- The `src-tauri/` Rust package is not yet scaffolded; all Tauri commands use
  HTTP fallbacks or return `{ path: null }`.
- The `MelonDsIpcStub` is a no-op; no actual IPC channel exists.
- `inferGameId()` in `LibraryPage` uses a simple slug heuristic and may not
  match all game catalog entries. A fuzzy-match step will be added in Phase 17.
