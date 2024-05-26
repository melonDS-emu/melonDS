/*
    Copyright 2016-2023 melonDS team

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

#ifndef DSI_AES_H
#define DSI_AES_H

#include "types.h"
#include "Savestate.h"
#include "FIFO.h"
#include "tiny-AES-c/aes.hpp"

namespace melonDS
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#if defined(__GNUC__) && (__GNUC__ >= 11) // gcc 11.*
// NOTE: Yes, the compiler does *not* recognize this code pattern, so it is indeed an optimization.
__attribute((always_inline)) static void Bswap128(void* Dst, const void* Src)
{
    *(__int128*)Dst = __builtin_bswap128(*(__int128*)Src);
}
#else
__attribute((always_inline)) static void Bswap128(void* Dst, const void* Src)
{
    for (int i = 0; i < 16; ++i)
    {
        ((u8*)Dst)[i] = ((u8*)Src)[15 - i];
    }
}
#endif
#pragma GCC diagnostic pop

class DSi;
class DSi_AES
{
public:
    DSi_AES(melonDS::DSi& dsi);
    ~DSi_AES();
    void Reset();
    void DoSavestate(Savestate* file);

    u32 ReadCnt() const;
    void WriteCnt(u32 val);
    void WriteBlkCnt(u32 val);

    u32 ReadOutputFIFO();
    void WriteInputFIFO(u32 val);
    void CheckInputDMA();
    void CheckOutputDMA();
    void Update();

    void WriteIV(u32 offset, u32 val, u32 mask);
    void WriteMAC(u32 offset, u32 val, u32 mask);
    void WriteKeyNormal(u32 slot, u32 offset, u32 val, u32 mask);
    void WriteKeyX(u32 slot, u32 offset, u32 val, u32 mask);
    void WriteKeyY(u32 slot, u32 offset, u32 val, u32 mask);

    static void ROL16(u8* val, u32 n);
    static void DeriveNormalKey(u8* keyX, u8* keyY, u8* normalkey);

private:
    melonDS::DSi& DSi;
    u32 Cnt;

    u32 BlkCnt;
    u32 RemExtra;
    u32 RemBlocks;

    bool OutputFlush;

    u32 InputDMASize, OutputDMASize;
    u32 AESMode;

    FIFO<u32, 16> InputFIFO;
    FIFO<u32, 16> OutputFIFO;

    u8 IV[16];

    u8 MAC[16];

    u8 KeyNormal[4][16];
    u8 KeyX[4][16];
    u8 KeyY[4][16];

    u8 CurKey[16];
    u8 CurMAC[16];

    // output MAC for CCM encrypt
    u8 OutputMAC[16];
    bool OutputMACDue;

    AES_ctx Ctx;

    void ProcessBlock_CCM_Extra();
    void ProcessBlock_CCM_Decrypt();
    void ProcessBlock_CCM_Encrypt();
    void ProcessBlock_CTR();
};

}
#endif // DSI_AES_H
