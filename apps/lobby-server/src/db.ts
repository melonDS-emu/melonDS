/**
 * SQLite database initialisation.
 *
 * Opens (or creates) the database file specified by `DB_PATH` env var.
 * Defaults to an in-memory database so the server works out-of-the-box
 * without any configuration.  Set DB_PATH to a file path for persistence
 * across restarts, e.g. `DB_PATH=/var/data/retro-oasis.db`.
 *
 * All tables are created with IF NOT EXISTS so this module is safe to import
 * in tests (each test gets its own in-memory instance via `openDatabase()`).
 */

import Database from 'better-sqlite3';
import type { Database as DatabaseType } from 'better-sqlite3';

export type { DatabaseType };

/**
 * Open a SQLite database and apply the RetroOasis schema.
 *
 * @param path Filesystem path or `:memory:` for an in-memory database.
 */
export function openDatabase(path = ':memory:'): DatabaseType {
  const db = new Database(path);

  // Enable WAL mode for better concurrent read/write performance.
  db.pragma('journal_mode = WAL');
  db.pragma('foreign_keys = ON');

  db.exec(`
    -- -----------------------------------------------------------------------
    -- Player identity
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS players (
      id          TEXT PRIMARY KEY,
      display_name TEXT NOT NULL,
      created_at  TEXT NOT NULL,
      last_seen_at TEXT NOT NULL
    );

    -- -----------------------------------------------------------------------
    -- Rooms
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS rooms (
      id          TEXT PRIMARY KEY,
      name        TEXT NOT NULL,
      host_id     TEXT NOT NULL,
      game_id     TEXT NOT NULL,
      game_title  TEXT NOT NULL,
      system      TEXT NOT NULL,
      is_public   INTEGER NOT NULL DEFAULT 1,
      room_code   TEXT NOT NULL UNIQUE,
      max_players INTEGER NOT NULL DEFAULT 4,
      status      TEXT NOT NULL DEFAULT 'waiting',
      relay_port  INTEGER,
      created_at  TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS room_players (
      room_id           TEXT NOT NULL,
      player_id         TEXT NOT NULL,
      display_name      TEXT NOT NULL,
      ready_state       TEXT NOT NULL DEFAULT 'not-ready',
      slot              INTEGER NOT NULL DEFAULT 0,
      is_host           INTEGER NOT NULL DEFAULT 0,
      joined_at         TEXT NOT NULL,
      connection_quality TEXT NOT NULL DEFAULT 'unknown',
      latency_ms        INTEGER,
      PRIMARY KEY (room_id, player_id),
      FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS room_spectators (
      room_id      TEXT NOT NULL,
      spectator_id TEXT NOT NULL,
      display_name TEXT NOT NULL,
      joined_at    TEXT NOT NULL,
      PRIMARY KEY (room_id, spectator_id),
      FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE
    );

    -- -----------------------------------------------------------------------
    -- Session history
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS sessions (
      id           TEXT PRIMARY KEY,
      room_id      TEXT NOT NULL,
      game_id      TEXT NOT NULL,
      game_title   TEXT NOT NULL,
      system       TEXT NOT NULL,
      started_at   TEXT NOT NULL,
      ended_at     TEXT,
      duration_secs INTEGER,
      player_count INTEGER NOT NULL DEFAULT 0
    );

    CREATE TABLE IF NOT EXISTS session_players (
      session_id    TEXT NOT NULL,
      display_name  TEXT NOT NULL,
      PRIMARY KEY (session_id, display_name),
      FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE
    );

    -- -----------------------------------------------------------------------
    -- Save sync
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS saves (
      id         TEXT PRIMARY KEY,
      game_id    TEXT NOT NULL,
      name       TEXT NOT NULL,
      data       TEXT NOT NULL,
      mime_type  TEXT NOT NULL DEFAULT 'application/octet-stream',
      created_at TEXT NOT NULL,
      updated_at TEXT NOT NULL,
      version    INTEGER NOT NULL DEFAULT 1,
      UNIQUE (game_id, name)
    );

    -- -----------------------------------------------------------------------
    -- Friends & friend requests
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS friends (
      user_id    TEXT NOT NULL,
      friend_id  TEXT NOT NULL,
      created_at TEXT NOT NULL,
      PRIMARY KEY (user_id, friend_id)
    );

    CREATE TABLE IF NOT EXISTS friend_requests (
      id          TEXT PRIMARY KEY,
      from_id     TEXT NOT NULL,
      to_id       TEXT NOT NULL,
      status      TEXT NOT NULL DEFAULT 'pending',
      created_at  TEXT NOT NULL,
      updated_at  TEXT NOT NULL,
      UNIQUE (from_id, to_id)
    );

    -- -----------------------------------------------------------------------
    -- Matchmaking
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS matchmaking_queue (
      player_id    TEXT PRIMARY KEY,
      display_name TEXT NOT NULL,
      game_id      TEXT NOT NULL,
      game_title   TEXT NOT NULL,
      system       TEXT NOT NULL,
      max_players  INTEGER NOT NULL,
      joined_at    TEXT NOT NULL
    );

    -- -----------------------------------------------------------------------
    -- Achievement persistence (Phase 9)
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS player_achievements (
      player_id      TEXT NOT NULL,
      achievement_id TEXT NOT NULL,
      unlocked_at    TEXT NOT NULL,
      PRIMARY KEY (player_id, achievement_id)
    );

    CREATE TABLE IF NOT EXISTS player_achievement_meta (
      player_id        TEXT PRIMARY KEY,
      display_name     TEXT NOT NULL,
      last_checked_at  TEXT NOT NULL
    );

    -- -----------------------------------------------------------------------
    -- Tournament persistence (Phase 11)
    -- -----------------------------------------------------------------------
    CREATE TABLE IF NOT EXISTS tournaments (
      id          TEXT PRIMARY KEY,
      name        TEXT NOT NULL,
      game_id     TEXT NOT NULL,
      game_title  TEXT NOT NULL,
      system      TEXT NOT NULL,
      status      TEXT NOT NULL DEFAULT 'pending',
      winner      TEXT,
      created_at  TEXT NOT NULL,
      updated_at  TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS tournament_players (
      tournament_id TEXT NOT NULL,
      display_name  TEXT NOT NULL,
      seed          INTEGER NOT NULL,
      PRIMARY KEY (tournament_id, display_name),
      FOREIGN KEY (tournament_id) REFERENCES tournaments(id) ON DELETE CASCADE
    );

    CREATE TABLE IF NOT EXISTS tournament_matches (
      id            TEXT PRIMARY KEY,
      tournament_id TEXT NOT NULL,
      round         INTEGER NOT NULL,
      slot          INTEGER NOT NULL,
      player_a      TEXT,
      player_b      TEXT,
      winner        TEXT,
      status        TEXT NOT NULL DEFAULT 'pending',
      FOREIGN KEY (tournament_id) REFERENCES tournaments(id) ON DELETE CASCADE
    );
  `);

  return db;
}

/** Singleton database instance, initialised from DB_PATH env var. */
let _db: DatabaseType | null = null;

export function getDatabase(): DatabaseType {
  if (!_db) {
    const path = process.env.DB_PATH ?? ':memory:';
    _db = openDatabase(path);
  }
  return _db;
}
