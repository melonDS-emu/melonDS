import { MULTIPLAYER_GAMES_SEED } from './seed-data';

export interface GameCatalogQuery {
  system?: string;
  tags?: string[];
  maxPlayers?: number;
  searchText?: string;
}

/**
 * In-memory game catalog for browsing and filtering multiplayer games.
 * In production, this would be backed by a database or remote API.
 */
export class GameCatalog {
  private games = MULTIPLAYER_GAMES_SEED;

  getAll() {
    return this.games;
  }

  getById(id: string) {
    return this.games.find((g) => g.id === id) ?? null;
  }

  query(q: GameCatalogQuery) {
    let results = [...this.games];

    if (q.system) {
      results = results.filter((g) => g.system === q.system);
    }

    if (q.tags && q.tags.length > 0) {
      results = results.filter((g) =>
        q.tags!.some((tag) => g.tags.includes(tag))
      );
    }

    if (q.maxPlayers !== undefined) {
      results = results.filter((g) => g.maxPlayers >= q.maxPlayers!);
    }

    if (q.searchText) {
      const search = q.searchText.toLowerCase();
      results = results.filter(
        (g) =>
          g.title.toLowerCase().includes(search) ||
          g.description.toLowerCase().includes(search)
      );
    }

    return results;
  }

  getBySystem(system: string) {
    return this.games.filter((g) => g.system === system);
  }

  getPartyGames() {
    return this.games.filter((g) => g.tags.includes('Party'));
  }

  getQuickMatchGames() {
    return this.games.filter((g) => g.onlineRecommended && g.supportsPublicLobby);
  }

  getTradeGames() {
    return this.games.filter((g) => g.tags.includes('Trade'));
  }
}
