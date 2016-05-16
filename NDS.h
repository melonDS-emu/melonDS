
#ifndef NDS_H
#define NDS_H

#include "types.h"

namespace NDS
{

void Init();
void Reset();

template<typename T> T Read(u32 addr);
template<typename T> void Write(u32 addr, T val);

}

#endif // NDS_H
