
#ifndef CP15_H
#define CP15_H

namespace CP15
{

void Reset();

void Write(u32 id, u32 val);
u32 Read(u32 id);

}

#endif
