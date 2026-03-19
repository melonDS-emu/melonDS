# Phase 21 — SEGA Genesis / Mega Drive

**Status:** ✅ Complete  
**Date:** 2026-03-19

## Summary

Phase 21 adds SEGA Genesis / Mega Drive as RetroOasis's first non-Nintendo platform. All sessions use RetroArch with the Genesis Plus GX libretro core (`genesis_plus_gx_libretro.so`), running through the standard RetroOasis TCP relay netplay system — no port forwarding required.

## What Was Built

### Core Platform Support
- `genesis` added to `SupportedSystem` type in `packages/shared-types/src/systems.ts`
- `SYSTEM_INFO['genesis']`: generation 4, 2 local players, SEGA blue `#0066CC`, `supportsLink: false`
- RetroArch backend `systems` list updated to include `genesis`; Genesis Plus GX core documented in backend notes

### Emulator Adapter
- `createSystemAdapter('genesis')` in `packages/emulator-bridge/src/adapters.ts`:
  - Preferred backend: `retroarch` (no fallback — RetroArch is the de-facto Genesis emulation frontend)
  - Launch args: delegates to `buildRetroArchArgs` — supports `-L <core>`, `--fullscreen`, `--host`/`--connect` relay flags, `--verbose` debug
  - Save path: `<baseDir>/genesis/<gameId>`
- Recommended core: `/usr/lib/libretro/genesis_plus_gx_libretro.so` (install via RetroArch menu)

### Session Templates (10)
| Template ID | Game | Players |
|---|---|---|
| `genesis-default-2p` | genesis-default (fallback) | 2 |
| `genesis-sonic-the-hedgehog-2-2p` | Sonic the Hedgehog 2 | 2 |
| `genesis-streets-of-rage-2-2p` | Streets of Rage 2 | 2 |
| `genesis-mortal-kombat-3-2p` | Mortal Kombat 3 | 2 |
| `genesis-nba-jam-2p` | NBA Jam | 2 |
| `genesis-contra-hard-corps-2p` | Contra: Hard Corps | 2 |
| `genesis-gunstar-heroes-2p` | Gunstar Heroes | 2 |
| `genesis-golden-axe-2p` | Golden Axe | 2 |
| `genesis-toejam-and-earl-2p` | ToeJam & Earl | 2 |
| `genesis-earthworm-jim-2-2p` | Earthworm Jim 2 | 2 |

All templates: `emulatorBackendId: 'retroarch'`, `netplayMode: 'online-relay'`, `latencyTarget: 100`

### Mock Game Catalog (9 games)
All games have `system: 'Genesis'` and `systemColor: '#0066CC'`:

| Game | Genre | Max Players | Tags |
|---|---|---|---|
| Sonic the Hedgehog 2 | Platformer | 2 | Competitive |
| Streets of Rage 2 | Beat-em-up | 2 | Co-op |
| Mortal Kombat 3 | Fighting | 2 | Competitive |
| NBA Jam | Sports | 2 | Competitive |
| Contra: Hard Corps | Run-and-Gun | 2 | Co-op |
| Gunstar Heroes | Run-and-Gun | 2 | Co-op |
| Golden Axe | Beat-em-up | 2 | Co-op |
| ToeJam & Earl | Adventure | 2 | Co-op |
| Earthworm Jim 2 | Platformer | 2 | Competitive |

### Frontend
- `GenesisPage` (`/genesis`) in `apps/desktop/src/pages/GenesisPage.tsx`:
  - **Lobby tab**: 3-column game grid, each card has Quick Match + Host Room buttons; Genesis Plus GX info banner explaining relay netplay and supported ROM formats (`.md`, `.bin`, `.gen`, `.smd`)
  - **Leaderboard tab**: top 10 global players by session count
  - SEGA blue colour scheme (`#0066CC`)
- `--gradient-genesis: linear-gradient(135deg, #0066cc 0%, #003366 100%)` added to `index.css`
- 🔵 Genesis nav item added in `Layout.tsx`
- `/genesis` route added in `App.tsx`

## Test Coverage
- 45 unit tests in `apps/lobby-server/src/__tests__/phase-21.test.ts`
- **Genesis system type** (5 tests): SYSTEM_INFO presence, generation, player count, link-cable, color
- **RetroArch backend** (3 tests): systems list, netplay support, Genesis Plus GX core note
- **Adapter launch args** (9 tests): preferred backend, fallback list, ROM path, fullscreen, core library, host port, client connect, save path, non-dolphin check
- **Session templates** (14 tests): all 10 templates present, fallback resolution, retroarch backend, latency, netplay mode, template count
- **Mock catalog** (11 tests): all 9 games present with correct metadata, system label, systemColor
- **Regression** (1 test): no duplicate IDs in combined catalog

## Known Limitations
- Genesis Plus GX core must be installed separately via RetroArch's core downloader
- All Genesis sessions are 2-player only (Genesis hardware supports 2 controllers on standard ports)
- Multitap (4-player) support is possible with some games but not implemented in this phase
- ROM format auto-detection relies on file extension (`.md`, `.bin`, `.gen`, `.smd`)
