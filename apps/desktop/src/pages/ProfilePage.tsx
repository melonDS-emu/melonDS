import { useState, useEffect, useCallback } from 'react';
import {
  fetchAchievementDefs,
  fetchPlayerAchievements,
  fetchPlayerStats,
  fetchLeaderboard,
  fetchPlayerTournaments,
  formatDuration,
  type AchievementDef,
  type PlayerAchievement,
  type PlayerStats,
  type LeaderboardEntry,
  type LeaderboardMetric,
  type PlayerTournamentEntry,
} from '../lib/stats-service';
import {
  fetchPlayerRank,
  TIER_COLORS,
  TIER_ICONS,
  playerTitle,
  type PlayerRank,
} from '../lib/community-service';

// ─── Category config ──────────────────────────────────────────────────────────

const CATEGORY_LABELS: Record<string, string> = {
  sessions: '🎮 Sessions',
  social: '👥 Social',
  systems: '🕹️ Systems',
  games: '🏆 Games',
  streaks: '📅 Streaks',
};

const METRIC_LABELS: Record<LeaderboardMetric, string> = {
  sessions: 'Most Sessions',
  playtime: 'Most Playtime',
  games: 'Most Games',
  systems: 'Most Systems',
};

const METRIC_UNIT: Record<LeaderboardMetric, (v: number) => string> = {
  sessions: (v) => `${v} sessions`,
  playtime: (v) => formatDuration(v),
  games: (v) => `${v} games`,
  systems: (v) => `${v} systems`,
};

// ─── Subcomponents ────────────────────────────────────────────────────────────

function AchievementBadge({
  def,
  earned,
}: {
  def: AchievementDef;
  earned: PlayerAchievement | undefined;
}) {
  const unlocked = !!earned;
  return (
    <div
      className="rounded-xl p-3 flex gap-3 items-start"
      style={{
        backgroundColor: unlocked
          ? 'var(--color-oasis-card)'
          : 'var(--color-oasis-surface)',
        opacity: unlocked ? 1 : 0.45,
        border: unlocked ? '1px solid var(--color-oasis-accent)' : '1px solid transparent',
      }}
      title={unlocked ? `Unlocked: ${new Date(earned!.unlockedAt).toLocaleDateString()}` : 'Locked'}
    >
      <span className="text-2xl">{def.icon}</span>
      <div className="min-w-0">
        <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text)' }}>
          {def.name}
        </p>
        <p className="text-xs mt-0.5" style={{ color: 'var(--color-oasis-text-muted)' }}>
          {def.description}
        </p>
        {unlocked && (
          <p className="text-[10px] mt-1" style={{ color: 'var(--color-oasis-accent-light)' }}>
            ✓ {new Date(earned!.unlockedAt).toLocaleDateString()}
          </p>
        )}
      </div>
    </div>
  );
}

function StatCard({ label, value }: { label: string; value: string | number }) {
  return (
    <div
      className="rounded-xl p-4 text-center"
      style={{ backgroundColor: 'var(--color-oasis-card)' }}
    >
      <p className="text-xl font-bold" style={{ color: 'var(--color-oasis-accent-light)' }}>
        {value}
      </p>
      <p className="text-xs mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
        {label}
      </p>
    </div>
  );
}

// ─── Page ─────────────────────────────────────────────────────────────────────

export function ProfilePage() {
  const storedName =
    typeof localStorage !== 'undefined' ? localStorage.getItem('displayName') : null;
  const defaultName = storedName ?? '';

  const [nameInput, setNameInput] = useState(defaultName);
  const [activeName, setActiveName] = useState(defaultName);

  const [defs, setDefs] = useState<AchievementDef[]>([]);
  const [earned, setEarned] = useState<PlayerAchievement[]>([]);
  const [stats, setStats] = useState<PlayerStats | null>(null);
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [lbMetric, setLbMetric] = useState<LeaderboardMetric>('sessions');
  const [activeCategory, setActiveCategory] = useState<string>('all');
  const [tournaments, setTournaments] = useState<PlayerTournamentEntry[]>([]);
  const [playerRank, setPlayerRank] = useState<PlayerRank | null>(null);
  const [loading, setLoading] = useState(false);

  const load = useCallback(
    async (name: string) => {
      if (!name) return;
      setLoading(true);
      try {
        const [defsData, earnedData, statsData, tourneyData, rankData] = await Promise.all([
          fetchAchievementDefs(),
          fetchPlayerAchievements(name),
          fetchPlayerStats(name),
          fetchPlayerTournaments(name),
          fetchPlayerRank(name).catch(() => null),
        ]);
        setDefs(defsData);
        setEarned(earnedData);
        setStats(statsData);
        setTournaments(tourneyData);
        setPlayerRank(rankData);
      } finally {
        setLoading(false);
      }
    },
    []
  );

  useEffect(() => {
    void load(activeName);
  }, [activeName, load]);

  useEffect(() => {
    void fetchLeaderboard(lbMetric).then(setLeaderboard);
  }, [lbMetric]);

  function handleLookup() {
    const trimmed = nameInput.trim();
    if (trimmed) setActiveName(trimmed);
  }

  const earnedMap = new Map(earned.map((e) => [e.achievementId, e]));
  const categories = ['all', ...Object.keys(CATEGORY_LABELS)];
  const filteredDefs =
    activeCategory === 'all'
      ? defs
      : defs.filter((d) => d.category === activeCategory);

  const earnedCount = earned.length;
  const totalCount = defs.length;

  return (
    <div className="space-y-8 max-w-4xl">
      {/* Header */}
      <div>
        <h1 className="text-2xl font-bold" style={{ color: 'var(--color-oasis-text)' }}>
          👤 Player Profile
        </h1>
        <p className="text-sm mt-1" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Stats, achievements, and leaderboard rankings.
        </p>
      </div>

      {/* Name lookup */}
      <div
        className="rounded-2xl p-5"
        style={{ backgroundColor: 'var(--color-oasis-surface)' }}
      >
        <p className="text-sm font-semibold mb-3" style={{ color: 'var(--color-oasis-text)' }}>
          Look up a player
        </p>
        <div className="flex gap-3">
          <input
            type="text"
            value={nameInput}
            onChange={(e) => setNameInput(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && handleLookup()}
            placeholder="Enter display name…"
            className="flex-1 px-4 py-2 rounded-xl text-sm"
            style={{
              backgroundColor: 'var(--color-oasis-card)',
              color: 'var(--color-oasis-text)',
              border: '1px solid var(--color-oasis-accent)',
              outline: 'none',
            }}
          />
          <button
            onClick={handleLookup}
            className="px-5 py-2 rounded-xl text-sm font-bold"
            style={{
              backgroundColor: 'var(--color-oasis-accent)',
              color: 'white',
            }}
          >
            Look Up
          </button>
        </div>
        {activeName && (
          <p className="text-xs mt-2" style={{ color: 'var(--color-oasis-text-muted)' }}>
            Showing stats for <strong style={{ color: 'var(--color-oasis-accent-light)' }}>{activeName}</strong>
          </p>
        )}
      </div>

      {loading && (
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Loading…
        </p>
      )}

      {/* Rank badge */}
      {playerRank && !loading && (
        <section>
          <h2 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-text)' }}>
            🏅 Ranked Standing
          </h2>
          <div
            className="rounded-2xl px-5 py-4 flex items-center gap-4"
            style={{
              backgroundColor: 'var(--color-oasis-surface)',
              border: `1px solid ${TIER_COLORS[playerRank.tier]}44`,
            }}
          >
            <span className="text-4xl">{TIER_ICONS[playerRank.tier]}</span>
            <div className="flex-1">
              <p className="font-black text-lg" style={{ color: TIER_COLORS[playerRank.tier] }}>
                {playerRank.tier}
              </p>
              <p className="text-sm font-semibold" style={{ color: 'var(--color-oasis-text-muted)' }}>
                {playerTitle(playerRank.tier, playerRank.gamesPlayed)}
              </p>
            </div>
            <div className="text-right">
              <p className="font-black text-2xl" style={{ color: TIER_COLORS[playerRank.tier] }}>{playerRank.elo}</p>
              <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>ELO</p>
            </div>
            <div className="text-right">
              <p className="font-bold text-sm" style={{ color: 'var(--color-oasis-text)' }}>{playerRank.wins}W / {playerRank.losses}L</p>
              <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>{playerRank.gamesPlayed} ranked games</p>
            </div>
          </div>
        </section>
      )}

      {/* Stats */}
      {stats && !loading && (
        <section>
          <h2 className="text-base font-bold mb-4" style={{ color: 'var(--color-oasis-text)' }}>
            📊 Stats
          </h2>
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 mb-4">
            <StatCard label="Sessions" value={stats.completedSessions} />
            <StatCard label="Playtime" value={formatDuration(stats.totalPlaytimeSecs)} />
            <StatCard label="Avg Session" value={formatDuration(stats.avgSessionSecs)} />
            <StatCard label="Active Days" value={stats.activeDays} />
          </div>
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
            <StatCard label="Longest Session" value={formatDuration(stats.longestSessionSecs)} />
            <StatCard label="Playmates" value={stats.uniquePlaymates} />
            <StatCard label="Systems" value={stats.bySystem.length} />
            <StatCard label="Games" value={stats.uniqueGamesCount} />
          </div>

          {stats.topGames.length > 0 && (
            <div
              className="rounded-2xl p-4 mt-4"
              style={{ backgroundColor: 'var(--color-oasis-surface)' }}
            >
              <p className="text-xs font-semibold mb-3" style={{ color: 'var(--color-oasis-text-muted)' }}>
                TOP GAMES
              </p>
              <div className="space-y-2">
                {stats.topGames.map((g) => (
                  <div key={g.gameId} className="flex items-center justify-between">
                    <span className="text-sm" style={{ color: 'var(--color-oasis-text)' }}>
                      {g.gameTitle}
                    </span>
                    <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {g.sessions} sessions · {formatDuration(g.totalSecs)}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </section>
      )}

      {!stats && !loading && activeName && (
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          No session data found for <strong>{activeName}</strong>. Start playing to build your stats!
        </p>
      )}

      {/* Achievements */}
      {defs.length > 0 && (
        <section>
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-base font-bold" style={{ color: 'var(--color-oasis-text)' }}>
              🏅 Achievements
            </h2>
            <span className="text-sm font-semibold" style={{ color: 'var(--color-oasis-accent-light)' }}>
              {earnedCount} / {totalCount}
            </span>
          </div>

          {/* Progress bar */}
          <div
            className="w-full h-2 rounded-full mb-4 overflow-hidden"
            style={{ backgroundColor: 'var(--color-oasis-card)' }}
          >
            <div
              className="h-full rounded-full transition-all"
              style={{
                width: `${totalCount > 0 ? (earnedCount / totalCount) * 100 : 0}%`,
                backgroundColor: 'var(--color-oasis-accent)',
              }}
            />
          </div>

          {/* Category tabs */}
          <div className="flex flex-wrap gap-2 mb-4">
            {categories.map((cat) => (
              <button
                key={cat}
                onClick={() => setActiveCategory(cat)}
                className="px-3 py-1 rounded-lg text-xs font-medium transition-colors"
                style={{
                  backgroundColor:
                    activeCategory === cat
                      ? 'var(--color-oasis-accent)'
                      : 'var(--color-oasis-surface)',
                  color:
                    activeCategory === cat
                      ? 'white'
                      : 'var(--color-oasis-text-muted)',
                }}
              >
                {cat === 'all' ? '🗂 All' : CATEGORY_LABELS[cat]}
              </button>
            ))}
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
            {filteredDefs.map((def) => (
              <AchievementBadge
                key={def.id}
                def={def}
                earned={earnedMap.get(def.id)}
              />
            ))}
          </div>
        </section>
      )}

      {/* Tournament History */}
      {tournaments.length > 0 && (
        <section>
          <h2 className="text-base font-bold mb-3" style={{ color: 'var(--color-oasis-text)' }}>
            🏅 Tournament History
          </h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
            {tournaments.map((t) => {
              const isWinner = t.winner === activeName;
              const isFinal = t.status === 'completed';
              return (
                <div
                  key={t.id}
                  className="rounded-xl p-4 flex gap-3 items-start"
                  style={{
                    backgroundColor: 'var(--color-oasis-card)',
                    border: isWinner ? '1px solid #FFD700' : '1px solid transparent',
                  }}
                >
                  <span className="text-2xl">{isWinner ? '🥇' : isFinal ? '🏅' : '🎮'}</span>
                  <div className="min-w-0 flex-1">
                    <p className="text-sm font-semibold truncate" style={{ color: 'var(--color-oasis-text)' }}>
                      {t.name}
                    </p>
                    <p className="text-xs mt-0.5 truncate" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {t.gameTitle} · {t.playerCount} players
                    </p>
                    <p className="text-[10px] mt-1" style={{ color: isWinner ? '#FFD700' : 'var(--color-oasis-text-muted)' }}>
                      {isWinner
                        ? '🏆 Winner'
                        : isFinal && t.winner
                          ? `Won by ${t.winner}`
                          : t.status === 'active'
                            ? '⚔️ In progress'
                            : 'Pending'}
                    </p>
                  </div>
                  <span className="text-[10px] flex-shrink-0" style={{ color: 'var(--color-oasis-text-muted)' }}>
                    {new Date(t.createdAt).toLocaleDateString()}
                  </span>
                </div>
              );
            })}
          </div>
        </section>
      )}

      {/* Leaderboard */}
      <section>
        <div className="flex items-center justify-between mb-4">
          <h2 className="text-base font-bold" style={{ color: 'var(--color-oasis-text)' }}>
            🏆 Global Leaderboard
          </h2>
          <div className="flex gap-2">
            {(Object.keys(METRIC_LABELS) as LeaderboardMetric[]).map((m) => (
              <button
                key={m}
                onClick={() => setLbMetric(m)}
                className="px-2.5 py-1 rounded-lg text-xs font-medium transition-colors"
                style={{
                  backgroundColor:
                    lbMetric === m
                      ? 'var(--color-oasis-accent)'
                      : 'var(--color-oasis-surface)',
                  color: lbMetric === m ? 'white' : 'var(--color-oasis-text-muted)',
                }}
              >
                {METRIC_LABELS[m]}
              </button>
            ))}
          </div>
        </div>

        {leaderboard.length > 0 ? (
          <div
            className="rounded-2xl overflow-hidden"
            style={{ backgroundColor: 'var(--color-oasis-surface)' }}
          >
            {leaderboard.map((entry) => (
              <div
                key={entry.playerId}
                className="flex items-center gap-3 px-4 py-3 border-b last:border-b-0"
                style={{ borderColor: 'var(--color-oasis-card)' }}
              >
                <span
                  className="w-6 text-center text-sm font-bold"
                  style={{
                    color:
                      entry.rank === 1
                        ? '#FFD700'
                        : entry.rank === 2
                          ? '#C0C0C0'
                          : entry.rank === 3
                            ? '#CD7F32'
                            : 'var(--color-oasis-text-muted)',
                  }}
                >
                  {entry.rank === 1 ? '🥇' : entry.rank === 2 ? '🥈' : entry.rank === 3 ? '🥉' : `#${entry.rank}`}
                </span>
                <span
                  className="flex-1 text-sm font-medium"
                  style={{ color: 'var(--color-oasis-text)' }}
                >
                  {entry.displayName}
                </span>
                <span className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
                  {METRIC_UNIT[lbMetric](entry.value)}
                </span>
              </div>
            ))}
          </div>
        ) : (
          <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
            No sessions yet — play a game to appear on the leaderboard!
          </p>
        )}
      </section>
    </div>
  );
}
