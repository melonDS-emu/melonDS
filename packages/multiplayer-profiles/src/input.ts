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
      for (const profile of N64_DEFAULT_PROFILES) {
        this.profiles.set(profile.id, profile);
      }
      for (const profile of NDS_DEFAULT_PROFILES) {
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
