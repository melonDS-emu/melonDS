/**
 * React hooks that expose the GameApiClient with loading / error state.
 *
 * useGames(query?)   – pageable/filtered game list
 * useGame(id)        – single game by ID
 * useSystems()       – list of supported system strings
 */

import { useState, useEffect, useCallback } from 'react';
import { apiClient } from './api-client';
import type { ApiGame, ApiGameQuery, FetchState } from '../types/api';

// ---------------------------------------------------------------------------
// useGames
// ---------------------------------------------------------------------------

/** Subscribe to a filtered list of games. Re-fetches when `query` changes. */
export function useGames(query?: ApiGameQuery): FetchState<ApiGame[]> & { refetch: () => void } {
  const [state, setState] = useState<FetchState<ApiGame[]>>({
    data: [],
    loading: true,
    error: null,
  });

  // Stable serialised key for deep-equality comparison of the query object.
  // `query` is a plain object so we can't use it directly in the dep array
  // without risking infinite re-fetch loops on every render.
  const queryKey = JSON.stringify(query ?? null);

  const load = useCallback(() => {
    setState((s) => ({ ...s, loading: true, error: null }));
    apiClient
      .getGames(query)
      .then((data) => setState({ data, loading: false, error: null }))
      .catch((err: unknown) => {
        const msg = err instanceof Error ? err.message : 'Failed to load games';
        setState({ data: [], loading: false, error: msg });
      });
    // queryKey is a serialised snapshot of `query`; using it instead of `query`
    // avoids infinite loops when callers pass an inline object literal.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [queryKey]);

  useEffect(() => {
    load();
  }, [load]);

  return { ...state, refetch: load };
}

// ---------------------------------------------------------------------------
// useGame
// ---------------------------------------------------------------------------

/** Subscribe to a single game by ID. */
export function useGame(id: string | undefined): FetchState<ApiGame | null> & { refetch: () => void } {
  const [state, setState] = useState<FetchState<ApiGame | null>>({
    data: null,
    loading: Boolean(id),
    error: null,
  });

  const load = useCallback(() => {
    if (!id) {
      setState({ data: null, loading: false, error: null });
      return;
    }
    setState((s) => ({ ...s, loading: true, error: null }));
    apiClient
      .getGameById(id)
      .then((data) => setState({ data, loading: false, error: null }))
      .catch((err: unknown) => {
        const msg = err instanceof Error ? err.message : 'Failed to load game';
        setState({ data: null, loading: false, error: msg });
      });
  }, [id]);

  useEffect(() => {
    load();
  }, [load]);

  return { ...state, refetch: load };
}

// ---------------------------------------------------------------------------
// useSystems
// ---------------------------------------------------------------------------

/** Return the list of supported system abbreviations. */
export function useSystems(): FetchState<string[]> {
  const [state, setState] = useState<FetchState<string[]>>({
    data: [],
    loading: true,
    error: null,
  });

  useEffect(() => {
    apiClient
      .getSystems()
      .then((data) => setState({ data, loading: false, error: null }))
      .catch((err: unknown) => {
        const msg = err instanceof Error ? err.message : 'Failed to load systems';
        setState({ data: [], loading: false, error: msg });
      });
  }, []);

  return state;
}
