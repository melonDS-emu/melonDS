/**
 * Custom input profile service — persists user-edited controller mappings to localStorage.
 *
 * Each entry in storage is a full InputProfile JSON object keyed by profile ID.
 * When a user edits a default profile, a copy is stored here under the same ID,
 * which shadows the default. Resetting deletes the override and the default
 * reappears automatically.
 */

import type { InputProfile } from '@retro-oasis/multiplayer-profiles';

const STORAGE_KEY = 'retro-oasis-custom-profiles';

// ---------------------------------------------------------------------------
// Persistence helpers
// ---------------------------------------------------------------------------

function loadAll(): Record<string, InputProfile> {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return raw ? (JSON.parse(raw) as Record<string, InputProfile>) : {};
  } catch {
    return {};
  }
}

function saveAll(profiles: Record<string, InputProfile>): void {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(profiles));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/** Return a custom override for a profile ID, or null if no override exists. */
export function getCustomProfile(id: string): InputProfile | null {
  return loadAll()[id] ?? null;
}

/** Persist a custom (user-edited) profile override. */
export function saveCustomProfile(profile: InputProfile): void {
  const all = loadAll();
  all[profile.id] = profile;
  saveAll(all);
}

/** Remove a custom override, restoring the built-in default. */
export function resetCustomProfile(id: string): void {
  const all = loadAll();
  delete all[id];
  saveAll(all);
}

/** Return all custom override IDs currently stored. */
export function listCustomProfileIds(): string[] {
  return Object.keys(loadAll());
}

/**
 * Merge a list of default profiles with any stored custom overrides.
 * If a custom override exists for a profile ID, it replaces the default.
 */
export function mergeWithCustomProfiles(defaults: InputProfile[]): InputProfile[] {
  const all = loadAll();
  return defaults.map((p) => all[p.id] ?? p);
}
