#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "ARM.h"
#include "CP15.h"


namespace NDS
{

ARM* ARM9;
ARM* ARM7;

s32 ARM9Cycles, ARM7Cycles;

u8 ARM9BIOS[0x1000];
u8 ARM7BIOS[0x4000];

u8 MainRAM[0x400000];

u8 ARM7WRAM[0x10000];

u8 ARM9ITCM[0x8000];
u32 ARM9ITCMSize;
u8 ARM9DTCM[0x4000];
u32 ARM9DTCMBase, ARM9DTCMSize;

// IO shit
u16 IPCSync9, IPCSync7;

bool Running;


void Init()
{
    ARM9 = new ARM(0);
    ARM7 = new ARM(1);

    Reset();
}

void Reset()
{
    FILE* f;

    f = fopen("bios9.bin", "rb");
    if (!f)
        printf("ARM9 BIOS not found\n");
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM9BIOS, 0x1000, 1, f);

        printf("ARM9 BIOS loaded: %08X\n", ARM9Read32(0xFFFF0000));
        fclose(f);
    }

    f = fopen("bios7.bin", "rb");
    if (!f)
        printf("ARM7 BIOS not found\n");
    else
    {
        fseek(f, 0, SEEK_SET);
        fread(ARM7BIOS, 0x4000, 1, f);

        printf("ARM7 BIOS loaded: %08X\n", ARM7Read32(0x00000000));
        fclose(f);
    }

    memset(MainRAM, 0, 0x400000);
    memset(ARM7WRAM, 0, 0x10000);
    memset(ARM9ITCM, 0, 0x8000);
    memset(ARM9DTCM, 0, 0x4000);

    ARM9ITCMSize = 0;
    ARM9DTCMBase = 0xFFFFFFFF;
    ARM9DTCMSize = 0;

    IPCSync9 = 0;
    IPCSync7 = 0;

    ARM9->Reset();
    ARM7->Reset();
    CP15::Reset();

    ARM9Cycles = 0;
    ARM7Cycles = 0;

    Running = true; // hax
}


void RunFrame()
{
    s32 framecycles = 560190<<1;

    // very gross and temp. loop

    while (Running && framecycles>0)
    {
        ARM9Cycles = ARM9->Execute(32 + ARM9Cycles);
        ARM7Cycles = ARM7->Execute(16 + ARM7Cycles);

        framecycles -= 32;
    }
}


void Halt()
{
    Running = false;
}



u8 ARM9Read8(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u8*)&ARM9BIOS[addr & 0xFFF];
    }
    if (addr < ARM9ITCMSize)
    {
        return *(u8*)&ARM9ITCM[addr & 0x7FFF];
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        return *(u8*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u8*)&MainRAM[addr & 0x3FFFFF];
    }

    printf("unknown arm9 read8 %08X\n", addr);
    return 0;
}

u16 ARM9Read16(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u16*)&ARM9BIOS[addr & 0xFFF];
    }
    if (addr < ARM9ITCMSize)
    {
        return *(u16*)&ARM9ITCM[addr & 0x7FFF];
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        return *(u16*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u16*)&MainRAM[addr & 0x3FFFFF];

    case 0x04000000:
        switch (addr)
        {
        case 0x04000180: return IPCSync9;
        }
    }

    printf("unknown arm9 read16 %08X\n", addr);
    return 0;
}

u32 ARM9Read32(u32 addr)
{
    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u32*)&ARM9BIOS[addr & 0xFFF];
    }
    if (addr < ARM9ITCMSize)
    {
        return *(u32*)&ARM9ITCM[addr & 0x7FFF];
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        return *(u32*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        return *(u32*)&MainRAM[addr & 0x3FFFFF];
    }

    printf("unknown arm9 read32 %08X | %08X\n", addr, ARM9->R[15]);
    return 0;
}

void ARM9Write8(u32 addr, u8 val)
{
    if (addr < ARM9ITCMSize)
    {
        *(u8*)&ARM9ITCM[addr & 0x7FFF] = val;
        return;
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        *(u8*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF] = val;
        return;
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u8*)&MainRAM[addr & 0x3FFFFF] = val;
        return;
    }

    printf("unknown arm9 write8 %08X %02X\n", addr, val);
}

void ARM9Write16(u32 addr, u16 val)
{
    if (addr < ARM9ITCMSize)
    {
        *(u16*)&ARM9ITCM[addr & 0x7FFF] = val;
        return;
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        *(u16*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF] = val;
        return;
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u16*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000180:
            IPCSync7 &= 0xFFF0;
            IPCSync7 |= ((val & 0x0F00) >> 8);
            IPCSync9 &= 0xB0FF;
            IPCSync9 |= (val & 0x4F00);
            if ((val & 0x2000) && (IPCSync7 & 0x4000))
            {
                printf("ARM9 IPCSYNC IRQ TODO\n");
            }
            return;
        }
    }

    printf("unknown arm9 write16 %08X %04X\n", addr, val);
}

void ARM9Write32(u32 addr, u32 val)
{
    if (addr < ARM9ITCMSize)
    {
        *(u32*)&ARM9ITCM[addr & 0x7FFF] = val;
        return;
    }
    if (addr >= ARM9DTCMBase && addr < (ARM9DTCMBase + ARM9DTCMSize))
    {
        *(u32*)&ARM9DTCM[(addr - ARM9DTCMBase) & 0x3FFF] = val;
        return;
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        *(u32*)&MainRAM[addr & 0x3FFFFF] = val;
        return;
    }

    printf("unknown arm9 write32 %08X %08X | %08X\n", addr, val, ARM9->R[15]);
}



u8 ARM7Read8(u32 addr)
{
    if (addr < 0x00004000)
    {
        return *(u8*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        return *(u8*)&MainRAM[addr & 0x3FFFFF];

    case 0x03800000:
        return *(u8*)&ARM7WRAM[addr & 0xFFFF];
    }

    printf("unknown arm7 read8 %08X\n", addr);
    return 0;
}

u16 ARM7Read16(u32 addr)
{
    if (addr < 0x00004000)
    {
        return *(u16*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        return *(u16*)&MainRAM[addr & 0x3FFFFF];

    case 0x03800000:
        return *(u16*)&ARM7WRAM[addr & 0xFFFF];

    case 0x04000000:
        switch (addr)
        {
        case 0x04000180: return IPCSync7;
        }
    }

    printf("unknown arm7 read16 %08X\n", addr);
    return 0;
}

u32 ARM7Read32(u32 addr)
{
    if (addr < 0x00004000)
    {
        return *(u32*)&ARM7BIOS[addr];
    }

    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        return *(u32*)&MainRAM[addr & 0x3FFFFF];

    case 0x03800000:
        return *(u32*)&ARM7WRAM[addr & 0xFFFF];
    }
if ((addr&0xFF000000) == 0xEA000000) Halt();
    printf("unknown arm7 read32 %08X | %08X\n", addr, ARM7->R[15]);
    return 0;
}

void ARM7Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        *(u8*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03800000:
        *(u8*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;
    }

    printf("unknown arm7 write8 %08X %02X\n", addr, val);
}

void ARM7Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        *(u16*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03800000:
        *(u16*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;

    case 0x04000000:
        switch (addr)
        {
        case 0x04000180:
            IPCSync9 &= 0xFFF0;
            IPCSync9 |= ((val & 0x0F00) >> 8);
            IPCSync7 &= 0xB0FF;
            IPCSync7 |= (val & 0x4F00);
            if ((val & 0x2000) && (IPCSync9 & 0x4000))
            {
                printf("ARM7 IPCSYNC IRQ TODO\n");
            }
            return;
        }
    }

    printf("unknown arm7 write16 %08X %04X\n", addr, val);
}

void ARM7Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
        *(u32*)&MainRAM[addr & 0x3FFFFF] = val;
        return;

    case 0x03800000:
        *(u32*)&ARM7WRAM[addr & 0xFFFF] = val;
        return;
    }

    printf("unknown arm7 write32 %08X %08X\n", addr, val);
}

}
