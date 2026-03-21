export type ControllerType = 'keyboard' | 'xbox' | 'playstation' | 'generic' | 'unknown';

export interface InputBinding {
  action: string;
  key?: string;
  button?: string;
  axis?: string;
}

export interface InputProfile {
  id: string;
  name: string;
  controllerType: ControllerType;
  system: string;
  bindings: InputBinding[];
  isDefault: boolean;
}

/**
 * Default NES input profiles.
 *
 * The NES controller layout: A, B, Start, Select, D-Pad.
 *
 * FCEUX and Nestopia UE both use SDL2 input APIs:
 *  - Face buttons A/B map to gamepad buttons
 *  - D-Pad maps to hat or left stick
 */
export const NES_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'nes-xbox-default',
    name: 'NES — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'nes',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // A → NES A
      { action: 'B',      button: 'button(1)' },  // B → NES B
      { action: 'Start',  button: 'button(7)' },  // Menu/Start → NES Start
      { action: 'Select', button: 'button(6)' },  // View/Select → NES Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'nes-playstation-default',
    name: 'NES — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'nes',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // Cross  → NES A
      { action: 'B',      button: 'button(1)' },  // Circle → NES B
      { action: 'Start',  button: 'button(9)' },  // Options → NES Start
      { action: 'Select', button: 'button(8)' },  // Share/Create → NES Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'nes-keyboard-default',
    name: 'NES — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'nes',
    isDefault: true,
    bindings: [
      { action: 'A',      key: 'key(x)' },
      { action: 'B',      key: 'key(z)' },
      { action: 'Start',  key: 'key(return)' },
      { action: 'Select', key: 'key(rshift)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
    ],
  },
];

/**
 * Default SNES input profiles.
 *
 * The SNES controller layout: A, B, X, Y, L, R, Start, Select, D-Pad.
 *
 * Snes9x and higan/ares both use SDL2 conventions:
 *  - SNES B (bottom)  → Xbox A / PS Cross
 *  - SNES A (right)   → Xbox B / PS Circle
 *  - SNES Y (left)    → Xbox X / PS Square
 *  - SNES X (top)     → Xbox Y / PS Triangle
 */
export const SNES_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'snes-xbox-default',
    name: 'SNES — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'snes',
    isDefault: true,
    bindings: [
      { action: 'B',      button: 'button(0)' },  // A     → SNES B
      { action: 'A',      button: 'button(1)' },  // B     → SNES A
      { action: 'Y',      button: 'button(2)' },  // X     → SNES Y
      { action: 'X',      button: 'button(3)' },  // Y     → SNES X
      { action: 'L',      button: 'button(4)' },  // LB    → SNES L
      { action: 'R',      button: 'button(5)' },  // RB    → SNES R
      { action: 'Start',  button: 'button(7)' },  // Menu  → SNES Start
      { action: 'Select', button: 'button(6)' },  // View  → SNES Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'snes-playstation-default',
    name: 'SNES — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'snes',
    isDefault: true,
    bindings: [
      { action: 'B',      button: 'button(0)' },  // Cross     → SNES B
      { action: 'A',      button: 'button(1)' },  // Circle    → SNES A
      { action: 'Y',      button: 'button(2)' },  // Square    → SNES Y
      { action: 'X',      button: 'button(3)' },  // Triangle  → SNES X
      { action: 'L',      button: 'button(4)' },  // L1        → SNES L
      { action: 'R',      button: 'button(5)' },  // R1        → SNES R
      { action: 'Start',  button: 'button(9)' },  // Options   → SNES Start
      { action: 'Select', button: 'button(8)' },  // Share     → SNES Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'snes-keyboard-default',
    name: 'SNES — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'snes',
    isDefault: true,
    bindings: [
      { action: 'B',      key: 'key(z)' },
      { action: 'A',      key: 'key(x)' },
      { action: 'Y',      key: 'key(a)' },
      { action: 'X',      key: 'key(s)' },
      { action: 'L',      key: 'key(q)' },
      { action: 'R',      key: 'key(w)' },
      { action: 'Start',  key: 'key(return)' },
      { action: 'Select', key: 'key(rshift)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
    ],
  },
];

/**
 * Default GB / GBC input profiles.
 *
 * The Game Boy / Game Boy Color controller layout: A, B, Start, Select, D-Pad.
 * Used by mGBA, SameBoy, Gambatte, and higan for GB/GBC.
 */
export const GB_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'gb-xbox-default',
    name: 'GB/GBC — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'gb',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // A     → GB A
      { action: 'B',      button: 'button(1)' },  // B     → GB B
      { action: 'Start',  button: 'button(7)' },  // Menu  → GB Start
      { action: 'Select', button: 'button(6)' },  // View  → GB Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'gb-playstation-default',
    name: 'GB/GBC — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'gb',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // Cross  → GB A
      { action: 'B',      button: 'button(1)' },  // Circle → GB B
      { action: 'Start',  button: 'button(9)' },  // Options → GB Start
      { action: 'Select', button: 'button(8)' },  // Share   → GB Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'gb-keyboard-default',
    name: 'GB/GBC — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'gb',
    isDefault: true,
    bindings: [
      { action: 'A',      key: 'key(x)' },
      { action: 'B',      key: 'key(z)' },
      { action: 'Start',  key: 'key(return)' },
      { action: 'Select', key: 'key(rshift)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
    ],
  },
];

/**
 * Default GBC (Game Boy Color) input profiles.
 * GBC uses the same physical controls as the original Game Boy — the profiles
 * mirror GB_DEFAULT_PROFILES with `system` set to 'gbc'.
 */
export const GBC_DEFAULT_PROFILES: InputProfile[] = GB_DEFAULT_PROFILES.map((p) => ({
  ...p,
  id: p.id.replace(/^gb-/, 'gbc-'),
  name: p.name.replace('GB/GBC', 'GBC'),
  system: 'gbc',
}));

/**
 * Default GBA input profiles.
 *
 * The Game Boy Advance controller layout: A, B, L, R, Start, Select, D-Pad.
 * Used by mGBA, VBA-M, higan, and RetroArch for GBA.
 */
export const GBA_DEFAULT_PROFILES: InputProfile[] = [
  {
    id: 'gba-xbox-default',
    name: 'GBA — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'gba',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // A     → GBA A
      { action: 'B',      button: 'button(1)' },  // B     → GBA B
      { action: 'L',      button: 'button(4)' },  // LB    → GBA L
      { action: 'R',      button: 'button(5)' },  // RB    → GBA R
      { action: 'Start',  button: 'button(7)' },  // Menu  → GBA Start
      { action: 'Select', button: 'button(6)' },  // View  → GBA Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'gba-playstation-default',
    name: 'GBA — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'gba',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // Cross    → GBA A
      { action: 'B',      button: 'button(1)' },  // Circle   → GBA B
      { action: 'L',      button: 'button(4)' },  // L1       → GBA L
      { action: 'R',      button: 'button(5)' },  // R1       → GBA R
      { action: 'Start',  button: 'button(9)' },  // Options  → GBA Start
      { action: 'Select', button: 'button(8)' },  // Share    → GBA Select
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'gba-keyboard-default',
    name: 'GBA — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'gba',
    isDefault: true,
    bindings: [
      { action: 'A',      key: 'key(x)' },
      { action: 'B',      key: 'key(z)' },
      { action: 'L',      key: 'key(q)' },
      { action: 'R',      key: 'key(w)' },
      { action: 'Start',  key: 'key(return)' },
      { action: 'Select', key: 'key(rshift)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
    ],
  },
];

/**
 * Default N64 input profiles.
 *
 * The N64 controller layout: A, B, Start, Z, L, R,
 * C-Up/Down/Left/Right (camera/secondary actions),
 * D-Pad (Up/Down/Left/Right), and the analog stick.
 *
 * Analog stick mapping notes for Mupen64Plus (mupen64plus-input-sdl):
 *   - Xbox left stick  → Axis 0 (X) and Axis 1 (Y) — maps directly to N64 analog stick
 *   - Deadzone ~4096 and peak ~32768 work well for most analog-heavy games
 *   - C-buttons are mapped to the right stick (Axis 2/3) or face buttons
 */
export const N64_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'n64-xbox-default',
    name: 'N64 — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'n64',
    isDefault: true,
    bindings: [
      // Analog stick → Xbox left stick
      { action: 'AnalogStickX', axis: 'axis(0+,0-)' },
      { action: 'AnalogStickY', axis: 'axis(1-,1+)' },
      // C-buttons → Xbox right stick / bumpers
      { action: 'C-Right', axis: 'axis(2+)' },
      { action: 'C-Left',  axis: 'axis(2-)' },
      { action: 'C-Down',  axis: 'axis(3+)' },
      { action: 'C-Up',    axis: 'axis(3-)' },
      // Face buttons
      { action: 'A',     button: 'button(0)' },   // A
      { action: 'B',     button: 'button(2)' },   // X (acts as B)
      { action: 'Start', button: 'button(7)' },   // Menu/Start
      { action: 'Z',     button: 'button(4)' },   // LB → Z trigger
      // Shoulder buttons
      { action: 'L',     axis: 'axis(4+)' },      // LT → L
      { action: 'R',     axis: 'axis(5+)' },      // RT → R
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
    ],
  },
  // --- PlayStation controller (DualShock / DualSense) ---
  {
    id: 'n64-playstation-default',
    name: 'N64 — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'n64',
    isDefault: true,
    bindings: [
      // Analog stick → PS left stick
      { action: 'AnalogStickX', axis: 'axis(0+,0-)' },
      { action: 'AnalogStickY', axis: 'axis(1-,1+)' },
      // C-buttons → PS right stick
      { action: 'C-Right', axis: 'axis(2+)' },
      { action: 'C-Left',  axis: 'axis(2-)' },
      { action: 'C-Down',  axis: 'axis(3+)' },
      { action: 'C-Up',    axis: 'axis(3-)' },
      // Face buttons (PS layout: Cross=0, Circle=1, Square=2, Triangle=3)
      { action: 'A',     button: 'button(0)' },   // Cross
      { action: 'B',     button: 'button(2)' },   // Square
      { action: 'Start', button: 'button(9)' },   // Options
      { action: 'Z',     button: 'button(4)' },   // L1 → Z
      // Triggers
      { action: 'L',     axis: 'axis(4+)' },      // L2
      { action: 'R',     axis: 'axis(5+)' },      // R2
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
    ],
  },
  // --- Keyboard fallback (WASD + keys) ---
  {
    id: 'n64-keyboard-default',
    name: 'N64 — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'n64',
    isDefault: true,
    bindings: [
      // Analog stick → WASD (digital approximation)
      { action: 'AnalogStickUp',    key: 'key(w)' },
      { action: 'AnalogStickDown',  key: 'key(s)' },
      { action: 'AnalogStickLeft',  key: 'key(a)' },
      { action: 'AnalogStickRight', key: 'key(d)' },
      // C-buttons → arrow keys
      { action: 'C-Up',    key: 'key(up)'    },
      { action: 'C-Down',  key: 'key(down)'  },
      { action: 'C-Left',  key: 'key(left)'  },
      { action: 'C-Right', key: 'key(right)' },
      // Face buttons
      { action: 'A',     key: 'key(x)' },
      { action: 'B',     key: 'key(z)' },
      { action: 'Start', key: 'key(return)' },
      { action: 'Z',     key: 'key(shift)' },
      // Shoulder
      { action: 'L',     key: 'key(q)' },
      { action: 'R',     key: 'key(e)' },
      // D-Pad → numpad
      { action: 'DPadUp',    key: 'key(kp8)' },
      { action: 'DPadDown',  key: 'key(kp2)' },
      { action: 'DPadLeft',  key: 'key(kp4)' },
      { action: 'DPadRight', key: 'key(kp6)' },
    ],
  },
];

/**
 * Default NDS input profiles.
 *
 * The Nintendo DS button layout: A, B, X, Y, Start, Select, L, R,
 * D-Pad (Up/Down/Left/Right), and the touchscreen (mapped to mouse).
 *
 * melonDS CLI / SDL input conventions:
 *  - Face buttons A/B/X/Y map directly to standard gamepad buttons
 *  - L/R shoulders: triggers or bumpers depending on controller
 *  - Touchscreen: mouse or touchpad; melonDS uses the bottom screen area
 *  - No analog stick on original DS hardware; left stick → D-Pad digital
 */
export const NDS_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'nds-xbox-default',
    name: 'NDS — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'nds',
    isDefault: true,
    bindings: [
      // Face buttons (Xbox layout maps 1:1 to DS)
      { action: 'A',      button: 'button(0)' },  // A → DS A
      { action: 'B',      button: 'button(1)' },  // B → DS B
      { action: 'X',      button: 'button(2)' },  // X → DS X (held for menu shortcuts)
      { action: 'Y',      button: 'button(3)' },  // Y → DS Y
      // System buttons
      { action: 'Start',  button: 'button(7)' },  // Menu/Start → DS Start
      { action: 'Select', button: 'button(6)' },  // View/Select → DS Select
      // Shoulder buttons
      { action: 'L',      button: 'button(4)' },  // LB → DS L
      { action: 'R',      button: 'button(5)' },  // RB → DS R
      // D-Pad (Xbox hat/directional buttons)
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative (common for games that use D-Pad heavily)
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
      // Touchscreen — mouse controls the bottom screen in melonDS
      { action: 'TouchScreen', button: 'mouse(left)' },
    ],
  },
  // --- PlayStation controller (DualShock / DualSense) ---
  {
    id: 'nds-playstation-default',
    name: 'NDS — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'nds',
    isDefault: true,
    bindings: [
      // Face buttons (PS layout: Cross=A, Circle=B, Square=X, Triangle=Y)
      { action: 'A',      button: 'button(0)' },  // Cross  → DS A
      { action: 'B',      button: 'button(1)' },  // Circle → DS B
      { action: 'X',      button: 'button(2)' },  // Square → DS X
      { action: 'Y',      button: 'button(3)' },  // Triangle → DS Y
      // System buttons
      { action: 'Start',  button: 'button(9)' },  // Options → DS Start
      { action: 'Select', button: 'button(8)' },  // Share/Create → DS Select
      // Shoulder buttons (L1/R1 = DS L/R; L2/R2 not standard on DS)
      { action: 'L',      button: 'button(4)' },  // L1 → DS L
      { action: 'R',      button: 'button(5)' },  // R1 → DS R
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
      // Touchscreen — mouse controls bottom screen
      { action: 'TouchScreen', button: 'mouse(left)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'nds-keyboard-default',
    name: 'NDS — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'nds',
    isDefault: true,
    bindings: [
      // Face buttons
      { action: 'A',      key: 'key(x)' },
      { action: 'B',      key: 'key(z)' },
      { action: 'X',      key: 'key(s)' },
      { action: 'Y',      key: 'key(a)' },
      // System buttons
      { action: 'Start',  key: 'key(return)' },
      { action: 'Select', key: 'key(rshift)' },
      // Shoulder buttons
      { action: 'L',      key: 'key(q)' },
      { action: 'R',      key: 'key(w)' },
      // D-Pad → arrow keys
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      // Touchscreen — mouse click on bottom screen area
      { action: 'TouchScreen', button: 'mouse(left)' },
    ],
  },
];

/**
 * Default GameCube input profiles.
 *
 * The GameCube controller has an unusual layout:
 *  - Large A button, B (smaller), X/Y above
 *  - Analog L/R triggers (also digital click)
 *  - Z button (right shoulder, smaller)
 *  - Main analog stick (left), C-Stick (right)
 *  - D-Pad (lower-left, smaller than Nintendo standard)
 *  - Start (centre, no Select)
 *
 * Dolphin maps these to SDL2 gamepad axes and buttons.
 */
export const GC_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'gc-xbox-default',
    name: 'GC — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'gc',
    isDefault: true,
    bindings: [
      { action: 'A',     button: 'button(0)' },  // A → GC A
      { action: 'B',     button: 'button(1)' },  // B → GC B
      { action: 'X',     button: 'button(2)' },  // X → GC X
      { action: 'Y',     button: 'button(3)' },  // Y → GC Y
      { action: 'Z',     button: 'button(5)' },  // RB → GC Z
      { action: 'Start', button: 'button(7)' },  // Menu/Start → GC Start
      // Shoulder triggers (analog; also support digital press)
      { action: 'L',     axis: 'axis(4+)' },     // LT → GC L
      { action: 'R',     axis: 'axis(5+)' },     // RT → GC R
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Main stick
      { action: 'StickUp',    axis: 'axis(1-)' },
      { action: 'StickDown',  axis: 'axis(1+)' },
      { action: 'StickLeft',  axis: 'axis(0-)' },
      { action: 'StickRight', axis: 'axis(0+)' },
      // C-Stick (mapped to right stick)
      { action: 'CStickUp',    axis: 'axis(3-)' },
      { action: 'CStickDown',  axis: 'axis(3+)' },
      { action: 'CStickLeft',  axis: 'axis(2-)' },
      { action: 'CStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'gc-playstation-default',
    name: 'GC — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'gc',
    isDefault: true,
    bindings: [
      { action: 'A',     button: 'button(0)' },  // Cross  → GC A
      { action: 'B',     button: 'button(1)' },  // Circle → GC B
      { action: 'X',     button: 'button(2)' },  // Square → GC X
      { action: 'Y',     button: 'button(3)' },  // Triangle → GC Y
      { action: 'Z',     button: 'button(5)' },  // R1 → GC Z
      { action: 'Start', button: 'button(9)' },  // Options → GC Start
      { action: 'L',     axis: 'axis(4+)' },     // L2 → GC L
      { action: 'R',     axis: 'axis(5+)' },     // R2 → GC R
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'StickUp',    axis: 'axis(1-)' },
      { action: 'StickDown',  axis: 'axis(1+)' },
      { action: 'StickLeft',  axis: 'axis(0-)' },
      { action: 'StickRight', axis: 'axis(0+)' },
      { action: 'CStickUp',    axis: 'axis(3-)' },
      { action: 'CStickDown',  axis: 'axis(3+)' },
      { action: 'CStickLeft',  axis: 'axis(2-)' },
      { action: 'CStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'gc-keyboard-default',
    name: 'GC — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'gc',
    isDefault: true,
    bindings: [
      { action: 'A',     key: 'key(x)' },
      { action: 'B',     key: 'key(z)' },
      { action: 'X',     key: 'key(s)' },
      { action: 'Y',     key: 'key(a)' },
      { action: 'Z',     key: 'key(e)' },
      { action: 'Start', key: 'key(return)' },
      { action: 'L',     key: 'key(q)' },
      { action: 'R',     key: 'key(w)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      { action: 'StickUp',    key: 'key(i)' },
      { action: 'StickDown',  key: 'key(k)' },
      { action: 'StickLeft',  key: 'key(j)' },
      { action: 'StickRight', key: 'key(l)' },
    ],
  },
];

/**
 * Default Nintendo 3DS input profiles.
 *
 * The 3DS control layout (base model):
 *  - Face buttons: A, B, X, Y
 *  - System: Start, Select
 *  - Shoulder: L, R  (New 3DS adds ZL, ZR)
 *  - Circle Pad (left analog stick)
 *  - D-Pad
 *  - Touchscreen (bottom screen)
 *
 * Citra / Lime3DS maps these to SDL2 gamepad inputs.
 */
export const N3DS_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: '3ds-xbox-default',
    name: '3DS — Xbox Controller (default)',
    controllerType: 'xbox',
    system: '3ds',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // A → 3DS A
      { action: 'B',      button: 'button(1)' },  // B → 3DS B
      { action: 'X',      button: 'button(2)' },  // X → 3DS X
      { action: 'Y',      button: 'button(3)' },  // Y → 3DS Y
      { action: 'Start',  button: 'button(7)' },  // Menu/Start → 3DS Start
      { action: 'Select', button: 'button(6)' },  // View/Select → 3DS Select
      { action: 'L',      button: 'button(4)' },  // LB → 3DS L
      { action: 'R',      button: 'button(5)' },  // RB → 3DS R
      { action: 'ZL',     axis: 'axis(4+)' },     // LT → 3DS ZL (New 3DS)
      { action: 'ZR',     axis: 'axis(5+)' },     // RT → 3DS ZR (New 3DS)
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Circle Pad (left analog)
      { action: 'CirclePadUp',    axis: 'axis(1-)' },
      { action: 'CirclePadDown',  axis: 'axis(1+)' },
      { action: 'CirclePadLeft',  axis: 'axis(0-)' },
      { action: 'CirclePadRight', axis: 'axis(0+)' },
      // C-Stick (New 3DS — right stick)
      { action: 'CStickUp',    axis: 'axis(3-)' },
      { action: 'CStickDown',  axis: 'axis(3+)' },
      { action: 'CStickLeft',  axis: 'axis(2-)' },
      { action: 'CStickRight', axis: 'axis(2+)' },
      // Touchscreen
      { action: 'TouchScreen', button: 'mouse(left)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: '3ds-playstation-default',
    name: '3DS — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: '3ds',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // Cross  → 3DS A
      { action: 'B',      button: 'button(1)' },  // Circle → 3DS B
      { action: 'X',      button: 'button(2)' },  // Square → 3DS X
      { action: 'Y',      button: 'button(3)' },  // Triangle → 3DS Y
      { action: 'Start',  button: 'button(9)' },  // Options → 3DS Start
      { action: 'Select', button: 'button(8)' },  // Share/Create → 3DS Select
      { action: 'L',      button: 'button(4)' },  // L1 → 3DS L
      { action: 'R',      button: 'button(5)' },  // R1 → 3DS R
      { action: 'ZL',     axis: 'axis(4+)' },     // L2 → 3DS ZL (New 3DS)
      { action: 'ZR',     axis: 'axis(5+)' },     // R2 → 3DS ZR (New 3DS)
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'CirclePadUp',    axis: 'axis(1-)' },
      { action: 'CirclePadDown',  axis: 'axis(1+)' },
      { action: 'CirclePadLeft',  axis: 'axis(0-)' },
      { action: 'CirclePadRight', axis: 'axis(0+)' },
      { action: 'CStickUp',    axis: 'axis(3-)' },
      { action: 'CStickDown',  axis: 'axis(3+)' },
      { action: 'CStickLeft',  axis: 'axis(2-)' },
      { action: 'CStickRight', axis: 'axis(2+)' },
      { action: 'TouchScreen', button: 'mouse(left)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: '3ds-keyboard-default',
    name: '3DS — Keyboard (default)',
    controllerType: 'keyboard',
    system: '3ds',
    isDefault: true,
    bindings: [
      { action: 'A',      key: 'key(x)' },
      { action: 'B',      key: 'key(z)' },
      { action: 'X',      key: 'key(s)' },
      { action: 'Y',      key: 'key(a)' },
      { action: 'Start',  key: 'key(return)' },
      { action: 'Select', key: 'key(rshift)' },
      { action: 'L',      key: 'key(q)' },
      { action: 'R',      key: 'key(w)' },
      { action: 'ZL',     key: 'key(1)' },
      { action: 'ZR',     key: 'key(2)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      { action: 'CirclePadUp',    key: 'key(i)' },
      { action: 'CirclePadDown',  key: 'key(k)' },
      { action: 'CirclePadLeft',  key: 'key(j)' },
      { action: 'CirclePadRight', key: 'key(l)' },
      { action: 'TouchScreen', button: 'mouse(left)' },
    ],
  },
];

// ---------------------------------------------------------------------------
// Wii (Dolphin) — Classic Controller / Pro Controller layout
// ---------------------------------------------------------------------------

/**
 * Default Wii input profiles (Classic Controller / Classic Controller Pro).
 *
 * Dolphin emulates Wii Remotes and also supports the Classic Controller.
 * For online multiplayer the Classic Controller layout is preferred since
 * it provides two analog sticks and enough face/shoulder buttons.
 *
 * Classic Controller button layout:
 *  - Face: A, B, X, Y   - Shoulders: L, R, ZL, ZR
 *  - System: Start (+), Select (−), Home
 *  - D-Pad, Left stick, Right stick
 */
export const WII_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'wii-xbox-default',
    name: 'Wii — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'wii',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // A → Classic A
      { action: 'B',      button: 'button(1)' },  // B → Classic B
      { action: 'X',      button: 'button(2)' },  // X → Classic X
      { action: 'Y',      button: 'button(3)' },  // Y → Classic Y
      { action: 'Start',  button: 'button(7)' },  // Menu/Start → Classic +
      { action: 'Select', button: 'button(6)' },  // View/Select → Classic −
      { action: 'Home',   button: 'button(8)' },  // Home button
      { action: 'L',      axis:   'axis(4+)' },   // LT → Classic L
      { action: 'R',      axis:   'axis(5+)' },   // RT → Classic R
      { action: 'ZL',     button: 'button(4)' },  // LB → Classic ZL
      { action: 'ZR',     button: 'button(5)' },  // RB → Classic ZR
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      // Right stick
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'wii-playstation-default',
    name: 'Wii — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'wii',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // Cross     → Classic A
      { action: 'B',      button: 'button(1)' },  // Circle    → Classic B
      { action: 'X',      button: 'button(2)' },  // Square    → Classic X
      { action: 'Y',      button: 'button(3)' },  // Triangle  → Classic Y
      { action: 'Start',  button: 'button(9)' },  // Options   → Classic +
      { action: 'Select', button: 'button(8)' },  // Share/Create → Classic −
      { action: 'Home',   button: 'button(12)' }, // PS button  → Home
      { action: 'L',      axis:   'axis(4+)' },   // L2 → Classic L
      { action: 'R',      axis:   'axis(5+)' },   // R2 → Classic R
      { action: 'ZL',     button: 'button(4)' },  // L1 → Classic ZL
      { action: 'ZR',     button: 'button(5)' },  // R1 → Classic ZR
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'wii-keyboard-default',
    name: 'Wii — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'wii',
    isDefault: true,
    bindings: [
      { action: 'A',      key: 'key(x)' },
      { action: 'B',      key: 'key(z)' },
      { action: 'X',      key: 'key(s)' },
      { action: 'Y',      key: 'key(a)' },
      { action: 'Start',  key: 'key(return)' },
      { action: 'Select', key: 'key(rshift)' },
      { action: 'Home',   key: 'key(escape)' },
      { action: 'L',      key: 'key(q)' },
      { action: 'R',      key: 'key(w)' },
      { action: 'ZL',     key: 'key(1)' },
      { action: 'ZR',     key: 'key(2)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      { action: 'LStickUp',    key: 'key(i)' },
      { action: 'LStickDown',  key: 'key(k)' },
      { action: 'LStickLeft',  key: 'key(j)' },
      { action: 'LStickRight', key: 'key(l)' },
    ],
  },
];

// ---------------------------------------------------------------------------
// Wii U (Cemu) — GamePad / Pro Controller layout
// ---------------------------------------------------------------------------

/**
 * Default Wii U input profiles (GamePad / Pro Controller).
 *
 * Cemu supports the Wii U GamePad and Pro Controller through its emulated
 * controller API.  The Pro Controller layout is close to a standard modern
 * controller and maps naturally.
 */
export const WIIU_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'wiiu-xbox-default',
    name: 'Wii U — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'wiiu',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // A → Wii U A
      { action: 'B',      button: 'button(1)' },  // B → Wii U B
      { action: 'X',      button: 'button(2)' },  // X → Wii U X
      { action: 'Y',      button: 'button(3)' },  // Y → Wii U Y
      { action: 'Plus',   button: 'button(7)' },  // Menu/Start → +
      { action: 'Minus',  button: 'button(6)' },  // View/Select → −
      { action: 'Home',   button: 'button(8)' },  // Xbox button → Home
      { action: 'L',      button: 'button(4)' },  // LB → L
      { action: 'R',      button: 'button(5)' },  // RB → R
      { action: 'ZL',     axis:   'axis(4+)' },   // LT → ZL
      { action: 'ZR',     axis:   'axis(5+)' },   // RT → ZR
      { action: 'LStickClick', button: 'button(9)'  },
      { action: 'RStickClick', button: 'button(10)' },
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      // Right stick
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'wiiu-playstation-default',
    name: 'Wii U — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'wiiu',
    isDefault: true,
    bindings: [
      { action: 'A',      button: 'button(0)' },  // Cross     → A
      { action: 'B',      button: 'button(1)' },  // Circle    → B
      { action: 'X',      button: 'button(2)' },  // Square    → X
      { action: 'Y',      button: 'button(3)' },  // Triangle  → Y
      { action: 'Plus',   button: 'button(9)' },  // Options   → +
      { action: 'Minus',  button: 'button(8)' },  // Share/Create → −
      { action: 'Home',   button: 'button(12)' }, // PS button → Home
      { action: 'L',      button: 'button(4)' },  // L1 → L
      { action: 'R',      button: 'button(5)' },  // R1 → R
      { action: 'ZL',     axis:   'axis(4+)' },   // L2 → ZL
      { action: 'ZR',     axis:   'axis(5+)' },   // R2 → ZR
      { action: 'LStickClick', button: 'button(10)' },
      { action: 'RStickClick', button: 'button(11)' },
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'wiiu-keyboard-default',
    name: 'Wii U — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'wiiu',
    isDefault: true,
    bindings: [
      { action: 'A',      key: 'key(x)' },
      { action: 'B',      key: 'key(z)' },
      { action: 'X',      key: 'key(s)' },
      { action: 'Y',      key: 'key(a)' },
      { action: 'Plus',   key: 'key(return)' },
      { action: 'Minus',  key: 'key(rshift)' },
      { action: 'Home',   key: 'key(escape)' },
      { action: 'L',      key: 'key(q)' },
      { action: 'R',      key: 'key(w)' },
      { action: 'ZL',     key: 'key(1)' },
      { action: 'ZR',     key: 'key(2)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      { action: 'LStickUp',    key: 'key(i)' },
      { action: 'LStickDown',  key: 'key(k)' },
      { action: 'LStickLeft',  key: 'key(j)' },
      { action: 'LStickRight', key: 'key(l)' },
    ],
  },
];

// ---------------------------------------------------------------------------
// SEGA Genesis (RetroArch + Genesis Plus GX) — 6-button pad layout
// ---------------------------------------------------------------------------

/**
 * Default SEGA Genesis input profiles.
 *
 * The 6-button Genesis pad: A, B, C (face), X, Y, Z (upper), Start, Mode,
 * and D-Pad.  RetroArch / Genesis Plus GX maps these to standard gamepad
 * buttons.
 *
 * Common mapping convention:
 *  - B → button(0), C → button(1), A → button(2)
 *  - Y → button(3), Z → button(4), X → button(5)
 *  - Start → button(7), Mode → button(6)
 */
export const GENESIS_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'genesis-xbox-default',
    name: 'Genesis — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'genesis',
    isDefault: true,
    bindings: [
      { action: 'B',     button: 'button(0)' },  // A (Xbox) → Genesis B
      { action: 'C',     button: 'button(1)' },  // B (Xbox) → Genesis C
      { action: 'A',     button: 'button(2)' },  // X (Xbox) → Genesis A
      { action: 'Y',     button: 'button(3)' },  // Y (Xbox) → Genesis Y
      { action: 'X',     button: 'button(4)' },  // LB → Genesis X
      { action: 'Z',     button: 'button(5)' },  // RB → Genesis Z
      { action: 'Start', button: 'button(7)' },  // Menu → Start
      { action: 'Mode',  button: 'button(6)' },  // View → Mode
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left stick as D-Pad alternative
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'genesis-playstation-default',
    name: 'Genesis — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'genesis',
    isDefault: true,
    bindings: [
      { action: 'B',     button: 'button(0)' },  // Cross     → Genesis B
      { action: 'C',     button: 'button(1)' },  // Circle    → Genesis C
      { action: 'A',     button: 'button(2)' },  // Square    → Genesis A
      { action: 'Y',     button: 'button(3)' },  // Triangle  → Genesis Y
      { action: 'X',     button: 'button(4)' },  // L1 → Genesis X
      { action: 'Z',     button: 'button(5)' },  // R1 → Genesis Z
      { action: 'Start', button: 'button(9)' },  // Options   → Start
      { action: 'Mode',  button: 'button(8)' },  // Share/Create → Mode
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'DPadUp',    axis: 'axis(1-)' },
      { action: 'DPadDown',  axis: 'axis(1+)' },
      { action: 'DPadLeft',  axis: 'axis(0-)' },
      { action: 'DPadRight', axis: 'axis(0+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'genesis-keyboard-default',
    name: 'Genesis — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'genesis',
    isDefault: true,
    bindings: [
      { action: 'B',     key: 'key(x)' },
      { action: 'C',     key: 'key(c)' },
      { action: 'A',     key: 'key(z)' },
      { action: 'Y',     key: 'key(a)' },
      { action: 'X',     key: 'key(s)' },
      { action: 'Z',     key: 'key(d)' },
      { action: 'Start', key: 'key(return)' },
      { action: 'Mode',  key: 'key(rshift)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
    ],
  },
];

// ---------------------------------------------------------------------------
// SEGA Dreamcast (Flycast) — standard controller layout
// ---------------------------------------------------------------------------

/**
 * Default SEGA Dreamcast input profiles.
 *
 * The Dreamcast controller: A, B, X, Y face buttons, Start, analog triggers
 * (L / R — not digital), one analog thumb stick, and a D-Pad.
 * Flycast maps these through its own SDL2 input layer.
 */
export const DREAMCAST_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'dreamcast-xbox-default',
    name: 'Dreamcast — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'dreamcast',
    isDefault: true,
    bindings: [
      { action: 'A',     button: 'button(0)' },  // A → DC A
      { action: 'B',     button: 'button(1)' },  // B → DC B
      { action: 'X',     button: 'button(2)' },  // X → DC X
      { action: 'Y',     button: 'button(3)' },  // Y → DC Y
      { action: 'Start', button: 'button(7)' },  // Menu → Start
      // Analog triggers
      { action: 'L',     axis: 'axis(4+)' },     // LT → DC L trigger
      { action: 'R',     axis: 'axis(5+)' },     // RT → DC R trigger
      // Analog stick
      { action: 'StickUp',    axis: 'axis(1-)' },
      { action: 'StickDown',  axis: 'axis(1+)' },
      { action: 'StickLeft',  axis: 'axis(0-)' },
      { action: 'StickRight', axis: 'axis(0+)' },
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'dreamcast-playstation-default',
    name: 'Dreamcast — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'dreamcast',
    isDefault: true,
    bindings: [
      { action: 'A',     button: 'button(0)' },  // Cross     → DC A
      { action: 'B',     button: 'button(1)' },  // Circle    → DC B
      { action: 'X',     button: 'button(2)' },  // Square    → DC X
      { action: 'Y',     button: 'button(3)' },  // Triangle  → DC Y
      { action: 'Start', button: 'button(9)' },  // Options   → Start
      { action: 'L',     axis:   'axis(4+)' },   // L2 → DC L trigger
      { action: 'R',     axis:   'axis(5+)' },   // R2 → DC R trigger
      { action: 'StickUp',    axis: 'axis(1-)' },
      { action: 'StickDown',  axis: 'axis(1+)' },
      { action: 'StickLeft',  axis: 'axis(0-)' },
      { action: 'StickRight', axis: 'axis(0+)' },
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'dreamcast-keyboard-default',
    name: 'Dreamcast — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'dreamcast',
    isDefault: true,
    bindings: [
      { action: 'A',     key: 'key(x)' },
      { action: 'B',     key: 'key(z)' },
      { action: 'X',     key: 'key(s)' },
      { action: 'Y',     key: 'key(a)' },
      { action: 'Start', key: 'key(return)' },
      { action: 'L',     key: 'key(q)' },
      { action: 'R',     key: 'key(w)' },
      { action: 'StickUp',    key: 'key(i)' },
      { action: 'StickDown',  key: 'key(k)' },
      { action: 'StickLeft',  key: 'key(j)' },
      { action: 'StickRight', key: 'key(l)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
    ],
  },
];

// ---------------------------------------------------------------------------
// Sony PlayStation (DuckStation) — DualShock layout
// ---------------------------------------------------------------------------

/**
 * Default PSX/PS1 input profiles (DuckStation).
 *
 * DualShock controller: Cross, Circle, Square, Triangle, L1/L2/R1/R2,
 * Start, Select, L3/R3 (stick clicks), D-Pad, and two analog sticks.
 */
export const PSX_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'psx-xbox-default',
    name: 'PSX — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'psx',
    isDefault: true,
    bindings: [
      { action: 'Cross',    button: 'button(0)' },  // A → Cross
      { action: 'Circle',   button: 'button(1)' },  // B → Circle
      { action: 'Square',   button: 'button(2)' },  // X → Square
      { action: 'Triangle', button: 'button(3)' },  // Y → Triangle
      { action: 'Start',    button: 'button(7)' },  // Menu → Start
      { action: 'Select',   button: 'button(6)' },  // View → Select
      { action: 'L1',       button: 'button(4)' },  // LB → L1
      { action: 'R1',       button: 'button(5)' },  // RB → R1
      { action: 'L2',       axis:   'axis(4+)' },   // LT → L2
      { action: 'R2',       axis:   'axis(5+)' },   // RT → R2
      { action: 'L3',       button: 'button(9)'  }, // LS click → L3
      { action: 'R3',       button: 'button(10)' }, // RS click → R3
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Left analog stick
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      // Right analog stick
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- PlayStation controller (1:1 natural mapping) ---
  {
    id: 'psx-playstation-default',
    name: 'PSX — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'psx',
    isDefault: true,
    bindings: [
      { action: 'Cross',    button: 'button(0)' },
      { action: 'Circle',   button: 'button(1)' },
      { action: 'Square',   button: 'button(2)' },
      { action: 'Triangle', button: 'button(3)' },
      { action: 'Start',    button: 'button(9)' },  // Options
      { action: 'Select',   button: 'button(8)' },  // Share/Create
      { action: 'L1',       button: 'button(4)' },
      { action: 'R1',       button: 'button(5)' },
      { action: 'L2',       axis:   'axis(4+)' },
      { action: 'R2',       axis:   'axis(5+)' },
      { action: 'L3',       button: 'button(10)' },
      { action: 'R3',       button: 'button(11)' },
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'psx-keyboard-default',
    name: 'PSX — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'psx',
    isDefault: true,
    bindings: [
      { action: 'Cross',    key: 'key(x)' },
      { action: 'Circle',   key: 'key(c)' },
      { action: 'Square',   key: 'key(z)' },
      { action: 'Triangle', key: 'key(s)' },
      { action: 'Start',    key: 'key(return)' },
      { action: 'Select',   key: 'key(rshift)' },
      { action: 'L1',       key: 'key(q)' },
      { action: 'R1',       key: 'key(w)' },
      { action: 'L2',       key: 'key(1)' },
      { action: 'R2',       key: 'key(2)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      { action: 'LStickUp',    key: 'key(i)' },
      { action: 'LStickDown',  key: 'key(k)' },
      { action: 'LStickLeft',  key: 'key(j)' },
      { action: 'LStickRight', key: 'key(l)' },
    ],
  },
];

// ---------------------------------------------------------------------------
// Sony PlayStation 2 (PCSX2) — DualShock 2 layout
// ---------------------------------------------------------------------------

/**
 * Default PS2 input profiles (PCSX2).
 *
 * DualShock 2 is identical to DualShock 1 but adds pressure-sensitive face
 * buttons.  The digital mapping is the same as PSX.
 */
export const PS2_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'ps2-xbox-default',
    name: 'PS2 — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'ps2',
    isDefault: true,
    bindings: [
      { action: 'Cross',    button: 'button(0)' },
      { action: 'Circle',   button: 'button(1)' },
      { action: 'Square',   button: 'button(2)' },
      { action: 'Triangle', button: 'button(3)' },
      { action: 'Start',    button: 'button(7)' },
      { action: 'Select',   button: 'button(6)' },
      { action: 'L1',       button: 'button(4)' },
      { action: 'R1',       button: 'button(5)' },
      { action: 'L2',       axis:   'axis(4+)' },
      { action: 'R2',       axis:   'axis(5+)' },
      { action: 'L3',       button: 'button(9)'  },
      { action: 'R3',       button: 'button(10)' },
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'ps2-playstation-default',
    name: 'PS2 — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'ps2',
    isDefault: true,
    bindings: [
      { action: 'Cross',    button: 'button(0)' },
      { action: 'Circle',   button: 'button(1)' },
      { action: 'Square',   button: 'button(2)' },
      { action: 'Triangle', button: 'button(3)' },
      { action: 'Start',    button: 'button(9)' },
      { action: 'Select',   button: 'button(8)' },
      { action: 'L1',       button: 'button(4)' },
      { action: 'R1',       button: 'button(5)' },
      { action: 'L2',       axis:   'axis(4+)' },
      { action: 'R2',       axis:   'axis(5+)' },
      { action: 'L3',       button: 'button(10)' },
      { action: 'R3',       button: 'button(11)' },
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'LStickUp',    axis: 'axis(1-)' },
      { action: 'LStickDown',  axis: 'axis(1+)' },
      { action: 'LStickLeft',  axis: 'axis(0-)' },
      { action: 'LStickRight', axis: 'axis(0+)' },
      { action: 'RStickUp',    axis: 'axis(3-)' },
      { action: 'RStickDown',  axis: 'axis(3+)' },
      { action: 'RStickLeft',  axis: 'axis(2-)' },
      { action: 'RStickRight', axis: 'axis(2+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'ps2-keyboard-default',
    name: 'PS2 — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'ps2',
    isDefault: true,
    bindings: [
      { action: 'Cross',    key: 'key(x)' },
      { action: 'Circle',   key: 'key(c)' },
      { action: 'Square',   key: 'key(z)' },
      { action: 'Triangle', key: 'key(s)' },
      { action: 'Start',    key: 'key(return)' },
      { action: 'Select',   key: 'key(rshift)' },
      { action: 'L1',       key: 'key(q)' },
      { action: 'R1',       key: 'key(w)' },
      { action: 'L2',       key: 'key(1)' },
      { action: 'R2',       key: 'key(2)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      { action: 'LStickUp',    key: 'key(i)' },
      { action: 'LStickDown',  key: 'key(k)' },
      { action: 'LStickLeft',  key: 'key(j)' },
      { action: 'LStickRight', key: 'key(l)' },
    ],
  },
];

// ---------------------------------------------------------------------------
// Sony PSP (PPSSPP) — PSP button layout
// ---------------------------------------------------------------------------

/**
 * Default PSP input profiles (PPSSPP).
 *
 * PSP button layout: Cross, Circle, Square, Triangle, L, R, Start, Select,
 * D-Pad, and one analog stick (the PSP nub — left stick).
 * PPSSPP adds virtual right-stick support (via right analog) for camera.
 */
export const PSP_DEFAULT_PROFILES: InputProfile[] = [
  // --- Xbox controller ---
  {
    id: 'psp-xbox-default',
    name: 'PSP — Xbox Controller (default)',
    controllerType: 'xbox',
    system: 'psp',
    isDefault: true,
    bindings: [
      { action: 'Cross',    button: 'button(0)' },  // A → Cross
      { action: 'Circle',   button: 'button(1)' },  // B → Circle
      { action: 'Square',   button: 'button(2)' },  // X → Square
      { action: 'Triangle', button: 'button(3)' },  // Y → Triangle
      { action: 'Start',    button: 'button(7)' },  // Menu → Start
      { action: 'Select',   button: 'button(6)' },  // View → Select
      { action: 'L',        button: 'button(4)' },  // LB → L
      { action: 'R',        button: 'button(5)' },  // RB → R
      // D-Pad
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      // Analog nub (left stick)
      { action: 'NubUp',    axis: 'axis(1-)' },
      { action: 'NubDown',  axis: 'axis(1+)' },
      { action: 'NubLeft',  axis: 'axis(0-)' },
      { action: 'NubRight', axis: 'axis(0+)' },
    ],
  },
  // --- PlayStation controller ---
  {
    id: 'psp-playstation-default',
    name: 'PSP — PlayStation Controller (default)',
    controllerType: 'playstation',
    system: 'psp',
    isDefault: true,
    bindings: [
      { action: 'Cross',    button: 'button(0)' },
      { action: 'Circle',   button: 'button(1)' },
      { action: 'Square',   button: 'button(2)' },
      { action: 'Triangle', button: 'button(3)' },
      { action: 'Start',    button: 'button(9)' },  // Options
      { action: 'Select',   button: 'button(8)' },  // Share/Create
      { action: 'L',        button: 'button(4)' },  // L1
      { action: 'R',        button: 'button(5)' },  // R1
      { action: 'DPadUp',    button: 'hat(0 Up)'    },
      { action: 'DPadDown',  button: 'hat(0 Down)'  },
      { action: 'DPadLeft',  button: 'hat(0 Left)'  },
      { action: 'DPadRight', button: 'hat(0 Right)' },
      { action: 'NubUp',    axis: 'axis(1-)' },
      { action: 'NubDown',  axis: 'axis(1+)' },
      { action: 'NubLeft',  axis: 'axis(0-)' },
      { action: 'NubRight', axis: 'axis(0+)' },
    ],
  },
  // --- Keyboard fallback ---
  {
    id: 'psp-keyboard-default',
    name: 'PSP — Keyboard (default)',
    controllerType: 'keyboard',
    system: 'psp',
    isDefault: true,
    bindings: [
      { action: 'Cross',    key: 'key(x)' },
      { action: 'Circle',   key: 'key(c)' },
      { action: 'Square',   key: 'key(z)' },
      { action: 'Triangle', key: 'key(s)' },
      { action: 'Start',    key: 'key(return)' },
      { action: 'Select',   key: 'key(rshift)' },
      { action: 'L',        key: 'key(q)' },
      { action: 'R',        key: 'key(w)' },
      { action: 'DPadUp',    key: 'key(up)'    },
      { action: 'DPadDown',  key: 'key(down)'  },
      { action: 'DPadLeft',  key: 'key(left)'  },
      { action: 'DPadRight', key: 'key(right)' },
      { action: 'NubUp',    key: 'key(i)' },
      { action: 'NubDown',  key: 'key(k)' },
      { action: 'NubLeft',  key: 'key(j)' },
      { action: 'NubRight', key: 'key(l)' },
    ],
  },
];

/**
 * InputProfileManager handles controller mapping and input configuration.
 * It provides per-game and per-system templates for unified input handling.
 *
 * Constructed with `loadDefaults = true` (default), the manager is pre-seeded
 * with the built-in N64 and NDS profiles.
 */
export class InputProfileManager {
  private profiles: Map<string, InputProfile> = new Map();

  constructor(loadDefaults = true) {
    if (loadDefaults) {
      const allDefaults = [
        ...NES_DEFAULT_PROFILES,
        ...SNES_DEFAULT_PROFILES,
        ...GB_DEFAULT_PROFILES,
        ...GBC_DEFAULT_PROFILES,
        ...GBA_DEFAULT_PROFILES,
        ...N64_DEFAULT_PROFILES,
        ...NDS_DEFAULT_PROFILES,
        ...GC_DEFAULT_PROFILES,
        ...N3DS_DEFAULT_PROFILES,
        ...WII_DEFAULT_PROFILES,
        ...WIIU_DEFAULT_PROFILES,
        ...GENESIS_DEFAULT_PROFILES,
        ...DREAMCAST_DEFAULT_PROFILES,
        ...PSX_DEFAULT_PROFILES,
        ...PS2_DEFAULT_PROFILES,
        ...PSP_DEFAULT_PROFILES,
      ];
      for (const profile of allDefaults) {
        this.profiles.set(profile.id, profile);
      }
    }
  }

  /**
   * Get an input profile by ID.
   */
  get(id: string): InputProfile | null {
    return this.profiles.get(id) ?? null;
  }

  /**
   * Get the default profile for a system and controller type.
   */
  getDefault(system: string, controllerType: ControllerType): InputProfile | null {
    for (const profile of this.profiles.values()) {
      if (
        profile.system === system &&
        profile.controllerType === controllerType &&
        profile.isDefault
      ) {
        return profile;
      }
    }
    return null;
  }

  /**
   * Save an input profile.
   */
  save(profile: InputProfile): void {
    this.profiles.set(profile.id, profile);
  }

  /**
   * Delete an input profile.
   */
  delete(id: string): boolean {
    return this.profiles.delete(id);
  }

  /**
   * List all profiles for a system.
   */
  listForSystem(system: string): InputProfile[] {
    return Array.from(this.profiles.values()).filter((p) => p.system === system);
  }

  /**
   * List all profiles.
   */
  listAll(): InputProfile[] {
    return Array.from(this.profiles.values());
  }
}
