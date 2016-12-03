
#ifndef NDS_H
#define NDS_H

#include "types.h"

namespace NDS
{

extern u32 ARM9ITCMSize;
extern u32 ARM9DTCMBase, ARM9DTCMSize;

void Init();
void Reset();

void RunFrame();

void Halt();

void MapSharedWRAM();

u8 ARM9Read8(u32 addr);
u16 ARM9Read16(u32 addr);
u32 ARM9Read32(u32 addr);
void ARM9Write8(u32 addr, u8 val);
void ARM9Write16(u32 addr, u16 val);
void ARM9Write32(u32 addr, u32 val);

u8 ARM7Read8(u32 addr);
u16 ARM7Read16(u32 addr);
u32 ARM7Read32(u32 addr);
void ARM7Write8(u32 addr, u8 val);
void ARM7Write16(u32 addr, u16 val);
void ARM7Write32(u32 addr, u32 val);

}

#endif // NDS_H
