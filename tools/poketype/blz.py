#!/usr/bin/env python3
"""BLZ (BottomLZ) decoder for retail NDS ARM9 binaries, plus NDS header reading.

Used by extract_layout.py to decompress each retail dump's ARM9 binary before
scanning it for the layout tables.
"""
import struct


def blz_decode(data: bytes) -> bytes:
    pak_len = len(data)
    inc_len = struct.unpack_from('<I', data, pak_len - 4)[0]
    if inc_len == 0:
        return data  # not compressed
    hdr_len = data[pak_len - 5]
    if not (8 <= hdr_len <= 0x0B):
        raise ValueError('bad BLZ hdr_len 0x%02X' % hdr_len)
    enc_len = struct.unpack_from('<I', data, pak_len - 8)[0] & 0x00FFFFFF
    dec_len = pak_len - enc_len           # size of uncompressed prefix
    comp_len = enc_len - hdr_len          # size of compressed stream
    raw_len = dec_len + enc_len + inc_len

    raw = bytearray(raw_len)
    raw[:dec_len] = data[:dec_len]

    pak_stream = bytearray(data[dec_len:dec_len + comp_len])
    pak_stream.reverse()                  # BLZ_Invert
    pi = 0
    out = bytearray()                     # reversed output region
    target = raw_len - dec_len
    mask = 0
    flags = 0
    while len(out) < target:
        if mask == 0:
            if pi >= len(pak_stream):
                raise ValueError(
                    'BLZ stream truncated: expected a flags byte at input '
                    'offset %d, only %d byte(s) available' % (pi, len(pak_stream)))
            flags = pak_stream[pi]; pi += 1
            mask = 0x80
        if not (flags & mask):
            if pi >= len(pak_stream):
                raise ValueError(
                    'BLZ stream truncated: expected a literal byte at input '
                    'offset %d, only %d byte(s) available' % (pi, len(pak_stream)))
            out.append(pak_stream[pi]); pi += 1
        else:
            if pi + 1 >= len(pak_stream):
                raise ValueError(
                    'BLZ stream truncated: expected a 2-byte match token at '
                    'input offset %d, only %d byte(s) available'
                    % (pi, len(pak_stream)))
            pos = pak_stream[pi] << 8; pi += 1
            pos |= pak_stream[pi]; pi += 1
            n = (pos >> 12) + 3
            if len(out) + n > target:
                n = target - len(out)
            pos = (pos & 0xFFF) + 3
            for _ in range(n):
                out.append(out[len(out) - pos])
        mask >>= 1
    out.reverse()
    raw[dec_len:dec_len + len(out)] = out
    return bytes(raw)


def read_header(rom: bytes):
    return {
        'gamecode': rom[0x0C:0x10].decode('ascii', 'replace'),
        'title': rom[0x00:0x0C].decode('ascii', 'replace').rstrip('\x00'),
        'arm9_off':  struct.unpack_from('<I', rom, 0x20)[0],
        'arm9_entry':struct.unpack_from('<I', rom, 0x24)[0],
        'arm9_ram':  struct.unpack_from('<I', rom, 0x28)[0],
        'arm9_size': struct.unpack_from('<I', rom, 0x2C)[0],
    }
