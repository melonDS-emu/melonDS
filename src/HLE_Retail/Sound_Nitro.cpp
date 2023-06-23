/*
    Copyright 2016-2022 melonDS team

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

#include "../NDS.h"
#include "../NDSCart.h"
#include "../HLE.h"
#include "../SPU.h"
#include "../FIFO.h"

#include "IPC.h"
#include "Sound_Nitro.h"


namespace HLE
{
namespace Retail
{
namespace Sound_Nitro
{

struct Track;

struct Channel
{
    u8 ID;
    u8 Type;
    u8 VolRampPhase;
    u8 StatusFlags;
    u8 PanBase1;
    u8 FreqBase1;
    s16 VolBase1;
    u8 FreqBase2;
    u8 VolBase2;
    s8 PanBase2;
    s8 PanBase3;
    s16 VolBase3;
    s16 FreqBase3;
    s32 BaseVolume;
    s32 FreqRampPos;
    s32 FreqRampLen;
    u8 AttackRate;
    u8 SustainRate;
    u16 DecayRate;
    u16 ReleaseRate;
    u8 Priority;
    u8 Pan;
    u8 Volume;
    u8 VolumeDiv;
    u16 Frequency;
    u8 ModulationType;
    u8 ModulationSpeed;
    u8 ModulationDepth;
    u8 ModulationRange;
    u16 ModulationDelay;
    u16 ModulationCount1;
    u16 ModulationCount2;
    s16 FreqRampTarget;
    s32 NoteLength;
    u8 DataFormat;
    u8 Repeat;
    u16 SampleRate;
    u16 SwavFrequency;
    u16 LoopPos;
    u32 Length;
    u32 DataAddr;
    bool Linked;
    Track* LinkedTrack;
    Channel* Next;
};

struct Sequence
{
    u8 StatusFlags;
    u8 ID;
    u8 SeqUnk02;
    u8 SeqUnk03;
    u8 Pan;
    u8 Volume;
    s16 SeqUnk06;
    u8 Tracks[16];
    u16 Tempo;
    u16 SeqUnk1A;
    u16 TickCounter;
    u32 SBNKAddr;
};

struct Track
{
    u8 StatusFlags;
    u8 TrackUnk01;
    u16 InstrIndex; // index into SBNK
    u8 Volume;
    u8 Expression;
    s8 PitchBend;
    u8 PitchBendRange;
    s8 Pan;
    s8 TrackUnk09;
    s16 TrackUnk0A;
    s16 Frequency;
    u8 AttackRate;
    u8 DecayRate;
    u8 SustainRate;
    u8 ReleaseRate;
    u8 Priority;
    s8 Transpose;
    s8 TrackUnk14;
    u8 PortamentoTime;
    s16 SweepPitch;
    u8 ModulationType;
    u8 ModulationSpeed;
    u8 ModulationDepth;
    u8 ModulationRange;
    u16 ModulationDelay;
    u16 ChannelMask;
    s32 RestCounter;
    u32 NoteBuffer;
    u32 CurNoteAddr;
    u32 LoopAddr[3];
    u8 LoopCount[3];
    u8 LoopLevel;
    Channel* ChanList;
};

struct Alarm
{
    bool Active;
    u32 Delay;
    u32 Repeat;
    u32 Param;
};

const s16 BaseVolumeTable[128] =
{
    -0x8000, -0x02D2, -0x02D1, -0x028B, -0x0259, -0x0232, -0x0212, -0x01F7,
    -0x01E0, -0x01CC, -0x01BA, -0x01A9, -0x019A, -0x018C, -0x017F, -0x0173,
    -0x0168, -0x015D, -0x0153, -0x014A, -0x0141, -0x0139, -0x0131, -0x0129,
    -0x0121, -0x011A, -0x0114, -0x010D, -0x0107, -0x0101, -0x00FB, -0x00F5,
    -0x00EF, -0x00EA, -0x00E5, -0x00E0, -0x00DB, -0x00D6, -0x00D2, -0x00CD,
    -0x00C9, -0x00C4, -0x00C0, -0x00BC, -0x00B8, -0x00B4, -0x00B0, -0x00AD,
    -0x00A9, -0x00A5, -0x00A2, -0x009E, -0x009B, -0x0098, -0x0095, -0x0091,
    -0x008E, -0x008B, -0x0088, -0x0085, -0x0082, -0x007F, -0x007D, -0x007A,
    -0x0077, -0x0074, -0x0072, -0x006F, -0x006D, -0x006A, -0x0067, -0x0065,
    -0x0063, -0x0060, -0x005E, -0x005B, -0x0059, -0x0057, -0x0055, -0x0052,
    -0x0050, -0x004E, -0x004C, -0x004A, -0x0048, -0x0046, -0x0044, -0x0042,
    -0x0040, -0x003E, -0x003C, -0x003A, -0x0038, -0x0036, -0x0034, -0x0032,
    -0x0031, -0x002F, -0x002D, -0x002B, -0x002A, -0x0028, -0x0026, -0x0024,
    -0x0023, -0x0021, -0x001F, -0x001E, -0x001C, -0x001B, -0x0019, -0x0017,
    -0x0016, -0x0014, -0x0013, -0x0011, -0x0010, -0x000E, -0x000D, -0x000B,
    -0x000A, -0x0008, -0x0007, -0x0006, -0x0004, -0x0003, -0x0001,  0x0000
};

const s8 ModulationTable[32] =
{
    0x00, 0x06, 0x0C, 0x13, 0x19, 0x1F, 0x25, 0x2B, 0x31, 0x36, 0x3C, 0x41, 0x47, 0x4C, 0x51, 0x55,
    0x5A, 0x5E, 0x62, 0x66, 0x6A, 0x6D, 0x70, 0x73, 0x75, 0x78, 0x7A, 0x7B, 0x7D, 0x7E, 0x7E, 0x7F
};

const u8 SWIVolumeTable[724] =
{
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E,
    0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x18,
    0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C,
    0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1F, 0x1F, 0x1F, 0x20, 0x20, 0x20, 0x21, 0x21, 0x22, 0x22,
    0x22, 0x23, 0x23, 0x24, 0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x27, 0x28, 0x28, 0x29,
    0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x31, 0x31,
    0x32, 0x32, 0x33, 0x33, 0x34, 0x35, 0x35, 0x36, 0x36, 0x37, 0x38, 0x38, 0x39, 0x3A, 0x3A, 0x3B,
    0x3C, 0x3C, 0x3D, 0x3E, 0x3F, 0x3F, 0x40, 0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6D, 0x6E, 0x6F, 0x71, 0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7A, 0x7B,
    0x7D, 0x7E, 0x7F, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25,
    0x26, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D,
    0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x30, 0x31, 0x31, 0x32, 0x33, 0x33, 0x34, 0x34, 0x35, 0x36,
    0x36, 0x37, 0x37, 0x38, 0x39, 0x39, 0x3A, 0x3B, 0x3B, 0x3C, 0x3D, 0x3E, 0x3E, 0x3F, 0x40, 0x40,
    0x41, 0x42, 0x43, 0x43, 0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4D,
    0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
    0x5E, 0x5F, 0x60, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6F, 0x70,
    0x71, 0x73, 0x74, 0x75, 0x77, 0x78, 0x79, 0x7B, 0x7C, 0x7E, 0x7E, 0x40, 0x41, 0x42, 0x43, 0x43,
    0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51,
    0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
    0x62, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6B, 0x6C, 0x6D, 0x6E, 0x70, 0x71, 0x72, 0x74, 0x75,
    0x76, 0x78, 0x79, 0x7B, 0x7C, 0x7D, 0x7E, 0x40, 0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x46, 0x46,
    0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
    0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x6C, 0x6D, 0x6E, 0x6F, 0x71, 0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7A,
    0x7C, 0x7D, 0x7E, 0x7F
};

const u16 SWIPitchTable[768] =
{
    0x0000, 0x003B, 0x0076, 0x00B2, 0x00ED, 0x0128, 0x0164, 0x019F,
    0x01DB, 0x0217, 0x0252, 0x028E, 0x02CA, 0x0305, 0x0341, 0x037D,
    0x03B9, 0x03F5, 0x0431, 0x046E, 0x04AA, 0x04E6, 0x0522, 0x055F,
    0x059B, 0x05D8, 0x0614, 0x0651, 0x068D, 0x06CA, 0x0707, 0x0743,
    0x0780, 0x07BD, 0x07FA, 0x0837, 0x0874, 0x08B1, 0x08EF, 0x092C,
    0x0969, 0x09A7, 0x09E4, 0x0A21, 0x0A5F, 0x0A9C, 0x0ADA, 0x0B18,
    0x0B56, 0x0B93, 0x0BD1, 0x0C0F, 0x0C4D, 0x0C8B, 0x0CC9, 0x0D07,
    0x0D45, 0x0D84, 0x0DC2, 0x0E00, 0x0E3F, 0x0E7D, 0x0EBC, 0x0EFA,
    0x0F39, 0x0F78, 0x0FB6, 0x0FF5, 0x1034, 0x1073, 0x10B2, 0x10F1,
    0x1130, 0x116F, 0x11AE, 0x11EE, 0x122D, 0x126C, 0x12AC, 0x12EB,
    0x132B, 0x136B, 0x13AA, 0x13EA, 0x142A, 0x146A, 0x14A9, 0x14E9,
    0x1529, 0x1569, 0x15AA, 0x15EA, 0x162A, 0x166A, 0x16AB, 0x16EB,
    0x172C, 0x176C, 0x17AD, 0x17ED, 0x182E, 0x186F, 0x18B0, 0x18F0,
    0x1931, 0x1972, 0x19B3, 0x19F5, 0x1A36, 0x1A77, 0x1AB8, 0x1AFA,
    0x1B3B, 0x1B7D, 0x1BBE, 0x1C00, 0x1C41, 0x1C83, 0x1CC5, 0x1D07,
    0x1D48, 0x1D8A, 0x1DCC, 0x1E0E, 0x1E51, 0x1E93, 0x1ED5, 0x1F17,
    0x1F5A, 0x1F9C, 0x1FDF, 0x2021, 0x2064, 0x20A6, 0x20E9, 0x212C,
    0x216F, 0x21B2, 0x21F5, 0x2238, 0x227B, 0x22BE, 0x2301, 0x2344,
    0x2388, 0x23CB, 0x240E, 0x2452, 0x2496, 0x24D9, 0x251D, 0x2561,
    0x25A4, 0x25E8, 0x262C, 0x2670, 0x26B4, 0x26F8, 0x273D, 0x2781,
    0x27C5, 0x280A, 0x284E, 0x2892, 0x28D7, 0x291C, 0x2960, 0x29A5,
    0x29EA, 0x2A2F, 0x2A74, 0x2AB9, 0x2AFE, 0x2B43, 0x2B88, 0x2BCD,
    0x2C13, 0x2C58, 0x2C9D, 0x2CE3, 0x2D28, 0x2D6E, 0x2DB4, 0x2DF9,
    0x2E3F, 0x2E85, 0x2ECB, 0x2F11, 0x2F57, 0x2F9D, 0x2FE3, 0x302A,
    0x3070, 0x30B6, 0x30FD, 0x3143, 0x318A, 0x31D0, 0x3217, 0x325E,
    0x32A5, 0x32EC, 0x3332, 0x3379, 0x33C1, 0x3408, 0x344F, 0x3496,
    0x34DD, 0x3525, 0x356C, 0x35B4, 0x35FB, 0x3643, 0x368B, 0x36D3,
    0x371A, 0x3762, 0x37AA, 0x37F2, 0x383A, 0x3883, 0x38CB, 0x3913,
    0x395C, 0x39A4, 0x39ED, 0x3A35, 0x3A7E, 0x3AC6, 0x3B0F, 0x3B58,
    0x3BA1, 0x3BEA, 0x3C33, 0x3C7C, 0x3CC5, 0x3D0E, 0x3D58, 0x3DA1,
    0x3DEA, 0x3E34, 0x3E7D, 0x3EC7, 0x3F11, 0x3F5A, 0x3FA4, 0x3FEE,
    0x4038, 0x4082, 0x40CC, 0x4116, 0x4161, 0x41AB, 0x41F5, 0x4240,
    0x428A, 0x42D5, 0x431F, 0x436A, 0x43B5, 0x4400, 0x444B, 0x4495,
    0x44E1, 0x452C, 0x4577, 0x45C2, 0x460D, 0x4659, 0x46A4, 0x46F0,
    0x473B, 0x4787, 0x47D3, 0x481E, 0x486A, 0x48B6, 0x4902, 0x494E,
    0x499A, 0x49E6, 0x4A33, 0x4A7F, 0x4ACB, 0x4B18, 0x4B64, 0x4BB1,
    0x4BFE, 0x4C4A, 0x4C97, 0x4CE4, 0x4D31, 0x4D7E, 0x4DCB, 0x4E18,
    0x4E66, 0x4EB3, 0x4F00, 0x4F4E, 0x4F9B, 0x4FE9, 0x5036, 0x5084,
    0x50D2, 0x5120, 0x516E, 0x51BC, 0x520A, 0x5258, 0x52A6, 0x52F4,
    0x5343, 0x5391, 0x53E0, 0x542E, 0x547D, 0x54CC, 0x551A, 0x5569,
    0x55B8, 0x5607, 0x5656, 0x56A5, 0x56F4, 0x5744, 0x5793, 0x57E2,
    0x5832, 0x5882, 0x58D1, 0x5921, 0x5971, 0x59C1, 0x5A10, 0x5A60,
    0x5AB0, 0x5B01, 0x5B51, 0x5BA1, 0x5BF1, 0x5C42, 0x5C92, 0x5CE3,
    0x5D34, 0x5D84, 0x5DD5, 0x5E26, 0x5E77, 0x5EC8, 0x5F19, 0x5F6A,
    0x5FBB, 0x600D, 0x605E, 0x60B0, 0x6101, 0x6153, 0x61A4, 0x61F6,
    0x6248, 0x629A, 0x62EC, 0x633E, 0x6390, 0x63E2, 0x6434, 0x6487,
    0x64D9, 0x652C, 0x657E, 0x65D1, 0x6624, 0x6676, 0x66C9, 0x671C,
    0x676F, 0x67C2, 0x6815, 0x6869, 0x68BC, 0x690F, 0x6963, 0x69B6,
    0x6A0A, 0x6A5E, 0x6AB1, 0x6B05, 0x6B59, 0x6BAD, 0x6C01, 0x6C55,
    0x6CAA, 0x6CFE, 0x6D52, 0x6DA7, 0x6DFB, 0x6E50, 0x6EA4, 0x6EF9,
    0x6F4E, 0x6FA3, 0x6FF8, 0x704D, 0x70A2, 0x70F7, 0x714D, 0x71A2,
    0x71F7, 0x724D, 0x72A2, 0x72F8, 0x734E, 0x73A4, 0x73FA, 0x7450,
    0x74A6, 0x74FC, 0x7552, 0x75A8, 0x75FF, 0x7655, 0x76AC, 0x7702,
    0x7759, 0x77B0, 0x7807, 0x785E, 0x78B4, 0x790C, 0x7963, 0x79BA,
    0x7A11, 0x7A69, 0x7AC0, 0x7B18, 0x7B6F, 0x7BC7, 0x7C1F, 0x7C77,
    0x7CCF, 0x7D27, 0x7D7F, 0x7DD7, 0x7E2F, 0x7E88, 0x7EE0, 0x7F38,
    0x7F91, 0x7FEA, 0x8042, 0x809B, 0x80F4, 0x814D, 0x81A6, 0x81FF,
    0x8259, 0x82B2, 0x830B, 0x8365, 0x83BE, 0x8418, 0x8472, 0x84CB,
    0x8525, 0x857F, 0x85D9, 0x8633, 0x868E, 0x86E8, 0x8742, 0x879D,
    0x87F7, 0x8852, 0x88AC, 0x8907, 0x8962, 0x89BD, 0x8A18, 0x8A73,
    0x8ACE, 0x8B2A, 0x8B85, 0x8BE0, 0x8C3C, 0x8C97, 0x8CF3, 0x8D4F,
    0x8DAB, 0x8E07, 0x8E63, 0x8EBF, 0x8F1B, 0x8F77, 0x8FD4, 0x9030,
    0x908C, 0x90E9, 0x9146, 0x91A2, 0x91FF, 0x925C, 0x92B9, 0x9316,
    0x9373, 0x93D1, 0x942E, 0x948C, 0x94E9, 0x9547, 0x95A4, 0x9602,
    0x9660, 0x96BE, 0x971C, 0x977A, 0x97D8, 0x9836, 0x9895, 0x98F3,
    0x9952, 0x99B0, 0x9A0F, 0x9A6E, 0x9ACD, 0x9B2C, 0x9B8B, 0x9BEA,
    0x9C49, 0x9CA8, 0x9D08, 0x9D67, 0x9DC7, 0x9E26, 0x9E86, 0x9EE6,
    0x9F46, 0x9FA6, 0xA006, 0xA066, 0xA0C6, 0xA127, 0xA187, 0xA1E8,
    0xA248, 0xA2A9, 0xA30A, 0xA36B, 0xA3CC, 0xA42D, 0xA48E, 0xA4EF,
    0xA550, 0xA5B2, 0xA613, 0xA675, 0xA6D6, 0xA738, 0xA79A, 0xA7FC,
    0xA85E, 0xA8C0, 0xA922, 0xA984, 0xA9E7, 0xAA49, 0xAAAC, 0xAB0E,
    0xAB71, 0xABD4, 0xAC37, 0xAC9A, 0xACFD, 0xAD60, 0xADC3, 0xAE27,
    0xAE8A, 0xAEED, 0xAF51, 0xAFB5, 0xB019, 0xB07C, 0xB0E0, 0xB145,
    0xB1A9, 0xB20D, 0xB271, 0xB2D6, 0xB33A, 0xB39F, 0xB403, 0xB468,
    0xB4CD, 0xB532, 0xB597, 0xB5FC, 0xB662, 0xB6C7, 0xB72C, 0xB792,
    0xB7F7, 0xB85D, 0xB8C3, 0xB929, 0xB98F, 0xB9F5, 0xBA5B, 0xBAC1,
    0xBB28, 0xBB8E, 0xBBF5, 0xBC5B, 0xBCC2, 0xBD29, 0xBD90, 0xBDF7,
    0xBE5E, 0xBEC5, 0xBF2C, 0xBF94, 0xBFFB, 0xC063, 0xC0CA, 0xC132,
    0xC19A, 0xC202, 0xC26A, 0xC2D2, 0xC33A, 0xC3A2, 0xC40B, 0xC473,
    0xC4DC, 0xC544, 0xC5AD, 0xC616, 0xC67F, 0xC6E8, 0xC751, 0xC7BB,
    0xC824, 0xC88D, 0xC8F7, 0xC960, 0xC9CA, 0xCA34, 0xCA9E, 0xCB08,
    0xCB72, 0xCBDC, 0xCC47, 0xCCB1, 0xCD1B, 0xCD86, 0xCDF1, 0xCE5B,
    0xCEC6, 0xCF31, 0xCF9C, 0xD008, 0xD073, 0xD0DE, 0xD14A, 0xD1B5,
    0xD221, 0xD28D, 0xD2F8, 0xD364, 0xD3D0, 0xD43D, 0xD4A9, 0xD515,
    0xD582, 0xD5EE, 0xD65B, 0xD6C7, 0xD734, 0xD7A1, 0xD80E, 0xD87B,
    0xD8E9, 0xD956, 0xD9C3, 0xDA31, 0xDA9E, 0xDB0C, 0xDB7A, 0xDBE8,
    0xDC56, 0xDCC4, 0xDD32, 0xDDA0, 0xDE0F, 0xDE7D, 0xDEEC, 0xDF5B,
    0xDFC9, 0xE038, 0xE0A7, 0xE116, 0xE186, 0xE1F5, 0xE264, 0xE2D4,
    0xE343, 0xE3B3, 0xE423, 0xE493, 0xE503, 0xE573, 0xE5E3, 0xE654,
    0xE6C4, 0xE735, 0xE7A5, 0xE816, 0xE887, 0xE8F8, 0xE969, 0xE9DA,
    0xEA4B, 0xEABC, 0xEB2E, 0xEB9F, 0xEC11, 0xEC83, 0xECF5, 0xED66,
    0xEDD9, 0xEE4B, 0xEEBD, 0xEF2F, 0xEFA2, 0xF014, 0xF087, 0xF0FA,
    0xF16D, 0xF1E0, 0xF253, 0xF2C6, 0xF339, 0xF3AD, 0xF420, 0xF494,
    0xF507, 0xF57B, 0xF5EF, 0xF663, 0xF6D7, 0xF74C, 0xF7C0, 0xF834,
    0xF8A9, 0xF91E, 0xF992, 0xFA07, 0xFA7C, 0xFAF1, 0xFB66, 0xFBDC,
    0xFC51, 0xFCC7, 0xFD3C, 0xFDB2, 0xFE28, 0xFE9E, 0xFF14, 0xFF8A
};

FIFO<u32, 64> CmdQueue;
u32 SharedMem;
u32 Counter;

Channel Channels[16];
Sequence Sequences[16];
Track Tracks[32];
Alarm Alarms[8];

u8 ChanVolume[16];
u8 ChanPan[16];

int MasterPan;
int SurroundDecay;

// 0=regular 1=early/SM64DS
int Version;

void ReportHardwareStatus();
u16 UpdateCounter();
void UnlinkChannel(Channel* chan, bool unlink);


void Reset()
{
    CmdQueue.Clear();
    SharedMem = 0;
    Counter = 0;

    memset(Channels, 0, sizeof(Channels));
    memset(Sequences, 0, sizeof(Sequences));
    memset(Tracks, 0, sizeof(Tracks));
    memset(Alarms, 0, sizeof(Alarms));

    memset(ChanVolume, 0, sizeof(ChanVolume));
    memset(ChanPan, 0, sizeof(ChanPan));

    MasterPan = -1;
    SurroundDecay = 0;

    for (int i = 0; i < 16; i++)
    {
        Channel* chan = &Channels[i];

        chan->StatusFlags &= ~0xF9;
        chan->ID = i;
    }

    for (int i = 0; i < 16; i++)
    {
        Sequence* seq = &Sequences[i];

        seq->StatusFlags &= ~(1<<0);
        seq->ID = i;
    }

    for (int i = 0; i < 32; i++)
    {
        Track* track = &Tracks[i];

        track->StatusFlags &= ~(1<<0);
    }

    SPU::Write16(0x04000500, 0x807F);

    Version = 0;

    if (NDSCart::CartROM)
    {
        u32 gamecode = *(u32*)&NDSCart::CartROM[0xC];
        gamecode &= 0xFFFFFF;
        if      (gamecode == 0x4D5341) // ASMx / Super Mario 64 DS
            Version = 1;
        else if (gamecode == 0x595241) // ARYx / Rayman DS
            Version = 2;
        else if (gamecode == 0x475541) // AUGx / Need for Speed Underground
            Version = 2;
    }

    NDS::ScheduleEvent(NDS::Event_HLE_SoundCmd, true, 174592, Process, 1);
}


void OnAlarm(u32 num)
{
    Alarm& alarm = Alarms[num];
    if (!alarm.Active) return;

    SendIPCReply(0x7, num | (alarm.Param << 8));

    u32 delay = alarm.Repeat;
    if (delay)
    {
        NDS::ScheduleEvent(NDS::Event_HLE_SoundAlarm0+num, true, delay*64, OnAlarm, num);
    }
    else
    {
        alarm.Active = false;
    }
}


bool IsChannelPlaying(int id)
{
    u32 addr = 0x04000400 + (id*16);

    u8 cnt = SPU::Read8(addr+0x03);
    return (cnt & 0x80) != 0;
}

void StopChannel(int id, bool hold)
{
    u32 addr = 0x04000400 + (id*16);

    u32 cnt = SPU::Read32(addr);
    cnt &= ~(1<<31);
    if (hold) cnt |= (1<<15);
    SPU::Write32(addr, cnt);
}

int CalcChannelVolume(int vol, int pan)
{
    if (pan < 24)
    {
        pan += 40;
        pan *= SurroundDecay;
        pan += ((0x7FFF - SurroundDecay) << 6);
        return (vol * pan) >> 21;
    }
    else if (pan >= 104)
    {
        pan -= 40;
        pan *= (-SurroundDecay);
        pan += ((0x7FFF + SurroundDecay) << 6);
        return (vol * pan) >> 21;
    }

    return vol;
}

void SetupChannel_Wave(int id, u32 srcaddr, int format, int repeat, u16 looppos, u32 len, int vol, int voldiv, u16 freq, int pan)
{
    u32 addr = 0x04000400 + (id*16);

    ChanVolume[id] = vol;
    ChanPan[id] = pan;

    if (MasterPan >= 0)
        pan = MasterPan;

    if ((SurroundDecay > 0) && (id != 1) && (id != 3))
        vol = CalcChannelVolume(vol, pan);

    u32 cnt = (format << 29) | (repeat << 27) | (pan << 16) | (voldiv << 8) | vol;
    SPU::Write32(addr+0x00, cnt);
    SPU::Write16(addr+0x08, 0x10000-freq);
    SPU::Write16(addr+0x0A, looppos);
    SPU::Write32(addr+0x0C, len);
    SPU::Write32(addr+0x04, srcaddr);
}

void SetupChannel_PSG(int id, int duty, int vol, int voldiv, u16 freq, int pan)
{
    u32 addr = 0x04000400 + (id*16);

    ChanVolume[id] = vol;
    ChanPan[id] = pan;

    if (MasterPan >= 0)
        pan = MasterPan;

    if ((SurroundDecay > 0) && (id != 1) && (id != 3))
        vol = CalcChannelVolume(vol, pan);

    u32 cnt = (3 << 29) | (duty << 24) | (pan << 16) | (voldiv << 8) | vol;
    SPU::Write32(addr+0x00, cnt);
    SPU::Write16(addr+0x08, 0x10000-freq);
}

void SetupChannel_Noise(int id, int vol, int voldiv, u16 freq, int pan)
{
    u32 addr = 0x04000400 + (id*16);

    ChanVolume[id] = vol;
    ChanPan[id] = pan;

    if (MasterPan >= 0)
        pan = MasterPan;

    if ((SurroundDecay > 0) && (id != 1) && (id != 3))
        vol = CalcChannelVolume(vol, pan);

    u32 cnt = (3 << 29) | (pan << 16) | (voldiv << 8) | vol;
    SPU::Write32(addr+0x00, cnt);
    SPU::Write16(addr+0x08, 0x10000-freq);
}

void SetChannelFrequency(int id, u16 freq)
{
    u32 addr = 0x04000400 + (id*16);

    SPU::Write16(addr+0x08, 0x10000-freq);
}

void SetChannelVolume(int id, int vol, int voldiv)
{
    u32 addr = 0x04000400 + (id*16);

    ChanVolume[id] = vol;

    if ((SurroundDecay > 0) && (id != 1) && (id != 3))
    {
        int pan = SPU::Read8(addr+0x02);
        vol = CalcChannelVolume(vol, pan);
    }

    u16 cnt = (voldiv << 8) | vol;
    SPU::Write16(addr+0x00, cnt);
}

void SetChannelPan(int id, int pan)
{
    u32 addr = 0x04000400 + (id*16);

    ChanPan[id] = pan;

    if (MasterPan >= 0)
        pan = MasterPan;

    SPU::Write8(addr+0x02, pan);

    if ((SurroundDecay > 0) && (id != 1) && (id != 3))
    {
        int vol = CalcChannelVolume(ChanVolume[id], pan);
        SPU::Write8(addr+0x00, vol);
    }
}

int CalcRate(int rate)
{
    if (rate == 0x7F) return 0xFFFF;
    if (rate == 0x7E) return 0x3C00;
    if (rate < 0x32)
    {
        rate = (rate << 1) + 1;
        return rate & 0xFFFF;
    }

    rate = 0x1E00 / (0x7E - rate);
    return rate & 0xFFFF;
}

void SetChannelAttackRate(Channel* chan, int rate)
{
    if (rate < 109)
    {
        chan->AttackRate = 255 - rate;
    }
    else
    {
        const u8 ratetbl[19] =
        {
            0x00, 0x01, 0x05, 0x0E, 0x1A, 0x26, 0x33, 0x3F,
            0x49, 0x54, 0x5C, 0x64, 0x6D, 0x74, 0x7B, 0x7F,
            0x84, 0x89, 0x8F
        };
        rate = 127 - rate;
        chan->AttackRate = ratetbl[rate];
    }
}

void SetChannelDecayRate(Channel* chan, int rate)
{
    chan->DecayRate = CalcRate(rate);
}

void SetChannelSustainRate(Channel* chan, int rate)
{
    chan->SustainRate = rate;
}

void SetChannelReleaseRate(Channel* chan, int rate)
{
    chan->ReleaseRate = CalcRate(rate);
}


bool IsCapturePlaying(int id)
{
    u32 addr = 0x04000508 + id;

    u8 cnt = SPU::Read8(addr);
    return (cnt & 0x80) != 0;
}


void UpdateHardwareChannels()
{
    for (int i = 0; i < 16; i++)
    {
        Channel* chan = &Channels[i];
        if (!(chan->StatusFlags & 0xF8)) continue;

        if (chan->StatusFlags & (1<<4))
        {
            StopChannel(i, false);
        }

        if (chan->StatusFlags & (1<<3))
        {
            if (chan->Type == 0)
            {
                SetupChannel_Wave(i, chan->DataAddr, chan->DataFormat, chan->Repeat?1:2, chan->LoopPos, chan->Length, chan->Volume, chan->VolumeDiv, chan->Frequency, chan->Pan);
            }
            else if (chan->Type == 1)
            {
                SetupChannel_PSG(i, chan->DataAddr, chan->Volume, chan->VolumeDiv, chan->Frequency, chan->Pan);
            }
            else if (chan->Type == 2)
            {
                SetupChannel_Noise(i, chan->Volume, chan->VolumeDiv, chan->Frequency, chan->Pan);
            }

            continue;
        }

        if (chan->StatusFlags & (1<<5))
        {
            SetChannelFrequency(i, chan->Frequency);
        }

        if (chan->StatusFlags & (1<<6))
        {
            SetChannelVolume(i, chan->Volume, chan->VolumeDiv);
        }

        if (chan->StatusFlags & (1<<7))
        {
            SetChannelPan(i, chan->Pan);
        }
    }

    for (int i = 0; i < 16; i++)
    {
        Channel* chan = &Channels[i];
        if (!(chan->StatusFlags & 0xF8)) continue;

        if (chan->StatusFlags & (1<<3))
        {
            u32 addr = 0x04000400 + (i*16) + 0x03;
            SPU::Write8(addr, SPU::Read8(addr) | 0x80);
        }

        chan->StatusFlags &= ~0xF8;
    }
}


void InitTrack(Track* track)
{
    track->NoteBuffer = 0;
    track->CurNoteAddr = 0;

    track->StatusFlags |= 0x42;
    track->StatusFlags &= ~0xBC;

    track->LoopLevel = 0;
    track->InstrIndex = 0;
    track->Priority = 64;
    track->Volume = 127;
    track->Expression = 127;
    track->TrackUnk0A = 0;
    track->Pan = 0;
    track->TrackUnk09 = 0;
    track->PitchBend = 0;
    track->Frequency = 0;
    track->AttackRate = 255;
    track->DecayRate = 255;
    track->SustainRate = 255;
    track->ReleaseRate = 255;
    track->TrackUnk01 = 127;
    track->PitchBendRange = 2;
    track->TrackUnk14 = 60;
    track->PortamentoTime = 0;
    track->SweepPitch = 0;
    track->Transpose = 0;
    track->ChannelMask = 0xFFFF;

    track->ModulationType = 0;
    track->ModulationDepth = 0;
    track->ModulationRange = 1;
    track->ModulationSpeed = 16;
    track->ModulationDelay = 0;

    track->RestCounter = 0;
    track->ChanList = nullptr;
}

void ReleaseTrack(Track* track, Sequence* seq, bool flag)
{
    int volbase3 = BaseVolumeTable[seq->Volume] + BaseVolumeTable[track->Volume] + BaseVolumeTable[track->Expression];
    if (volbase3 < -0x8000) volbase3 = -0x8000;

    int volbase1 = track->TrackUnk0A + seq->SeqUnk06;
    if (volbase1 < -0x8000) volbase1 = -0x8000;

    int freqbase = track->Frequency + ((track->PitchBend * (track->PitchBendRange << 6)) >> 7);

    int panbase = track->Pan;
    if (track->TrackUnk01 != 0x7F)
        panbase = ((panbase * track->TrackUnk01) + 64) >> 7;
    panbase += track->TrackUnk09;
    if (panbase < -0x80) panbase = -0x80;
    else if (panbase > 0x7F) panbase = 0x7F;

    Channel* chan = track->ChanList;
    while (chan)
    {
        chan->VolBase1 = volbase1;
        if (chan->VolRampPhase != 3)
        {
            chan->VolBase3 = volbase3;
            chan->FreqBase3 = freqbase;
            chan->PanBase3 = panbase;
            chan->PanBase1 = track->TrackUnk01;
            chan->ModulationType = track->ModulationType;
            chan->ModulationSpeed = track->ModulationSpeed;
            chan->ModulationDepth = track->ModulationDepth;
            chan->ModulationRange = track->ModulationRange;
            chan->ModulationDelay = track->ModulationDelay;

            if ((chan->NoteLength == 0) && flag)
            {
                chan->Priority = 1;
                chan->VolRampPhase = 3;
            }
        }

        chan = chan->Next;
    }
}

void FinishTrack(Track* track, Sequence* seq, int rate)
{
    ReleaseTrack(track, seq, false);

    Channel* chan = track->ChanList;
    while (chan)
    {
        if (chan->StatusFlags & (1<<0))
        {
            if (rate >= 0)
                SetChannelReleaseRate(chan, rate & 0xFF);

            chan->Priority = 1;
            chan->VolRampPhase = 3;
        }

        chan = chan->Next;
    }
}

void UnlinkTrackChannels(Track* track)
{
    Channel* chan = track->ChanList;
    while (chan)
    {
        chan->Linked = false;
        chan->LinkedTrack = nullptr;

        chan = chan->Next;
    }

    track->ChanList = nullptr;
}

int FindFreeTrack()
{
    for (int i = 0; i < 32; i++)
    {
        Track* track = &Tracks[i];

        if (!(track->StatusFlags & (1<<0)))
        {
            track->StatusFlags |= (1<<0);
            return i;
        }
    }

    return -1;
}

Track* GetSequenceTrack(Sequence* seq, int id)
{
    if (id > 15) return nullptr;

    id = seq->Tracks[id];
    if (id == 0xFF) return nullptr;
    return &Tracks[id];
}

void FinishSequenceTrack(Sequence* seq, int id)
{
    Track* track = GetSequenceTrack(seq, id);
    if (!track) return;

    FinishTrack(track, seq, -1);
    UnlinkTrackChannels(track);

    track->StatusFlags &= ~(1<<0);
    seq->Tracks[id] = 0xFF;
}

void InitSequence(Sequence* seq, u32 sbnk)
{
    seq->StatusFlags &= ~(1<<2);

    seq->SBNKAddr = sbnk;
    seq->Tempo = 0x78;
    seq->SeqUnk1A = 0x100;
    seq->TickCounter = 240;
    seq->Volume = 127;
    seq->SeqUnk06 = 0;
    seq->Pan = 64;

    for (int i = 0; i < 16; i++)
        seq->Tracks[i] = 0xFF;

    if (SharedMem)
    {
        u32 seqdata = SharedMem + (seq->ID * 0x24);

        NDS::ARM7Write32(seqdata+0x40, 0);
        for (int i = 0; i < 16; i++)
        {
            NDS::ARM7Write16(seqdata+0x20 + (i<<1), 0xFFFF);
        }
    }
}

void FinishSequence(Sequence* seq)
{
    for (int i = 0; i < 16; i++)
        FinishSequenceTrack(seq, i);

    seq->StatusFlags &= ~(1<<0);
}


void PrepareSequence(int id, u32 notedata, u32 noteoffset, u32 sbnk)
{
    Sequence* seq = &Sequences[id];

    if (seq->StatusFlags & (1<<0))
        FinishSequence(seq);

    InitSequence(seq, sbnk);
    int tnum = FindFreeTrack();
    if (tnum < 0) return;

    Track* track0 = &Tracks[tnum];
    InitTrack(track0);
    track0->NoteBuffer = notedata;
    track0->CurNoteAddr = track0->NoteBuffer + noteoffset;
    seq->Tracks[0] = tnum;

    u8 firstcmd = NDS::ARM7Read8(track0->CurNoteAddr);
    if (firstcmd == 0xFE)
    {
        // multi track setup

        track0->CurNoteAddr++;

        u16 mask = NDS::ARM7Read8(track0->CurNoteAddr++);
        mask |= (NDS::ARM7Read8(track0->CurNoteAddr++) << 8);

        for (int i = 1; i < 16; i++)
        {
            if (!(mask & (1<<i))) continue;

            int tnum2 = FindFreeTrack();
            if (tnum2 < 0) break;

            Track* track = &Tracks[tnum2];
            InitTrack(track);
            seq->Tracks[i] = tnum2;
        }
    }

    seq->StatusFlags |= (1<<0);
    seq->StatusFlags &= ~(1<<1);

    if (SharedMem)
    {
        u32 mask = NDS::ARM7Read32(SharedMem+4);
        mask |= (1<<id);
        NDS::ARM7Write32(SharedMem+4, mask);
    }
}

void StartSequence(int id)
{
    Sequence* seq = &Sequences[id];
    seq->StatusFlags |= (1<<1);
}

void ProcessCommands()
{
    const u32 cmd_trans_early[30] =
    {
        0x0, 0x1, 0x4, 0x6, 0x7, 0x8, 0x9, 0xA,
        0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12,
        0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x21, 0x1E, 0x1F, 0x20
    };

    while (!CmdQueue.IsEmpty())
    {
        u32 cmdbuf = CmdQueue.Read();
        for (;;)
        {
            if (!cmdbuf)
            {
                u32 val = NDS::ARM7Read32(SharedMem);
                val++;
                NDS::ARM7Write32(SharedMem, val);

                break;
            }

            u32 next = NDS::ARM7Read32(cmdbuf+0x00);
            u32 cmd = NDS::ARM7Read32(cmdbuf+0x04);

            u32 args[4];
            for (u32 i = 0; i < 4; i++)
                args[i] = NDS::ARM7Read32(cmdbuf+0x8 + (i*4));

            if (Version == 1)
            {
                // COMMAND TRANSLATE for SM64DS (early sound engine version)
                cmd = cmd_trans_early[cmd];
            }
            else if (Version == 2)
            {
                if (cmd >= 2)
                    cmd += 3;
            }

            switch (cmd)
            {
            case 0x0: // play sequence, directly
                {
                    PrepareSequence(args[0], args[1], args[2], args[3]);
                    StartSequence(args[0]);
                }
                break;

            case 0x1: // stop sequence
                {
                    u32 id = args[0];
                    Sequence* seq = &Sequences[id];
                    if (!(seq->StatusFlags & (1<<0))) break;

                    FinishSequence(seq);

                    if (SharedMem)
                    {
                        u32 mask = NDS::ARM7Read32(SharedMem+4);
                        mask &= ~(1<<id);
                        NDS::ARM7Write32(SharedMem+4, mask);
                    }
                }
                break;

            case 0x2: // prepare sequence
                {
                    PrepareSequence(args[0], args[1], args[2], args[3]);
                }
                break;

            case 0x3: // start sequence
                {
                    StartSequence(args[0]);
                }
                break;

            case 0x6: // set sequence param
                {
                    // normally, writes directly to the sequence structure

                    Sequence* seq = &Sequences[args[0]];
                    u32 key = (args[3] << 8) | args[1];
                    u32 val = args[2];

                    switch (key)
                    {
                    case 0x104: seq->Pan = val & 0xFF; break;
                    case 0x105: seq->Volume = val & 0xFF; break;

                    case 0x206: seq->SeqUnk06 = val & 0xFFFF; break;
                    case 0x218: seq->Tempo = val & 0xFFFF; break;
                    case 0x21A: seq->SeqUnk1A = val & 0xFFFF; break;
                    case 0x21C: seq->TickCounter = val & 0xFFFF; break;

                    default:
                        printf("cmd6: unknown write %08X %08X\n", key, val);
                        break;
                    }
                }
                break;

            case 0x7: // set track param
                {
                    // normally, writes to the track structure

                    Sequence* seq = &Sequences[args[0] & 0xFFFFFF];
                    u32 trackmask = args[1];
                    u32 key = ((args[0] >> 24) << 8) | args[2];
                    u32 val = args[3];

                    for (int i = 0; i < 16; i++)
                    {
                        if (!(trackmask & (1<<i))) continue;
                        Track* track = GetSequenceTrack(seq, i);
                        if (!track) continue;

                        switch (key)
                        {
                        case 0x104: track->Volume = val & 0xFF; break;
                        case 0x105: track->Expression = val & 0xFF; break;
                        case 0x106: track->PitchBend = val & 0xFF; break;
                        case 0x107: track->PitchBendRange = val & 0xFF; break;
                        case 0x108: track->Pan = val & 0xFF; break;
                        case 0x109: track->TrackUnk09 = val & 0xFF; break;

                        case 0x20A: track->TrackUnk0A = val & 0xFFFF; break;
                        case 0x20C: track->Frequency = val & 0xFFFF; break;

                        default:
                            printf("cmd7: unknown write %08X %08X\n", key, val);
                            break;
                        }
                    }
                }
                break;

            case 0x9: // set track channel mask
                {
                    Sequence* seq = &Sequences[args[0]];
                    u32 trackmask = args[1];
                    u32 chanmask = args[2] & 0xFFFF;

                    for (int i = 0; i < 16; i++)
                    {
                        if (!(trackmask & (1<<i))) continue;
                        Track* track = GetSequenceTrack(seq, i);
                        if (!track) continue;

                        track->ChannelMask = chanmask;
                        track->StatusFlags |= (1<<7);
                    }
                }
                break;

            case 0xA: // write to sharedmem
                {
                    u32 addr = SharedMem + (args[0] * 0x24) + (args[1] * 2) + 0x20;
                    NDS::ARM7Write16(addr, args[2] & 0xFFFF);
                }
                break;

            case 0xC: // start
                {
                    u32 mask_chan = args[0];
                    u32 mask_cap = args[1];
                    u32 mask_alarm = args[2];

                    for (int i = 0; i < 16; i++)
                    {
                        if (!(mask_chan & (1<<i))) continue;

                        u8 cnt = SPU::Read8(0x04000403 + (i*16));
                        cnt |= 0x80;
                        SPU::Write8(0x04000403 + (i*16), cnt);
                    }

                    mask_cap &= 0x3;
                    if (mask_cap == 0x3)
                    {
                        u16 cnt = SPU::Read16(0x04000508);
                        cnt |= 0x8080;
                        SPU::Write16(0x04000508, cnt);
                    }
                    else if (mask_cap == 0x1)
                    {
                        u8 cnt = SPU::Read8(0x04000508);
                        cnt |= 0x80;
                        SPU::Write8(0x04000508, cnt);
                    }
                    else if (mask_cap == 0x2)
                    {
                        u8 cnt = SPU::Read8(0x04000509);
                        cnt |= 0x80;
                        SPU::Write8(0x04000509, cnt);
                    }

                    for (int i = 0; i < 8; i++)
                    {
                        if (!(mask_alarm & (1<<i))) continue;

                        Alarm& alarm = Alarms[i];
                        alarm.Active = true;

                        u32 delay = alarm.Repeat;
                        if (!delay) delay = alarm.Delay;
                        NDS::ScheduleEvent(NDS::Event_HLE_SoundAlarm0+i, false, delay*64, OnAlarm, i);
                    }

                    ReportHardwareStatus();
                }
                break;

            case 0xD: // stop
                {
                    u32 mask_chan = args[0];
                    u32 mask_cap = args[1];
                    u32 mask_alarm = args[2];

                    for (int i = 0; i < 8; i++)
                    {
                        if (!(mask_alarm & (1<<i))) continue;

                        Alarm& alarm = Alarms[i];
                        alarm.Active = false;

                        NDS::CancelEvent(NDS::Event_HLE_SoundAlarm0+i);
                    }

                    for (int i = 0; i < 16; i++)
                    {
                        if (!(mask_chan & (1<<i))) continue;

                        StopChannel(i, args[3]!=0);
                    }

                    if (mask_cap & 0x1)
                    {
                        SPU::Write8(0x04000508, 0);
                    }
                    if (mask_cap & 0x2)
                    {
                        SPU::Write8(0x04000509, 0);
                    }

                    ReportHardwareStatus();
                }
                break;

            case 0xE: // setup channel (wave)
                {
                    int id = args[0] & 0xFFFF;
                    u32 srcaddr = args[1] & 0x07FFFFFF;
                    int format = (args[3] >> 24) & 0x3;
                    int repeat = (args[3] >> 26) & 0x3;
                    u16 looppos = args[3] & 0xFFFF;
                    u32 len = args[2] & 0x3FFFFF;
                    int vol = (args[2] >> 24) & 0x7F;
                    int voldiv = (args[2] >> 22) & 0x3;
                    u16 freq = args[0] >> 16;
                    int pan = (args[3] >> 16) & 0x7F;

                    SetupChannel_Wave(id, srcaddr, format, repeat, looppos, len, vol, voldiv, freq, pan);
                }
                break;

            case 0xF: // setup channel (PSG)
                {
                    int id = args[0];
                    int duty = args[3];
                    int vol = args[1] & 0x7F;
                    int voldiv = (args[1] >> 8) & 0x3;
                    u16 freq = (args[2] >> 8) & 0xFFFF;
                    int pan = args[2] & 0x7F;

                    SetupChannel_PSG(id, duty, vol, voldiv, freq, pan);
                }
                break;

            case 0x10: // setup channel (noise)
                {
                    int id = args[0];
                    int vol = args[1] & 0x7F;
                    int voldiv = (args[1] >> 8) & 0x3;
                    u16 freq = (args[2] >> 8) & 0xFFFF;
                    int pan = args[2] & 0x7F;

                    SetupChannel_Noise(id, vol, voldiv, freq, pan);
                }
                break;

            case 0x11: // setup capture
                {
                    u32 dstaddr = args[0];
                    u16 len = args[1] & 0xFFFF;

                    u32 num = (args[2] >> 31) & 0x1;

                    u32 cnt = ((args[2] >> 30) & 0x1) << 3;
                    cnt |= (((args[2] >> 29) & 0x1) ? 0 : 0x04);
                    cnt |= ((args[2] >> 28) & 0x1) << 1;
                    cnt |= ((args[2] >> 27) & 0x1);

                    SPU::Write8(0x04000508+num, cnt);
                    SPU::Write32(0x04000510+(num*8), dstaddr);
                    SPU::Write16(0x04000514+(num*8), len);
                }
                break;

            case 0x12: // setup alarm
                {
                    u32 num = args[0];

                    Alarm& alarm = Alarms[num & 0x7];
                    alarm.Delay = args[1];
                    alarm.Repeat = args[2];
                    alarm.Param = args[3] & 0xFF;
                    alarm.Active = false; // checkme
                }
                break;

            case 0x14: // set channel volume
                {
                    for (int i = 0; i < 16; i++)
                    {
                        if (!(args[0] & (1<<i))) continue;

                        SetChannelVolume(i, args[1], args[2]);
                    }
                }
                break;

            case 0x15: // set channel pan
                {
                    for (int i = 0; i < 16; i++)
                    {
                        if (!(args[0] & (1<<i))) continue;

                        SetChannelPan(i, args[1]);
                    }
                }
                break;

            case 0x16: // set surround decay
                {
                    SurroundDecay = args[0];

                    for (int i = 0; i < 16; i++)
                    {
                        if (i == 1 || i == 3) continue;

                        u32 chanaddr = 0x04000400 + (i*16);
                        u8 pan = SPU::Read8(chanaddr+0x02);
                        int vol = CalcChannelVolume(ChanVolume[i], pan);
                        SPU::Write8(chanaddr+0x00, vol);
                    }
                }
                break;

            case 0x17: // set master volume
                {
                    SPU::Write8(0x04000500, args[0] & 0xFF);
                }
                break;

            case 0x18: // set master pan
                {
                    MasterPan = args[0];
                    if (MasterPan >= 0)
                    {
                        u8 pan = MasterPan & 0xFF;
                        for (int i = 0; i < 16; i++)
                        {
                            SPU::Write8(0x04000402 + (i*16), pan);
                        }
                    }
                    else
                    {
                        for (int i = 0; i < 16; i++)
                        {
                            SPU::Write8(0x04000402 + (i*16), ChanPan[i]);
                        }
                    }
                }
                break;

            case 0x19: // output sel
                {
                    u32 outputL = args[0];
                    u32 outputR = args[1];
                    u32 mixch1 = args[2];
                    u32 mixch3 = args[3];

                    u8 cnt = NDS::ARM7Read8(0x04000501);
                    cnt &= 0x80;
                    cnt |= (outputL & 0x3);
                    cnt |= (outputR & 0x3) << 2;
                    cnt |= (mixch1 & 0x1) << 4;
                    cnt |= (mixch3 & 0x1) << 5;
                    NDS::ARM7Write8(0x04000501, cnt);
                }
                break;

            case 0x1A:
                {
                    //
                    printf("unknown sound cmd %08X, %08X %08X %08X %08X\n",
                       cmd, args[0], args[1], args[2], args[3]);
                }
                break;

            case 0x1D:
                SharedMem = args[0];
                break;

            case 0x20:
                {
                    // cmdbuf+08 and cmdbuf+0C = bounds
                    // stop channels conditionally
                    printf("unknown sound cmd %08X, %08X %08X %08X %08X\n",
                       cmd, args[0], args[1], args[2], args[3]);
                }
                break;

            default:
                printf("unknown sound cmd %08X, %08X %08X %08X %08X\n",
                       cmd, args[0], args[1], args[2], args[3]);
                break;
            }

            cmdbuf = next;
        }
    }
}


bool ReadInstrument(u32 sbnk, int index, int tune, u8* out)
{
    if (index < 0) return false;

    u32 numinstr = NDS::ARM7Read32(sbnk + 0x38);
    if ((u32)index >= numinstr) return false;

    u32 val = NDS::ARM7Read32(sbnk + 0x3C + (index << 2));
    out[0] = val & 0xFF;
    if (out[0] >= 1 && out[0] <= 5)
    {
        u32 addr = sbnk + (val >> 8);
        for (int i = 0; i < 5; i++)
        {
            *(u16*)&out[2 + (i << 1)] = NDS::ARM7Read16(addr + (i << 1));
        }
        return true;
    }
    else if (out[0] == 16)
    {
        u32 addr = sbnk + (val >> 8);
        u8 lower = NDS::ARM7Read8(addr);
        u8 upper = NDS::ARM7Read8(addr + 1);

        if (tune < lower) return false;
        if (tune > upper) return false;

        addr += ((tune - lower) * 0xC) + 2;
        for (int i = 0; i < 6; i++)
        {
            *(u16*)&out[(i << 1)] = NDS::ARM7Read16(addr + (i << 1));
        }
        return true;
    }
    else if (out[0] == 17)
    {
        u32 addr = sbnk + (val >> 8);

        int num = -1;
        for (int i = 0; i < 8; i++)
        {
            u8 val = NDS::ARM7Read8(addr + i);
            if (tune > val) continue;

            num = i;
            break;
        }

        if (num < 0) return false;

        addr += (num * 0xC) + 8;
        for (int i = 0; i < 6; i++)
        {
            *(u16*)&out[(i << 1)] = NDS::ARM7Read16(addr + (i << 1));
        }
        return true;
    }

    return false;
}

void InitInstrumentChannel(Channel* chan, int len)
{
    chan->BaseVolume = -92544;
    chan->VolRampPhase = 0;
    chan->NoteLength = len;
    chan->ModulationCount1 = 0;
    chan->ModulationCount2 = 0;
    chan->StatusFlags |= 0x03;
}

bool SetupInstrument(Channel* chan, int tune, int speed, int len, u32 sbnk, u8* data)
{
    u8 release = data[0x0A];
    if (release == 0xFF)
    {
        release = 0;
        len = -1;
    }

    switch (data[0x00])
    {
    case 1: // wave
    case 4:
        {
            u32 swav;
            if (data[0x00] != 1)
            {
                // direct pointer

                swav = (*(u16*)&data[0x02]) | ((*(u16*)&data[0x04]) << 16);
            }
            else
            {
                // load from SBNK

                u16 swav_num = *(u16*)&data[0x02];
                u16 swar_num = *(u16*)&data[0x04];

                u32 swar = NDS::ARM7Read32(sbnk + 0x18 + (swar_num << 3));
                if (!swar) return false;

                u32 num_samples = NDS::ARM7Read32(swar + 0x38);
                if (swav_num >= num_samples) return false;

                swav = NDS::ARM7Read32(swar + 0x3C + (swav_num << 2));
                if (!swav) return false;
                if (swav < 0x02000000)
                    swav += swar;
            }

            if (!swav) return false;

            chan->Type = 0; // wave
            chan->DataFormat = NDS::ARM7Read8(swav+0x00);
            chan->Repeat = NDS::ARM7Read8(swav+0x01);
            chan->SampleRate = NDS::ARM7Read16(swav+0x02);
            chan->SwavFrequency = NDS::ARM7Read16(swav+0x04);
            chan->LoopPos = NDS::ARM7Read16(swav+0x06);
            chan->Length = NDS::ARM7Read32(swav+0x08);
            chan->DataAddr = swav+0xC;
        }
        break;

    case 2: // PSG
        {
            u16 duty = *(u16*)&data[0x02];

            if ((chan->ID < 8) || (chan->ID > 13))
                return false;

            chan->Type = 1; // PSG
            chan->DataAddr = duty;
            chan->SwavFrequency = 0x1F46;
        }
        break;

    case 3: // noise
        {
            if ((chan->ID < 14) || (chan->ID > 15))
                return false;

            chan->Type = 2; // noise
            chan->SwavFrequency = 0x1F46;
        }
        break;

    default:
        return false;
    }

    InitInstrumentChannel(chan, len);
    chan->FreqBase2 = tune;
    chan->FreqBase1 = *(u16*)&data[0x06]; // note number
    chan->VolBase2 = speed;
    SetChannelAttackRate(chan, data[0x07]);
    SetChannelDecayRate(chan, data[0x08]);
    SetChannelSustainRate(chan, data[0x09]);
    SetChannelReleaseRate(chan, release);
    chan->PanBase2 = data[0x0B] - 64;
    return true;
}

Channel* AllocateChannel(u16 chanmask, int prio, bool flag, Track* track)
{
    const int chanorder[16] = {4, 5, 6, 7, 2, 0, 3, 1, 8, 9, 10, 11, 14, 12, 15, 13};
    const int voldiv[4] = {0, 1, 2, 4};
    Channel* ret = nullptr;

    // TODO: channel lock masks
    chanmask &= 0xFFF5;

    for (int i = 0; i < 16; i++)
    {
        int id = chanorder[i];
        if (!(chanmask & (1 << id))) continue;

        Channel* chan = &Channels[id];
        if (!ret)
        {
            ret = chan;
            continue;
        }

        if (chan->Priority > ret->Priority)
            continue;

        if (chan->Priority == ret->Priority)
        {
            u32 vol1 = (chan->Volume << 4) >> voldiv[chan->VolumeDiv];
            u32 vol2 = (ret->Volume << 4) >> voldiv[ret->VolumeDiv];

            if (vol1 >= vol2)
                continue;
        }

        ret = chan;
    }

    if (!ret) return nullptr;
    if (prio < ret->Priority) return nullptr;

    if (ret->Linked)
        UnlinkChannel(ret, false);

    ret->StatusFlags &= ~0xF9;
    ret->StatusFlags |= (1<<4);

    ret->Next = nullptr;
    ret->Linked = true;
    ret->LinkedTrack = track;
    ret->NoteLength = 0;
    ret->Priority = prio;
    ret->Volume = 127;
    ret->VolumeDiv = 0;
    ret->StatusFlags &= ~(1<<1);
    ret->StatusFlags |= (1<<2);
    ret->FreqBase2 = 60;
    ret->FreqBase1 = 60;
    ret->VolBase2 = 127;
    ret->PanBase2 = 0;
    ret->VolBase3 = 0;
    ret->VolBase1 = 0;
    ret->FreqBase3 = 0;
    ret->PanBase3 = 0;
    ret->PanBase1 = 127;
    ret->FreqRampTarget = 0;
    ret->FreqRampLen = 0;
    ret->FreqRampPos = 0;
    SetChannelAttackRate(ret, 127);
    SetChannelDecayRate(ret, 127);
    SetChannelSustainRate(ret, 127);
    SetChannelReleaseRate(ret, 127);
    ret->ModulationType = 0;
    ret->ModulationDepth = 0;
    ret->ModulationRange = 1;
    ret->ModulationSpeed = 16;
    ret->ModulationDelay = 0;

    return ret;
}

void TrackKeyOn(Track* track, Sequence* seq, int tune, int speed, int len)
{
    //printf("Sound_Nitro: KEY ON, seq=%d, tune=%d speed=%d len=%d\n", seq->ID, tune, speed, len);

    Channel* chan = nullptr;
    if (track->StatusFlags & (1<<3))
    {
        chan = track->ChanList;
        if (chan)
        {
            chan->FreqBase2 = tune;
            chan->VolBase2 = speed;
        }
    }

    if (!chan)
    {
        // allocate channel

        u8 instrdata[16];
        if (!ReadInstrument(seq->SBNKAddr, track->InstrIndex, tune, instrdata))
            return;

        u16 chanmask;
        switch (instrdata[0])
        {
        case 1:
        case 4: chanmask = 0xFFFF; break;
        case 2: chanmask = 0x3F00; break;
        case 3: chanmask = 0xC000; break;
        default: return;
        }

        chanmask &= track->ChannelMask;
        chan = AllocateChannel(chanmask, track->Priority, (track->StatusFlags & (1<<7)) != 0, track);
        if (!chan) return;

        int _len;
        if (track->StatusFlags & (1<<3))
            _len = -1;
        else
            _len = len;

        if (!SetupInstrument(chan, tune, speed, _len, seq->SBNKAddr, instrdata))
        {
            chan->Priority = 0;
            chan->Linked = false;
            chan->LinkedTrack = nullptr;
            return;
        }

        chan->Next = track->ChanList;
        track->ChanList = chan;
    }

    if (track->AttackRate != 0xFF)
        SetChannelAttackRate(chan, track->AttackRate);
    if (track->DecayRate != 0xFF)
        SetChannelDecayRate(chan, track->DecayRate);
    if (track->SustainRate != 0xFF)
        SetChannelSustainRate(chan, track->SustainRate);
    if (track->ReleaseRate != 0xFF)
        SetChannelReleaseRate(chan, track->ReleaseRate);

    chan->FreqRampTarget = track->SweepPitch;
    if (track->StatusFlags & (1<<5))
    {
        chan->FreqRampTarget += (((track->TrackUnk14 - tune) << 22) >> 16);
    }

    if (!track->PortamentoTime)
    {
        chan->FreqRampLen = len;
        chan->StatusFlags &= ~(1<<2);
    }
    else
    {
        int time = track->PortamentoTime;
        time *= time;

        int target = chan->FreqRampTarget;
        if (target < 0) target = -target;
        time *= target;
        chan->FreqRampLen = time >> 11;
    }
    chan->FreqRampPos = 0;
}

u32 GetNoteParamAddr(Sequence* seq, u8 idx)
{
    if (!SharedMem) return 0;

    if (idx >= 0x10)
    {
        return SharedMem + 0x260 + ((idx-0x10) << 1);
    }

    return SharedMem + 0x20 + (seq->ID * 0x24) + (idx << 1);
}

u32 ReadNoteOpParam(Track* track, Sequence* seq, int type)
{
    switch (type)
    {
    case 0: // 8-bit value
        {
            u8 val = NDS::ARM7Read8(track->CurNoteAddr++);
            return val;
        }

    case 1: // 16-bit value
        {
            u32 val = NDS::ARM7Read8(track->CurNoteAddr++);
            val |= (NDS::ARM7Read8(track->CurNoteAddr++) << 8);
            return val;
        }

    case 2: // variable length
        {
            u32 val = 0;
            for (;;)
            {
                u8 byte = NDS::ARM7Read8(track->CurNoteAddr++);
                val = (val << 7) | (byte & 0x7F);
                if (!(byte & 0x80)) break;
            }
            return val;
        }

    case 3: // based on counter
        {
            s16 val1 = NDS::ARM7Read8(track->CurNoteAddr++);
            val1 |= (NDS::ARM7Read8(track->CurNoteAddr++) << 8);
            s16 val2 = NDS::ARM7Read8(track->CurNoteAddr++);
            val2 |= (NDS::ARM7Read8(track->CurNoteAddr++) << 8);

            u16 cnt = UpdateCounter();
            int res = ((val2 - val1) + 1) * cnt;
            res = val1 + (res >> 16);
            return res;
        }

    case 4: // variable in sharedmem
        {
            u8 idx = NDS::ARM7Read8(track->CurNoteAddr++);
            u32 addr = GetNoteParamAddr(seq, idx);
            if (addr)
            {
                u32 val = NDS::ARM7Read16(addr);
                return ((s32)(val << 16)) >> 16;
            }

            return 0;
        }
    }

    return 0;
}

int UpdateTrack(Track* track, Sequence* seq, int id, bool process)
{
    Channel* chan = track->ChanList;
    while (chan)
    {
        if (chan->NoteLength > 0)
            chan->NoteLength--;

        if (!(chan->StatusFlags & (1<<2)))
        {
            if (chan->FreqRampPos < chan->FreqRampLen)
                chan->FreqRampPos++;
        }

        chan = chan->Next;
    }

    if (track->StatusFlags & (1<<4))
    {
        if (track->ChanList) return 0;
        track->StatusFlags &= ~(1<<4);
    }

    if (track->RestCounter > 0)
    {
        track->RestCounter--;
        if (track->RestCounter > 0)
            return 0;
    }

    while (!track->RestCounter)
    {
        if (track->StatusFlags & (1<<4)) break;

        bool cond = true;
        int paramtype = 2;

        u8 note_op = NDS::ARM7Read8(track->CurNoteAddr++);
        if (note_op == 0xA2)
        {
            note_op = NDS::ARM7Read8(track->CurNoteAddr++);
            cond = (track->StatusFlags & (1<<6)) != 0;
        }
        if (note_op == 0xA0)
        {
            note_op = NDS::ARM7Read8(track->CurNoteAddr++);
            paramtype = 3;
        }
        if (note_op == 0xA1)
        {
            note_op = NDS::ARM7Read8(track->CurNoteAddr++);
            paramtype = 4;
        }

        if (!(note_op & 0x80))
        {
            // key on

            u8 speed = NDS::ARM7Read8(track->CurNoteAddr++);
            s32 len = ReadNoteOpParam(track, seq, paramtype);
            int tune = note_op + track->Transpose;
            if (!cond) continue;

            if (tune < 0) tune = 0;
            else if (tune > 127) tune = 127;

            if ((!(track->StatusFlags & (1<<2))) && process)
            {
                TrackKeyOn(track, seq, tune, speed, (len<=0) ? -1 : len);
            }

            track->TrackUnk14 = tune;
            if (track->StatusFlags & (1<<1))
            {
                track->RestCounter = len;
                if (!len) track->StatusFlags |= (1<<4);
            }
        }
        else
        {
            switch (note_op & 0xF0)
            {
            case 0x80:
                {
                    u32 param = ReadNoteOpParam(track, seq, paramtype);
                    if (!cond) break;

                    switch (note_op)
                    {
                    case 0x80: // rest
                        track->RestCounter = param;
                        break;

                    case 0x81: // bank/program number
                        if ((s32)param < 0x10000)
                            track->InstrIndex = param;
                        break;

                    default:
                        printf("Sound_Nitro: unknown note op %02X\n", note_op);
                        break;
                    }
                }
                break;

            case 0x90:
                {
                    switch (note_op)
                    {
                    case 0x93: // setup track
                        {
                            u8 tnum = NDS::ARM7Read8(track->CurNoteAddr++);
                            u32 trackaddr = NDS::ARM7Read8(track->CurNoteAddr++);
                            trackaddr |= (NDS::ARM7Read8(track->CurNoteAddr++) << 8);
                            trackaddr |= (NDS::ARM7Read8(track->CurNoteAddr++) << 16);
                            if (!cond) break;

                            Track* thetrack = GetSequenceTrack(seq, tnum);
                            if (!thetrack) break;
                            if (thetrack == track) break;

                            FinishTrack(thetrack, seq, -1);
                            UnlinkTrackChannels(thetrack);

                            thetrack->NoteBuffer = track->NoteBuffer;
                            thetrack->CurNoteAddr = thetrack->NoteBuffer + trackaddr;
                        }
                        break;

                    case 0x94: // jump
                        {
                            u32 jumpaddr = NDS::ARM7Read8(track->CurNoteAddr++);
                            jumpaddr |= (NDS::ARM7Read8(track->CurNoteAddr++) << 8);
                            jumpaddr |= (NDS::ARM7Read8(track->CurNoteAddr++) << 16);
                            if (!cond) break;

                            track->CurNoteAddr = track->NoteBuffer + jumpaddr;
                        }
                        break;

                    case 0x95: // call
                        {
                            u32 jumpaddr = NDS::ARM7Read8(track->CurNoteAddr++);
                            jumpaddr |= (NDS::ARM7Read8(track->CurNoteAddr++) << 8);
                            jumpaddr |= (NDS::ARM7Read8(track->CurNoteAddr++) << 16);
                            if (!cond) break;

                            if (track->LoopLevel >= 3) break;
                            track->LoopAddr[track->LoopLevel] = track->CurNoteAddr;
                            track->LoopLevel++;
                            track->CurNoteAddr = track->NoteBuffer + jumpaddr;
                        }
                        break;

                    default:
                        printf("Sound_Nitro: unknown note op %02X\n", note_op);
                        break;
                    }
                }
                break;

            case 0xB0:
                {
                    u8 idx = NDS::ARM7Read8(track->CurNoteAddr++);
                    if (paramtype == 2) paramtype = 1;
                    s16 param = (s16)((s32)(ReadNoteOpParam(track, seq, paramtype) << 16) >> 16);
                    u32 paramaddr = GetNoteParamAddr(seq, idx);
                    if (!cond) break;
                    if (!paramaddr) break;

                    switch (note_op)
                    {
                    case 0xB0: // set
                        NDS::ARM7Write16(paramaddr, param);
                        break;
                    case 0xB1: // add
                        NDS::ARM7Write16(paramaddr, (s16)NDS::ARM7Read16(paramaddr) + param);
                        break;
                    case 0xB2: // sub
                        NDS::ARM7Write16(paramaddr, (s16)NDS::ARM7Read16(paramaddr) - param);
                        break;
                    case 0xB3: // mul
                        NDS::ARM7Write16(paramaddr, (s16)NDS::ARM7Read16(paramaddr) * param);
                        break;
                    case 0xB4: // div
                        if (!param) break;
                        NDS::ARM7Write16(paramaddr, (s16)NDS::ARM7Read16(paramaddr) / param);
                        break;
                    case 0xB5: // shift
                        if (param >= 0)
                            NDS::ARM7Write16(paramaddr, (s16)NDS::ARM7Read16(paramaddr) << param);
                        else
                            NDS::ARM7Write16(paramaddr, (s16)NDS::ARM7Read16(paramaddr) >> (-param));
                        break;
                    case 0xB6: //
                        {
                            bool neg = false;
                            if (param < 0)
                            {
                                neg = true;
                                param = -param;
                            }

                            u16 cnt = UpdateCounter();
                            int val = ((int)cnt * (param + 1)) >> 16;
                            if (neg) val = -val;
                            NDS::ARM7Write16(paramaddr, val);
                        }
                        break;
                    case 0xB8: // compare ==
                        track->StatusFlags &= ~(1<<6);
                        if ((s16)NDS::ARM7Read16(paramaddr) == param)
                            track->StatusFlags |= (1<<6);
                        break;
                    case 0xB9: // compare >=
                        track->StatusFlags &= ~(1<<6);
                        if ((s16)NDS::ARM7Read16(paramaddr) >= param)
                            track->StatusFlags |= (1<<6);
                        break;
                    case 0xBA: // compare >
                        track->StatusFlags &= ~(1<<6);
                        if ((s16)NDS::ARM7Read16(paramaddr) > param)
                            track->StatusFlags |= (1<<6);
                        break;
                    case 0xBB: // compare <=
                        track->StatusFlags &= ~(1<<6);
                        if ((s16)NDS::ARM7Read16(paramaddr) <= param)
                            track->StatusFlags |= (1<<6);
                        break;
                    case 0xBC: // compare <
                        track->StatusFlags &= ~(1<<6);
                        if ((s16)NDS::ARM7Read16(paramaddr) < param)
                            track->StatusFlags |= (1<<6);
                        break;
                    case 0xBD: // compare !=
                        track->StatusFlags &= ~(1<<6);
                        if ((s16)NDS::ARM7Read16(paramaddr) != param)
                            track->StatusFlags |= (1<<6);
                        break;

                    default:
                        printf("Sound_Nitro: unknown note op %02X\n", note_op);
                        break;
                    }
                }
                break;

            case 0xC0:
            case 0xD0:
                {
                    if (paramtype == 2) paramtype = 0;
                    u8 param = (u8)ReadNoteOpParam(track, seq, paramtype);
                    if (!cond) break;

                    switch (note_op)
                    {
                    case 0xC0: // pan
                        track->Pan = param - 64;
                        break;
                    case 0xC1: // volume
                        track->Volume = param;
                        break;
                    case 0xC2: // master volume
                        seq->Volume = param;
                        break;
                    case 0xC3: // transpose
                        track->Transpose = param;
                        break;
                    case 0xC4: // pitch bend
                        track->PitchBend = param;
                        break;
                    case 0xC5: // pitch bend range
                        track->PitchBendRange = param;
                        break;
                    case 0xC6: // priority
                        track->Priority = param;
                        break;
                    case 0xC7: // mono/poly
                        track->StatusFlags &= ~(1<<1);
                        track->StatusFlags |= ((param & 0x1) << 1);
                        break;
                    case 0xC8: // tie
                        track->StatusFlags &= ~(1<<3);
                        track->StatusFlags |= ((param & 0x1) << 3);
                        FinishTrack(track, seq, -1);
                        UnlinkTrackChannels(track);
                        break;
                    case 0xC9: // portamento control
                        track->TrackUnk14 = param + track->Transpose;
                        track->StatusFlags |= (1<<5);
                        if (Version == 1)
                        {
                            FinishTrack(track, seq, -1);
                            UnlinkTrackChannels(track);
                        }
                        break;
                    case 0xCA: // modulation depth
                        track->ModulationDepth = param;
                        break;
                    case 0xCB: // modulation speed
                        track->ModulationSpeed = param;
                        break;
                    case 0xCC: // modulation type
                        track->ModulationType = param;
                        break;
                    case 0xCD: // modulation range
                        track->ModulationRange = param;
                        break;
                    case 0xCE: // portamento on/off
                        track->StatusFlags &= ~(1<<5);
                        track->StatusFlags |= ((param & 0x1) << 5);
                        if (Version == 1)
                        {
                            FinishTrack(track, seq, -1);
                            UnlinkTrackChannels(track);
                        }
                        break;
                    case 0xCF: // portamento time
                        track->PortamentoTime = param;
                        break;
                    case 0xD0: // attack rate
                        track->AttackRate = param;
                        break;
                    case 0xD1: // decay rate
                        track->DecayRate = param;
                        break;
                    case 0xD2: // sustain rate
                        track->SustainRate = param;
                        break;
                    case 0xD3: // release rate
                        track->ReleaseRate = param;
                        break;
                    case 0xD4: // loop start marker
                        if (track->LoopLevel >= 3) break;
                        track->LoopAddr[track->LoopLevel] = track->CurNoteAddr;
                        track->LoopCount[track->LoopLevel] = param;
                        track->LoopLevel++;
                        break;
                    case 0xD5: // expression
                        track->Expression = param;
                        break;

                    default:
                        printf("Sound_Nitro: unknown note op %02X\n", note_op);
                        break;
                    }
                }
                break;

            case 0xE0:
                {
                    if (paramtype == 2) paramtype = 1;
                    s16 param = (s16)((s32)(ReadNoteOpParam(track, seq, paramtype) << 16) >> 16);
                    if (!cond) break;

                    switch (note_op)
                    {
                    case 0xE0: // modulation delay
                        track->ModulationDelay = param;
                        break;
                    case 0xE1: // tempo
                        seq->Tempo = param;
                        break;
                    case 0xE3: // sweep pitch
                        track->SweepPitch = param;
                        break;

                    default:
                        printf("Sound_Nitro: unknown note op %02X\n", note_op);
                        break;
                    }
                }
                break;

            case 0xF0:
                {
                    if (!cond) break;
                    switch (note_op)
                    {
                    case 0xFC: // loop end marker
                        {
                            if (track->LoopLevel == 0) break;

                            int level = track->LoopLevel - 1;
                            u8 cnt = track->LoopCount[level];
                            if (cnt != 0)
                            {
                                cnt--;
                                if (!cnt)
                                {
                                    track->LoopLevel--;
                                    break;
                                }
                            }

                            track->LoopCount[level] = cnt;
                            track->CurNoteAddr = track->LoopAddr[level];
                        }
                        break;

                    case 0xFD: // return from call
                        {
                            if (track->LoopLevel == 0) break;

                            int level = track->LoopLevel - 1;
                            track->CurNoteAddr = track->LoopAddr[level];
                            track->LoopLevel--;
                        }
                        break;

                    case 0xFE: // multitrack marker (handled at sequence setup)
                        break;
                    case 0xFF: // track end
                        return -1;

                    default:
                        printf("Sound_Nitro: unknown note op %02X\n", note_op);
                        break;
                    }
                }
                break;

            default:
                printf("Sound_Nitro: unknown note op %02X\n", note_op);
                break;
            }
        }
    }

    return 0;
}

bool UpdateSequenceTracks(Sequence* seq, bool process)
{
    bool ret = true;

    for (int i = 0; i < 16; i++)
    {
        Track* track = GetSequenceTrack(seq, i);
        if (!track) continue;
        if (!track->CurNoteAddr) continue;

        if (!UpdateTrack(track, seq, i, process))
        {
            ret = false;
            continue;
        }

        FinishSequenceTrack(seq, i);
    }

    return ret;
}

void UpdateSequence(Sequence* seq)
{
    int cnt = 0;
    while (seq->TickCounter >= 240)
    {
        seq->TickCounter -= 240;
        cnt++;
    }

    int i;
    for (i = 0; i < cnt; i++)
    {
        if (UpdateSequenceTracks(seq, true))
        {
            FinishSequence(seq);
            break;
        }
    }

    if (SharedMem)
    {
        u32 addr = SharedMem + 0x40 + (seq->ID * 0x24);
        NDS::ARM7Write32(addr, NDS::ARM7Read32(addr) + i);
    }

    seq->TickCounter += ((seq->Tempo * seq->SeqUnk1A) >> 8);
}

void ProcessSequences(bool update)
{
    u16 activemask = 0;

    for (int i = 0; i < 16; i++)
    {
        Sequence* seq = &Sequences[i];
        if (!(seq->StatusFlags & (1<<0))) continue;

        if (seq->StatusFlags & (1<<1))
        {
            if (update && (!(seq->StatusFlags & (1<<2))))
                UpdateSequence(seq);

            for (int j = 0; j < 16; j++)
            {
                Track* track = GetSequenceTrack(seq, j);
                if (!track) continue;

                ReleaseTrack(track, seq, true);
            }
        }

        if (seq->StatusFlags & (1<<0))
            activemask |= (1<<i);
    }

    if (SharedMem)
        NDS::ARM7Write32(SharedMem+4, activemask);
}


void UnlinkChannel(Channel* chan, bool unlink)
{
    Track* track = chan->LinkedTrack;

    if (unlink)
    {
        chan->Priority = 0;
        chan->Linked = false;
        chan->LinkedTrack = nullptr;
    }

    if (track->ChanList == chan)
    {
        track->ChanList = chan->Next;
        return;
    }

    Channel* chan2 = track->ChanList;
    do
    {
        if (chan2->Next == chan)
        {
            chan2->Next = chan->Next;
            return;
        }

        chan2 = chan2->Next;
    }
    while (chan2);
}

int ChannelVolumeRamp(Channel* chan, bool update)
{
    if (update)
    {
        if (chan->VolRampPhase == 0)
        {
            // attack

            chan->BaseVolume = -(((-chan->BaseVolume) * chan->AttackRate) >> 8);
            if (chan->BaseVolume == 0)
                chan->VolRampPhase = 1;
        }
        else if (chan->VolRampPhase == 1)
        {
            // decay

            chan->BaseVolume -= chan->DecayRate;
            int target = BaseVolumeTable[chan->SustainRate & 0x7F] << 7;
            if (chan->BaseVolume <= target)
            {
                chan->BaseVolume = target;
                chan->VolRampPhase = 2;
            }
        }
        else if (chan->VolRampPhase == 3)
        {
            // release

            chan->BaseVolume -= chan->ReleaseRate;
        }
    }

    return chan->BaseVolume >> 7;
}

int ChannelFreqRamp(Channel* chan, bool update)
{
    if (chan->FreqRampTarget == 0)
        return 0;

    if (chan->FreqRampPos >= chan->FreqRampLen)
        return 0;

    s64 tmp = (s64)chan->FreqRampTarget * (s64)(chan->FreqRampLen-chan->FreqRampPos);
    int ret = (int)(tmp / (s64)chan->FreqRampLen);

    if (update)
    {
        if (chan->StatusFlags & (1<<2))
            chan->FreqRampPos++;
    }

    return ret;
}

int ChannelModulation(Channel* chan, bool update)
{
    s64 modfactor = 0;
    if ((chan->ModulationDepth != 0) &&
        (chan->ModulationCount1 >= chan->ModulationDelay))
    {
        int index = chan->ModulationCount2 >> 8;
        if (index < 32)
            modfactor = ModulationTable[index];
        else if (index < 64)
            modfactor = ModulationTable[64 - index];
        else if (index < 96)
            modfactor = -ModulationTable[index - 64];
        else
            modfactor = -ModulationTable[32 - (index - 96)];

        modfactor *= chan->ModulationDepth;
        modfactor *= chan->ModulationRange;
    }

    if (modfactor)
    {
        switch (chan->ModulationType)
        {
        case 0:
            modfactor <<= 6;
            break;
        case 1:
            modfactor *= 60;
            break;
        case 2:
            modfactor <<= 6;
            break;
        }

        modfactor >>= 14;
    }

    if (update)
    {
        if (chan->ModulationCount1 < chan->ModulationDelay)
        {
            chan->ModulationCount1++;
        }
        else
        {
            u32 cnt = (chan->ModulationCount2 + (chan->ModulationSpeed << 6)) >> 8;
            while (cnt >= 128) cnt -= 128;

            chan->ModulationCount2 += (chan->ModulationSpeed << 6);
            chan->ModulationCount2 &= 0x00FF;
            chan->ModulationCount2 |= (cnt << 8);
        }
    }

    return (int)modfactor;
}

u16 CalcVolume(int vol)
{
    if (vol < -723) vol = -723;
    else if (vol > 0) vol = 0;

    u8 ret = SWIVolumeTable[vol + 723];

    int div = 0;
    if (vol < -240) div = 3;
    else if (vol < -120) div = 2;
    else if (vol < -60) div = 1;

    return (div << 8) | ret;
}

u16 CalcFreq(u32 unk, int freq)
{
    freq = -freq;

    int div = 0;
    while (freq < 0)
    {
        div--;
        freq += 768;
    }
    while (freq >= 768)
    {
        div++;
        freq -= 768;
    }

    u64 pitch = SWIPitchTable[freq] + 0x10000;
    pitch *= unk;

    div -= 16;
    if (div <= 0)
    {
        pitch >>= (-div);
    }
    else if (div < 32)
    {
        pitch <<= div;
        // CHECKME
        if (!pitch) return 0xFFFF;
    }
    else
    {
        return 0xFFFF;
    }

    if (pitch < 0x10)
        pitch = 0x10;
    else if (pitch > 0xFFFF)
        pitch = 0xFFFF;

    return (u16)(pitch & 0xFFFF);
}

void UpdateChannels(bool updateramps)
{
    for (int i = 0; i < 16; i++)
    {
        Channel* chan = &Channels[i];
        if (!(chan->StatusFlags & (1<<0))) continue;

        if (chan->StatusFlags & (1<<1))
        {
            chan->StatusFlags |= (1<<3);
            chan->StatusFlags &= ~(1<<1);
        }
        else if (!IsChannelPlaying(i))
        {
            if (chan->Linked)
                UnlinkChannel(chan, true);
            else
                chan->Priority = 0;

            chan->Volume = 0;
            chan->VolumeDiv = 0;
            chan->StatusFlags &= ~(1<<0);
            continue;
        }

        int vol = BaseVolumeTable[chan->VolBase2 & 0x7F];
        int freq = (chan->FreqBase2 - chan->FreqBase1) << 6;
        int pan = 0;

        vol += ChannelVolumeRamp(chan, updateramps);
        freq += ChannelFreqRamp(chan, updateramps);

        vol += chan->VolBase3;
        vol += chan->VolBase1;
        freq += chan->FreqBase3;

        int mod = ChannelModulation(chan, updateramps);
        switch (chan->ModulationType)
        {
        case 0:
            freq += mod;
            break;
        case 1:
            if (vol > -0x8000)
                vol += mod;
            break;
        case 2:
            pan += mod;
            break;
        }

        if (chan->PanBase1 != 0x7F)
        {
            pan = ((pan * (int)chan->PanBase1) + 64) >> 7;
        }
        pan += chan->PanBase3;

        if ((chan->VolRampPhase == 3) && (vol <= -0x2D3))
        {
            chan->StatusFlags &= ~0xF8;
            chan->StatusFlags |= (1<<4);

            if (chan->Linked)
                UnlinkChannel(chan, true);
            else
                chan->Priority = 0;

            chan->Volume = 0;
            chan->VolumeDiv = 0;
            chan->StatusFlags &= ~(1<<0);
            continue;
        }

        u16 finalvol = CalcVolume(vol);

        u16 finalfreq = CalcFreq(chan->SwavFrequency, freq);
        if (chan->Type == 1) finalfreq &= 0xFFFC;

        pan += 64;
        if (pan < 0) pan = 0;
        else if (pan > 127) pan = 127;
        u8 finalpan = (u8)pan;

        if (finalvol != (chan->Volume | (chan->VolumeDiv<<8)))
        {
            chan->Volume = finalvol & 0xFF;
            chan->VolumeDiv = finalvol >> 8;
            chan->StatusFlags |= (1<<6);
        }

        if (finalfreq != chan->Frequency)
        {
            chan->Frequency = finalfreq;
            chan->StatusFlags |= (1<<5);
        }

        if (finalpan != chan->Pan)
        {
            chan->Pan = finalpan;
            chan->StatusFlags |= (1<<7);
        }
    }
}


void ReportHardwareStatus()
{
    if (!SharedMem) return;

    u16 chanmask = 0;
    for (int i = 0; i < 16; i++)
    {
        if (IsChannelPlaying(i))
            chanmask |= (1<<i);
    }

    u16 capmask = 0;
    if (IsCapturePlaying(0)) capmask |= (1<<0);
    if (IsCapturePlaying(1)) capmask |= (1<<1);

    NDS::ARM7Write16(SharedMem+0x08, chanmask);
    NDS::ARM7Write16(SharedMem+0x0A, capmask);
}


u16 UpdateCounter()
{
    Counter = (Counter * 0x19660D) + 0x3C6EF35F;
    return (Counter >> 16);
}


void Process(u32 param)
{
    if (param)
    {
        NDS::ScheduleEvent(NDS::Event_HLE_SoundCmd, true, 174592, Process, 1);
    }

    UpdateHardwareChannels();
    ProcessCommands();
    ProcessSequences(param!=0);
    UpdateChannels(param!=0);
    ReportHardwareStatus();
    UpdateCounter();
}


void OnIPCRequest(u32 data)
{
    if (data == 0)
    {
        // process pending command buffers

        Process(0);
    }
    else if (data >= 0x02000000)
    {
        // receiving a command buffer, add it to the queue

        CmdQueue.Write(data);
    }
}

}
}
}
