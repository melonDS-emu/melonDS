/**
 * Tests for the per-IP token-bucket rate limiter.
 */
import { describe, it, expect, beforeEach, afterEach, vi } from 'vitest';
import { RateLimiter } from '../rate-limiter';

describe('RateLimiter', () => {
  let limiter: RateLimiter;

  beforeEach(() => {
    // Disable the GC timer for tests by using a very long interval
    limiter = new RateLimiter({ capacity: 5, refillRate: 1, gcIntervalMs: 99_999_999 });
  });

  afterEach(() => {
    limiter.destroy();
  });

  it('allows requests up to the capacity burst', () => {
    for (let i = 0; i < 5; i++) {
      expect(limiter.allow('10.0.0.1')).toBe(true);
    }
  });

  it('rejects requests that exceed the burst capacity', () => {
    for (let i = 0; i < 5; i++) {
      limiter.allow('10.0.0.1');
    }
    expect(limiter.allow('10.0.0.1')).toBe(false);
  });

  it('tracks different IPs independently', () => {
    for (let i = 0; i < 5; i++) {
      limiter.allow('10.0.0.1');
    }
    // IP A is exhausted but IP B should still be allowed
    expect(limiter.allow('10.0.0.1')).toBe(false);
    expect(limiter.allow('10.0.0.2')).toBe(true);
  });

  it('refills tokens over time', () => {
    vi.useFakeTimers();
    try {
      for (let i = 0; i < 5; i++) {
        limiter.allow('10.0.0.1');
      }
      expect(limiter.allow('10.0.0.1')).toBe(false);

      // Advance 1 second — should refill 1 token (refillRate = 1 token/sec)
      vi.advanceTimersByTime(1000);
      expect(limiter.allow('10.0.0.1')).toBe(true);
      expect(limiter.allow('10.0.0.1')).toBe(false);
    } finally {
      vi.useRealTimers();
    }
  });

  it('does not exceed capacity when refilling', () => {
    vi.useFakeTimers();
    try {
      // Let it sit for a very long time — tokens should cap at capacity
      vi.advanceTimersByTime(100_000); // 100 seconds
      let allowed = 0;
      for (let i = 0; i < 10; i++) {
        if (limiter.allow('10.0.0.3')) allowed++;
      }
      // Should allow exactly `capacity` (5) requests in the burst
      expect(allowed).toBe(5);
    } finally {
      vi.useRealTimers();
    }
  });

  it('tokenCount returns capacity for new IPs', () => {
    expect(limiter.tokenCount('10.0.0.99')).toBe(5);
  });

  it('tokenCount reflects consumed tokens', () => {
    limiter.allow('10.0.0.1');
    limiter.allow('10.0.0.1');
    expect(limiter.tokenCount('10.0.0.1')).toBeLessThan(5);
  });

  it('destroy stops the GC timer without throwing', () => {
    expect(() => limiter.destroy()).not.toThrow();
  });
});
