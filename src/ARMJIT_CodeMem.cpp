#include "ARMJIT_CodeMem.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <stdio.h>

#include <mutex>

namespace melonDS
{

namespace ARMJIT_CodeMem
{

std::mutex codeMemoryMutex;

static constexpr size_t NumCodeMemSlices = 4;

// I haven't heard of pages larger than 16 KB
alignas(16*1024) u8 CodeMemory[NumCodeMemSlices * CodeMemorySliceSize];

u32 AvailableCodeMemSlices = (1 << NumCodeMemSlices) - 1;

int RefCounter = 0;

void* Allocate()
{
    std::lock_guard guard(codeMemoryMutex);

    if (AvailableCodeMemSlices)
    {
        int slice = __builtin_ctz(AvailableCodeMemSlices);
        AvailableCodeMemSlices &= ~(1 << slice);
        //printf("allocating slice %d\n", slice);
        return &CodeMemory[slice * CodeMemorySliceSize];
    }

    // allocate
#ifdef _WIN32
    // FIXME
#else
    //printf("mmaping...\n");
    return mmap(NULL, CodeMemorySliceSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}

void Free(void* codeMem)
{
    std::lock_guard guard(codeMemoryMutex);

    for (int i = 0; i < NumCodeMemSlices; i++)
    {
        if (codeMem == &CodeMemory[CodeMemorySliceSize * i])
        {
            //printf("freeing slice\n");
            AvailableCodeMemSlices |= 1 << i;
            return;
        }
    }

#ifdef _WIN32
    // FIXME
#else
    munmap(codeMem, CodeMemorySliceSize);
#endif
}

void Init()
{
    std::lock_guard guard(codeMemoryMutex);

    RefCounter++;
    if (RefCounter == 1)
    {
        #ifdef _WIN32
            DWORD dummy;
            VirtualProtect(CodeMemory, sizeof(CodeMemory), PAGE_EXECUTE_READWRITE, &dummy);
        #elif defined(__APPLE__)
            // Apple always uses dynamic allocation
        #else
            mprotect(CodeMemory, sizeof(CodeMemory), PROT_EXEC | PROT_READ | PROT_WRITE);
        #endif
    }
}

}

}
