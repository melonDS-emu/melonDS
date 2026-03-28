/**
 * Phase 29 — Desktop Productization
 *
 * Tests for:
 *  - crash-reporting.ts  — opt-in flag, recordCrash, getCrashReports, export
 *  - first-run-service.ts — isFirstRunComplete, markFirstRunComplete, reset,
 *    completeStep, getSetupProgress, isStepComplete, nextIncompleteStep
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  isCrashReportingEnabled,
  enableCrashReporting,
  disableCrashReporting,
  recordCrash,
  getCrashReports,
  clearCrashReports,
  exportCrashReports,
} from '../crash-reporting';
import {
  isFirstRunComplete,
  markFirstRunComplete,
  resetFirstRun,
  completeStep,
  getSetupProgress,
  isStepComplete,
  nextIncompleteStep,
} from '../first-run-service';

// ---------------------------------------------------------------------------
// localStorage mock — follows the same pattern as phase-1.test.ts
// ---------------------------------------------------------------------------

const storageData: Record<string, string> = {};
const mockLocalStorage = {
  getItem: (key: string) => storageData[key] ?? null,
  setItem: (key: string, value: string) => { storageData[key] = value; },
  removeItem: (key: string) => { delete storageData[key]; },
  clear: () => { Object.keys(storageData).forEach((k) => delete storageData[k]); },
  get length() { return Object.keys(storageData).length; },
  key: (i: number) => Object.keys(storageData)[i] ?? null,
};
Object.defineProperty(globalThis, 'localStorage', {
  value: mockLocalStorage,
  writable: true,
  configurable: true,
});

// Clear storage before every single test.
beforeEach(() => {
  mockLocalStorage.clear();
});

// ---------------------------------------------------------------------------
// crash-reporting
// ---------------------------------------------------------------------------

describe('crash-reporting — opt-in flag', () => {
  it('is disabled by default', () => {
    expect(isCrashReportingEnabled()).toBe(false);
  });

  it('enableCrashReporting sets flag to true', () => {
    enableCrashReporting();
    expect(isCrashReportingEnabled()).toBe(true);
  });

  it('disableCrashReporting sets flag to false', () => {
    enableCrashReporting();
    disableCrashReporting();
    expect(isCrashReportingEnabled()).toBe(false);
  });
});

describe('crash-reporting — recordCrash', () => {
  it('returns null when reporting is disabled', () => {
    const result = recordCrash(new Error('boom'));
    expect(result).toBeNull();
  });

  it('returns a report when reporting is enabled', () => {
    enableCrashReporting();
    const report = recordCrash(new Error('test error'), '/library');
    expect(report).not.toBeNull();
    expect(report!.message).toBe('test error');
    expect(report!.context).toBe('/library');
    expect(report!.id).toMatch(/^cr-/);
    expect(report!.timestamp).toBeTruthy();
  });

  it('stores error stack when an Error object is provided', () => {
    enableCrashReporting();
    const err = new Error('with stack');
    const report = recordCrash(err);
    expect(report!.stack).toContain('Error');
  });

  it('accepts a plain string error message', () => {
    enableCrashReporting();
    const report = recordCrash('string error');
    expect(report!.message).toBe('string error');
    expect(report!.stack).toBeUndefined();
  });

  it('getCrashReports returns recorded reports', () => {
    enableCrashReporting();
    recordCrash(new Error('first'));
    recordCrash(new Error('second'));
    const reports = getCrashReports();
    expect(reports.length).toBeGreaterThanOrEqual(2);
  });
});

describe('crash-reporting — clearCrashReports', () => {
  it('removes all stored reports', () => {
    enableCrashReporting();
    recordCrash(new Error('x'));
    clearCrashReports();
    expect(getCrashReports().length).toBe(0);
  });
});

describe('crash-reporting — exportCrashReports', () => {
  it('returns valid JSON with exportedAt and reports fields', () => {
    enableCrashReporting();
    recordCrash(new Error('export test'));
    const json = exportCrashReports();
    const parsed = JSON.parse(json) as { exportedAt: string; reports: unknown[] };
    expect(parsed.exportedAt).toBeTruthy();
    expect(Array.isArray(parsed.reports)).toBe(true);
    expect(parsed.reports.length).toBeGreaterThan(0);
  });

  it('returns valid JSON even with no reports', () => {
    clearCrashReports();
    const json = exportCrashReports();
    const parsed = JSON.parse(json) as { reports: unknown[] };
    expect(parsed.reports).toEqual([]);
  });
});

// ---------------------------------------------------------------------------
// first-run-service
// ---------------------------------------------------------------------------

describe('first-run-service — isFirstRunComplete', () => {
  it('returns false when never completed', () => {
    expect(isFirstRunComplete()).toBe(false);
  });

  it('returns true after markFirstRunComplete', () => {
    markFirstRunComplete();
    expect(isFirstRunComplete()).toBe(true);
  });

  it('returns false after resetFirstRun', () => {
    markFirstRunComplete();
    resetFirstRun();
    expect(isFirstRunComplete()).toBe(false);
  });
});

describe('first-run-service — completeStep', () => {
  it('marks a step as complete', () => {
    completeStep('display-name');
    expect(isStepComplete('display-name')).toBe(true);
  });

  it('does not duplicate steps when called twice', () => {
    completeStep('display-name');
    completeStep('display-name');
    const progress = getSetupProgress();
    expect(progress.completedSteps.filter((s) => s === 'display-name').length).toBe(1);
  });

  it('stores displayName in progress', () => {
    completeStep('display-name', { displayName: 'RetroFan64' });
    expect(getSetupProgress().displayName).toBe('RetroFan64');
  });

  it('stores romDirectory in progress', () => {
    completeStep('rom-directory', { romDirectory: '/home/user/ROMs' });
    expect(getSetupProgress().romDirectory).toBe('/home/user/ROMs');
  });
});

describe('first-run-service — nextIncompleteStep', () => {
  it('starts at display-name', () => {
    expect(nextIncompleteStep()).toBe('display-name');
  });

  it('advances to emulator-paths after display-name', () => {
    completeStep('display-name');
    expect(nextIncompleteStep()).toBe('emulator-paths');
  });

  it('advances to rom-directory after emulator-paths', () => {
    completeStep('display-name');
    completeStep('emulator-paths');
    expect(nextIncompleteStep()).toBe('rom-directory');
  });

  it('returns done when all steps are complete', () => {
    completeStep('display-name');
    completeStep('emulator-paths');
    completeStep('rom-directory');
    expect(nextIncompleteStep()).toBe('done');
  });
});

describe('first-run-service — getSetupProgress default', () => {
  it('returns empty completedSteps by default', () => {
    expect(getSetupProgress().completedSteps).toEqual([]);
  });

  it('returns empty displayName and romDirectory by default', () => {
    const p = getSetupProgress();
    expect(p.displayName).toBe('');
    expect(p.romDirectory).toBe('');
  });
});
