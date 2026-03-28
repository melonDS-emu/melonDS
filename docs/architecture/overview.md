# RetroOasis Architecture Overview

## What is RetroOasis?

RetroOasis is a **multiplayer-first unified Nintendo emulator platform**. It supports systems from NES through Nintendo DS, built around lobbies, friend sessions, and social play — not just single-player emulation.

## Who is it for?

- Friends who want to play retro Nintendo multiplayer games together quickly
- Casual gamers who want a curated, inviting experience
- Users who want a "retro gaming social hub" rather than a technical emulator frontend

## Why Multiplayer-First?

Most emulators treat multiplayer as an afterthought. RetroOasis makes it the main event:
- **Host/Join** rooms in seconds
- **Friend presence** and invites
- **Per-game intelligence** — the app knows which games are best for multiplayer
- **Session templates** — launch preconfigured multiplayer setups

## How is this different from RetroArch?

| Aspect | RetroArch | RetroOasis |
|--------|-----------|------------|
| Focus | Universal emulation | Nintendo multiplayer |
| UX | Technical/power-user | Casual/inviting |
| Multiplayer | Available but complex | First-class feature |
| Discovery | File browser | Curated multiplayer library |
| Social | None | Friends, lobbies, presence |

## Layered Architecture

```
┌─────────────────────────────────────────────┐
│            Frontend (React + Vite)           │
│   Home · Library · Game Details · Lobby      │
├─────────────────────────────────────────────┤
│          Multiplayer Services Layer          │
│   Lobby Server · Presence · Session Engine   │
├─────────────────────────────────────────────┤
│          Orchestration / Bridge Layer        │
│   Emulator Bridge · Save System · Game DB    │
├─────────────────────────────────────────────┤
│           Emulator Backends (External)       │
│  FCEUX · Snes9x · mGBA · SameBoy · VBA-M    │
│  Mupen64Plus · melonDS                        │
└─────────────────────────────────────────────┘
```

## Monorepo Structure

```
/apps
  /desktop          — React + Vite desktop frontend (browser-based today; Tauri/native packaging planned)
  /lobby-server     — WebSocket lobby server + local HTTP launch API

/packages
  /ui               — Shared UI components
  /shared-types     — Core TypeScript domain types
  /game-db          — Game catalog + multiplayer metadata seed
  /session-engine   — Session lifecycle management
  /emulator-bridge  — Emulator backend abstraction layer
  /save-system      — Local + cloud save management
  /presence-client  — Friend presence and social features
  /multiplayer-profiles — Per-game multiplayer configuration + input

/docs
  /architecture     — System design documentation
  /mvp              — MVP planning and scope
  /ux               — UX design guidance

/src                — melonDS emulator core (existing C++ codebase)
```

## Key Design Decisions

1. **Do not build emulator cores** — Use proven backends (mGBA, melonDS, etc.)
2. **TypeScript frontend** — React + Vite for fast, maintainable UI
3. **WebSocket lobby server** — Real-time room management
4. **Per-game intelligence** — Metadata drives the UX (player count, modes, badges)
5. **Modular packages** — Each concern is a separate, testable package
6. **Native packaging is planned, not finished** — the current frontend can be packaged with Tauri later, but the Tauri shell and IPC bridge are not in this repo yet

## Supported Systems

| System | Backend | Fallback(s) | Phase |
|--------|---------|-------------|-------|
| NES | FCEUX | — | 1 |
| SNES | Snes9x | bsnes | 1 |
| Game Boy | mGBA | SameBoy, Gambatte | 1 |
| Game Boy Color | mGBA | SameBoy, Gambatte | 1 |
| Game Boy Advance | mGBA | VisualBoyAdvance-M | 1 |
| Nintendo 64 | Mupen64Plus | Project64 | 2 |
| Nintendo DS | melonDS | DeSmuME | 3 |

### Link Cable Emulation

GB, GBC, and GBA games that use the link cable (Pokémon trades/battles, Zelda Four Swords, etc.) connect
through the RetroOasis TCP relay using the emulator's link cable TCP flag:

| Backend | Link Cable Flag |
|---------|----------------|
| mGBA | `--link-host <host>:<port>` |
| SameBoy | `--link-address <host>:<port>` |
| VisualBoyAdvance-M | `--link-host <host>:<port>` |
