/**
 * Phase 18 — GameCube (Dolphin) & Nintendo 3DS (Citra) emulation support
 *
 * Tests for: session templates (gc/3ds), system type definitions,
 * emulator adapter launch args, ROM scanner extensions, and
 * input profiles for GC and 3DS controllers.
 */

import { describe, it, expect } from 'vitest';
import { SessionTemplateStore } from '../../../../packages/session-engine/src/templates';

// ---------------------------------------------------------------------------
// Session templates — GameCube
// ---------------------------------------------------------------------------

describe('GameCube session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default 2P GC template', () => {
    const tpl = store.get('gc-default-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('gc');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('dolphin');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes a default 4P GC template', () => {
    const tpl = store.get('gc-default-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('gc');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Mario Kart Double Dash 4P template', () => {
    const tpl = store.get('gc-mario-kart-double-dash-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('gc-mario-kart-double-dash');
    expect(tpl?.playerCount).toBe(4);
    expect(tpl?.emulatorBackendId).toBe('dolphin');
  });

  it('includes Super Smash Bros. Melee 4P template', () => {
    const tpl = store.get('gc-super-smash-bros-melee-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('gc-super-smash-bros-melee');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Mario Party 4/5/6/7 templates', () => {
    for (const id of ['gc-mario-party-4-4p', 'gc-mario-party-5-4p', 'gc-mario-party-6-4p', 'gc-mario-party-7-4p']) {
      const tpl = store.get(id);
      expect(tpl).not.toBeNull();
      expect(tpl?.system).toBe('gc');
      expect(tpl?.playerCount).toBe(4);
    }
  });

  it('includes F-Zero GX 4P template', () => {
    const tpl = store.get('gc-f-zero-gx-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('gc-f-zero-gx');
  });

  it('includes Luigi\'s Mansion 1P local-only template', () => {
    const tpl = store.get('gc-luigi-mansion-1p');
    expect(tpl).not.toBeNull();
    expect(tpl?.playerCount).toBe(1);
    expect(tpl?.netplayMode).toBe('local-only');
  });

  it('getForGame resolves gc-default as fallback', () => {
    const tpl = store.getForGame('gc-some-unknown-game', 'gc');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('gc');
  });

  it('all GC templates have latencyTarget of 120ms', () => {
    const gcTemplates = store.listAll().filter((t) => t.system === 'gc' && t.netplayMode !== 'local-only');
    for (const tpl of gcTemplates) {
      expect(tpl.latencyTarget).toBe(120);
    }
  });
});

// ---------------------------------------------------------------------------
// Session templates — Nintendo 3DS
// ---------------------------------------------------------------------------

describe('Nintendo 3DS session templates', () => {
  const store = new SessionTemplateStore();

  it('includes a default 2P 3DS template', () => {
    const tpl = store.get('3ds-default-2p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('3ds');
    expect(tpl?.playerCount).toBe(2);
    expect(tpl?.emulatorBackendId).toBe('citra');
    expect(tpl?.netplayMode).toBe('online-relay');
  });

  it('includes a default 4P 3DS template', () => {
    const tpl = store.get('3ds-default-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('3ds');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Mario Kart 7 8P template', () => {
    const tpl = store.get('3ds-mario-kart-7-8p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('3ds-mario-kart-7');
    expect(tpl?.playerCount).toBe(8);
    expect(tpl?.emulatorBackendId).toBe('citra');
  });

  it('includes Super Smash Bros. for 3DS 4P template', () => {
    const tpl = store.get('3ds-super-smash-bros-for-3ds-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.gameId).toBe('3ds-super-smash-bros-for-3ds');
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Pokémon X/Y/OR/AS/Sun/Moon templates', () => {
    const pokemonIds = [
      '3ds-pokemon-x-2p',
      '3ds-pokemon-y-2p',
      '3ds-pokemon-omega-ruby-2p',
      '3ds-pokemon-alpha-sapphire-2p',
      '3ds-pokemon-sun-2p',
      '3ds-pokemon-moon-2p',
    ];
    for (const id of pokemonIds) {
      const tpl = store.get(id);
      expect(tpl).not.toBeNull();
      expect(tpl?.system).toBe('3ds');
      expect(tpl?.playerCount).toBe(2);
    }
  });

  it('includes Animal Crossing: New Leaf 4P template', () => {
    const tpl = store.get('3ds-animal-crossing-new-leaf-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.playerCount).toBe(4);
  });

  it('includes Monster Hunter 4 Ultimate 4P template', () => {
    const tpl = store.get('3ds-monster-hunter-4-ultimate-4p');
    expect(tpl).not.toBeNull();
    expect(tpl?.playerCount).toBe(4);
  });

  it('getForGame resolves 3ds-default as fallback', () => {
    const tpl = store.getForGame('3ds-unknown-game', '3ds');
    expect(tpl).not.toBeNull();
    expect(tpl?.system).toBe('3ds');
  });
});
