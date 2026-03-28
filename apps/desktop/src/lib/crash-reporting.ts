/**
 * Crash reporting service — Phase 29 (Desktop Productization)
 *
 * Opt-in crash report collection stored in localStorage.
 * Reports are NOT sent anywhere unless a future backend endpoint is
 * configured — this is purely local storage for diagnosis and export.
 */

const OPT_IN_KEY = 'retro-oasis-crash-reporting-opt-in';
const REPORTS_KEY = 'retro-oasis-crash-reports';
const MAX_REPORTS = 50;

export interface CrashReport {
  id: string;
  timestamp: string; // ISO
  message: string;
  stack?: string;
  /** Current page/route when the crash occurred. */
  context?: string;
  /** App version string (from import.meta.env.VITE_APP_VERSION). */
  appVersion?: string;
}

// ---------------------------------------------------------------------------
// Opt-in preference
// ---------------------------------------------------------------------------

/** Returns true when the user has opted in to crash reporting. */
export function isCrashReportingEnabled(): boolean {
  try {
    return localStorage.getItem(OPT_IN_KEY) === 'true';
  } catch {
    return false;
  }
}

/** Enable crash reporting. */
export function enableCrashReporting(): void {
  localStorage.setItem(OPT_IN_KEY, 'true');
}

/** Disable crash reporting and clear all stored reports. */
export function disableCrashReporting(): void {
  localStorage.setItem(OPT_IN_KEY, 'false');
  clearCrashReports();
}

// ---------------------------------------------------------------------------
// Report storage
// ---------------------------------------------------------------------------

function loadReports(): CrashReport[] {
  try {
    const raw = localStorage.getItem(REPORTS_KEY);
    return raw ? (JSON.parse(raw) as CrashReport[]) : [];
  } catch {
    return [];
  }
}

function saveReports(reports: CrashReport[]): void {
  try {
    localStorage.setItem(REPORTS_KEY, JSON.stringify(reports));
  } catch {
    // localStorage quota exceeded — drop the oldest entry and retry once
    const trimmed = reports.slice(1);
    try {
      localStorage.setItem(REPORTS_KEY, JSON.stringify(trimmed));
    } catch {
      // give up silently
    }
  }
}

/**
 * Record a crash report.  Does nothing if crash reporting is disabled.
 * Reports are capped at MAX_REPORTS; oldest entries are evicted.
 */
export function recordCrash(error: Error | string, context?: string): CrashReport | null {
  if (!isCrashReportingEnabled()) return null;

  const report: CrashReport = {
    id: `cr-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
    timestamp: new Date().toISOString(),
    message: typeof error === 'string' ? error : error.message,
    stack: typeof error === 'string' ? undefined : error.stack,
    context,
    appVersion: typeof import.meta !== 'undefined'
      ? (import.meta as { env?: { VITE_APP_VERSION?: string } }).env?.VITE_APP_VERSION
      : undefined,
  };

  const reports = loadReports();
  reports.push(report);
  if (reports.length > MAX_REPORTS) {
    reports.splice(0, reports.length - MAX_REPORTS);
  }
  saveReports(reports);
  return report;
}

/** Get all stored crash reports. */
export function getCrashReports(): CrashReport[] {
  return loadReports();
}

/** Delete all stored crash reports. */
export function clearCrashReports(): void {
  try {
    localStorage.removeItem(REPORTS_KEY);
  } catch {
    // ignore
  }
}

/**
 * Export crash reports as a JSON string suitable for filing a bug report.
 */
export function exportCrashReports(): string {
  return JSON.stringify(
    {
      exportedAt: new Date().toISOString(),
      reports: loadReports(),
    },
    null,
    2,
  );
}
