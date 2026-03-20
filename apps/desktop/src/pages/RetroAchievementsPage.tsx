import { useState, useEffect } from 'react';
import { useLobby } from '../context/LobbyContext';
import {
  fetchRetroAchievementDefs,
  fetchRetroPlayerProgress,
  fetchRetroLeaderboard,
  gameIdToTitle,
  gameIdToSystem,
  type RetroGameAchievementDef,
  type EarnedRetroAchievement,
  type RetroLeaderboardEntry,
} from '../lib/retro-achievement-service';

// ─── Sub-components ───────────────────────────────────────────────────────────

function RetroBadge({
  def,
  earned,
}: {
  def: RetroGameAchievementDef;
  earned: EarnedRetroAchievement | undefined;
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
        border: unlocked
          ? '1px solid var(--color-oasis-accent)'
          : '1px solid transparent',
      }}
      title={
        unlocked
          ? `Unlocked: ${new Date(earned!.earnedAt).toLocaleDateString()}`
          : 'Locked'
      }
    >
      <span className="text-2xl">{def.badge}</span>
      <div className="min-w-0 flex-1">
        <div className="flex items-center gap-2">
          <p
            className="text-sm font-semibold"
            style={{ color: 'var(--color-oasis-text)' }}
          >
            {def.title}
          </p>
          <span
            className="text-[10px] font-bold px-1.5 py-0.5 rounded-full"
            style={{
              backgroundColor: unlocked
                ? 'var(--color-oasis-accent)'
                : 'var(--color-oasis-border)',
              color: unlocked ? '#fff' : 'var(--color-oasis-text-muted)',
            }}
          >
            {def.points}pts
          </span>
        </div>
        <p
          className="text-xs mt-0.5"
          style={{ color: 'var(--color-oasis-text-muted)' }}
        >
          {def.description}
        </p>
        {unlocked && (
          <p
            className="text-[10px] mt-1"
            style={{ color: 'var(--color-oasis-accent-light)' }}
          >
            ✓ {new Date(earned!.earnedAt).toLocaleDateString()}
          </p>
        )}
      </div>
    </div>
  );
}

// ─── Page ─────────────────────────────────────────────────────────────────────

interface GameGroup {
  gameId: string;
  title: string;
  system: string;
  defs: RetroGameAchievementDef[];
  earnedCount: number;
  totalPoints: number;
  earnedPoints: number;
}

export function RetroAchievementsPage() {
  const { playerId } = useLobby();

  const [activeTab, setActiveTab] = useState<'achievements' | 'leaderboard'>('achievements');
  const [defs, setDefs] = useState<RetroGameAchievementDef[]>([]);
  const [earnedMap, setEarnedMap] = useState<Map<string, EarnedRetroAchievement>>(
    new Map()
  );
  const [leaderboard, setLeaderboard] = useState<RetroLeaderboardEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [activeGame, setActiveGame] = useState<string | null>(null);

  useEffect(() => {
    let alive = true;
    async function load() {
      setLoading(true);
      const [allDefs, progress, lb] = await Promise.all([
        fetchRetroAchievementDefs(),
        playerId ? fetchRetroPlayerProgress(playerId) : Promise.resolve(null),
        fetchRetroLeaderboard(20),
      ]);
      if (!alive) return;
      setDefs(allDefs);
      const map = new Map<string, EarnedRetroAchievement>();
      for (const e of progress?.earned ?? []) {
        map.set(e.achievementId, e);
      }
      setEarnedMap(map);
      setLeaderboard(lb);
      setLoading(false);
    }
    void load();
    return () => { alive = false; };
  }, [playerId]);

  // Group defs by game
  const groups: GameGroup[] = [];
  const seen = new Set<string>();
  for (const def of defs) {
    if (!seen.has(def.gameId)) {
      seen.add(def.gameId);
      const gameDefs = defs.filter((d) => d.gameId === def.gameId);
      const earnedDefs = gameDefs.filter((d) => earnedMap.has(d.id));
      groups.push({
        gameId: def.gameId,
        title: gameIdToTitle(def.gameId),
        system: gameIdToSystem(def.gameId),
        defs: gameDefs,
        earnedCount: earnedDefs.length,
        totalPoints: gameDefs.reduce((s, d) => s + d.points, 0),
        earnedPoints: earnedDefs.reduce((s, d) => s + d.points, 0),
      });
    }
  }

  const totalAchievements = defs.length;
  const totalEarned = defs.filter((d) => earnedMap.has(d.id)).length;
  const totalPoints = defs.reduce((s, d) => s + d.points, 0);
  const earnedPoints = defs
    .filter((d) => earnedMap.has(d.id))
    .reduce((s, d) => s + d.points, 0);

  const selectedGroup = activeGame ? groups.find((g) => g.gameId === activeGame) : null;

  return (
    <div className="p-6 max-w-5xl mx-auto">
      {/* Header */}
      <div className="mb-4">
        <h1
          className="text-2xl font-bold mb-1"
          style={{ color: 'var(--color-oasis-text)' }}
        >
          🏅 Retro Achievements
        </h1>
        <p className="text-sm" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Per-game milestones across your retro library
        </p>
      </div>

      {/* Tab bar */}
      <div className="flex gap-2 mb-6">
        {(['achievements', 'leaderboard'] as const).map((tab) => (
          <button
            key={tab}
            onClick={() => setActiveTab(tab)}
            className="px-4 py-1.5 rounded-full text-sm font-medium transition-all"
            style={{
              backgroundColor:
                activeTab === tab ? 'var(--color-oasis-accent)' : 'var(--color-oasis-card)',
              color: activeTab === tab ? '#fff' : 'var(--color-oasis-text)',
            }}
          >
            {tab === 'achievements' ? '🏅 Achievements' : '🏆 Leaderboard'}
          </button>
        ))}
      </div>

      {/* Summary bar — only on Achievements tab */}
      {activeTab === 'achievements' && !loading && (
        <div
          className="rounded-xl p-4 mb-6 flex gap-6 flex-wrap"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          <div>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Achievements
            </p>
            <p className="text-xl font-bold" style={{ color: 'var(--color-oasis-text)' }}>
              {totalEarned} / {totalAchievements}
            </p>
          </div>
          <div>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Points
            </p>
            <p className="text-xl font-bold" style={{ color: 'var(--color-oasis-text)' }}>
              {earnedPoints} / {totalPoints}
            </p>
          </div>
          <div>
            <p className="text-xs" style={{ color: 'var(--color-oasis-text-muted)' }}>
              Games
            </p>
            <p className="text-xl font-bold" style={{ color: 'var(--color-oasis-text)' }}>
              {groups.filter((g) => g.earnedCount > 0).length} / {groups.length}
            </p>
          </div>
          {/* Progress bar */}
          <div className="flex-1 min-w-48 flex flex-col justify-center">
            <div
              className="h-2 rounded-full overflow-hidden"
              style={{ backgroundColor: 'var(--color-oasis-border)' }}
            >
              <div
                className="h-full rounded-full transition-all"
                style={{
                  width: totalAchievements > 0
                    ? `${Math.round((totalEarned / totalAchievements) * 100)}%`
                    : '0%',
                  backgroundColor: 'var(--color-oasis-accent)',
                }}
              />
            </div>
            <p
              className="text-[10px] mt-1 text-right"
              style={{ color: 'var(--color-oasis-text-muted)' }}
            >
              {totalAchievements > 0
                ? `${Math.round((totalEarned / totalAchievements) * 100)}% complete`
                : '0% complete'}
            </p>
          </div>
        </div>
      )}

      {loading ? (
        <p className="text-center py-12" style={{ color: 'var(--color-oasis-text-muted)' }}>
          Loading achievements…
        </p>
      ) : activeTab === 'leaderboard' ? (
        /* ── Leaderboard tab ──────────────────────────────────────────────── */
        <div
          className="rounded-xl overflow-hidden"
          style={{ backgroundColor: 'var(--color-oasis-card)' }}
        >
          {leaderboard.length === 0 ? (
            <p
              className="text-center py-12 text-sm"
              style={{ color: 'var(--color-oasis-text-muted)' }}
            >
              No players have earned retro achievements yet.
            </p>
          ) : (
            <table className="w-full text-sm">
              <thead>
                <tr style={{ borderBottom: '1px solid var(--color-oasis-border)' }}>
                  <th
                    className="text-left px-4 py-3 font-semibold text-xs"
                    style={{ color: 'var(--color-oasis-text-muted)' }}
                  >
                    Rank
                  </th>
                  <th
                    className="text-left px-4 py-3 font-semibold text-xs"
                    style={{ color: 'var(--color-oasis-text-muted)' }}
                  >
                    Player
                  </th>
                  <th
                    className="text-right px-4 py-3 font-semibold text-xs"
                    style={{ color: 'var(--color-oasis-text-muted)' }}
                  >
                    Points
                  </th>
                  <th
                    className="text-right px-4 py-3 font-semibold text-xs"
                    style={{ color: 'var(--color-oasis-text-muted)' }}
                  >
                    Unlocked
                  </th>
                </tr>
              </thead>
              <tbody>
                {leaderboard.map((entry, idx) => (
                  <tr
                    key={entry.playerId}
                    style={{ borderBottom: '1px solid var(--color-oasis-border)' }}
                  >
                    <td className="px-4 py-3 font-bold" style={{ color: 'var(--color-oasis-text-muted)' }}>
                      {idx === 0 ? '🥇' : idx === 1 ? '🥈' : idx === 2 ? '🥉' : `#${idx + 1}`}
                    </td>
                    <td className="px-4 py-3 font-medium" style={{ color: 'var(--color-oasis-text)' }}>
                      {entry.playerId === playerId ? (
                        <span>
                          {entry.playerId}
                          <span
                            className="ml-2 text-[10px] px-1.5 py-0.5 rounded-full"
                            style={{
                              backgroundColor: 'var(--color-oasis-accent)',
                              color: '#fff',
                            }}
                          >
                            You
                          </span>
                        </span>
                      ) : (
                        entry.playerId
                      )}
                    </td>
                    <td
                      className="px-4 py-3 text-right font-bold"
                      style={{ color: 'var(--color-oasis-accent)' }}
                    >
                      {entry.totalPoints}
                    </td>
                    <td
                      className="px-4 py-3 text-right"
                      style={{ color: 'var(--color-oasis-text-muted)' }}
                    >
                      {entry.earnedCount}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      ) : (
        /* ── Achievements tab ─────────────────────────────────────────────── */
        <div className="flex gap-6">
          {/* Game list sidebar */}
          <div className="w-56 shrink-0 flex flex-col gap-2">
            <button
              onClick={() => setActiveGame(null)}
              className="rounded-lg px-3 py-2 text-left text-xs font-bold transition-all"
              style={{
                backgroundColor: activeGame === null
                  ? 'var(--color-oasis-accent)'
                  : 'var(--color-oasis-card)',
                color: activeGame === null ? '#fff' : 'var(--color-oasis-text)',
              }}
            >
              All Games
            </button>
            {groups.map((g) => (
              <button
                key={g.gameId}
                onClick={() => setActiveGame(g.gameId)}
                className="rounded-lg px-3 py-2 text-left transition-all"
                style={{
                  backgroundColor:
                    activeGame === g.gameId
                      ? 'var(--color-oasis-accent)'
                      : 'var(--color-oasis-card)',
                  color:
                    activeGame === g.gameId ? '#fff' : 'var(--color-oasis-text)',
                  border: '1px solid transparent',
                }}
              >
                <p className="text-xs font-semibold">{g.title}</p>
                <p
                  className="text-[10px]"
                  style={{
                    color:
                      activeGame === g.gameId
                        ? 'rgba(255,255,255,0.75)'
                        : 'var(--color-oasis-text-muted)',
                  }}
                >
                  {g.system} · {g.earnedCount}/{g.defs.length}
                </p>
              </button>
            ))}
          </div>

          {/* Achievement grid */}
          <div className="flex-1 min-w-0">
            {selectedGroup ? (
              <>
                <div className="mb-4 flex items-baseline gap-3">
                  <h2
                    className="text-lg font-bold"
                    style={{ color: 'var(--color-oasis-text)' }}
                  >
                    {selectedGroup.title}
                  </h2>
                  <span
                    className="text-xs"
                    style={{ color: 'var(--color-oasis-text-muted)' }}
                  >
                    {selectedGroup.earnedCount}/{selectedGroup.defs.length} · {selectedGroup.earnedPoints}/{selectedGroup.totalPoints}pts
                  </span>
                </div>
                <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
                  {selectedGroup.defs.map((def) => (
                    <RetroBadge
                      key={def.id}
                      def={def}
                      earned={earnedMap.get(def.id)}
                    />
                  ))}
                </div>
              </>
            ) : (
              <>
                <h2
                  className="text-base font-bold mb-4"
                  style={{ color: 'var(--color-oasis-text)' }}
                >
                  All Games
                </h2>
                {groups.map((g) => (
                  <div key={g.gameId} className="mb-6">
                    <div className="flex items-center gap-2 mb-2">
                      <h3
                        className="text-sm font-bold"
                        style={{ color: 'var(--color-oasis-text)' }}
                      >
                        {g.title}
                      </h3>
                      <span
                        className="text-[10px] px-1.5 py-0.5 rounded"
                        style={{
                          backgroundColor: 'var(--color-oasis-border)',
                          color: 'var(--color-oasis-text-muted)',
                        }}
                      >
                        {g.system}
                      </span>
                      <span
                        className="text-[10px]"
                        style={{ color: 'var(--color-oasis-text-muted)' }}
                      >
                        {g.earnedCount}/{g.defs.length} · {g.earnedPoints}/{g.totalPoints}pts
                      </span>
                    </div>
                    <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                      {g.defs.map((def) => (
                        <RetroBadge
                          key={def.id}
                          def={def}
                          earned={earnedMap.get(def.id)}
                        />
                      ))}
                    </div>
                  </div>
                ))}
              </>
            )}
          </div>
        </div>
      )}
    </div>
  );
}
