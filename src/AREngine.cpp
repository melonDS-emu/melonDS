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
    // SM64DS EUR swim through floor
    /*TEMP_PUTCODE(0x927FFFA8 , 0xFBFF0000); // IF 0000 = ((~FBFF) & u16[027FFFA8])
    TEMP_PUTCODE(0x823FDFF8 , 0xFBFF0000); // IF 0000 < ((~FBFF) & u16[023FDFF8])
    TEMP_PUTCODE(0xDA000000 , 0x023FDFFA); // datareg = u16[023FDFFA + offset]
    TEMP_PUTCODE(0xD4000000 , 0x00000001); // datareg += 1
    TEMP_PUTCODE(0xD7000000 , 0x023FDFFA); // u16[023FDFFA + offset] = datareg; offset += 2
    TEMP_PUTCODE(0xD2000000 , 0x00000000); // NEXT/FLUSH
    TEMP_PUTCODE(0xD3000000 , 0x027FFFA8); // offset = 027FFFA8
    TEMP_PUTCODE(0xF23FDFF8 , 0x00000002); // copy offset->023FDFF8, 2
    TEMP_PUTCODE(0xD2000000 , 0x00000000); // NEXT/FLUSH
    TEMP_PUTCODE(0x923FDFFA , 0xFFFE0000); // IF 0000 = ((~FFFE) & u16[023FDFFA])
    TEMP_PUTCODE(0x02037E7C , 0xE2000020); // u32[02037E7C] = E2000020
    TEMP_PUTCODE(0xD0000000 , 0x00000000); // ENDIF
    TEMP_PUTCODE(0x923FDFFA , 0xFFFE0001); // IF 0001 = ((~FFFE) & u16[023FDFFA])
    TEMP_PUTCODE(0x02037E7C , 0xE2000000); // u32[02037E7C] = E2000000
    TEMP_PUTCODE(0xD0000000 , 0x00000000); // ENDIF
    TEMP_PUTCODE(0x927FFFA8 , 0xFBFF0000); // IF 0000 = ((~FBFF) & u16[027FFFA8])
    TEMP_PUTCODE(0x02037E7C , 0xE3A000FF); // u32[02037E7C] = E3A000FF
    TEMP_PUTCODE(0xD2000000 , 0x00000000); // NEXT/FLUSH*/
    // MKDS EUR shitty CPU-stalker code
    /*TEMP_PUTCODE(0x94000130 , 0xFEBB0000);
    TEMP_PUTCODE(0x023CDD4C , 0x00000001);
    TEMP_PUTCODE(0xD2000000 , 0x00000000);
    TEMP_PUTCODE(0x923CDD4C , 0x00000001);
    TEMP_PUTCODE(0x6217AD18 , 0x00000000);
    TEMP_PUTCODE(0xB217AD18 , 0x00000000);
    TEMP_PUTCODE(0xC0000000 , 0x00000002);
    TEMP_PUTCODE(0xD9000000 , 0x00000628);
    TEMP_PUTCODE(0xD6000000 , 0x00000080);
    TEMP_PUTCODE(0xDC000000 , 0x00000000);
    TEMP_PUTCODE(0xD2000000 , 0x00000000);
    entry->Enabled = true;
    NumCheatCodes++;*/
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

    u32* loopstart = code;
    u32 loopcount = 0;
    u32 loopcond = 1;
    u32 loopcondstack = 0;

    // TODO: does anything reset this??
    u32 c5count = 0;

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
                if ((op & 0xF0) == 0xE0)
                {
                    for (u32 i = 0; i < b; i += 8)
                        *code += 2;
                }

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

        case 0xC0: // FOR 0..b
            loopstart = code; // points to the first opcode after the FOR
            loopcount = b;
            loopcond = cond;           // checkme
            loopcondstack = condstack; // (GBAtek is not very clear there)
            break;

        case 0xC4: // offset = pointer to C4000000 opcode
            // theoretically used for safe storage, by accessing [offset+4]
            // in practice could be used for a self-modifying AR code
            // could be implemented with some hackery, but, does anything even
            // use it??
            printf("AR: !! THE FUCKING C4000000 OPCODE. TELL ARISOTURA.\n");
            return;

        case 0xC5: // count++ / IF (count & b.l) == b.h
            {
                // with weird condition checking, apparently
                // oh well

                c5count++;
                if (!cond) break;

                condstack <<= 1;
                condstack |= cond;

                u16 mask = b & 0xFFFF;
                u16 chk = b >> 16;

                cond = ((c5count & mask) == chk) ? 1:0;
            }
            break;

        case 0xC6: // u32[b] = offset
            NDS::ARM7Write32(b, offset);
            break;

        case 0xD0: // ENDIF
            cond = condstack & 0x1;
            condstack >>= 1;
            break;

        case 0xD1: // NEXT
            if (loopcount > 0)
            {
                loopcount--;
                code = loopstart;
            }
            else
            {
                cond = loopcond;
                condstack = loopcondstack;
            }
            break;

        case 0xD2: // NEXT+FLUSH
            if (loopcount > 0)
            {
                loopcount--;
                code = loopstart;
            }
            else
            {
                offset = 0;
                datareg = 0;
                condstack = 0;
                cond = 1;
            }
            break;

        case 0xD3: // offset = b
            offset = b;
            break;

        case 0xD4: // datareg += b
            datareg += b;
            break;

        case 0xD5: // datareg = b
            datareg = b;
            break;

        case 0xD6: // u32[b+offset] = datareg / offset += 4
            NDS::ARM7Write32(b + offset, datareg);
            offset += 4;
            break;

        case 0xD7: // u16[b+offset] = datareg / offset += 2
            NDS::ARM7Write16(b + offset, datareg & 0xFFFF);
            offset += 2;
            break;

        case 0xD8: // u8[b+offset] = datareg / offset += 1
            NDS::ARM7Write8(b + offset, datareg & 0xFF);
            offset += 1;
            break;

        case 0xD9: // datareg = u32[b+offset]
            datareg = NDS::ARM7Read32(b + offset);
            break;

        case 0xDA: // datareg = u16[b+offset]
            datareg = NDS::ARM7Read16(b + offset);
            break;

        case 0xDB: // datareg = u8[b+offset]
            datareg = NDS::ARM7Read8(b + offset);
            break;

        case 0xDC: // offset += b
            offset += b;
            break;

        case16(0xE0): // copy b param bytes to address a+offset
            {
                // TODO: check for bad alignment of dstaddr

                u32 dstaddr = (a & 0x0FFFFFFF) + offset;
                u32 bytesleft = b;
                while (bytesleft >= 8)
                {
                    NDS::ARM7Write32(dstaddr, *code++); dstaddr += 4;
                    NDS::ARM7Write32(dstaddr, *code++); dstaddr += 4;
                    bytesleft -= 8;
                }
                if (bytesleft > 0)
                {
                    u8* leftover = (u8*)code;
                    *code += 2;
                    if (bytesleft >= 4)
                    {
                        NDS::ARM7Write32(dstaddr, *(u32*)leftover); dstaddr += 4;
                        leftover += 4;
                        bytesleft -= 4;
                    }
                    while (bytesleft > 0)
                    {
                        NDS::ARM7Write8(dstaddr, *leftover++); dstaddr++;
                        bytesleft--;
                    }
                }
            }
            break;

        case16(0xF0): // copy b bytes from address offset to address a
            {
                // TODO: check for bad alignment of srcaddr/dstaddr

                u32 srcaddr = offset;
                u32 dstaddr = (a & 0x0FFFFFFF);
                u32 bytesleft = b;
                while (bytesleft >= 4)
                {
                    NDS::ARM7Write32(dstaddr, NDS::ARM7Read32(srcaddr));
                    srcaddr += 4;
                    dstaddr += 4;
                    bytesleft -= 4;
                }
                while (bytesleft > 0)
                {
                    NDS::ARM7Write8(dstaddr, NDS::ARM7Read8(srcaddr));
                    srcaddr++;
                    dstaddr++;
                    bytesleft--;
                }
            }
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
