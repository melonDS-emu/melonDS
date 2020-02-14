/*
    Copyright 2016-2020 Arisotura

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
*/

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "AREngine.h"


namespace AREngine
{

typedef struct
{
    u32 Code[2 * 64]; // TODO: more sensible size for this? allocate on demand?
    bool Enabled;

} CheatEntry;

// TODO: more sensible size for this? allocate on demand?
CheatEntry CheatCodes[64];
u32 NumCheatCodes;


bool Init()
{
    return true;
}

void DeInit()
{
    //
}

void Reset()
{
    memset(CheatCodes, 0, sizeof(CheatCodes));
    NumCheatCodes = 0;

    // TODO: acquire codes from a sensible source!
#define TEMP_PUTCODE(a, b) *ptr++ = a; *ptr++ = b;
    CheatEntry* entry = &CheatCodes[0];
    u32* ptr = &entry->Code[0];
    /*// NSMBDS EUR - giant fucking Mario
    TEMP_PUTCODE(0x1209DBD0, 0x0000027C);
    TEMP_PUTCODE(0x2209DBC0, 0x00000003);
    TEMP_PUTCODE(0x94000130, 0xFFFD0000);
    TEMP_PUTCODE(0x1209DBD0, 0x00000000);
    TEMP_PUTCODE(0x2209DBC0, 0x00000003);
    entry->Enabled = true;
    NumCheatCodes++;

    entry = &CheatCodes[1];
    ptr = &entry->Code[0];*/
    // NSMBDS EUR - jump to the sky
    /*TEMP_PUTCODE(0x9209DC90, 0xFFFD0002);
    TEMP_PUTCODE(0x920DC910, 0x00000000);
    TEMP_PUTCODE(0x021C1944, 0x00004000);
    TEMP_PUTCODE(0xD2000000, 0x00000000);
    TEMP_PUTCODE(0x9209DC90, 0xFFFE0001);
    TEMP_PUTCODE(0x920DC910, 0x00000000);
    TEMP_PUTCODE(0x021C1944, 0x00004000);
    TEMP_PUTCODE(0xD2000000, 0x00000000);*/
    // SM64DS EUR redcoin
    /*TEMP_PUTCODE(0x0210CC3E, 0x00000121);
    TEMP_PUTCODE(0x5209F30C, 0x00000008);
    TEMP_PUTCODE(0x0209F30C, 0x00000000);
    TEMP_PUTCODE(0xD2000000, 0x00000000);*/
    // SM64DS EUR shroom-o-matic
    /*TEMP_PUTCODE(0x9209D09A , 0x00000000);
    TEMP_PUTCODE(0x6209B468 , 0x00000000);
    TEMP_PUTCODE(0xB209B468 , 0x00000000);
    TEMP_PUTCODE(0x10000672 , 0x000003FF);
    TEMP_PUTCODE(0xD2000000 , 0x00000000);
    TEMP_PUTCODE(0x9209D09A , 0x00000000);
    TEMP_PUTCODE(0x94000130 , 0xFCBF0000);
    TEMP_PUTCODE(0x6209B468 , 0x00000000);
    TEMP_PUTCODE(0xB209B468 , 0x00000000);
    TEMP_PUTCODE(0x200006B3 , 0x00000001);
    TEMP_PUTCODE(0x200006B4 , 0x00000001);
    TEMP_PUTCODE(0xD2000000 , 0x00000000);
    TEMP_PUTCODE(0x9209D09A , 0x00000000);
    TEMP_PUTCODE(0x94000130 , 0xFC7F0000);
    TEMP_PUTCODE(0x6209B468 , 0x00000000);
    TEMP_PUTCODE(0xB209B468 , 0x00000000);
    TEMP_PUTCODE(0x10000672 , 0x00000000);
    TEMP_PUTCODE(0xD2000000 , 0x00000000);*/
    entry->Enabled = true;
    NumCheatCodes++;
}


#define case16(x) \
    case ((x)+0x00): case ((x)+0x01): case ((x)+0x02): case ((x)+0x03): \
    case ((x)+0x04): case ((x)+0x05): case ((x)+0x06): case ((x)+0x07): \
    case ((x)+0x08): case ((x)+0x09): case ((x)+0x0A): case ((x)+0x0B): \
    case ((x)+0x0C): case ((x)+0x0D): case ((x)+0x0E): case ((x)+0x0F)

void RunCheat(CheatEntry* entry)
{
    u32* code = &entry->Code[0];

    u32 offset = 0;
    u32 datareg = 0;
    u32 cond = 1;
    u32 condstack = 0;

    for (;;)
    {
        u32 a = *code++;
        u32 b = *code++;
        if ((a|b) == 0) break;

        u8 op = a >> 24;

        if ((op < 0xD0 && op != 0xC5) || op > 0xD2)
        {
            if (!cond)
            {
                // TODO: skip parameters for opcode Ex
                continue;
            }
        }

        switch (op)
        {
        case16(0x00): // 32-bit write
            NDS::ARM7Write32((a & 0x0FFFFFFF) + offset, b);
            break;

        case16(0x10): // 16-bit write
            NDS::ARM7Write16((a & 0x0FFFFFFF) + offset, b & 0xFFFF);
            break;

        case16(0x20): // 8-bit write
            NDS::ARM7Write8((a & 0x0FFFFFFF) + offset, b & 0xFF);
            break;

        case16(0x30): // IF b > u32[a]
            {
                condstack <<= 1;
                condstack |= cond;

                u32 chk = NDS::ARM7Read32(a & 0x0FFFFFFF);

                cond = (b > chk) ? 1:0;
            }
            break;

        case16(0x40): // IF b < u32[a]
            {
                condstack <<= 1;
                condstack |= cond;

                u32 chk = NDS::ARM7Read32(a & 0x0FFFFFFF);

                cond = (b < chk) ? 1:0;
            }
            break;

        case16(0x50): // IF b == u32[a]
            {
                condstack <<= 1;
                condstack |= cond;

                u32 chk = NDS::ARM7Read32(a & 0x0FFFFFFF);

                cond = (b == chk) ? 1:0;
            }
            break;

        case16(0x60): // IF b != u32[a]
            {
                condstack <<= 1;
                condstack |= cond;

                u32 chk = NDS::ARM7Read32(a & 0x0FFFFFFF);

                cond = (b != chk) ? 1:0;
            }
            break;

        case16(0x70): // IF b.l > ((~b.h) & u16[a])
            {
                condstack <<= 1;
                condstack |= cond;

                u16 val = NDS::ARM7Read16(a & 0x0FFFFFFF);
                u16 chk = ~(b >> 16);
                chk &= val;

                cond = ((b & 0xFFFF) > chk) ? 1:0;
            }
            break;

        case16(0x80): // IF b.l < ((~b.h) & u16[a])
            {
                condstack <<= 1;
                condstack |= cond;

                u16 val = NDS::ARM7Read16(a & 0x0FFFFFFF);
                u16 chk = ~(b >> 16);
                chk &= val;

                cond = ((b & 0xFFFF) < chk) ? 1:0;
            }
            break;

        case16(0x90): // IF b.l == ((~b.h) & u16[a])
            {
                condstack <<= 1;
                condstack |= cond;

                u16 val = NDS::ARM7Read16(a & 0x0FFFFFFF);
                u16 chk = ~(b >> 16);
                chk &= val;

                cond = ((b & 0xFFFF) == chk) ? 1:0;
            }
            break;

        case16(0xA0): // IF b.l != ((~b.h) & u16[a])
            {
                condstack <<= 1;
                condstack |= cond;

                u16 val = NDS::ARM7Read16(a & 0x0FFFFFFF);
                u16 chk = ~(b >> 16);
                chk &= val;

                cond = ((b & 0xFFFF) != chk) ? 1:0;
            }
            break;

        case16(0xB0): // offset = u32[a + offset]
            offset = NDS::ARM7Read32((a & 0x0FFFFFFF) + offset);
            break;

        case 0xD0: // ENDIF
            cond = condstack & 0x1;
            condstack >>= 1;
            break;

        case 0xD2: // NEXT+FLUSH
            // TODO: loop shenanigans!
            offset = 0;
            datareg = 0;
            condstack = 0;
            cond = 1;
            break;

        default:
            printf("!! bad AR opcode %08X %08X\n", a, b);
            return;
        }
    }
}

void RunCheats()
{
    // TODO: make it disableable in general

    for (u32 i = 0; i < NumCheatCodes; i++)
    {
        CheatEntry* entry = &CheatCodes[i];
        if (entry->Enabled)
            RunCheat(entry);
    }
}

}
