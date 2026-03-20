<h2 align="center">🌴 <b>RetroOasis</b></h2>
<p align="center"><em>A multiplayer-first retro gaming emulator platform — from the SMS to the Wii U</em></p>
<p align="center">
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="License: GPLv3"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg"></a>
</p>

RetroOasis is a social retro gaming hub spanning multiple platforms and generations — not just another emulator frontend. It unifies systems from the **Sega Master System era through the Wii U and 3DS** and puts **multiplayer and social play** at the center of the experience. Think Discord + game lobby system + console-style launcher for retro games across Nintendo, SEGA, and Sony platforms.

## ✨ Features

- **Multiplayer-first** — Host/join rooms in seconds, ready up, chat, and surface local launch/relay info from the lobby flow
- **Multi-platform library** — NES, SNES, GB, GBC, GBA, N64, NDS, GC, Wii, Wii U, 3DS, Genesis, Dreamcast, PSX, PS2, and PSP in one app
- **Curated discovery** — Browse games by multiplayer mode (Party, Co-op, Versus, Link, Trade)
- **Per-game intelligence** — Player counts, compatibility badges, session templates
- **Live social layer** — Friends, direct messaging, presence, achievements, and player profile stats are integrated into the desktop app
- **Community hub + ranked play** — `/community` includes activity feed, game ratings/reviews, and global/per-game ELO leaderboards with Bronze→Diamond tiers
- **Save management UI** — Local save flows and conflict UX are available today; cloud sync remains planned

## 🚀 Quick Start

```bash
# Install dependencies
npm install

# Start the desktop frontend (React + Vite, http://localhost:5173)
npm run dev:desktop

# Start the lobby server (WebSocket + HTTP on ws://localhost:8080)
npm run dev:server

# Build the desktop app for production
npm run build:desktop

# Type-check all packages
npm run typecheck
```

> All commands run from the repository root.  No other setup is required for
> local development — `npm install` handles all workspace packages.
>
> **Configuration:** copy `apps/desktop/.env.example` → `apps/desktop/.env.local`
> and `apps/lobby-server/.env.example` → `apps/lobby-server/.env` to override
> defaults.  The defaults work out-of-the-box for local development.

## 📁 Project Structure

```
/apps
  /desktop          — React + Vite desktop frontend (browser-based today; Tauri planned)
  /lobby-server     — WebSocket lobby/room server + local HTTP launch API

/packages
  /ui               — Shared UI components
  /shared-types     — Core TypeScript domain types
  /game-db          — Game catalog + multiplayer metadata
  /session-engine   — Session lifecycle management
  /emulator-bridge  — Emulator backend abstraction layer
  /save-system      — Local + cloud save management
  /presence-client  — Friend presence and social features
  /multiplayer-profiles — Per-game multiplayer config + input profiles

/src                — melonDS emulator core (existing C++ codebase)

/docs
  /architecture     — System design documentation
  /mvp              — MVP planning and phase scope
  /ux               — UX design guidance and style guide
```

## 🎮 Supported Systems

| System | Manufacturer | Backend | Generation |
|--------|-------------|---------|------------|
| NES | Nintendo | FCEUX | 3rd |
| SNES | Nintendo | Snes9x | 4th |
| Game Boy | Nintendo | mGBA | 4th |
| Game Boy Color | Nintendo | mGBA | 4th |
| SEGA Genesis | SEGA | RetroArch / Genesis Plus GX | 4th |
| Game Boy Advance | Nintendo | mGBA | 6th |
| Nintendo 64 | Nintendo | Mupen64Plus | 5th |
| Sony PlayStation | Sony | DuckStation | 5th |
| Nintendo GameCube | Nintendo | Dolphin | 6th |
| Sony PlayStation 2 | Sony | PCSX2 | 6th |
| SEGA Dreamcast | SEGA | Flycast | 6th |
| Nintendo DS | Nintendo | melonDS | 7th |
| Nintendo Wii | Nintendo | Dolphin | 7th |
| Sony PSP | Sony | PPSSPP | 7th |
| Nintendo 3DS | Nintendo | Citra / Lime3DS | 8th |
| Nintendo Wii U | Nintendo | Cemu | 8th |

## 🏗️ Architecture

RetroOasis uses a layered architecture:

1. **Frontend** — React + Vite + Tailwind CSS (native packaging via Tauri is still planned)
2. **Multiplayer Services** — WebSocket lobby server, presence scaffolding, session engine
3. **Orchestration** — Emulator bridge, save system, game database
4. **Emulator Backends** — External proven emulators (mGBA, melonDS, Dolphin, Cemu, PPSSPP, etc.)

The app does **not** build emulator cores from scratch. It orchestrates existing backends behind a unified interface.

Today the backend/server side combines live WebSocket rooms with a growing REST surface (games, saves, friends, matchmaking, achievements/stats, events, direct messages, reviews/activity/rankings). Native desktop packaging + deeper emulator-core integration remain roadmap work.

> Users must provide their own legally obtained game files.

## 📖 Documentation

- [Architecture Overview](./docs/architecture/overview.md)
- [MVP Phase 1](./docs/mvp/phase1.md)
- [Development Roadmap](./docs/mvp/roadmap.md)
- [UX Design Guide](./docs/ux/design-guide.md)

## melonDS Core

This repository is built on top of [melonDS](https://melonds.kuribo64.net/), a fast and accurate Nintendo DS emulator. The melonDS C++ codebase is in `src/` and can be built separately — see [BUILD.md](./BUILD.md) for build instructions.

## Licenses

[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

RetroOasis and melonDS are free software: you can redistribute and/or modify
them under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

### External
* Images used in the Input Config Dialog - see `src/frontend/qt_sdl/InputConfig/resources/LICENSE.md`
