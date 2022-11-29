
#ifndef DEBUG_ST_H
#define DEBUG_ST_H

#include <stddef.h>
#include <stdint.h>

#include "lxt_write.h"

namespace debug
{

class DebugStorageNDS
{
public:
    DebugStorageNDS();
    ~DebugStorageNDS();

    void Reset();
    void AllocNew();

    void DoSavestate(Savestate* file);

    int32_t AddTraceSym(const char* name, unsigned int arrlen, int bits, int typ);
    int32_t GetTraceSym(const char* name);
    void TraceValue(int32_t sym, unsigned int ind, int value);
    void TraceValue(int32_t sym, unsigned int ind, unsigned int value);
    void TraceValue(int32_t sym, unsigned int ind, double value);
    void TraceValue(int32_t sym, unsigned int ind, char* value); // bitstring
    void TraceString(int32_t sym, unsigned int ind, char* value);

    void BeginTracing();
    void PauseTracing();
    void SetTime(uint64_t tim);

private:
    bool tracing;

    struct lt_trace* tracer;

    struct TSymFD
    {
        struct lt_symbol* sym;
        char* name;
        uint16_t arrlen;
        uint8_t bits;
        uint8_t typ;
    };

    size_t NTSyms, CapTSyms;
    struct TSymFD* TSyms;
};

}

#endif

