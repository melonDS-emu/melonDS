#include <stdio.h>
#include "NDS.h"


namespace NDS
{

//


void Init()
{
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
        // load BIOS here

        fclose(f);
    }
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
