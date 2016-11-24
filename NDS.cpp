#include <stdio.h>
#include "NDS.h"
#include "ARM.h"


namespace NDS
{

ARM* ARM9;
ARM* ARM7;

s32 ARM9Cycles, ARM7Cycles;

u8 ARM9BIOS[0x1000];
u8 ARM7BIOS[0x4000];

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

    ARM9->Reset();
    ARM7->Reset();

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


u32 ARM9Read32(u32 addr)
{
    // implement ARM9y shit here, like memory protection

    if ((addr & 0xFFFFF000) == 0xFFFF0000)
    {
        return *(u32*)&ARM9BIOS[addr & 0xFFF];
    }

    printf("unknown arm9 read32 %08X\n", addr);
    return 0;
}

u32 ARM7Read32(u32 addr)
{
    if (addr < 0x00004000)
    {
        return *(u32*)&ARM7BIOS[addr];
    }

    printf("unknown arm7 read32 %08X\n", addr);
    return 0;
}

template<typename T> T Read(u32 addr)
{
    return (T)0;
}

template<typename T> void Write(u32 addr, T val)
{
    //
}

}
