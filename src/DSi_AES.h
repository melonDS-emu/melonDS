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
    explicit DSi_AES(melonDS::DSi& dsi);
    void Reset();
    void DoSavestate(Savestate* file);

    [[nodiscard]] u32 ReadCnt() const noexcept;
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

    static constexpr void DeriveNormalKey(const u8* keyX, const u8* keyY, u8* normalkey) noexcept
    {
        constexpr u8 key_const[16] = {0xFF, 0xFE, 0xFB, 0x4E, 0x29, 0x59, 0x02, 0x58, 0x2A, 0x68, 0x0F, 0x5F, 0x1A, 0x4F, 0x3E, 0x79};
        u8 tmp[16] {};

        for (int i = 0; i < 16; i++)
            tmp[i] = keyX[i] ^ keyY[i];

        u32 carry = 0;
        for (int i = 0; i < 16; i++)
        {
            u32 res = tmp[i] + key_const[15-i] + carry;
            tmp[i] = res & 0xFF;
            carry = res >> 8;
        }

        ROL16(tmp, 42);

        memcpy(normalkey, tmp, 16);
    }

private:
    melonDS::DSi& DSi;
    u32 Cnt = 0;

    u32 BlkCnt = 0;
    u32 RemExtra = 0;
    u32 RemBlocks = 0;

    bool OutputFlush = false;

    u32 InputDMASize = 0, OutputDMASize = 0;
    u32 AESMode = 0;

    FIFO<u32, 16> InputFIFO {};
    FIFO<u32, 16> OutputFIFO {};

    u8 IV[16] {};

    u8 MAC[16] {};

    u8 KeyNormal[4][16] {};
    u8 KeyX[4][16] {};
    u8 KeyY[4][16] {};

    u8 CurKey[16] {};
    u8 CurMAC[16] {};

    // output MAC for CCM encrypt
    u8 OutputMAC[16] {};
    bool OutputMACDue = false;

    AES_ctx Ctx {};

    void ProcessBlock_CCM_Extra();
    void ProcessBlock_CCM_Decrypt();
    void ProcessBlock_CCM_Encrypt();
    void ProcessBlock_CTR();

    static constexpr void ROL16(u8* val, u32 n) noexcept
    {
        u32 n_coarse = n >> 3;
        u32 n_fine = n & 7;
        u8 tmp[16] {};

        for (u32 i = 0; i < 16; i++)
        {
            tmp[i] = val[(i - n_coarse) & 0xF];
        }

        for (u32 i = 0; i < 16; i++)
        {
            val[i] = (tmp[i] << n_fine) | (tmp[(i - 1) & 0xF] >> (8-n_fine));
        }
    }
};

}
#endif // DSI_AES_H
