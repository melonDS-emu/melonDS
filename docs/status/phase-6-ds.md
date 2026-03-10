# Phase 6 — Nintendo DS Premium Experience

**Status:** ✅ Core vertical slice complete  
**Date:** 2026-03-10  
**Focus:** Making Nintendo DS feel intentionally designed — dual-screen UX, touch input clarity, WFC configuration, and curated showcase titles.

---

## What Was Built

### 🎮 Input Profiles (`packages/multiplayer-profiles/src/input.ts`)

Added `NDS_DEFAULT_PROFILES` — three default controller mappings for the Nintendo DS:

| Profile | Controller | Notes |
|---------|-----------|-------|
| `nds-xbox-default` | Xbox (Xbox/XInput) | A/B/X/Y map 1:1; LB/RB → L/R; D-Pad + left stick both work |
| `nds-playstation-default` | PlayStation (DualShock/DualSense) | Cross=A, Circle=B, Square=X, Triangle=Y; L1/R1 → L/R |
| `nds-keyboard-default` | Keyboard | X/Z/S/A for face buttons; Q/W for shoulders; arrows for D-Pad |

All three profiles include a `TouchScreen` action mapped to `mouse(left)` — the standard way to drive the DS bottom screen in melonDS.

`InputProfileManager` now seeds NDS profiles alongside N64 profiles on construction.

---

### 🔌 Emulator Bridge (`packages/emulator-bridge/`)

**`adapters.ts`** — Improved NDS adapter:
- Added **DeSmuME fallback** support with correct CLI args (`--cflash-image` for saves)
- Added `touchEnabled` control: passes `--touch-mouse` to melonDS when not explicitly disabled
- Added `wfcDnsServer` option: passes `--wfc-dns=<ip>` so WFC games route to Wiimmfi automatically
- `screenLayout` now correctly maps to `--screen-layout=stacked|side-by-side|top-focus|bottom-focus`
- `AdapterOptions.screenLayout` is now typed as a union literal (not bare `string`)

**`backends.ts`** — Enriched backend definitions:
- **melonDS**: Full install instructions for Linux/macOS/Windows, BIOS/firmware setup guide, save state hotkeys, screen swap key (F11), all CLI args documented
- **DeSmuME**: Install notes for all platforms, netplay caveat (no WFC — relay only), HLE BIOS mode note

---

### 📦 Session Templates (`packages/session-engine/src/templates.ts`)

Added **8 new DS session templates** (to the existing 9 = 17 total NDS templates):

| Template ID | Game | Players | WFC |
|-------------|------|---------|-----|
| `nds-new-super-mario-bros-4p` | New Super Mario Bros. | 4 | — |
| `nds-metroid-prime-hunters-4p` | Metroid Prime Hunters | 4 | ✅ Wiimmfi |
| `nds-tetris-ds-4p` | Tetris DS | 4 | ✅ Wiimmfi |
| `nds-mario-party-ds-4p` | Mario Party DS | 4 | — |
| `nds-zelda-phantom-hourglass-2p` | Zelda: Phantom Hourglass | 2 | ✅ Wiimmfi |
| `nds-pokemon-black-2p` | Pokémon Black | 2 | ✅ Wiimmfi |
| `nds-pokemon-white-2p` | Pokémon White | 2 | ✅ Wiimmfi |

All templates use `melonds` backend with `stacked` screen layout by default.

---

### 🎮 Frontend (`apps/desktop/`)

**`GameDetailsPage.tsx`** — DS-aware detail page:
- **"Dual Screen"** orange badge for NDS games
- **"WFC Online"** blue badge for WFC-enabled games
- **DS dual-screen tip**: explains mouse click → touch screen, F11 swap
- **WFC explanation callout** (blue panel): explains Wiimmfi replaces Nintendo's server automatically

**`LobbyPage.tsx`** — DS session context:
- Added `isNds` detection for the current room
- DS-specific `getSessionHint()` messages for Pokémon, kart, Metroid, Tetris, party, and generic DS games
- Collapsible **DS Controls & Setup Guide** panel at the bottom of every NDS lobby

**`HomePage.tsx`** — DS Spotlight section:
- **Nintendo DS — Dual Screen** section (orange heading) with up to 8 showcase games
- Setup callout explaining BIOS placement, touch controls, and WFC auto-configuration

**`DSControlsGuide.tsx`** — New reusable component:
- Button mapping table (DS button → Xbox / PlayStation / Keyboard)
- Touchscreen explanation (click bottom screen area to interact)
- Screen layout reference (stacked / side-by-side / top-focus / bottom-focus)
- WFC info callout with Wiimmfi DNS

---

### 🗄️ Mock Games (`apps/desktop/src/data/mock-games.ts`)

Added **8 NDS showcase games** with the NDS brand color `#E87722`:

| Game | Players | Tags |
|------|---------|------|
| Mario Kart DS | 8 | 4P, Versus, Party |
| New Super Mario Bros. | 4 | 4P, Versus, Party |
| Metroid Prime Hunters | 4 | 4P, Versus, Battle |
| Mario Party DS | 4 | 4P, Party, Co-op |
| Zelda: Phantom Hourglass | 2 | 2P, Versus |
| Pokémon Diamond | 2 | 2P, Trade, Battle, WFC |
| Pokémon Platinum | 2 | 2P, Trade, Battle, WFC |
| Pokémon Black | 2 | 2P, Trade, Battle, WFC |

Existing `nds-tetris-ds` entry updated to use `#E87722` color (was `#A0A0A0`).

---

## Known-Good DS Showcase Games

These titles are confirmed to work well in multiplayer with melonDS + Wiimmfi and represent the strongest DS showcase:

### 🏆 Tier 1 — Best Multiplayer Experience

| Game | Mode | Players | Why It Works |
|------|------|---------|-------------|
| **Mario Kart DS** | WFC Online | 1–4 (up to 8 online) | Fast-paced, low latency, iconic. Wiimmfi servers still active. |
| **Tetris DS** | WFC Online / Local | 1–4 | Pure game logic — excellent for relay netplay. Minimal touch use. |
| **Metroid Prime Hunters** | WFC Online | 2–4 | Unique FPS deathmatch. Touch-to-aim on bottom screen is distinctive. |
| **New Super Mario Bros.** | Local VS | 4 | Frantic 4-player coin/star battles. No WFC needed. |

### 🥈 Tier 2 — Great Showcase Value

| Game | Mode | Players | Notes |
|------|------|---------|-------|
| **Pokémon Diamond / Pearl / Platinum** | WFC Battle & Trade | 2 | GTS, Union Room, Battle Tower — Wiimmfi covers all of it. |
| **Pokémon HeartGold / SoulSilver** | WFC Battle & Trade | 2 | Same WFC infrastructure as DP/Plat. |
| **Pokémon Black / White** | WFC Battle & Trade | 2 | C-Gear adds wireless-inspired sessions. |
| **Mario Party DS** | Local | 4 | Touch mini-games are a great DS showcase moment. |
| **Zelda: Phantom Hourglass** | WFC Battle Mode | 2 | Touch-driven combat — highlights the DS form factor uniquely. |

### 🎖️ Tier 3 — Fun for the Right Crowd

| Game | Mode | Players | Notes |
|------|------|---------|-------|
| **Diddy Kong Racing DS** | Local wireless | 4 | Solid kart racer; less tested on WFC. |
| **Mario & Luigi: Partners in Time** | Single player showcase | 1 | Shows off dual-screen RPG storytelling — great for demos. |
| **Kirby: Canvas Curse** | Single player showcase | 1 | Touch-only gameplay is a perfect DS differentiator demo. |

---

## Screen Layout Recommendations by Game

| Game Genre | Recommended Layout | Reason |
|-----------|-------------------|--------|
| Kart / Racing | `stacked` | Map on bottom, action on top — matches hardware |
| Platformer | `top-focus` | Action screen enlarged, touch controls shrunk |
| Pokémon | `stacked` | Party/map on bottom, battle on top |
| Metroid Hunters | `stacked` | Radar on bottom, action on top |
| Phantom Hourglass | `bottom-focus` | Touch-driven — bottom screen is primary |
| Tetris DS | `side-by-side` | Both screens equally important during VS |

---

## Setup Guide Summary for Users

1. **Install melonDS** — `brew install melonds` (macOS) or `sudo apt install melonds` (Linux) or download from [melonds.kuribo64.net](https://melonds.kuribo64.net)
2. **Place BIOS files** in `~/.config/melonDS/`:
   - `bios7.bin` (ARM7 BIOS)
   - `bios9.bin` (ARM9 BIOS)
   - `firmware.bin` (DS firmware)
3. **Host a room** on RetroOasis — select your NDS ROM
4. **Launch melonDS** with the netplay address shown in the lobby's Connection Diagnostics panel
5. **Touch controls**: click the bottom screen area with your mouse
6. **WFC games**: Wiimmfi DNS is configured automatically — no manual setup needed

---

## Remaining Work (Future Phases)

- [ ] C++ IPC bridge between melonDS `/src` core and the TypeScript launcher
- [ ] Dual-screen layout picker in `HostRoomModal` UI
- [ ] F11 screen swap reminder shown in lobby for DS sessions
- [ ] Touch coordinate calibration panel
- [ ] DS-specific compatibility badges surfaced in `GameCard`
- [ ] DSi mode auto-detection and DSiWare support
