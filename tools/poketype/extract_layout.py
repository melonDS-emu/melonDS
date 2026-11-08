#!/usr/bin/env python3
"""Generate src/PokeTypeKeyboardLayout.cpp from the retail ROM dumps.

Each European build's ARM9 binary carries its keyboard layout in two tables,
located by Thumb byte signature (see KEYTAB_SIG and CHARTAB_SIG):

  CHARTAB  u16[]  index -> UTF-16 character (index 0 = "no character")
  KEYTAB   8-byte record per HID usage code (0x00..0x8F):
             u8 base, shift, altgr, shiftAltgr;  u32 capsSensitive;
           each byte is a CHARTAB index; base == 0 means the key is unused.

The game's decoder (UZPP 0x0205B00C) picks a level the way
PokeTypeKeyboard::CharForKey does. A letter key's unshifted character is the
uppercase letter, in all six builds. The Japanese build has no such tables;
see the comment above JP_CONVERT_SIG.

Needs all six retail dumps (any filenames; identified by game code):

    python3 tools/poketype/extract_layout.py /path/to/roms > src/PokeTypeKeyboardLayout.cpp
"""
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from blz import blz_decode, read_header

BASE = 0x02000000

# Thumb `lsls r5,r3,#3 ; ldrb r4,[r4,r5]`, with the `ldr r4,[pc,#imm]` two
# bytes earlier holding KEYTAB's address.
KEYTAB_SIG = bytes.fromhex('dd00645d')
# Thumb `ldrb r6,[r7,r5] ; ldrh r4,[r0,r1]`, with the `ldr r0,[pc,#imm]` two
# bytes earlier holding CHARTAB's address.
CHARTAB_SIG = bytes.fromhex('7e5d445a')

# KEYTAB ends at usage 0x8F; what follows is an unrelated table, identical in
# all five European builds.
KEYTAB_USAGE_COUNT = 0x90

REGIONS = {
    'UZPP': 'EUR',
    'UZPF': 'FRA',
    'UZPD': 'GER',
    'UZPI': 'ITA',
    'UZPS': 'SPA',
    'UZPJ': 'JPN',
}

# ---------------------------------------------------------------------------
# The Japanese build
#
# UZPJ decodes a key in two stages of code. Each signature covers all of the
# logic transcribed from it, so a dump that differs there fails to match.
#
# Stage 1, JP_CONVERT (UZPJ 0x0205A1DC), maps each held HID usage to an internal
# key code through an 8-byte-per-usage table, {u32 plain, shifted}:
#
#   - plain == 0 means the usage is unused;
#   - shifted is taken when either Shift is held (modifier mask 0x22) and it is
#     non-zero, otherwise plain;
#   - a Shift-held code of 0x35, the '0' key, is dropped, as on a JIS board;
#   - AltGr is never consulted.
#
# There are two tables: a US one taken when the converter's selector byte is
# 0x21, and a JIS one for any other value. The emulated keyboard gets the JIS
# one, and only that one is emitted.
JP_CONVERT_SIG = bytes.fromhex(
    'f8b5041c06a800788e4694461d1c00210f2803d0212802d0332802d101e0154f00e0154f'
    '62460020002a20dd2222224073461b5c002b16d0db00fc18fb58002b11d0002a05d06668'
    '002e02d06c186e5401e06c186b542378352b03d1002a01d000232370491c401c63469842'
    'e0db081c')
JP_CONVERT_ALT_LDR = 0x1E    # ldr r7,[pc,#imm] taken when the selector is 0x21
JP_CONVERT_MAIN_LDR = 0x22   # ldr r7,[pc,#imm] taken for any other selector

# The US table ends where the JIS one starts, giving 0xFF 8-byte entries. The
# JIS table is assumed the same size; its entries past 0x89 are zero.
JP_USAGE_COUNT = 0xFF

# The report handler (UZPJ 0x0205AE10) adds 0x60..0x64 to the held-key list for
# the Ctrl, Right Shift, Left Shift, Alt and GUI modifier bits, so those rows are
# modifiers, not keys (FF02, FF01, FF00, FF04, FF08, as special keys 0x70..0x74).
JP_MODIFIER_PSEUDO_USAGES = range(0x60, 0x65)

# Stage 2, JP_DECODE (UZPJ 0x0205B11A), maps an internal code to a character:
# 0x01..0x1A -> 'a'..'z', 0x1B..0x34 -> 'A'..'Z', 0x35..0x3E -> '0'..'9', and
# 0x3F..0x81 through a switch whose cases each leave a character in r1. 0x82 is
# the key-repeat marker. Caps Lock (0x7A) flips a flag stage 1 never reads, so
# no key is Caps-sensitive. The signature ends on the `add pc, r0` that enters
# the switch; its jump table follows immediately.
JP_DECODE_SIG = bytes.fromhex(
    '155c00210023012d04d31a2d02d8291c60311be01b2d04d3342d02d8291c263114e0352d'
    '03d33e2d01d8691f0ee03f2d08d37b2d06d87a2d08d1644ef569ed43f56103e0822d01d1'
    '01230021002b05d1002903d1105c3f38422800d9afe000187844c088000400148744')
JP_DECODE_OUT_OF_RANGE_B = 0x5C   # `b` past the switch: its target is the tail
JP_SWITCH_FIRST = 0x3F
JP_SWITCH_LAST = 0x81


def pcrel(a9, ins_off):
    """Resolve a Thumb `ldr rX,[pc,#imm]` at ins_off into (value, address)."""
    opcode_hi = a9[ins_off + 1]
    if (opcode_hi & 0xF8) != 0x48:
        raise ValueError(
            'expected a Thumb ldr rX,[pc,#imm] at ARM9 offset 0x%X, found '
            'opcode byte 0x%02X -- the signature match is not where this '
            'code expects it' % (ins_off, opcode_hi))
    imm = a9[ins_off] * 4
    lit = ((BASE + ins_off + 4) & ~3) + imm
    return struct.unpack_from('<I', a9, lit - BASE)[0], lit


def find_unique(a9, sig, name):
    """Locate sig in a9, raising unless it occurs exactly once."""
    i = a9.find(sig)
    if i == -1:
        raise ValueError('%s signature not found' % name)
    if a9.find(sig, i + 1) != -1:
        raise ValueError(
            '%s signature occurs more than once in this image -- the first '
            'match is not necessarily the right one' % name)
    return i


def resolve_in_image(addr, a9, name):
    """Raise unless addr lies inside the ARM9 image."""
    if not (BASE <= addr < BASE + len(a9)):
        raise ValueError(
            '%s address 0x%08X is outside the ARM9 image (0x%08X..0x%08X)'
            % (name, addr, BASE, BASE + len(a9)))
    return addr


def find_tables(a9):
    i = find_unique(a9, KEYTAB_SIG, 'KEYTAB')
    keytab, _ = pcrel(a9, i - 2)
    keytab = resolve_in_image(keytab, a9, 'KEYTAB')

    j = find_unique(a9, CHARTAB_SIG, 'CHARTAB')
    chartab, _ = pcrel(a9, j - 2)
    chartab = resolve_in_image(chartab, a9, 'CHARTAB')

    return keytab, chartab


def decode_arm9(rom_bytes):
    hdr = read_header(rom_bytes)
    a9 = rom_bytes[hdr['arm9_off']:hdr['arm9_off'] + hdr['arm9_size']]
    return hdr['gamecode'], blz_decode(a9)


def extract_region(a9):
    """(usage, base, shift, altgr, shiftAltgr, caps) for each live key,
    ascending by usage, from a decompressed ARM9 image."""
    keytab, chartab = find_tables(a9)

    def chartab_lookup(index):
        off = (chartab - BASE) + index * 2
        return struct.unpack_from('<H', a9, off)[0]

    keys = []
    for usage in range(KEYTAB_USAGE_COUNT):
        off = (keytab - BASE) + usage * 8
        raw_base, raw_shift, raw_altgr, raw_shiftaltgr = a9[off:off + 4]
        caps = struct.unpack_from('<I', a9, off + 4)[0]

        base = chartab_lookup(raw_base)
        if base == 0:
            # no entry, or a CHARTAB slot with no character (F1-F12,
            # PageUp/Delete/End/PageDown and one placeholder): not a live key
            continue

        keys.append((
            usage,
            base,
            chartab_lookup(raw_shift),
            chartab_lookup(raw_altgr),
            chartab_lookup(raw_shiftaltgr),
            1 if caps else 0,
        ))

    return keys


def jp_case_char(a9, addr, tail):
    """Run one case of JP_DECODE's switch and return the character it leaves
    in r1. A case is a few Thumb instructions ending in a branch to the shared
    tail, sometimes via another case; r1 is 0 on entry."""
    r1 = 0
    for _ in range(8):
        if addr == tail:
            return r1
        op = struct.unpack_from('<H', a9, addr - BASE)[0]
        if (op & 0xFF00) == 0x2100:                     # movs r1, #imm8
            r1 = op & 0xFF
        elif (op & 0xF83F) == 0x0009:                   # lsls r1, r1, #imm5
            r1 = (r1 << ((op >> 6) & 0x1F)) & 0xFFFFFFFF
        elif (op & 0xFF00) == 0x4900:                   # ldr r1, [pc, #imm8*4]
            lit = ((addr + 4) & ~3) + (op & 0xFF) * 4
            r1 = struct.unpack_from('<I', a9, lit - BASE)[0]
        elif (op & 0xF800) == 0xE000:                   # b imm11
            off = op & 0x7FF
            if off & 0x400:
                off -= 0x800
            addr = addr + 4 + off * 2
            continue
        else:
            raise ValueError(
                'unexpected Thumb opcode 0x%04X at 0x%08X in a JP_DECODE switch '
                'case' % (op, addr))
        addr += 2
    raise ValueError('JP_DECODE switch case at 0x%08X never reaches the tail'
                     % addr)


def find_jp_tables(a9):
    """Locate JP_CONVERT's JIS table, and JP_DECODE's jump table and tail."""
    i = find_unique(a9, JP_CONVERT_SIG, 'JP_CONVERT')
    alt, _ = pcrel(a9, i + JP_CONVERT_ALT_LDR)
    main, _ = pcrel(a9, i + JP_CONVERT_MAIN_LDR)
    resolve_in_image(alt, a9, 'JP_CONVERT 0x21 table')
    resolve_in_image(main + JP_USAGE_COUNT * 8 - 1, a9, 'JP_CONVERT table')
    if main - alt != JP_USAGE_COUNT * 8:
        raise ValueError(
            'JP_CONVERT tables at 0x%08X and 0x%08X no longer abut, so the '
            'entry count cannot be derived' % (alt, main))

    j = find_unique(a9, JP_DECODE_SIG, 'JP_DECODE')
    jumptab = BASE + j + len(JP_DECODE_SIG)

    b_at = BASE + j + JP_DECODE_OUT_OF_RANGE_B
    op = struct.unpack_from('<H', a9, b_at - BASE)[0]
    if (op & 0xF800) != 0xE000:
        raise ValueError('expected a Thumb b at 0x%08X, found 0x%04X' % (b_at, op))
    off = op & 0x7FF
    if off & 0x400:
        off -= 0x800
    tail = b_at + 4 + off * 2

    return main, jumptab, tail


def extract_japan(a9):
    """The Japanese build's layout, as the same (usage, base, shift, altgr,
    shiftAltgr, caps) tuples extract_region() returns."""
    table, jumptab, tail = find_jp_tables(a9)

    def code_char(code):
        if 0x01 <= code <= 0x1A:
            return code + 0x60
        if 0x1B <= code <= 0x34:
            return code + 0x26
        if 0x35 <= code <= 0x3E:
            return code - 0x05
        if JP_SWITCH_FIRST <= code <= JP_SWITCH_LAST:
            entry = struct.unpack_from(
                '<h', a9, jumptab - BASE + 2 * (code - JP_SWITCH_FIRST))[0]
            return jp_case_char(a9, jumptab + 2 + entry, tail)
        return 0    # 0x82 is the key-repeat marker; nothing above it has a case

    keys = []
    for usage in range(JP_USAGE_COUNT):
        if usage in JP_MODIFIER_PSEUDO_USAGES:
            continue

        plain, shifted = struct.unpack_from('<II', a9, table - BASE + usage * 8)
        if plain == 0:
            continue

        # the converter keeps the low byte of each code
        base = code_char(plain & 0xFF)
        if base == 0:
            # F1-F12, the navigation block and the 0x01 placeholder
            continue

        shift_code = (shifted or plain) & 0xFF
        shift = 0 if shift_code == 0x35 else code_char(shift_code)

        # AltGr is never consulted, so its levels are the plain ones
        keys.append((usage, base, shift, base, shift, 0))

    return keys


def find_roms(romdir):
    """Map region tag -> decompressed ARM9 bytes, for every dump found in
    romdir. Raises if any of the six is missing."""
    found = {}
    for path in sorted(glob.glob(os.path.join(romdir, '*.nds'))):
        with open(path, 'rb') as f:
            rom_bytes = f.read()
        gamecode, a9 = decode_arm9(rom_bytes)
        tag = REGIONS.get(gamecode)
        if tag is None:
            continue
        found[tag] = a9

    missing = [tag for tag in REGIONS.values() if tag not in found]
    if missing:
        raise SystemExit('missing ROM dump(s) for: %s' % ', '.join(missing))

    return found


def format_array(tag, keys):
    # `extern` on the definitions too: a namespace-scope `const` has internal
    # linkage in C++, so PokeTypeKeyboard.cpp would fail to link against them.
    lines = []
    lines.append('extern const PokeTypeKeyboard::KeyDesc PokeTypeLayout_%s[] =' % tag)
    lines.append('{')
    lines.append('    // KeyID, Base, Shift, AltGr, ShiftAltGr, CapsSensitive')
    for usage, base, shift, altgr, shiftaltgr, caps in keys:
        lines.append('    {0x%02X, 0x%04X, 0x%04X, 0x%04X, 0x%04X, %d},' % (
            usage, base, shift, altgr, shiftaltgr, caps))
    lines.append('};')
    lines.append('extern const u32 PokeTypeLayout_%s_Length =' % tag)
    lines.append('    (u32)(sizeof(PokeTypeLayout_%s) / sizeof(PokeTypeKeyboard::KeyDesc));' % tag)
    return '\n'.join(lines)


GPL_HEADER = '''/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/'''


def generate(romdir):
    roms = find_roms(romdir)

    out = []
    out.append(GPL_HEADER)
    out.append('')
    out.append('// Generated by tools/poketype/extract_layout.py -- do not edit by hand.')
    out.append("// Read from each European build's CHARTAB/KEYTAB tables (the key decoder's, at")
    out.append('// UZPP 0x0205B00C) and from the Japanese decoder at UZPJ 0x0205A1DC/0x0205B11A.')
    out.append('')
    out.append('#include "PokeTypeKeyboard.h"')
    out.append('')
    out.append('namespace melonDS')
    out.append('{')
    out.append('')

    for tag in ('EUR', 'FRA', 'GER', 'ITA', 'SPA'):
        keys = extract_region(roms[tag])
        out.append(format_array(tag, keys))
        out.append('')

    out.append(format_array('JPN', extract_japan(roms['JPN'])))
    out.append('')

    out.append('}')
    out.append('')
    return '\n'.join(out)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: extract_layout.py <directory of retail .nds dumps>')

    sys.stdout.write(generate(sys.argv[1]))
