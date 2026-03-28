/**
 * First-run wizard service — Phase 29 (Desktop Productization)
 *
 * Tracks whether the user has completed the first-run setup wizard and
 * which steps they have finished.  State is persisted to localStorage.
 */

const FIRST_RUN_KEY = 'retro-oasis-first-run-complete';
const SETUP_PROGRESS_KEY = 'retro-oasis-setup-progress';

export type SetupStep = 'display-name' | 'emulator-paths' | 'rom-directory' | 'done';

export interface SetupProgress {
  completedSteps: SetupStep[];
  /** Display name chosen during wizard (empty string if skipped). */
  displayName: string;
  /** ROM directory chosen during wizard (empty string if skipped). */
  romDirectory: string;
}

const DEFAULT_PROGRESS: Readonly<Omit<SetupProgress, 'completedSteps'>> = {
  displayName: '',
  romDirectory: '',
};

function makeDefaultProgress(): SetupProgress {
  return { completedSteps: [], ...DEFAULT_PROGRESS };
}

// ---------------------------------------------------------------------------
// First-run flag
// ---------------------------------------------------------------------------

/** True when the first-run wizard has been completed. */
export function isFirstRunComplete(): boolean {
  try {
    return localStorage.getItem(FIRST_RUN_KEY) === 'true';
  } catch {
    return false;
  }
}

/** Mark the first-run wizard as complete. */
export function markFirstRunComplete(): void {
  localStorage.setItem(FIRST_RUN_KEY, 'true');
}

/** Reset the first-run flag (useful for testing / re-running the wizard). */
export function resetFirstRun(): void {
  localStorage.removeItem(FIRST_RUN_KEY);
  localStorage.removeItem(SETUP_PROGRESS_KEY);
}

// ---------------------------------------------------------------------------
// Per-step progress
// ---------------------------------------------------------------------------

function loadProgress(): SetupProgress {
  try {
    const raw = localStorage.getItem(SETUP_PROGRESS_KEY);
    return raw ? (JSON.parse(raw) as SetupProgress) : makeDefaultProgress();
  } catch {
    return makeDefaultProgress();
  }
}

function saveProgress(progress: SetupProgress): void {
  localStorage.setItem(SETUP_PROGRESS_KEY, JSON.stringify(progress));
}

/** Get current setup progress. */
export function getSetupProgress(): SetupProgress {
  return loadProgress();
}

/** Mark a setup step as complete and persist. */
export function completeStep(step: SetupStep, data?: Partial<SetupProgress>): void {
  const progress = loadProgress();
  if (!progress.completedSteps.includes(step)) {
    progress.completedSteps.push(step);
  }
  if (data?.displayName !== undefined) progress.displayName = data.displayName;
  if (data?.romDirectory !== undefined) progress.romDirectory = data.romDirectory;
  saveProgress(progress);
}

/** True when a specific step has been completed. */
export function isStepComplete(step: SetupStep): boolean {
  return loadProgress().completedSteps.includes(step);
}

/**
 * Return the first incomplete step.
 * Returns 'done' when all required steps are complete.
 */
export function nextIncompleteStep(): SetupStep {
  const REQUIRED: SetupStep[] = ['display-name', 'emulator-paths', 'rom-directory'];
  const progress = loadProgress();
  const next = REQUIRED.find((s) => !progress.completedSteps.includes(s));
  return next ?? 'done';
}
