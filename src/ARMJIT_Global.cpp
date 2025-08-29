#include "ARMJIT_Global.h"
#include "ARMJIT_Memory.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdint.h>

#include <mutex>

namespace melonDS
{

namespace ARMJIT_Global
{

std::mutex globalMutex;

#if defined(__APPLE__) && defined(__aarch64__)
#define APPLE_AARCH64
#endif

#if !defined(APPLE_AARCH64) && !defined(__NetBSD__) && !defined(__OpenBSD__)
static constexpr size_t NumCodeMemSlices = 4;
static constexpr size_t CodeMemoryAlignedSize = NumCodeMemSlices * CodeMemorySliceSize;

// I haven't heard of pages larger than 16 KB
u8 CodeMemory[CodeMemoryAlignedSize + 16*1024];

u32 AvailableCodeMemSlices = (1 << NumCodeMemSlices) - 1;

u8* GetAlignedCodeMemoryStart()
{
    return reinterpret_cast<u8*>((reinterpret_cast<intptr_t>(CodeMemory) + (16*1024-1)) & ~static_cast<intptr_t>(16*1024-1));
}
#endif

int RefCounter = 0;

void* AllocateCodeMem()
{
    std::lock_guard guard(globalMutex);

#if !defined(APPLE_AARCH64) && !defined(__NetBSD__) && !defined(__OpenBSD__)
    if (AvailableCodeMemSlices)
    {
        int slice = __builtin_ctz(AvailableCodeMemSlices);
        AvailableCodeMemSlices &= ~(1 << slice);
        //printf("allocating slice %d\n", slice);
        return &GetAlignedCodeMemoryStart()[slice * CodeMemorySliceSize];
    }
#endif

    // allocate
#ifdef _WIN32
    return VirtualAlloc(nullptr, CodeMemorySliceSize, MEM_RESERVE|MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#elif defined(APPLE_AARCH64)
    return mmap(NULL, CodeMemorySliceSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT,-1, 0);
#elif defined(__NetBSD__)
    return mmap(nullptr, CodeMemorySliceSize, PROT_MPROTECT(PROT_READ | PROT_WRITE | PROT_EXEC), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#else
    //printf("mmaping...\n");
    return mmap(nullptr, CodeMemorySliceSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}

void FreeCodeMem(void* codeMem)
{
    std::lock_guard guard(globalMutex);

#if !defined(APPLE_AARCH64) && !defined(__NetBSD__) && !defined(__OpenBSD__)
    for (int i = 0; i < NumCodeMemSlices; i++)
    {
        if (codeMem == &GetAlignedCodeMemoryStart()[CodeMemorySliceSize * i])
        {
            //printf("freeing slice\n");
            AvailableCodeMemSlices |= 1 << i;
            return;
        }
    }
#endif

#ifdef _WIN32
    VirtualFree(codeMem, CodeMemorySliceSize, MEM_RELEASE|MEM_DECOMMIT);
#else
    munmap(codeMem, CodeMemorySliceSize);
#endif
}

void Init()
{
    std::lock_guard guard(globalMutex);

    RefCounter++;
    if (RefCounter == 1)
    {
        #ifdef _WIN32
            DWORD dummy;
            VirtualProtect(GetAlignedCodeMemoryStart(), CodeMemoryAlignedSize, PAGE_EXECUTE_READWRITE, &dummy);
        #elif defined(APPLE_AARCH64) || defined(__NetBSD__) || defined(__OpenBSD__)
            // Apple aarch64 always uses dynamic allocation
        #else
            mprotect(GetAlignedCodeMemoryStart(), CodeMemoryAlignedSize, PROT_EXEC | PROT_READ | PROT_WRITE);
        #endif

        ARMJIT_Memory::RegisterFaultHandler();
    }
}

void DeInit()
{
    std::lock_guard guard(globalMutex);

    RefCounter--;
    if (RefCounter == 0)
    {
        ARMJIT_Memory::UnregisterFaultHandler();
    }
}

}

}
