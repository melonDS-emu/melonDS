<h2 align="center">🌴 <b>RetroOasis</b></h2>
<p align="center"><em>A multiplayer-first unified Nintendo emulator platform</em></p>
<p align="center">
<a href="https://www.gnu.org/licenses/gpl-3.0" alt="License: GPLv3"><img src="https://img.shields.io/badge/License-GPL%20v3-%23ff554d.svg"></a>
</p>

RetroOasis is a social Nintendo retro gaming hub — not just another emulator frontend. It unifies systems from NES through Nintendo DS and puts **multiplayer and social play** at the center of the experience. Think Discord + game lobby system + console-style launcher for retro Nintendo games.

## ✨ Features

- **Multiplayer-first** — Host/join rooms in seconds, invite friends, ready up, and play
- **Unified library** — NES, SNES, GB, GBC, GBA, N64, and NDS in one app
- **Curated discovery** — Browse games by multiplayer mode (Party, Co-op, Versus, Link, Trade)
- **Per-game intelligence** — Player counts, compatibility badges, session templates
- **Social presence** — See what friends are playing, join their sessions
- **Save management** — Local and cloud save sync with conflict resolution

## 🚀 Quick Start

```bash
# Install dependencies
npm install

# Start the desktop frontend (React + Vite)
npm run dev:desktop

# Start the lobby server (WebSocket)
npm run dev:server

# Build the desktop app
npm run build:desktop
```

## 📁 Project Structure

```
/apps
  /desktop          — React + Vite desktop frontend (Tauri-ready)
  /lobby-server     — WebSocket-based lobby/room server

/packages
  /ui               — Shared UI components
  /shared-types     — Core TypeScript domain types
  /game-db          — Game catalog + multiplayer metadata (30 games)
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

| System | Backend | Phase |
|--------|---------|-------|
| NES | FCEUX | 1 |
| SNES | Snes9x | 1 |
| Game Boy | mGBA | 1 |
| Game Boy Color | mGBA | 1 |
| Game Boy Advance | mGBA | 1 |
| Nintendo 64 | Mupen64Plus | 2 |
| Nintendo DS | melonDS | 3 |

## 🏗️ Architecture

RetroOasis uses a layered architecture:

1. **Frontend** — React + Vite + Tailwind CSS (Tauri-ready)
2. **Multiplayer Services** — WebSocket lobby server, presence, session engine
3. **Orchestration** — Emulator bridge, save system, game database
4. **Emulator Backends** — External proven emulators (mGBA, melonDS, etc.)

The app does **not** build emulator cores from scratch. It orchestrates existing backends behind a unified interface.

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
