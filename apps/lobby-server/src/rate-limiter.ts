/**
 * Simple per-IP token-bucket rate limiter for WebSocket connections and HTTP requests.
 *
 * Each unique remote IP gets a bucket that refills at `refillRate` tokens per second
 * up to a maximum of `capacity` tokens. Each request/connection consumes one token.
 * When the bucket is empty the request is rejected (rate limited).
 *
 * This is intentionally lightweight — no external dependencies, no persistence.
 * Suitable for a local/LAN lobby server protecting against accidental hammering or
 * basic abuse, not a high-security production deployment.
 */

import * as http from 'http';
import * as net from 'net';

export interface RateLimiterOptions {
  /** Maximum tokens per IP (burst size). Default: 20 */
  capacity?: number;
  /** Tokens added per second per IP. Default: 2 */
  refillRate?: number;
  /** How often (ms) to clean up stale entries from the bucket map. Default: 60_000 */
  gcIntervalMs?: number;
  /** Minimum idle time (ms) before an entry is considered stale. Default: 120_000 */
  staleAfterMs?: number;
}

interface Bucket {
  tokens: number;
  lastRefill: number; // ms timestamp
}

export class RateLimiter {
  private readonly capacity: number;
  private readonly refillRate: number; // tokens per ms
  private readonly staleAfterMs: number;
  private readonly buckets = new Map<string, Bucket>();
  private readonly gcTimer: ReturnType<typeof setInterval>;

  constructor(options: RateLimiterOptions = {}) {
    this.capacity = options.capacity ?? 20;
    this.refillRate = (options.refillRate ?? 2) / 1000; // convert to per-ms
    this.staleAfterMs = options.staleAfterMs ?? 120_000;

    this.gcTimer = setInterval(
      () => this.gc(),
      options.gcIntervalMs ?? 60_000,
    );
    // Allow the process to exit even if this timer is still running
    if (this.gcTimer.unref) this.gcTimer.unref();
  }

  /**
   * Check whether a request from the given IP is allowed.
   * Returns `true` if the request may proceed, `false` if it is rate-limited.
   */
  allow(ip: string): boolean {
    const now = Date.now();
    let bucket = this.buckets.get(ip);

    if (!bucket) {
      bucket = { tokens: this.capacity, lastRefill: now };
      this.buckets.set(ip, bucket);
    }

    // Refill tokens based on elapsed time
    const elapsed = now - bucket.lastRefill;
    bucket.tokens = Math.min(
      this.capacity,
      bucket.tokens + elapsed * this.refillRate,
    );
    bucket.lastRefill = now;

    if (bucket.tokens < 1) {
      return false; // rate limited
    }

    bucket.tokens -= 1;
    return true;
  }

  /** Return the current token count for an IP (for diagnostics). */
  tokenCount(ip: string): number {
    const bucket = this.buckets.get(ip);
    if (!bucket) return this.capacity;
    const elapsed = Date.now() - bucket.lastRefill;
    return Math.min(this.capacity, bucket.tokens + elapsed * this.refillRate);
  }

  /** Stop the internal GC timer (call during shutdown / in tests). */
  destroy(): void {
    clearInterval(this.gcTimer);
  }

  private gc(): void {
    const cutoff = Date.now() - this.staleAfterMs;
    for (const [ip, bucket] of this.buckets) {
      if (bucket.lastRefill < cutoff) {
        this.buckets.delete(ip);
      }
    }
  }
}

/**
 * Extract the remote IP from an http.IncomingMessage or a net.Socket.
 * Falls back to '0.0.0.0' when the address is unavailable.
 */
export function getRemoteIp(source: http.IncomingMessage | net.Socket): string {
  if ('socket' in source) {
    return (source as http.IncomingMessage).socket?.remoteAddress ?? '0.0.0.0';
  }
  return (source as net.Socket).remoteAddress ?? '0.0.0.0';
}
