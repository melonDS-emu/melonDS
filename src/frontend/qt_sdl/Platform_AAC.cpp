/*
    Copyright 2016-2025 melonDS team

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

#include <neaacdec.h>

#include "Platform.h"


namespace melonDS::Platform
{

AACDecoder* AAC_Init()
{
    NeAACDecHandle handle = NeAACDecOpen();
    if (!handle)
        return nullptr;

    NeAACDecConfiguration* cfg = NeAACDecGetCurrentConfiguration(handle);
    cfg->defObjectType = LC;
    cfg->outputFormat = FAAD_FMT_16BIT;
    if (!NeAACDecSetConfiguration(handle, cfg))
        return nullptr;

    return (AACDecoder*)handle;
}

void AAC_DeInit(AACDecoder* dec)
{
    NeAACDecHandle handle = (NeAACDecHandle)dec;
    NeAACDecClose(handle);
}

bool AAC_Configure(AACDecoder* dec, int frequency, int channels)
{
    NeAACDecHandle handle = (NeAACDecHandle)dec;

    int freqlist[9] = {48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
    u8 freqnum = 3; // default to 48000
    for (int i = 0; i < 9; i++)
    {
        if (frequency == freqlist[i])
        {
            freqnum = 3 + i;
            break;
        }
    }

    channels &= 0xF;

    // produce a MP4 ASC to configure the decoder

    u8 asc[5];
    u32 asclen = sizeof(asc);
    asc[0] = 0x10 | (freqnum >> 1);
    asc[1] = (freqnum << 7) | (channels << 3);
    asc[2] = 0x56;
    asc[3] = 0xE5;
    asc[4] = 0x00;

    unsigned long freq_out;
    u8 chan_out;
    if (NeAACDecInit2(handle, asc, asclen, &freq_out, &chan_out) != 0)
        return false;

    return true;
}

bool AAC_DecodeFrame(AACDecoder* dec, const void* input, int inputlen, void* output, int outputlen)
{
    NeAACDecHandle handle = (NeAACDecHandle)dec;
    NeAACDecFrameInfo finfo;

    NeAACDecDecode2(handle, &finfo, (u8*)input, inputlen, &output, outputlen);

    if (finfo.error)
        return false;
    if (finfo.bytesconsumed != inputlen)
        return false;

    return true;
}

}
