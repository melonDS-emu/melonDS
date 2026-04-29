# MelonPrime Patch System

This document describes how to implement a new runtime ARM9 patch in MelonPrimeDS.

## Overview

Patches are standalone `.h` / `.cpp` pairs under `src/frontend/qt_sdl/MelonPrimePatch*.{h,cpp}`.
All headers are aggregated by `MelonPrimePatch.h` (included by `MelonPrime.cpp` and any other caller).

All patch code is wrapped in `#ifdef MELONPRIME_DS … #endif`.

When a patch needs code inside files that do **not** start with `MelonPrime`,
keep the added code behind `#ifdef MELONPRIME_DS … #endif`.  If the addition is
more than a tiny call site, put the MelonPrime-specific body in a
`MelonPrime*.inc` file and include that file from the guarded region.

---

## File structure

### Header pattern (`MelonPrimePatchFoo.h`)

```cpp
#ifndef MELON_PRIME_PATCH_FOO_H
#define MELON_PRIME_PATCH_FOO_H

#ifdef MELONPRIME_DS

#include <cstdint>

// Forward-declare only what the API uses
namespace Config { class Table; }
namespace melonDS { class NDS; }

namespace MelonPrime {

    void Foo_ApplyOnce(melonDS::NDS* nds, Config::Table& cfg, uint8_t romGroupIndex);
    void Foo_ResetPatchState();
    // Add Foo_RestoreOnce(melonDS::NDS*, const RomAddresses&) if mid-session restore is needed

} // namespace MelonPrime

#endif // MELONPRIME_DS
#endif // MELON_PRIME_PATCH_FOO_H
```

### Implementation pattern (`MelonPrimePatchFoo.cpp`)

```cpp
#ifdef MELONPRIME_DS

#include "MelonPrimePatchFoo.h"
#include "Config.h"
#include "NDS.h"

namespace MelonPrime {

// Per-ROM base addresses. Use 0xFFFFFFFFu for ROM versions where the patch
// does not apply (already fixed upstream, or different code layout).
// ROM group order: JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
static constexpr uint32_t kBase[7] = {
    0x0206XXXX, // JP1_0
    0xFFFFFFFF, // JP1_1  (not applicable)
    0x0206XXXX, // US1_0
    0xFFFFFFFF, // US1_1  (not applicable)
    0x0206XXXX, // EU1_0
    0xFFFFFFFF, // EU1_1  (not applicable)
    0xFFFFFFFF, // KR1_0  (not applicable)
};

// Patch entries: byte offset from base, value to write (apply), original value (revert).
// If all ROM versions share the same instruction values (common), store offsets+values once.
// If values differ per ROM, use a per-ROM table instead.
struct PatchWord { uint32_t offset, applyVal, revertVal; };
static constexpr PatchWord kWords[] = {
    {0x000u, 0xXXXXXXXX, 0xXXXXXXXX},
    // ...
};

static bool s_applied = false;

void Foo_ApplyOnce(melonDS::NDS* nds, Config::Table& cfg, uint8_t romGroupIndex)
{
    if (s_applied) return;
    if (!cfg.GetBool("Metroid.BugFix.Foo")) return;
    if (romGroupIndex >= 7 || kBase[romGroupIndex] == 0xFFFFFFFFu) return;
    const uint32_t base = kBase[romGroupIndex];
    for (const auto& w : kWords)
        nds->ARM9Write32(base + w.offset, w.applyVal);
    s_applied = true;
}

void Foo_ResetPatchState()
{
    s_applied = false;
}

// Optional: mid-session restore (e.g. on game exit)
// void Foo_RestoreOnce(melonDS::NDS* nds, uint8_t romGroupIndex)
// {
//     if (!s_applied || kBase[romGroupIndex] == 0xFFFFFFFFu) return;
//     const uint32_t base = kBase[romGroupIndex];
//     for (const auto& w : kWords)
//         nds->ARM9Write32(base + w.offset, w.revertVal);
//     s_applied = false;
// }

} // namespace MelonPrime

#endif // MELONPRIME_DS
```

---

## Checklist for a new patch

### 1. Patch files

- [ ] Create `MelonPrimePatchFoo.h` (see header pattern above)
- [ ] Create `MelonPrimePatchFoo.cpp` (see implementation pattern above)
- [ ] Add `#include "MelonPrimePatchFoo.h"` to `MelonPrimePatch.h`
- [ ] Add `MelonPrimePatchFoo.cpp` to `CMakeLists.txt` inside the `MELONPRIME_DS` source list (around line 88–91)

### 2. Apply call site — `MelonPrime.cpp`

`HandleGameJoinInit()` is the standard apply site (runs once per in-game join, cold path).
Add after the existing `OsdColor_ApplyOnce` call:

```cpp
#ifdef MELONPRIME_DS
    InGameAspectRatio_ApplyOnce(emuInstance, localCfg, m_currentRom);
    OsdColor_ApplyOnce(emuInstance, localCfg, m_currentRom);
    Foo_ApplyOnce(emuInstance->getNDS(), localCfg, m_currentRom.romGroupIndex);
#endif
```

If the patch needs per-frame re-evaluation (rare, like OsdColor), call inside the `isInGame` block in `RunFrameHook` as well.

### 3. Reset call sites — `MelonPrime.cpp`

Two `#ifdef MELONPRIME_DS` reset blocks exist: one inside the **OnEmuStart/Reset** handler
(search for `ReloadConfigFlags()` just below) and one inside `OnEmuStop()`.
Add `Foo_ResetPatchState();` to **both**:

```cpp
#ifdef MELONPRIME_DS
    InGameAspectRatio_ResetPatchState();
    OsdColor_ResetPatchState();
    FixWifi_ResetPatchState();
    Foo_ResetPatchState();   // ← add here
#endif
```

### 4. Settings UI — `MelonPrimeInputConfig.ui`

Add a `QCheckBox` (and optional `QLabel` description) inside an existing or new collapsible section.
For bug fixes, the **BUG FIXES** section (`btnToggleBugFix` / `sectionBugFix`) already exists
immediately above the GAMEPLAY TOGGLES section in the Settings tab.

Add to `sectionBugFix`'s `QVBoxLayout`:

```xml
<item>
    <widget class="QCheckBox" name="cbMetroidFoo">
        <property name="text"><string>Short label for the patch</string></property>
    </widget>
</item>
<item>
    <widget class="QLabel" name="lblMetroidFooDesc">
        <property name="text"><string>Multi-line description of what the patch does.</string></property>
        <property name="wordWrap"><bool>true</bool></property>
    </widget>
</item>
```

### 5. Settings wiring — `MelonPrimeInputConfig.cpp`

In `setupSensitivityAndToggles(instcfg)`, add:

```cpp
// Bug fixes
ui->cbMetroidFoo->setChecked(instcfg.GetBool("Metroid.BugFix.Foo"));
```

The BUG FIXES section toggle is already registered in `setupCollapsibleSections`.
No extra `setupToggle` call is needed for new checkboxes inside the existing section.

### 6. Save — `MelonPrimeInputConfigConfig.cpp`

In `saveConfig()`, inside the "Bug fixes" block:

```cpp
instcfg.SetBool("Metroid.BugFix.Foo", ui->cbMetroidFoo->checkState() == Qt::Checked);
```

### 7. Config defaults — `Config.cpp`

In `DefaultBools`, add inside the `#ifdef MELONPRIME_DS` block
(near other `Metroid.BugFix.*` or `Metroid.Visual.*` entries):

```cpp
{"Instance*.Metroid.BugFix.Foo", true},   // or false if opt-in
```

If the config key uses `GetInt` or `GetDouble`, add it to `DefaultInts` / `DefaultDoubles` instead
(see repo-architecture.md "Default value type classification" for the type-list rule).

---

## ROM group index reference

| Index | ROM | Notes |
|-------|-----|-------|
| 0 | JP1_0 | First JP release — often has bugs fixed in JP1_1 |
| 1 | JP1_1 | Revision |
| 2 | US1_0 | First US release |
| 3 | US1_1 | Revision |
| 4 | EU1_0 | First EU release |
| 5 | EU1_1 | Revision |
| 6 | KR1_0 | Korean release |

`m_currentRom.romGroupIndex` (type `uint8_t`) holds the index for the running ROM.
Use `0xFFFFFFFFu` as the sentinel "not applicable" base address.

---

## Existing patches — reference implementations

| Patch | Files | Apply site | Notes |
|-------|-------|-----------|-------|
| In-game aspect ratio | `MelonPrimePatchAspectRatio.*` | `HandleGameJoinInit` | Uses `MainRAM` read to detect/guard; `InGameAspectRatio_ResetPatchState` on stop/reset |
| OSD color | `MelonPrimePatchOsdColor.*` | `HandleGameJoinInit` + per-frame `isInGame` block | `OsdColor_RestoreOnce` on game exit; `OsdColor_InvalidatePatch` on settings save |
| No-HUD | `MelonPrimePatchNoHud.*` | Called from Custom HUD render path | Per-frame apply/restore depending on HUD state |
| No double-tap jump | `MelonPrimePatchNoDoubleTapJump.*` | `MelonPrimeGameWeapon.cpp` (transient, wraps `FrameAdvanceTwice`) | Not persistent; applied/restored around weapon-switch frames only |
| Wi-Fi bitset fix | `MelonPrimePatchFixWifi.*` | `HandleGameJoinInit` | JP1_0 / US1_0 / EU1_0 only; 51-word patch; `FixWifi_ResetPatchState` on stop/reset |

---

## BUG FIXES section (existing UI section)

A collapsible **BUG FIXES** section is already present in the Settings tab of the MelonPrimeInputConfig dialog, located immediately above **GAMEPLAY TOGGLES**.

- Toggle button: `btnToggleBugFix`
- Section widget: `sectionBugFix` (layout: `QVBoxLayout` named `vboxBugFix`)
- Persisted key: `Metroid.UI.SectionBugFix` (default: `true`, i.e. expanded)

To add a new bug-fix checkbox, append items to `sectionBugFix`'s layout in the `.ui` file
and follow steps 5–7 above. No new `setupToggle` call is required.
