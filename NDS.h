
#ifndef NDS_H
#define NDS_H

#include "types.h"

namespace NDS
{

void Init();
void Reset();

u32 ARM9Read32(u32 addr);
u32 ARM7Read32(u32 addr);

template<typename T> T Read(u32 addr);
template<typename T> void Write(u32 addr, T val);

}

#endif // NDS_H
