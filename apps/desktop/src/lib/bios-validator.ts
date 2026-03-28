/**
 * BIOS Validator
 *
 * Maintains a registry of required BIOS files per system, including
 * well-known MD5 hashes.  Consumers call `validateBiosEntry()` after
 * computing an MD5 on a candidate file to decide whether to allow launch.
 *
 * Supported systems: psx, gba, nds, dreamcast
 */

// ─────────────────────────────────────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────────────────────────────────────

/** A single BIOS file definition — one of potentially several accepted variants. */
export interface BiosEntry {
  /** Canonical filename expected by the emulator. */
  filename: string;
  /** Known-good MD5 hashes (lowercase hex).  More than one means regional / revision variants. */
  knownMd5: string[];
  /** Human-readable description shown in the UI. */
  description: string;
  /** Whether the emulator can start at all without this file. */
  required: boolean;
}

/** All BIOS requirements for a system. */
export interface BiosRequirement {
  /** Lowercase system identifier, e.g. `'psx'`. */
  system: string;
  /** Every BIOS file that may be needed for this system. */
  entries: BiosEntry[];
  /** Brief note surfaced in the UI / preflight dialog. */
  note: string;
}

/** Result of validating a single BIOS file against its known hashes. */
export interface BiosValidationResult {
  filename: string;
  provided: boolean;
  /** `true` when `providedMd5` matches at least one of the `knownMd5` values. */
  hashMatch: boolean;
  /** `true` when the BIOS is required AND either not provided or hash mismatch. */
  blocked: boolean;
  message: string;
}

// ─────────────────────────────────────────────────────────────────────────────
// Registry
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Known BIOS requirements keyed by system identifier.
 *
 * MD5 hashes sourced from community BIOS verification projects
 * (no copyrighted data — only checksums of known-good dumps).
 */
export const BIOS_REQUIREMENTS: Record<string, BiosRequirement> = {
  psx: {
    system: 'psx',
    note: 'PlayStation BIOS is required for SwanStation and Beetle PSX HW.',
    entries: [
      {
        filename: 'scph1001.bin',
        description: 'PS1 BIOS v2.2 (USA, 1994)',
        required: true,
        knownMd5: ['37157331346bb75cbddca0be29eaa6ea'],
      },
      {
        filename: 'scph5501.bin',
        description: 'PS1 BIOS v3.0 (USA, 1996)',
        required: false,
        knownMd5: ['8dd7d5296a650fac7319bce665a6a53c'],
      },
      {
        filename: 'scph7001.bin',
        description: 'PS1 BIOS v4.1 (USA, 1997)',
        required: false,
        knownMd5: ['516a55b93ef52fc0e55dd2d49d2b9d00'],
      },
      {
        filename: 'scph5502.bin',
        description: 'PS1 BIOS v3.0 (Europe, 1996)',
        required: false,
        knownMd5: ['d786f0b9d8b3856bc3a2e30e8521f2f7'],
      },
    ],
  },
  gba: {
    system: 'gba',
    note: 'GBA BIOS is optional but improves compatibility with mGBA.',
    entries: [
      {
        filename: 'gba_bios.bin',
        description: 'Game Boy Advance BIOS',
        required: false,
        knownMd5: ['a860e8c0b6d573d191e4ec7db1b1e4f6'],
      },
    ],
  },
  nds: {
    system: 'nds',
    note: 'NDS BIOS files are required for melonDS to boot retail ROMs.',
    entries: [
      {
        filename: 'bios7.bin',
        description: 'NDS ARM7 BIOS',
        required: true,
        knownMd5: ['df692a80a5b1bc90728bc3dfc76cd948'],
      },
      {
        filename: 'bios9.bin',
        description: 'NDS ARM9 BIOS',
        required: true,
        knownMd5: ['a392174eb3e572fed6447e956bde4b25'],
      },
      {
        filename: 'firmware.bin',
        description: 'NDS Firmware',
        required: true,
        knownMd5: [
          'e45a306441002b6c0dff7ec3d14d7e47', // 128 KB retail firmware
          '70ab74c3d62b60b3520f43c4f8e5b898', // 256 KB retail firmware
        ],
      },
    ],
  },
  dreamcast: {
    system: 'dreamcast',
    note: 'Dreamcast BIOS (dc_boot.bin) is required for Flycast and flycast_libretro. Place files in the Flycast data directory or RetroArch system/ folder.',
    entries: [
      {
        filename: 'dc_boot.bin',
        description: 'Dreamcast boot ROM (required)',
        required: true,
        knownMd5: [
          'e10c53c2f8b90bab96ead2d368858623', // v1.01d (Japan, most common)
          '0a93f7940c455905bea6e392dfde92a4', // v1.004 (USA)
          'cba5b9c44faa0b4d7f4fc1bc7a87bab5', // v1.01 (Europe)
          '89f2b1a1d2eb2a43f5d423d61924d8ff', // v1.00 (Japan, earlier revision)
          '4ced8d5e9b72d5e67ef5b48c9f55f9b0', // v1.01d (Japan, alt dump)
        ],
      },
      {
        filename: 'dc_flash.bin',
        description: 'Dreamcast Flash ROM — stores regional settings and date/time',
        required: false,
        knownMd5: [
          '69c036abf3baa8ea71f80c0a8438ffbe', // v1.01d (Japan)
          'b8ec8a4c6b69a9c2d03dba628a8c3d32', // v1.004 (USA)
          '9db8c786697b61c60e41a42809dfb4e1', // v1.01 (Europe)
        ],
      },
    ],
  },
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns `true` when `system` has any required BIOS entries in the registry.
 */
export function systemRequiresBios(system: string): boolean {
  const req = BIOS_REQUIREMENTS[system.toLowerCase()];
  if (!req) return false;
  return req.entries.some((e) => e.required);
}

/**
 * Returns the `BiosRequirement` for `system`, or `null` when the system has
 * no registered BIOS files.
 */
export function getBiosRequirements(system: string): BiosRequirement | null {
  return BIOS_REQUIREMENTS[system.toLowerCase()] ?? null;
}

/**
 * Validate a single BIOS candidate file against the registry.
 *
 * @param system     - Lowercase system identifier (e.g. `'psx'`).
 * @param filename   - Canonical filename of the BIOS file (case-insensitive).
 * @param providedMd5 - MD5 hex string of the on-disk file, or `null` when the
 *                      file does not exist.
 */
export function validateBiosEntry(
  system: string,
  filename: string,
  providedMd5: string | null,
): BiosValidationResult {
  const req = BIOS_REQUIREMENTS[system.toLowerCase()];
  const normalised = filename.toLowerCase();
  const entry = req?.entries.find((e) => e.filename.toLowerCase() === normalised);

  if (!entry) {
    // Unknown BIOS file — pass through, not our concern.
    return {
      filename,
      provided: providedMd5 !== null,
      hashMatch: true,
      blocked: false,
      message: `Unknown BIOS file '${filename}' — skipping hash check.`,
    };
  }

  const provided = providedMd5 !== null;
  const hashMatch = provided && entry.knownMd5.includes(providedMd5.toLowerCase());
  const blocked = entry.required && (!provided || !hashMatch);

  let message: string;
  if (!provided) {
    message = entry.required
      ? `Required BIOS file '${entry.filename}' (${entry.description}) is missing.`
      : `Optional BIOS file '${entry.filename}' (${entry.description}) is not present.`;
  } else if (!hashMatch) {
    message = entry.required
      ? `BIOS file '${entry.filename}' hash mismatch — this dump may be corrupted or from an unsupported revision.`
      : `Optional BIOS file '${entry.filename}' hash does not match any known-good dump.`;
  } else {
    message = `BIOS file '${entry.filename}' verified OK.`;
  }

  return { filename: entry.filename, provided, hashMatch, blocked, message };
}

/**
 * Convenience: validate ALL known BIOS entries for a system given a map of
 * `filename → md5 | null`.
 *
 * Returns one `BiosValidationResult` per registered entry.
 * Use `results.some(r => r.blocked)` to gate launch.
 */
export function validateAllBios(
  system: string,
  biosMap: Record<string, string | null>,
): BiosValidationResult[] {
  const req = BIOS_REQUIREMENTS[system.toLowerCase()];
  if (!req) return [];
  return req.entries.map((entry) => {
    const key = Object.keys(biosMap).find(
      (k) => k.toLowerCase() === entry.filename.toLowerCase(),
    );
    const md5 = key !== undefined ? biosMap[key] : null;
    return validateBiosEntry(system, entry.filename, md5 ?? null);
  });
}
