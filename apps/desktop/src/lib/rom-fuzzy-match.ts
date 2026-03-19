/**
 * Fuzzy ROM title matching using Levenshtein edit distance.
 *
 * Used to map a scanned ROM's inferred title to a game catalog entry more
 * reliably than simple slug generation.
 */

export interface CatalogEntry {
  id: string;
  title: string;
  system: string;
}

/** Normalise a string for fuzzy comparison. */
function normalise(s: string): string {
  return s
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, ' ')
    .trim()
    .replace(/\s+/g, ' ');
}

/** Compute the Levenshtein edit distance between two strings. */
function levenshtein(a: string, b: string): number {
  const m = a.length;
  const n = b.length;
  // Use a single-row rolling DP to keep memory O(n)
  const prev = Array.from({ length: n + 1 }, (_, i) => i);
  const curr = new Array<number>(n + 1);

  for (let i = 1; i <= m; i++) {
    curr[0] = i;
    for (let j = 1; j <= n; j++) {
      const cost = a[i - 1] === b[j - 1] ? 0 : 1;
      curr[j] = Math.min(
        curr[j - 1] + 1,     // insert
        prev[j] + 1,         // delete
        prev[j - 1] + cost   // substitute
      );
    }
    for (let j = 0; j <= n; j++) prev[j] = curr[j];
  }

  return prev[n];
}

/**
 * Find the closest game catalog entry for the given system and raw ROM title.
 *
 * Normalises both strings before comparing.  Returns the best matching game
 * ID only when the edit distance is within a confidence threshold of
 * `< 0.4 * max(len_a, len_b)`.  Returns null when no confident match is
 * found.
 */
export function fuzzyMatchGameId(
  system: string,
  rawTitle: string,
  catalog: CatalogEntry[] | null | undefined
): string | null {
  if (!catalog || catalog.length === 0) return null;
  const normTitle = normalise(rawTitle);
  if (!normTitle) return null;

  // Filter to the same system first (case-insensitive)
  const systemNorm = system.toLowerCase();
  const candidates = catalog.filter((e) => e.system.toLowerCase() === systemNorm);
  if (candidates.length === 0) return null;

  let bestId: string | null = null;
  let bestDist = Infinity;
  let bestNormEntry = '';

  for (const entry of candidates) {
    const normEntry = normalise(entry.title);
    const dist = levenshtein(normTitle, normEntry);
    if (dist < bestDist) {
      bestDist = dist;
      bestId = entry.id;
      bestNormEntry = normEntry;
    }
  }

  if (bestId === null) return null;

  // Confidence gate: reject if distance exceeds 40 % of the longer string
  const threshold = 0.4 * Math.max(normTitle.length, bestNormEntry.length);
  return bestDist < threshold ? bestId : null;
}
