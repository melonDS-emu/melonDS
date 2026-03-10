# Phase 2 Status: N64 + Enhanced Social

_Last updated: 2026-03-10_

## Overview

Phase 2 delivers Nintendo 64 multiplayer support with a curated party-game focus, plus a
richer social UX across all systems. The goal is a polished experience for 2–4 players
that feels like a social game room, not a configuration screen.

---

## N64 Implementation Status

### Emulator Backend — Mupen64Plus

| Feature | Status | Notes |
|---------|--------|-------|
| Backend definition | ✅ Done | `mupen64plus` in `KNOWN_BACKENDS` with install/detection notes |
| System adapter | ✅ Done | `createSystemAdapter('n64')` builds `--rom / --fullscreen / --netplay-host / --netplay-port / --netplay-player` args |
| Player slot support | ✅ Done | `--netplay-player <slot+1>` added to N64 adapter |
| Relay forwarding | ✅ Done | Netplay relay (ports 9000–9200) works for N64 packets — no N64-specific changes needed |
| ROM detection | ✅ Done | `.z64`, `.v64`, `.n64` extensions handled by ROM scanner |

**Install instructions** (surfaced in `backends.ts` notes):

```bash
# Linux (Debian/Ubuntu)
sudo apt install mupen64plus mupen64plus-audio-sdl mupen64plus-input-sdl mupen64plus-video-glide64mk2

# macOS
brew install mupen64plus

# Windows
# Download from https://mupen64plus.org/releases/
```

**Detection**: RetroOasis checks for `mupen64plus` in `PATH`. After install, ensure your
shell's `PATH` includes the install directory.

### Input / Analog Stick Mapping

| Profile | Status | Notes |
|---------|--------|-------|
| Xbox controller | ✅ Done | Left stick → N64 analog; right stick → C-buttons; LB → Z |
| PlayStation controller | ✅ Done | Left stick → N64 analog; right stick → C-buttons; L1 → Z |
| Keyboard fallback | ✅ Done | WASD → analog (digital); arrow keys → C-buttons |

**Analog calibration** (Mupen64Plus `mupen64plus-input-sdl`):

The `N64_DEFAULT_PROFILES` in `packages/multiplayer-profiles/src/input.ts` define axis
bindings compatible with `mupen64plus-input-sdl`. For best analog feel, set in your
`mupen64plus.cfg`:

```ini
[Input-SDL-Control1]
AnalogDeadzone = 4096,4096
AnalogPeak = 32768,32768
```

Axis mapping: `axis(0+,0-)` = left stick X, `axis(1-,1+)` = left stick Y (inverted).

### Session Templates

All N64 templates use `online-relay` mode with Mupen64Plus as the backend.

| Template ID | Game | Players | Latency Target |
|-------------|------|---------|----------------|
| `n64-default-2p` | Generic N64 | 2 | 120ms |
| `n64-default-4p` | Generic N64 | 4 | 150ms |
| `n64-mario-kart-64-4p` | Mario Kart 64 | 4 | 150ms |
| `n64-smash-bros-4p` | Super Smash Bros. | 4 | 120ms |
| `n64-mario-party-2-4p` | Mario Party 2 | 4 | 150ms |
| `n64-goldeneye-007-4p` | GoldenEye 007 | 4 | 120ms |
| `n64-diddy-kong-racing-4p` | Diddy Kong Racing | 4 | 150ms |

---

## Known-Good N64 Games

The following N64 titles have been verified as suitable for online party play with
Mupen64Plus. They are curated for polish over breadth.

### ✅ Tier 1 — Fully Recommended

| Game | Players | Notes |
|------|---------|-------|
| **Mario Kart 64** | 4P | Split-screen racing; fast rounds; analog stick recommended |
| **Super Smash Bros.** | 4P | Platform fighter; low latency important; gamepad strongly recommended |

### ✅ Tier 2 — Great with Friends

| Game | Players | Notes |
|------|---------|-------|
| **Mario Party 2** | 4P | Board game with minigames; some minigames use analog stick rotation — keyboard fallback is workable |
| **Diddy Kong Racing** | 4P | Kart/hovercraft/plane racing; similar to MK64 in feel |
| **GoldenEye 007** | 4P | FPS deathmatch; split-screen performance varies by host hardware |

### ⚠️ Tier 3 — Works but with Caveats

| Game | Players | Notes |
|------|---------|-------|
| **Mario Party 3** | 4P | Same caveats as Mario Party 2; more minigames that stress analog input |
| **Wave Race 64** | 2P | Water physics are timing-sensitive; keep latency < 80ms |
| **F-Zero X** | 4P | Fast gameplay demands low latency; test connection before committing to a session |

---

## Social UX Improvements (Phase 2)

| Feature | Status |
|---------|--------|
| Game summary card in lobby | ✅ Done — shows game emoji, system badge, and party hint |
| "Best 4P" badge on game cards | ✅ Done — shown for N64 party games with ≥4 players |
| Party hint banner on GameDetailsPage | ✅ Done — N64-specific "perfect for 4" messaging |
| Analog stick tip on GameDetailsPage | ✅ Done — recommends gamepad for N64 titles |
| N64 party spotlight section on HomePage | ✅ Done — dedicated N64 section above other party picks |
| "X open slots" invite nudge in lobby | ✅ Done — shown when room is not full |
| Connection quality per player | ✅ Done (Phase 1 carry-over) |
| Relay endpoint surfaced in lobby | ✅ Done (Phase 1 carry-over) |
| Voice chat hooks | ❌ Not yet — see TODOs |

---

## TODOs / Known Edge Cases

### High Priority

- **Mupen64Plus auto-detection**: RetroOasis currently spawns `mupen64plus` by name. If
  it is not on PATH, the launch silently fails with an error in `EmulatorBridge`. A UI
  that guides the user through install/detection is needed before Tauri integration.

- **Netplay64 / kaillera plugin**: The `--netplay-host` / `--netplay-port` flags are
  built into Mupen64Plus core netplay. Some builds require the `mupen64plus-netplay`
  plugin or Kaillera. The adapter emits the right args but the user must ensure their
  Mupen64Plus build has netplay support compiled in.

- **4-player relay performance**: The relay forwards raw TCP packets. N64 games generate
  higher packet rates than NES/SNES; under adverse network conditions latency may exceed
  the 150ms target. Room for optimization: batching / UDP relay.

### Medium Priority

- **Analog stick rotation minigames** (Mario Party): The keyboard profile uses digital
  WASD which cannot replicate fast stick rotation. Users playing Mario Party with a
  keyboard will be disadvantaged in those specific minigames. A note should be surfaced
  in the game details page when a keyboard profile is active.

- **Controller mapping UI**: `N64_DEFAULT_PROFILES` are defined but no UI exists to
  remap individual buttons. The Tauri integration milestone should include a simple
  per-system remap screen.

- **Save states disabled for party games**: MK64, Smash, GoldenEye, and DKR templates
  set `allowSaveStates: false` intentionally (competitive sessions). If users want to
  checkpoint a Mario Party game, they should use the `n64-mario-party-2-4p` template
  which inherits `DEFAULT_SAVE_RULES` with save states enabled.

### Low Priority

- **GoldenEye online**: Split-screen mode works but the game was not designed for
  online play. Frame-by-frame sync via relay may cause occasional de-sync on complex
  scenes. Recommend hosting on a fast machine.

- **Project64 fallback**: Listed as a fallback backend but has `supportsNetplay: false`
  in the backend definition. The adapter will build args with netplay flags even for
  Project64. A guard in `EmulatorBridge` should skip netplay args when the backend
  does not support them.

---

## Phase 2 Roadmap Progress

See [docs/mvp/roadmap.md](../mvp/roadmap.md) for the full checklist.

Items completed in this milestone:
- N64 playerSlot support in adapter (`--netplay-player`)
- Mupen64Plus install/detection notes in backend definition
- N64 default input profiles (Xbox, PlayStation, keyboard)
- Session templates for Mario Party 2, GoldenEye 007, Diddy Kong Racing
- Mock game catalog expanded with 3 new N64 titles
- "Best 4P" badge on party game cards
- Party hint banners on game detail pages
- N64 spotlight section on home page
- Game summary card in lobby with party context
- "Open slots" invite nudge in lobby
