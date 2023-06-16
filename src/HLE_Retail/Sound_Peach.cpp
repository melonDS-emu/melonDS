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

#include "IPC.h"
#include "Sound_Peach.h"


namespace HLE
{
namespace Retail
{
namespace Sound_Peach
{

struct Note
{
    u32 NoteUnk00;
    u8 Volume;
    s8 NoteUnk05;
    u8 Pan;
    u8 NoteUnk07;
    u8 NoteUnk08;
    u8 NoteUnk09;
    u16 NoteUnk0A;
    u16 NoteUnk0C;
    u16 NoteUnk0E;
    u32 DataAddr;
    u32 NoteUnk14;
};

struct Sequence
{
    u16 ID;
    u8 SeqUnk02;
    u8 SeqUnk03;
    u8 Flags;
    u8 Volume;
    s8 SeqUnk06;
    s8 SeqUnk07;
    u8 VolRampLen1;
    u8 VolRampPos1;
    u8 VolRampLen2;
    u8 VolRampPos2;
    u8 VolMode1;
    u8 VolMode2;
    u8 Volume1;
    u8 Volume2;
    u8 SeqUnk10;
    u8 MaxNotes;
    u8 SeqUnk12;
    s8 SeqUnk13;
    Note* NoteBuffer;
    u32 SoundDataBuffer;
};

struct Channel
{
    u32 SourceData;
    Note* CurNote;
    Sequence* CurSequence;
    u8 CurVolume;
    u8 CurPan;
    s16 ChanUnk0E;
    u8 BaseVolume;
    u8 Status;
    u8 BasePan;
    u8 ID; // hardware channel #
    u8 ChanUnk14;
    s8 ChanUnk15;
    u16 ChanUnk16;
    u8 VolPhase;
    u8 VolStep;
    u8 PanPhase;
    u8 PanStep;
    u8 VolLength;
    u8 PanLength;
    u16 ChanUnk1E;
};

const u16 FMTable[97] =
{
    0xFFFF,
    0xFE29, 0xFC54, 0xFA84, 0xF8B6, 0xF6EC, 0xF525, 0xF362, 0xF1A2,
    0xEFE5, 0xEE2B, 0xEC74, 0xEAC1, 0xE910, 0xE763, 0xE5B9, 0xE412,
    0xE26E, 0xE0CD, 0xDF2F, 0xDD94, 0xDBFC, 0xDA67, 0xD8D4, 0xD745,
    0xD5B9, 0xD42F, 0xD2A8, 0xD124, 0xCFA3, 0xCE25, 0xCCA9, 0xCB30,
    0xC9BA, 0xC846, 0xC6D5, 0xC567, 0xC3FC, 0xC293, 0xC12C, 0xBFC9,
    0xBE67, 0xBD09, 0xBBAC, 0xBA53, 0xB8FC, 0xB7A7, 0xB655, 0xB505,
    0xB3B8, 0xB26D, 0xB124, 0xAFDE, 0xAE9A, 0xAD58, 0xAC19, 0xAADC,
    0xA9A1, 0xA869, 0xA733, 0xA5FF, 0xA4CD, 0xA39E, 0xA270, 0xA145,
    0xA01C, 0x9EF5, 0x9DD0, 0x9CAE, 0x9B8D, 0x9A6F, 0x9952, 0x9838,
    0x9720, 0x9609, 0x94F5, 0x93E3, 0x92D2, 0x91C4, 0x90B7, 0x8FAD,
    0x8EA4, 0x8D9E, 0x8C99, 0x8B96, 0x8A95, 0x8995, 0x8898, 0x879C,
    0x86A3, 0x85AB, 0x84B5, 0x83C0, 0x82CE, 0x81DD, 0x80ED, 0x8000
};

bool Inited;
u32 SharedMem;
u32 IPCCmd;
u32 CaptureVolume;

u32 SoundBuffer[2];

Note NoteBuffer[64];
Sequence Sequences[16];
Channel Channels[16];
const int NumSequences = 16;
int NumChannels;
int NumPSGChannels, NumNoiseChannels;

u32 CommandBuffers[2][0x40];
int CurCommandBuffer;

u32 Cmd3Val;
u32 Cmd5Val;
u32 Cmd10Val;

u16 Stat[2];


void Reset()
{
    Inited = false;
    SharedMem = 0;
    IPCCmd = 0;
    CaptureVolume = 0;

    SoundBuffer[0] = 0;
    SoundBuffer[1] = 0;

    memset(NoteBuffer, 0, sizeof(NoteBuffer));
    memset(Sequences, 0, sizeof(Sequences));
    memset(Channels, 0, sizeof(Channels));
    NumChannels = 16;
    NumPSGChannels = 0;
    NumNoiseChannels = 0;

    memset(CommandBuffers, 0, sizeof(CommandBuffers));
    CurCommandBuffer = 0;

    Cmd3Val = 0;
    Cmd5Val = 0;
    Cmd10Val = 0;

    Stat[0] = 0;
    Stat[1] = 0;

    NDS::ScheduleEvent(NDS::Event_HLE_SoundCmd, true, 262144, Process, 0);
}


void ResetChannel(Channel* chan)
{
    u32 addr = 0x04000400 + (chan->ID * 0x10);

    SPU::Write32(addr+0x00, 0);
    SPU::Write16(addr+0x08, 0);
    SPU::Write32(addr+0x04, 0);
    SPU::Write32(addr+0x0C, 0);
    SPU::Write16(addr+0x0A, 0);
}


void RunSequenceCommand(Sequence* seq, u32 seqentry, int type);

void CmdDummy(u32 param) {}

void PlaySequence(u32 param)
{
    u32 seqid = param >> 22;
    u32 val = (param >> 16) & 0x3F;

    if (val != 0 && seqid == 0x1E)
    {
        u16 modul = FMTable[Stat[0]];
        seqid += (modul & 0x3);

        Stat[0]++;
        if (Stat[0] > 96)
            Stat[0] = 0;
    }

    if (!seqid) return;

    u32 soundbuf = SoundBuffer[val ? 1:0];
    if (seqid > NDS::ARM7Read32(soundbuf + 0x10)) return;

    u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);
    u32 seqentry = seqtable + ((seqid-1) * 8);
    u32 seqdata = NDS::ARM7Read32(seqentry);
    u32 seqflags = NDS::ARM7Read32(seqentry + 0x4);

    if (!seqdata) return;
    seqdata += soundbuf;

    Sequence* seq;
    u32 sharedmem;

    if (val == 9 && seqid > 0x96 && seqid < 0x9F)
    {
        printf("BIKI\n");
        // TODO!!
    }
    else
    {
        u32 seqnum = seqflags & 0xF;
        seq = &Sequences[seqnum];
        sharedmem = SharedMem + 0x87C00 + (seqnum * 16);
    }

    u32 tmp = (seqflags >> 14) & 0x3F;
    if (tmp == 0x3F)
    {
        u32 chk = (seqflags >> 11) & 0x7;
        if (((seq->Flags & 0xF) != 0) && (seq->SeqUnk10 >= chk))
            return;
    }
    else
    {
        u32 chk = (seqflags >> 11) & 0x7;
        if (((seq->Flags & 0xF) != 0) && (seq->SeqUnk10 > chk))
            return;
    }

    tmp = (seq->Flags & 0xF) - 1;
    if (tmp <= 1)
    {
        seq->Flags &= ~0xF;
        u32 oldoffset = seqtable + ((seq->ID-1) * 8);
        RunSequenceCommand(seq, oldoffset, 1);
    }

    for (int i = 0; i < NumChannels; i++)
    {
        Channel* chan = &Channels[i];

        if (chan->CurSequence == seq)
            chan->Status = 0;
    }

    memset(seq->NoteBuffer, 0, sizeof(Note)*seq->MaxNotes);

    u32 numnotes = NDS::ARM7Read32(seqdata);
    if (numnotes > seq->MaxNotes)
        numnotes = seq->MaxNotes;

    for (int i = 0; i < numnotes; i++)
    {
        Note* note = &seq->NoteBuffer[i];

        note->DataAddr = soundbuf + NDS::ARM7Read32(seqdata + 4 + (i*4));
        note->Volume = 0x7F;
        note->Pan = 0x3F;
    }

    seq->ID = 0;
    seq->SeqUnk02 = 0;
    seq->SeqUnk03 = 0;
    seq->Flags = 0;
    seq->Volume = 0;
    seq->SeqUnk06 = 0;
    seq->SeqUnk07 = 0;
    seq->VolRampLen1 = 0;
    seq->VolRampPos1 = 0;
    seq->VolRampLen2 = 0;
    seq->VolRampPos2 = 0;
    seq->VolMode1 = 0;
    seq->VolMode2 = 0;
    seq->Volume1 = 0;
    seq->Volume2 = 0;

    seq->SeqUnk10 = (seqflags >> 11) & 0x7;
    seq->ID = seqid;
    seq->Flags |= 0x1;
    seq->Volume1 = (seqflags >> 4) & 0x7F;
    seq->SeqUnk02 = ((seqflags >> 14) & 0x3F) << 1;
    seq->VolMode1 = 2;
    seq->VolRampLen1 = (param >> 8) & 0xFF;
    seq->VolRampPos1 = (seq->Volume * seq->VolRampLen1) / seq->Volume1;
    seq->SeqUnk12 = val;
    seq->SoundDataBuffer = soundbuf;
    seq->SeqUnk13 = 0;
    if (seq->SeqUnk02 == 0x7E) seq->SeqUnk02 = 0;

    NDS::ARM7Write16(sharedmem + 0x2, seq->ID);
    NDS::ARM7Write16(sharedmem + 0xE, (seqflags >> 4) & 0x7F);
    NDS::ARM7Write16(sharedmem + 0x8, seq->SeqUnk10);
    NDS::ARM7Write16(sharedmem + 0x4, seq->SeqUnk12);

    if (seq == &Sequences[0])
    {
        Cmd5Val = 0;
    }

    RunSequenceCommand(seq, seqentry, 0);
}

void Cmd3(u32 param)
{
    u32 seqid = param >> 22;
    u32 val = (param >> 16) & 0x3F;

    if (val == 9 && seqid > 0x96 && seqid < 0x9F)
    {printf("BOUF\n");
        for (int i = 4; i < 7; i++)
        {
            Sequence* seq = &Sequences[i];
            seq->VolMode2 = 0;
            seq->VolRampLen2 = 0;
            seq->VolRampPos2 = 0;
            seq->VolMode1 = 1;
            seq->VolRampLen1 = 0;
            seq->VolRampPos1 = 0;
        }

        return;
    }

    u32 soundbuf = SoundBuffer[val ? 1:0];
    u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);
    u32 seqentry = seqtable + ((seqid-1) * 8);
    u32 seqflags = NDS::ARM7Read32(seqentry + 0x4);

    u32 nseq = seqflags & 0xF;
    Sequence* seq = &Sequences[nseq];
    if ((seq->Flags & 0xF) == 0) return;
    if (seq->SeqUnk12 != val) return;
    if (seq->ID != seqid) return;

    seq->VolMode2 = 0;
    seq->VolRampLen2 = 0;
    seq->VolRampPos2 = 0;
    seq->VolMode1 = 1;
    seq->VolRampLen1 = (param >> 8) & 0xFF;

    if (seq->Volume1 == 0)
        seq->VolRampPos1 = (-seq->Volume * seq->VolRampLen1);
    else
        seq->VolRampPos1 = (-seq->Volume * seq->VolRampLen1) / seq->Volume1;

    RunSequenceCommand(seq, seqentry, 1);
}

void Cmd4(u32 param)
{
    u32 seqid = param >> 22;
    u32 val = (param >> 16) & 0x3F;

    u32 soundbuf = SoundBuffer[val ? 1:0];
    u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);
    u32 seqentry = seqtable + ((seqid-1) * 8);
    u32 seqflags = NDS::ARM7Read32(seqentry + 0x4);

    u32 nseq = seqflags & 0xF;
    Sequence* seq = &Sequences[nseq];
    if ((seq->Flags & 0xF) != 1) return;
    if (seq->SeqUnk12 != val) return;
    if (seq->ID != seqid) return;
    if (seq->VolMode2 == 1) return;

    if (seq->VolMode2 == 0)
    {
        seq->Volume2 = seq->Volume;
    }

    seq->VolMode2 = 1;
    seq->VolRampLen2 = (param >> 8) & 0xFF;
    if (seq->Volume2 == 0)
        seq->VolRampPos2 = (seq->VolRampLen2 * (-seq->Volume));
    else
        seq->VolRampPos2 = (seq->VolRampLen2 * (-seq->Volume)) / seq->Volume2;
}

void Cmd5(u32 param)
{
    u32 seqid = param >> 22;
    u32 val = (param >> 16) & 0x3F;

    u32 soundbuf = SoundBuffer[val ? 1:0];
    u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);
    u32 seqentry = seqtable + ((seqid-1) * 8);
    u32 seqflags = NDS::ARM7Read32(seqentry + 0x4);

    u32 nseq = seqflags & 0xF;
    Sequence* seq = &Sequences[nseq];
    if (seq->SeqUnk12 != val) return;
    if (seq->ID != seqid) return;
    if (seq->VolMode2 == 2 || seq->VolMode2 == 0) return;

    seq->Flags &= ~0xF;
    seq->Flags |= 0x1;

    seq->VolMode2 = 2;
    seq->VolRampLen2 = (param >> 8) & 0xFF;
    if (seq->Volume2 == 0)
        seq->VolRampPos2 = (seq->VolRampLen2 * seq->Volume);
    else
        seq->VolRampPos2 = (seq->VolRampLen2 * seq->Volume) / seq->Volume2;
}

void Cmd7(u32 param)
{
    u16 mask = param >> 16;

    for (int i = 0; i < NumSequences; i++)
    {
        if (!(mask & (1<<i))) continue;

        Sequence* seq = &Sequences[i];

        u32 cmd3param = seq->ID << 22;
        cmd3param |= (param & 0xFF00);
        cmd3param |= (seq->SeqUnk12 << 16);
        Cmd3(cmd3param);
    }
}

void SeqSetVolume(u32 param)
{
    u8 val = (param >> 8) & 0xFF;
    u16 mask = param >> 16;

    for (int i = 0; i < NumSequences; i++)
    {
        if (mask & (1<<i))
            Sequences[i].Volume = val;
    }
}

void SeqSetUnk06(u32 param)
{
    u8 val = (param >> 8) & 0xFF;
    u16 mask = param >> 16;

    for (int i = 0; i < NumSequences; i++)
    {
        if (mask & (1<<i))
            Sequences[i].SeqUnk06 = val;
    }
}

void SeqSetUnk07(u32 param)
{
    u8 val = (param >> 8) & 0xFF;
    u16 mask = param >> 16;

    for (int i = 0; i < NumSequences; i++)
    {
        if (mask & (1<<i))
            Sequences[i].SeqUnk07 = val;
    }
}

void SeqSetUnk13(u32 param)
{
    u8 val = (param >> 8) & 0xFF;
    u16 mask = param >> 16;

    for (int i = 0; i < NumSequences; i++)
    {
        if (mask & (1<<i))
            Sequences[i].SeqUnk13 = val;
    }
}

void SetupChannels(u32 param)
{
    u32 num_psg = (param >> 8) & 0xFF;
    u32 num_noise = (param >> 16) & 0xFF;
    u32 ch = 0;

    printf("Sound_Peach: setting up channels, param=%08X\n", param);

    NumPSGChannels = num_psg;
    NumNoiseChannels = num_noise;

    for (u32 i = 0; i < num_psg; i++)
    {
        Channel* chan = &Channels[ch];
        chan->ID = 8 + i;
        chan->Status = 0;
        ch++;
    }

    for (u32 i = 0; i < num_noise; i++)
    {
        Channel* chan = &Channels[ch];
        chan->ID = 14 + i;
        chan->Status = 0;
        ch++;
    }

    {
        Channel* chan = &Channels[ch];
        chan->ID = 0;
        chan->Status = 0;
        ch++;

        chan = &Channels[ch];
        chan->ID = 2;
        chan->Status = 0;
        ch++;
    }

    for (u32 i = 0; i < 4; i++)
    {
        Channel* chan = &Channels[ch];
        chan->ID = 4 + i;
        chan->Status = 0;
        ch++;
    }

    for (u32 i = num_psg; i < 6; i++)
    {
        Channel* chan = &Channels[ch];
        chan->ID = 8 + i;
        chan->Status = 0;
        ch++;
    }

    for (u32 i = num_noise; i < 2; i++)
    {
        Channel* chan = &Channels[ch];
        chan->ID = 14 + i;
        chan->Status = 0;
        ch++;
    }

    {
        Channel* chan = &Channels[ch];
        chan->ID = 1;
        chan->Status = 0;
        ch++;

        chan = &Channels[ch];
        chan->ID = 3;
        chan->Status = 0;
        ch++;
    }
}

void SetupCapture(u32 param)
{
    u32 buf0 = SharedMem + 0x83C00;
    u32 buf1 = SharedMem + 0x85C00;

    printf("Sound_Peach: setting up capture, param=%08X\n", param);

    u32 volume = (param >> 16) & 0xFF;

    if (CaptureVolume && (!volume))
    {
        SPU::Write16(0x04000508, 0);
        NumChannels = 16;
    }

    if (volume && (!CaptureVolume))
    {
        for (u32 i = 0; i < 0x4000; i+=4)
            NDS::ARM7Write32(buf0+i, 0);

        NumChannels = 14;

        ResetChannel(&Channels[14]);
        ResetChannel(&Channels[15]);

        SPU::Write32(0x04000510, buf0);
        SPU::Write32(0x04000518, buf1);
        SPU::Write16(0x04000514, 0x600);
        SPU::Write16(0x0400051C, 0x600);

        SPU::Write32(0x04000414, buf0);
        SPU::Write32(0x04000434, buf1);
        SPU::Write32(0x0400041C, 0x600);
        SPU::Write32(0x0400043C, 0x600);
        SPU::Write16(0x0400041A, 0);
        SPU::Write16(0x0400043A, 0);
        SPU::Write16(0x04000418, 0xFC00);
        SPU::Write16(0x04000438, 0xFC00);
        SPU::Write8(0x04000412, 0x00);
        SPU::Write8(0x04000432, 0x7F);
        SPU::Write8(0x04000413, 0xA8);
        SPU::Write8(0x04000433, 0xA8);

        SPU::Write16(0x04000508, 0x8080);

        SPU::Write16(0x04000410, volume);
        SPU::Write16(0x04000430, volume);
    }

    CaptureVolume = volume;
}

void CmdUnk(u32 param)
{
    printf("PEACH UNK CMD %08X\n", param);
}

void (*CommandFuncs[16])(u32) =
{
    CmdDummy,
    PlaySequence,
    Cmd3,
    Cmd4,
    Cmd5,
    CmdUnk,
    Cmd7,
    CmdUnk,
    CmdUnk,
    CmdUnk,
    SeqSetVolume,
    SeqSetUnk06,
    SeqSetUnk07,
    SeqSetUnk13,
    SetupChannels,
    SetupCapture,
};


void RunSequenceCommand(Sequence* seq, u32 seqentry, int type)
{
    u32 offset = seq->SoundDataBuffer + 0xA54;
    u32 seqflags = NDS::ARM7Read32(seqentry + 0x4);

    int start;
    if (type) start = (seqflags >> 26) - 1;
    else      start = ((seqflags >> 20) & 0x3F) - 1;
    if (start < 0) return;

    u32 val = NDS::ARM7Read32(offset + (start*4));
    u32 cmd = val & 0xFF;
    if (cmd < 1 || cmd > 16) return;

    if (cmd >= 2 && cmd <= 5)
    {
        u32 tmp = (val >> 16) & 0x3F;
        if (tmp && tmp != Cmd3Val)
            return;
    }

    cmd--;
    CommandFuncs[cmd](val);
}

void RunCommands()
{
    u32* cmd = CommandBuffers[CurCommandBuffer];
    for (u32 i = 0; i < 0x40; i++)
    {
        u32 val = cmd[i];
        u8 cmdid = val & 0xFF;
        if (cmdid < 1 || cmdid > 16) break;

        cmdid--;
        CommandFuncs[cmdid](val);
    }

    cmd[0] = 0;

    SendIPCReply(0x7, 2);
    SendIPCReply(0x7, 0x10001);
}


void UpdateSequenceVolume(Sequence* seq)
{
    if (seq->VolMode2 == 1)
    {
        if (seq->Volume == 0) return;

        u8 vol = 0;
        if (seq->VolRampLen2 != 0)
        {
            u8 pos = seq->VolRampPos2;
            seq->VolRampPos2++;

            vol = (seq->Volume2 * (seq->VolRampLen2 - pos)) / seq->VolRampLen2;
        }
        if (vol == 0)
        {
            for (int i = 0; i < NumChannels; i++)
            {
                Channel* chan = &Channels[i];
                if (chan->CurSequence == seq)
                    chan->Status = 0;
            }

            seq->Flags &= ~0xF;
            seq->Flags |= 0x2;
        }
        seq->Volume = vol;
    }
    else if (seq->VolMode2 == 2)
    {
        if (seq->VolRampLen2 == 0)
        {
            seq->Volume = seq->Volume2;
        }
        else
        {
            u8 pos = seq->VolRampPos2;
            seq->VolRampPos2++;

            seq->Volume = (seq->Volume2 * pos) / seq->VolRampLen2;
        }

        if (seq->Volume < seq->Volume2) return;

        if (!seq->SeqUnk03)
        {
            seq->Volume = seq->Volume2;
            seq->VolMode2 = 0;
            return;
        }

        seq->Volume2 = seq->Volume1;
        seq->VolRampLen2 = seq->SeqUnk03;
        seq->VolMode2 = 3;
        seq->SeqUnk03 = 0;
    }
    else if (seq->VolMode2 == 3)
    {
        if (seq->VolMode1 != 0)
        {
            seq->VolMode2 = 0;
            return;
        }

        seq->Volume = (seq->Volume1 * seq->VolRampLen2) / 128;
    }
    else if (seq->VolMode1 == 1)
    {
        u8 vol = 0;
        if (seq->VolRampLen1 != 0)
        {
            u8 pos = seq->VolRampPos1;
            seq->VolRampPos1++;

            vol = (seq->Volume1 * (seq->VolRampLen1 - pos)) / seq->VolRampLen1;
        }
        if (vol == 0)
        {
            seq->VolMode1 = 0;

            for (int i = 0; i < seq->MaxNotes; i++)
            {
                seq->NoteBuffer[i].DataAddr = 0;
            }

            for (int i = 0; i < NumChannels; i++)
            {
                Channel* chan = &Channels[i];
                if (chan->CurSequence == seq)
                    chan->Status = 0;
            }

            seq->Flags &= ~0xF;
        }
        seq->Volume = vol;
    }
    else if (seq->VolMode1 == 2)
    {
        if (seq->VolRampLen1 == 0)
        {
            seq->Volume = seq->Volume1;
        }
        else
        {
            u8 pos = seq->VolRampPos1;
            seq->VolRampPos1++;

            seq->Volume = (seq->Volume1 * pos) / seq->VolRampLen1;
        }

        if (seq->Volume == seq->Volume1)
        {
            seq->VolMode1 = 0;
        }
    }
}

int NoteFM(Sequence* seq, Note* note)
{
    int num = seq->SeqUnk07 + 96;
    int div = num / 96;
    int mod = num % 96;

    int val = note->NoteUnk0A * FMTable[96 - mod];
    div = 16 - div;
    val >>= div;

    int div2 = val ? (160 / val) : 160;
    int coarse = div2 * val;
    coarse = 160 - coarse;

    for (;;)
    {
        note->NoteUnk0C += coarse;
        if ((int)note->NoteUnk0C >= val)
        {
            div2++;
            note->NoteUnk0C -= val;
        }

        note->NoteUnk09--;
        if (note->NoteUnk09 == 0) break;
        if (div2 != 0) break;
    }

    note->NoteUnk0E = div2 & 0xFF;
    return div2 & 0xFFFF;
}

void ChannelFM(Channel* chan)
{
    Note* note = chan->CurNote;
    Sequence* seq = chan->CurSequence;
    int unk0C = note->NoteUnk0C;

    int num = seq->SeqUnk07 + 96;
    int div = num / 96;
    int mod = num % 96;

    int val = note->NoteUnk0A * FMTable[96 - mod];
    div = 16 - div;
    val >>= div;

    if (chan->ChanUnk16 == 0)
    {
        if (chan->VolPhase == 0) return;
        if (chan->VolPhase == 5) return;
        chan->Status = 2;
        return;
    }

    int div2 = val ? (160 / val) : 160;
    int coarse = div2 * val;
    coarse = 160 - coarse;

    for (;;)
    {
        unk0C += coarse;
        if (unk0C >= val)
        {
            div2++;
            unk0C -= val;
        }

        chan->ChanUnk16--;
        if (chan->ChanUnk16 == 0) break;
        if (div2 != 0) break;
    }

    if (div2 == 0)
        chan->Status = 2;

    chan->ChanUnk14 = div2 & 0xFF;
}

void ProcessNotes(Sequence* seq)
{
    if (seq->SeqUnk02 != 0)
    {
        seq->SeqUnk02--;
        return;
    }

    if (seq->MaxNotes == 0)
    {
        u32 soundbuf = SoundBuffer[seq->SeqUnk12 ? 1:0];
        u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);
        u32 seqoffset = seqtable + ((seq->ID-1) * 8);

        seq->Flags &= ~0xF;

        RunSequenceCommand(seq, seqoffset, 1);
        return;
    }

    int firstchan = 0;
    int nprocessed = 0;
    int r10 = -1;
    u32 r4 = 0;

    for (int i = 0; i < seq->MaxNotes; i++)
    {
        r10++;

        Note* note = &seq->NoteBuffer[i];
        u32 note0 = note->NoteUnk00;
        u32 note8 = NDS::ARM7Read32(note0 + 8);
        if (!note->DataAddr) continue;

        r4 = (16 - nprocessed) + ((note->NoteUnk08 & 0x3) << 10) + (seq->SeqUnk10 << 5);

        nprocessed++;

        if (note->NoteUnk0E)
        {
            if (((note->NoteUnk08 >> 2) & 0x3) == 0)
                note->NoteUnk0E--;

            continue;
        }
        else if (note->NoteUnk09 != 0)
        {
            if (NoteFM(seq, note) != 0)
            {
                if (((note->NoteUnk08 >> 2) & 0x3) == 0)
                    note->NoteUnk0E--;

                continue;
            }
        }

        u32 notedata = note->DataAddr;
        bool note_end = false;
        for (;;)
        {
            u16 data = NDS::ARM7Read16(notedata);
            u8 cmd = data & 0xFF;
            u8 param = data >> 8;

            if ((cmd >= 0xF0) && (!note_end))
            {
                notedata += 2;

                switch (cmd)
                {
                case 0xF0:
                    note->NoteUnk0A = param;
                    break;
                case 0xF1:
                    {
                        for (int j = 0; j < NumChannels; j++)
                        {
                            Channel* chan = &Channels[j];
                            if (chan->CurNote == note)
                                chan->Status = 0;
                        }

                        u32 soundbuf;
                        u32 index = note->NoteUnk07;
                        if (index < 0x40)
                        {
                            soundbuf = SoundBuffer[0];
                        }
                        else
                        {
                            soundbuf = SoundBuffer[1];
                            index -= 0x40;
                        }

                        u32 offset = NDS::ARM7Read32(soundbuf + 0x4);

                        u32 tmp = param >> 1;
                        tmp += (index*64);
                        tmp = NDS::ARM7Read16(soundbuf + 0x14 + (tmp*2));

                        if (param & 0x01)
                            tmp >>= 8;
                        else
                            tmp &= 0xFF;

                        s32 stmp = ((s32)(tmp << 24)) >> 24;
                        if (stmp == -1)
                        {
                            SendIPCReply(0x7, 3);
                            SendIPCReply(0x7, 0x10001);

                            note->NoteUnk00 = 0;
                            note0 = 0;
                            break;
                        }

                        u32 noteaddr = soundbuf + NDS::ARM7Read32(soundbuf + offset + (index*4));
                        noteaddr += (stmp * 12);
                        note->NoteUnk00 = noteaddr;
                        note0 = noteaddr;
                    }
                    break;
                case 0xF2:
                    note->NoteUnk07 = param;
                    break;
                case 0xF3:
                    note->Pan = param;
                    break;
                case 0xF4:
                    note->NoteUnk08 &= ~0xC;
                    note->NoteUnk08 |= 0x4;
                    break;
                case 0xF5:
                    note->NoteUnk05 = param;
                    break;
                case 0xF6:
                    note->Volume = param;
                    break;
                case 0xF8:
                    note->NoteUnk14 = notedata;
                    break;
                case 0xF9:
                    notedata = note->NoteUnk14;
                    break;
                case 0xFA:
                    note->NoteUnk08 &= ~0x3;
                    note->NoteUnk08 |= 0x1;
                    break;
                case 0xFB:
                    note->NoteUnk08 &= ~0x3;
                    break;
                case 0xFC:
                    {
                        if (seq != &Sequences[0]) break;
                        if (Cmd10Val == 0) break;

                        u32 soundbuf = SoundBuffer[seq->SeqUnk12 ? 1:0];
                        u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);
                        u32 seqoffset = seqtable + ((param-1) * 8);

                        u32 seqdata = soundbuf + NDS::ARM7Read32(seqoffset);
                        seqdata += (r10*4);
                        notedata = soundbuf + NDS::ARM7Read32(seqdata+4);
                    }
                    break;
                case 0xFD:
                    {
                        if (seq != &Sequences[0]) break;
                        if (Cmd10Val != 0) break;

                        u32 soundbuf = SoundBuffer[seq->SeqUnk12 ? 1:0];
                        u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);
                        u32 seqoffset = seqtable + ((param-1) * 8);

                        u32 seqdata = soundbuf + NDS::ARM7Read32(seqoffset);
                        seqdata += (r10*4);
                        notedata = soundbuf + NDS::ARM7Read32(seqdata+4);
                    }
                    break;
                case 0xFF:
                    note->DataAddr = 0; // checkme
                    note_end = true;
                    break;
                }
            }
            else
            {
                if (note_end) break;

haxloop:
                data = NDS::ARM7Read16(notedata);
                if ((data & 0xFF) > 0x7F)
                {
haxloop2:
                    u16 param = NDS::ARM7Read16(notedata);
                    notedata += 2;
                    if ((param & 0xFF) == 0) continue;

                    note->NoteUnk09 = param >> 8;
                    note->NoteUnk0E = 0;

                    if (NoteFM(seq, note) == 0)
                        continue;

                    note->DataAddr = notedata;
                    if (((note->NoteUnk08 >> 2) & 0x3) == 0)
                        note->NoteUnk0E--;

                    break;
                }
                else
                {
                    if (!note0) break;

                    note8 = NDS::ARM7Read32(note->NoteUnk00 + 8);
                    if ((note8 & 0x7) == 7)
                    {
                        int idx = data & 0xFF;
                        idx -= 0x24;
                        if (idx < 0 || idx > 0x47)
                        {
                            SendIPCReply(0x7, 3);
                            SendIPCReply(0x7, 0x10001);
                            break;
                        }

                        int tmp = (note8 >> 3) & 0x7F;
                        u32 soundbuf;
                        if (tmp < 0x40)
                        {
                            soundbuf = SoundBuffer[0];
                        }
                        else
                        {
                            soundbuf = SoundBuffer[1];
                            tmp -= 0x40;
                        }

                        u32 offset = soundbuf + NDS::ARM7Read32(soundbuf + 0x8);
                        u32 tmp2 = (tmp * 0x24) + (idx >> 1);
                        u16 tmp3 = NDS::ARM7Read16(soundbuf + 0x814 + (tmp2*2));

                        if (idx & 0x1)
                            tmp3 >>= 8;
                        else
                            tmp3 &= 0xFF;

                        s32 stmp = ((s32)(tmp3 << 24)) >> 24;
                        if (stmp == -1)
                        {
                            SendIPCReply(0x7, 3);
                            SendIPCReply(0x7, 0x10001);
                            break;
                        }

                        u32 tmpoffset = soundbuf + NDS::ARM7Read32(offset + (tmp*4));
                        tmpoffset += (tmp3 * 12);
                        note0 = tmpoffset;
                        note8 = NDS::ARM7Read32(tmpoffset + 8);
                    }

                    int lastchan = 0;
                    int chtype = (note8 & 0x7);
ch_again:
                    switch (chtype)
                    {
                    case 1:
                        firstchan = 0;
                        lastchan = NumPSGChannels;
                        r4 += 0x8000;
                        break;
                    case 2:
                        firstchan = NumPSGChannels;
                        lastchan = firstchan + NumNoiseChannels;
                        r4 += 0x8000;
                        break;
                    case 3:
                        firstchan = NumPSGChannels + NumNoiseChannels;
                        lastchan = NumChannels;
                        break;
                    case 8:
                        firstchan = 0;
                        lastchan = NumPSGChannels + NumNoiseChannels;
                        break;
                    default:
                        printf("???? %d\n", chtype);
                        break;
                    }

                    // allocate a channel
                    Channel* chan = nullptr;
                    for (int j = firstchan; j < lastchan; j++)
                    {
                        Channel* ch = &Channels[j];
                        if (ch->Status != 0) continue;
                        chan = ch;
                        break;
                    }
                    if (!chan)
                    {
                        for (int j = firstchan; j < lastchan; j++)
                        {
                            Channel* ch = &Channels[j];
                            if (ch->Status >= 3) continue;
                            chan = ch;
                            break;
                        }
                    }
                    if (!chan)
                    {
                        u16 chk = 0xFFFF;
                        chan = &Channels[firstchan];
                        for (int j = firstchan; j < lastchan; j++)
                        {
                            Channel* ch = &Channels[j];
                            if (ch->ChanUnk1E < chk)
                            {
                                chan = ch;
                                chk = ch->ChanUnk1E;
                            }
                        }

                        if ((firstchan == lastchan) || (r4 < chan->ChanUnk1E))
                        {
                            if (chtype == 3)
                            {//printf("type3 channel alloc fail (%d->%d)\n", firstchan, lastchan);
                            //for (int j = 0; j < 16; j++) printf("%d:%d/%04X  ", j, Channels[j].Status, Channels[j].ChanUnk1E); printf("\n");
                            //printf("tried to go for: unk1E=%04X, r4=%04X\n", chan->ChanUnk1E, r4);
                                chtype = 8;
                                goto ch_again;
                            }
printf("channel alloc: fail (type=%d)\n", chtype);
                            goto next_cmd;
                        }
                    }

                    chan->CurNote = note;
                    chan->CurSequence = seq;
                    chan->ChanUnk1E = r4;
                    chan->Status = 4;
                    chan->ChanUnk15 = data & 0xFF;
                    chan->BaseVolume = data >> 8;

                    u16 data2 = NDS::ARM7Read16(notedata+2);
                    chan->ChanUnk16 = data2;
                    chan->ChanUnk14 = 0;
                    ChannelFM(chan);

                    notedata += 4;
                    goto haxloop;
                }

next_cmd:
                do
                {
                    data = NDS::ARM7Read16(notedata + 4);
                    notedata += 4;
                }
                while ((data & 0xFF) <= 0x7F);
                goto haxloop2;
            }
        }
    }

    if (!nprocessed)
    {
        u32 soundbuf = SoundBuffer[seq->SeqUnk12 ? 1:0];
        u32 seqtable = soundbuf + NDS::ARM7Read32(soundbuf + 0xC);

        seq->Flags &= ~0xF;

        u32 seqoffset = seqtable + ((seq->ID-1) * 8);
        RunSequenceCommand(seq, seqoffset, 1);
    }
}


u8 UpdateChannelVolume(Channel* chan, u32 seqdata)
{
    Note* note = chan->CurNote;
    Sequence* seq = chan->CurSequence;

    u32 vol_param = NDS::ARM7Read32(seqdata + 4);

    u32 step_vol = (seq->Volume * chan->BaseVolume * note->Volume) >> 14;
    u32 sus_vol = ((vol_param >> 19) & 0x3F) + 1;
    sus_vol = (step_vol * sus_vol) >> 6;

    if (chan->VolPhase == 0)
    {
        chan->Status = 0;
        return 0;
    }

    if (chan->VolPhase == 1)
    {
        // attack

        if (chan->VolStep == 0)
        {
            chan->VolLength = (vol_param >> 5) & 0x7F;
        }

        u8 step = chan->VolStep;
        if (step == chan->VolLength)
        {
            chan->VolPhase++;
            chan->VolStep = 0;
        }
        else
        {
            chan->VolStep++;
            if (chan->VolLength == 0) return (step_vol * step);
            return (step_vol * step) / chan->VolLength;
        }
    }

    if (chan->VolPhase == 2)
    {
        // decay

        if (chan->VolStep == 0)
        {
            chan->VolLength = (vol_param >> 12) & 0x7F;
        }

        u8 step = chan->VolStep;
        if (step == chan->VolLength)
        {
            chan->VolPhase++;
            chan->VolStep = 0;
        }
        else
        {
            chan->VolStep++;
            u32 ret;
            if (chan->VolLength == 0)
                ret = ((step_vol - sus_vol) * step);
            else
                ret = ((step_vol - sus_vol) * step) / chan->VolLength;
            return step_vol - ret;
        }
    }

    if (chan->VolPhase == 3)
    {
        // sustain

        return sus_vol;
    }

    if (chan->VolPhase == 5)
    {
        // release

        if (chan->VolStep == 0)
        {
            chan->PanLength = vol_param >> 25;
            chan->VolLength = chan->PanLength;
        }

        u8 step = chan->VolStep;
        if (step == chan->VolLength)
        {
            chan->VolStep = 0;
            chan->VolPhase = 0;
            chan->Status = 0;
            return 0;
        }
        else
        {
            chan->VolStep++;
            u32 ret;
            if (chan->VolLength == 0)
                ret = (sus_vol * step);
            else
                ret = (sus_vol * step) / chan->VolLength;
            return sus_vol - ret;
        }
    }

    return 0;
}

s16 UpdateChannelPan(Channel* chan, u32 seqdata, u32 r2)
{
    Note* note = chan->CurNote;
    Sequence* seq = chan->CurSequence;

    u32 vol_param = NDS::ARM7Read32(seqdata + 4);
    int pan_param = NDS::ARM7Read32(seqdata + 8);

    u32 idx;
    u32 soundbuf;
    if ((pan_param & 0x7) == 7)
    {
        idx = (pan_param >> 3) & 0x7F;
        soundbuf = SoundBuffer[(idx<0x40) ? 0:1];
    }
    else
    {
        idx = note->NoteUnk07;
        soundbuf = SoundBuffer[(idx<0x40) ? 0:1];
    }

    soundbuf += 0xB54;
    u32 r4;
    if ((pan_param & 0x7) == 3)
    {
        u16 val = NDS::ARM7Read16(chan->SourceData + 2);
        r4 = (r2 - val) << 3;
    }
    else
    {
        r4 = (r2 - 0x40) << 3;
    }

    int xx = seq->SeqUnk06 << 1;
    int yy = vol_param & 0x1F;
    int r1 = note->NoteUnk05 * yy;
    xx += (r1 >> 4);
    r4 += xx;

    int zz = (pan_param << 15) >> 25;
    int var20;
    if (zz < 0 || zz > 15)
    {
        var20 = 0;
    }
    else
    {
        var20 = NDS::ARM7Read32(soundbuf + (zz*4));
    }

    if (chan->PanPhase == 1)
    {
        // attack

        if (chan->PanStep == 0)
        {
            chan->BasePan = 0;
            chan->PanLength = var20 & 0x3F;

            note->NoteUnk08 &= ~0xF0;

            if (Cmd5Val == 1)
            {
                Stat[0]++;
                if (Stat[0] > 96)
                    Stat[0] = 0;
            }
        }

        u8 step = chan->PanStep;
        if (step == chan->PanLength)
        {
            chan->PanPhase++;
            chan->PanStep = 0;
        }
        else
        {
            chan->PanStep++;

            int factor = (var20 << 21) >> 27;
            if (chan->PanLength == 0)
                r4 += ((chan->PanLength - step) * factor);
            else
                r4 += (((chan->PanLength - step) * factor) / chan->PanLength);
            goto ret;
        }
    }

    if (chan->PanPhase == 2)
    {
        // decay

        if (chan->PanStep == 0)
        {
            chan->BasePan = 0;
            chan->PanLength = (var20 >> 11) & 0x3F;
        }

        u8 step = chan->PanStep;
        if (step == chan->PanLength)
        {
            chan->PanPhase++;
            chan->PanStep = 0;
        }
        else
        {
            chan->PanStep++;
            goto ret;
        }
    }

    if (chan->PanPhase == 3)
    {
        // sustain

        int n = (var20 >> 17) & 0xF;
        if (n == 0)
        {
            chan->PanStep = 0;
            chan->PanPhase = 0;
            goto ret;
        }

        u8 step = chan->PanStep;
        if (step == n)
        {
            chan->BasePan++;
            chan->BasePan &= 0x3;
            chan->PanStep = 0;
        }

        if ((chan->BasePan & 0x1) == 0)
        {
            step = chan->PanStep;
            chan->PanStep++;
        }
        else
        {
            step = chan->PanStep;
            chan->PanStep++;
            step = n - step;
        }

        int factor = (var20 >> 21) & 0x3F;
        int ret;
        if (n == 0)
            ret = (factor * step);
        else
            ret = (factor * step) / n;
        int tmp = 1 - (chan->BasePan & 0x2);
        r4 += ret * tmp;
        goto ret;
    }

    if (chan->PanPhase == 5)
    {
        // release

        u8 step = chan->PanStep;
        if (step == chan->PanLength)
        {
            chan->PanStep = 0;
            chan->PanPhase = 0;
            goto ret;
        }
        else
        {
            chan->PanStep++;

            int factor = var20 >> 27;
            if (chan->PanLength == 0)
                r4 += (step * factor);
            else
                r4 += ((step * factor) / chan->PanLength);
            goto ret;
        }
    }

ret:
    if (seq == &Sequences[0])
    {
        r4 += (s16)Stat[1];
        r4 += (note->NoteUnk08 >> 4);
    }

    return (r4 << 16) >> 16;
}

void UpdateChannelRegs(Channel* chan, u32 param, bool restart)
{
    int unk15 = chan->ChanUnk0E;
    int lr;
    if (unk15 < 0)
    {
        lr = 16;
        while (unk15 < 0)
        {
            lr--;
            unk15 += 96;
        }
    }
    else
    {
        lr = 15;
        while (unk15 >= 0)
        {
            lr++;
            unk15 -= 96;
        }
        unk15 += 96;
    }

    int freq;
    if ((param & 0x3) == 3)
        freq = NDS::ARM7Read16(chan->SourceData + 0x4);
    else
        freq = 0x18DA;

    u32 addr = 0x04000400 + (chan->ID * 16);

    SPU::Write8(addr+0x00, chan->CurVolume);

    freq *= FMTable[unk15];
    freq >>= lr;
    freq = (0x10000 - freq) & 0xFFFC;
    SPU::Write16(addr+0x08, freq);

    SPU::Write8(addr+0x02, chan->CurPan);

    if (!restart) return;

    SPU::Write8(addr+0x03, 0);

    if ((param & 0x3) != 3)
    {
        param = (param >> 3) & 0x7F;
        param |= 0xE0;
        SPU::Write8(addr+0x03, param);
        return;
    }

    SPU::Write32(addr+0x04, chan->SourceData + 0xA);
    SPU::Write32(addr+0x0C, NDS::ARM7Read32(chan->SourceData + 0x8));
    SPU::Write16(addr+0x0A, NDS::ARM7Read16(chan->SourceData + 0x6));

    u16 chanparam = NDS::ARM7Read16(chan->SourceData);
    SPU::Write8(addr+0x03, (chanparam >> 8) | 0x80);
}

void UpdateChannel(Channel* chan)
{
    u32 param = NDS::ARM7Read32(chan->CurNote->NoteUnk00 + 8);

    u32 soundbuf;
    u32 seqdata;
    int unk15;
    int r8;
    int pan;
    bool restart = false;
    if ((param & 0x7) == 7)
    {
        u32 idx = (param >> 3) & 0x7F;
        if (idx < 0x40)
        {
            soundbuf = SoundBuffer[0];
        }
        else
        {
            soundbuf = SoundBuffer[1];
            idx -= 0x40;
        }

        seqdata = soundbuf + NDS::ARM7Read32(soundbuf + 0x8);
        unk15 = chan->ChanUnk15 - 0x24;

        int idxoffset = (idx * 0x24) + (unk15 >> 1);
        u16 val = NDS::ARM7Read16(soundbuf + 0x814 + (idxoffset*2));
        if (unk15 & 1)
            idxoffset = val >> 8;
        else
            idxoffset = val & 0xFF;

        u32 ptr = soundbuf + NDS::ARM7Read32(seqdata + (idx*4));
        ptr += (idxoffset * 12);
        seqdata = ptr;

        param = NDS::ARM7Read32(ptr + 8);

        int tmp = ((int)param << 7) >> 24;
        if (tmp < 0)
        {
            r8 = chan->ChanUnk15;
            pan = chan->CurNote->Pan + chan->CurSequence->SeqUnk13;
        }
        else
        {
            r8 = (tmp << 16) >> 16;
            pan = chan->CurSequence->SeqUnk13 + (param >> 25);
        }
    }
    else
    {
        u32 idx = chan->CurNote->NoteUnk07;
        seqdata = chan->CurNote->NoteUnk00;
        if (idx < 0x40)
        {
            soundbuf = SoundBuffer[0];
        }
        else
        {
            soundbuf = SoundBuffer[1];
        }

        // param

        r8 = chan->ChanUnk15;
        pan = chan->CurNote->Pan + chan->CurSequence->SeqUnk13;
    }

    if (pan < 0) pan = 0;
    if (pan > 127) pan = 127;
    chan->CurPan = pan;

    if (chan->Status == 2)
    {
        chan->Status = 1;
        chan->PanPhase = 5;
        chan->VolPhase = 5;
        chan->PanStep = 0;
        chan->VolStep = 0;
    }
    else if (chan->Status == 4)
    {
        chan->Status = 3;
        chan->PanPhase = 1;
        chan->VolPhase = 1;
        chan->PanStep = 0;
        chan->VolStep = 0;

        chan->SourceData = soundbuf + NDS::ARM7Read32(seqdata);
        restart = true;
    }

    if (chan->ChanUnk14)
    {
        int tmp = (chan->CurNote->NoteUnk08 >> 2) & 0x3;
        if (!tmp)
        {
            chan->ChanUnk14--;
            if (chan->ChanUnk14 == 0)
                ChannelFM(chan);
        }
    }

    chan->CurVolume = UpdateChannelVolume(chan, seqdata);
    chan->ChanUnk0E = UpdateChannelPan(chan, seqdata, r8);
    UpdateChannelRegs(chan, param, restart);
}


void Process(u32 param)
{
    NDS::ScheduleEvent(NDS::Event_HLE_SoundCmd, true, 262144, Process, 0);

    if (!SharedMem) return;
    if (!Inited) return;

    RunCommands();

    u32 seqstatus = SharedMem + 0x87C00;
    for (int i = 0; i < NumSequences; i++)
    {
        Sequence* seq = &Sequences[i];

        UpdateSequenceVolume(seq);

        if ((seq->Flags & 0xF) == 1)
        {
            ProcessNotes(seq);
        }

        NDS::ARM7Write16(seqstatus, seq->Flags & 0xF);
        NDS::ARM7Write16(seqstatus + 0x6, seq->Volume);
        NDS::ARM7Write16(seqstatus + 0xA, (u16)(s16)seq->SeqUnk06);
        NDS::ARM7Write16(seqstatus + 0xC, (u16)(s16)seq->SeqUnk07);
        seqstatus += 0x10;
    }

    for (int i = 0; i < NumChannels; i++)
    {
        Channel* chan = &Channels[i];

        if (chan->Status == 0)
        {
            ResetChannel(chan);
        }
        else
        {
            UpdateChannel(chan);
        }
    }

    if (Cmd5Val == 2)
    {
        s16 tbl[8] = {0, 6, 12, 6, 0, -6, -12, -6};
        Stat[1] = tbl[Stat[0] & 0x7];
        Stat[0]++;
        if (Stat[0] > 96)
            Stat[0] = 0;
    }
    else if (Cmd5Val == 3)
    {
        Stat[1] = (Stat[0] & 0x1) ? 24 : -24;
        Stat[0]++;
        if (Stat[0] > 96)
            Stat[0] = 0;
    }
    else
    {
        Stat[1] = 0;
    }
}


void OnIPCRequest(u32 data)
{
    if (IPCCmd == 0 || data < 0x10000)
    {
        IPCCmd = data;
        return;
    }

    switch (IPCCmd)
    {
    case 1: // set work buffer
        SharedMem = data;
        break;

    case 2: // init
        {
            NDS::ARM7IOWrite16(0x04000304, 0x0001);
            SPU::Write16(0x04000500, 0x807F);
            SPU::Write16(0x04000508, 0);
            for (u32 i = 0; i < 16; i++)
                SPU::Write32(0x04000400 + (i*16), 0);

            SoundBuffer[0] = SharedMem + NDS::ARM7Read32(SharedMem + 0x14);
            SoundBuffer[1] = SoundBuffer[0] + NDS::ARM7Read32(SharedMem + 0x18);

            int numnotes[16] = {14, 10, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0};
            int n = 0;
            for (int i = 0; i < 16; i++)
            {
                Sequences[i].NoteBuffer = &NoteBuffer[n];
                Sequences[i].MaxNotes = numnotes[i];
                n += numnotes[i];
            }

            u16 capvol = NDS::ARM7Read16(SharedMem + 0x6) & 0xFF;
            SetupCapture(capvol); // TODO bugged??

            u16 chans = NDS::ARM7Read16(SharedMem + 0xE);
            u32 chan_param = ((chans & 0x7) << 8) | (((chans >> 3) & 0x3) << 16);
            SetupChannels(chan_param);

            Inited = true;

            SendIPCReply(0x7, 4);
            SendIPCReply(0x7, 0x10001);
        }
        break;

    case 3: //
        {
            Cmd3Val = data & 0xFF;

            for (int i = 0; i < NumSequences; i++)
            {
                Sequence* seq = &Sequences[i];

                if (seq->SeqUnk12 == 0)
                {
                    if (seq->ID <= 0x64)
                        continue;
                }

                u32 cmd3param = (seq->ID << 22) | (Cmd3Val << 16);
                Cmd3(cmd3param);
            }

            SendIPCReply(0x7, 1);
            SendIPCReply(0x7, 0x10001);
        }
        break;

    case 4: // submit command buffer
        {
            u32* dst = CommandBuffers[CurCommandBuffer];
            for (u32 i = 0; i < 0x40; i++)
                dst[i] = NDS::ARM7Read32(data+(i*4));

            CurCommandBuffer ^= 1;
        }
        break;

    case 5:
        Cmd5Val = data & 0xFF;
        break;

    case 10:
        Cmd10Val = data & 0xFF;
        break;

    default:
        printf("Sound_Peach: unknown IPC command %08X %08X\n", IPCCmd, data);
        break;
    }

    IPCCmd = 0;
}

}
}
}
