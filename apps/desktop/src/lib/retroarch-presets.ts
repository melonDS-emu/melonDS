/**
 * RetroArch Enhancement Presets
 *
 * Defines named RetroArch configuration presets that are either safe for
 * netplay (no frame-time or audio-buffer changes that could cause desync)
 * or intended for single-player only.
 *
 * Each preset contains a flat map of RetroArch `.cfg` key/value pairs.
 * The lobby launcher merges these into the session config before start.
 */

// ─────────────────────────────────────────────────────────────────────────────
// Types
// ─────────────────────────────────────────────────────────────────────────────

export interface RetroArchPreset {
  /** Stable slug used in room configs and session templates. */
  id: string;
  /** Display name shown in the UI. */
  name: string;
  /** One-sentence description for tooltips / help text. */
  description: string;
  /**
   * Whether this preset is safe to use during an online netplay session.
   *
   * A preset is netplay-safe when it does NOT change:
   *  - frame timing (runahead, rewinding, fast-forward)
   *  - audio latency beyond the host's configured threshold
   *  - input latency reduction features (preemptive frames)
   *  - video shaders that add sub-frame delays
   */
  netplaySafe: boolean;
  /**
   * RetroArch `.cfg` key/value pairs applied for this preset.
   * Values are always strings (matching RetroArch's plain-text config format).
   */
  settings: Record<string, string>;
  /**
   * User-visible warning shown when the preset is selected for an online room.
   * Only required when `netplaySafe` is `false`.
   */
  onlineWarning?: string;
}

// ─────────────────────────────────────────────────────────────────────────────
// Preset catalog
// ─────────────────────────────────────────────────────────────────────────────

export const RETROARCH_PRESETS: RetroArchPreset[] = [
  // ── Netplay-safe presets ────────────────────────────────────────────────────

  {
    id: 'clean',
    name: 'Clean (No Enhancements)',
    description: 'No shaders, filters, or latency tweaks. Best for competitive netplay.',
    netplaySafe: true,
    settings: {
      video_shader_enable: 'false',
      video_smooth: 'false',
      audio_latency: '64',
      run_ahead_enabled: 'false',
      rewind_enable: 'false',
      fps_show: 'false',
      input_poll_type_behavior: '2', // late polling (default, netplay-safe)
    },
  },

  {
    id: 'performance',
    name: 'Performance',
    description: 'Mild integer-scale upscaling and nearest-neighbor filtering. Netplay-safe.',
    netplaySafe: true,
    settings: {
      video_shader_enable: 'false',
      video_smooth: 'false',
      video_scale_integer: 'true',
      audio_latency: '64',
      run_ahead_enabled: 'false',
      rewind_enable: 'false',
      input_poll_type_behavior: '2',
    },
  },

  {
    id: 'scanlines',
    name: 'Scanlines (Netplay-Safe)',
    description: 'Subtle scanline overlay via a zero-latency pass-through shader. Netplay-safe.',
    netplaySafe: true,
    settings: {
      video_shader_enable: 'true',
      video_shader: 'shaders/shaders_slang/misc/scanlines-sine-abs.slangp',
      video_smooth: 'false',
      audio_latency: '64',
      run_ahead_enabled: 'false',
      rewind_enable: 'false',
      input_poll_type_behavior: '2',
    },
  },

  {
    id: 'lcd-grid',
    name: 'LCD Grid (Handheld)',
    description: 'Simulates a handheld LCD panel for GB/GBC/GBA games. Netplay-safe.',
    netplaySafe: true,
    settings: {
      video_shader_enable: 'true',
      video_shader: 'shaders/shaders_slang/handheld/lcd-grid.slangp',
      video_smooth: 'false',
      audio_latency: '64',
      run_ahead_enabled: 'false',
      rewind_enable: 'false',
      input_poll_type_behavior: '2',
    },
  },

  // ── Single-player presets (NOT netplay-safe) ────────────────────────────────

  {
    id: 'crt-royale',
    name: 'CRT Royale',
    description: 'Full phosphor CRT simulation with bloom and curvature. Single-player only.',
    netplaySafe: false,
    onlineWarning:
      'CRT Royale uses multi-pass shaders that add sub-frame GPU delays and can cause desync in netplay sessions. Switch to Scanlines or Clean for online play.',
    settings: {
      video_shader_enable: 'true',
      video_shader: 'shaders/shaders_slang/crt/crt-royale.slangp',
      video_smooth: 'false',
      audio_latency: '64',
      run_ahead_enabled: 'false',
      rewind_enable: 'false',
      input_poll_type_behavior: '2',
    },
  },

  {
    id: 'runahead-1',
    name: 'Run-Ahead (1 frame)',
    description: 'Reduces perceived input latency by 1 frame. Single-player only.',
    netplaySafe: false,
    onlineWarning:
      'Run-ahead executes speculative frames locally and is incompatible with netplay synchronisation. This preset will be ignored when hosting a room.',
    settings: {
      video_shader_enable: 'false',
      video_smooth: 'false',
      audio_latency: '32',
      run_ahead_enabled: 'true',
      run_ahead_frames: '1',
      run_ahead_secondary_instance: 'false',
      rewind_enable: 'false',
      input_poll_type_behavior: '2',
    },
  },

  {
    id: 'rewind',
    name: 'Rewind',
    description: 'Enables rewind with a 120-second buffer. Single-player only.',
    netplaySafe: false,
    onlineWarning:
      'Rewind stores speculative save-state frames and is disabled automatically during netplay. Enable only for local solo sessions.',
    settings: {
      video_shader_enable: 'false',
      video_smooth: 'false',
      audio_latency: '64',
      run_ahead_enabled: 'false',
      rewind_enable: 'true',
      rewind_buffer_size: '131072', // 128 MB
      rewind_granularity: '4',
      input_poll_type_behavior: '2',
    },
  },

  {
    id: 'accurate',
    name: 'Accurate (High Fidelity)',
    description:
      'Highest-accuracy audio resampler and synchronisation. Heavier CPU load; single-player only.',
    netplaySafe: false,
    onlineWarning:
      'The accurate preset sets a low audio latency target (16 ms) that can exceed typical netplay buffer requirements and cause audio dropouts. Use Clean or Performance for online play.',
    settings: {
      video_shader_enable: 'false',
      video_smooth: 'true',
      audio_latency: '16',
      audio_resampler: 'sinc',
      audio_resampler_quality: '5',
      run_ahead_enabled: 'false',
      rewind_enable: 'false',
      input_poll_type_behavior: '2',
    },
  },
];

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/** Look up a preset by its stable `id`. Returns `null` when not found. */
export function getPreset(id: string): RetroArchPreset | null {
  return RETROARCH_PRESETS.find((p) => p.id === id) ?? null;
}

/** All presets that are safe to use during online netplay. */
export function netplaySafePresets(): RetroArchPreset[] {
  return RETROARCH_PRESETS.filter((p) => p.netplaySafe);
}

/** All presets that are NOT safe for netplay (single-player only). */
export function singlePlayerPresets(): RetroArchPreset[] {
  return RETROARCH_PRESETS.filter((p) => !p.netplaySafe);
}

/**
 * Returns the online warning for a preset, or `null` when the preset is
 * netplay-safe or not found.
 */
export function getPresetOnlineWarning(id: string): string | null {
  return getPreset(id)?.onlineWarning ?? null;
}

/**
 * Merge preset settings with an existing RetroArch config object.
 * Preset values take precedence over `base`.
 */
export function applyPreset(
  presetId: string,
  base: Record<string, string> = {},
): Record<string, string> {
  const preset = getPreset(presetId);
  if (!preset) return { ...base };
  return { ...base, ...preset.settings };
}
