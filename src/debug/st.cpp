
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <bitset>
#include <string>

#include "../NDS.h"

#include "st.h"

#define TRACE_OUT_FILE "melonds-trace.lxt"

static uint32_t str_hash_djb2(const char* s)
{
    uint32_t r = 5381;
    for (; *s; ++s) r += (r << 5) + *s;
    return r;
}

static std::string uint_to_binstr(size_t bits, uint32_t val)
{
    std::string rv(bits, '0');

    for (size_t i = 0; i < bits; ++i)
        if (val & (1u << i))
            rv[bits-i-1] = '1';

    return rv;
}
static std::string int_to_binstr(size_t bits, int32_t val)
{
    union {uint32_t u; int32_t s;} us;
    us.s = val;
    return uint_to_binstr(bits, us.u);
}

namespace debug
{

DebugStorageNDS::DebugStorageNDS()
    : tracer(NULL)
{
    Reset();
    AllocNew();
}
DebugStorageNDS::~DebugStorageNDS()
{
    Reset();
}

void DebugStorageNDS::Reset()
{
    /*struct lt_trace* t2 = lt_init("test.lxt");
    lt_set_timescale(t2, 0);
    lt_set_dumpon(t2);
    lt_set_initial_value(t2, '0');

    struct lt_symbol* s2 = lt_symbol_add(t2, "testtest", 0, 0, 0, LT_SYM_F_BITS);//INTEGER);
    struct lt_symbol* s3 = lt_symbol_add(t2, "testtes3", 0, 0, 1, LT_SYM_F_INTEGER);

    for (int i = 0; i < 10; ++i){
        if (i==4)lt_set_dumpoff(t2);
        else if (i==6)lt_set_dumpon(t2);
        lt_set_time64(t2, i);
        lt_emit_value_bit_string(t2, s2, 0, (1^(i&1)) ? (char*)"1" : (char*)"0");
        lt_emit_value_int(t2, s3, 0, i&3);
    }

    lt_close(t2);*/

    curtime = 0;

    //printf("reset\n");
    if (tracer) {
        //printf("end tracer\n");
        lt_close(tracer);
    }
    tracer = NULL;
    tracing = false;

    CapTSyms = NTSyms = 0;
    TSyms = NULL;

    EnabledSignals = (SystemSignal)(SystemSignal::DispCtl
            | SystemSignal::Interrupt | SystemSignal::Custom);
}
void DebugStorageNDS::AllocNew()
{
    if (!tracer)
    {
        //printf("alloc tracer\n");
        tracer = lt_init(TRACE_OUT_FILE);
        lt_set_timescale(tracer, -6); // TODO
        lt_set_time64(tracer, NDS::SysTimestamp);
        lt_set_dumpoff(tracer);
        lt_set_initial_value(tracer, '0');
    }

    if (!TSyms)
    {
        CapTSyms = 0x40;
        NTSyms = 0;
        TSyms = (struct TSymFD*)calloc(CapTSyms, sizeof(TSymFD));
    }
}

int32_t DebugStorageNDS::AddTraceSym(const char* name, int bits, int typ)
{
    if (bits < 0 || bits > 64) return -1;

    if (!TSyms || !tracer) AllocNew();

    typ = (typ == LT_SYM_F_INTEGER && bits != 32) ? LT_SYM_F_BITS : typ;

    int32_t sym = GetTraceSym(name);
    if (sym > -1)
    {
        uint32_t h = str_hash_djb2(name);
        if (str_hash_djb2(TSyms[sym].name) == h && TSyms[sym].bits == bits && TSyms[sym].typ == typ)
            return sym;

        return -1;
    }

    struct lt_symbol* ssym = lt_symbol_add(tracer, name, 0, 0, bits-1, typ);
    if (!ssym) return -1;

    if (NTSyms == CapTSyms)
    {
        CapTSyms <<= 1;
        TSyms = (struct TSymFD*)reallocarray(TSyms, CapTSyms, sizeof(struct TSymFD));
    }

    size_t nl = strlen(name);
    char* nn = (char*)calloc(nl+1, 1);
    memcpy(nn, name, nl+1);
    nn[nl] = 0;

    sym = NTSyms;
    //printf("sym %s type %d -> #%d %p\n", name, typ, sym, ssym);
    TSyms[NTSyms].sym      = ssym;
    TSyms[NTSyms].name     = nn;
    TSyms[NTSyms].bits     = bits;
    TSyms[NTSyms].typ      = typ;
    ++NTSyms;

    return sym;
}
int32_t DebugStorageNDS::GetTraceSym(const char* name)
{
    if (!TSyms || !tracer) return -1;

    uint32_t h = str_hash_djb2(name);
    for (size_t i = 0; i < NTSyms; ++i)
        if (str_hash_djb2(TSyms[i].name) == h)
            return i;

    return -1;
}
void DebugStorageNDS::TraceValue(int32_t sym, int value, enum SystemSignal categ)
{
    if (!tracer || !TSyms || sym < 0 || sym >= NTSyms || !tracing) return;
    if (!(categ & EnabledSignals)) return;

    //printf("trace %s(%d) to %d @ time %llu <-> %llu\n", TSyms[sym].name, sym, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    if (TSyms[sym].typ == LT_SYM_F_BITS)
    {
        std::string str = int_to_binstr(TSyms[sym].bits, value);
        lt_emit_value_bit_string(tracer, TSyms[sym].sym, 0, (char*)str.c_str());
    }
    else
        lt_emit_value_int(tracer, TSyms[sym].sym, 0, value);
}
void DebugStorageNDS::TraceValue(int32_t sym, unsigned int value, enum SystemSignal categ)
{
    if (!tracer || !TSyms || sym < 0 || sym >= NTSyms || !tracing) return;
    if (!(categ & EnabledSignals)) return;

    //printf("trace %s(%d) to %u @ time %llu <-> %llu\n", TSyms[sym].name, sym, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    if (TSyms[sym].typ == LT_SYM_F_BITS)
    {
        std::string str = uint_to_binstr(TSyms[sym].bits, value);
        lt_emit_value_bit_string(tracer, TSyms[sym].sym, 0, (char*)str.c_str());
    }
    else
        lt_emit_value_int(tracer, TSyms[sym].sym, 0, value);
}
void DebugStorageNDS::TraceValue(int32_t sym, double value, enum SystemSignal categ)
{
    if (!tracer || !TSyms || sym < 0 || sym >= NTSyms || !tracing) return;
    if (!(categ & EnabledSignals)) return;

    //printf("trace %s(%d) to %f @ time %llu <-> %llu\n", TSyms[sym].name, sym, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    lt_emit_value_double(tracer, TSyms[sym].sym, 0, value);
}
void DebugStorageNDS::TraceValue(int32_t sym, char* value, enum SystemSignal categ)
{
    if (!tracer || !TSyms || sym < 0 || sym >= NTSyms || !tracing) return;
    if (!(categ & EnabledSignals)) return;

    //printf("trace %s(%d) to %s @ time %llu <-> %llu\n", TSyms[sym].name, sym, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    lt_emit_value_bit_string(tracer, TSyms[sym].sym, 0, value);
}
void DebugStorageNDS::TraceString(int32_t sym, char* value, enum SystemSignal categ)
{
    if (!tracer || !TSyms || sym < 0 || sym >= NTSyms || !tracing) return;
    if (!(categ & EnabledSignals)) return;

    //printf("trace %s(%d) to %s @ time %llu <-> %llu\n", TSyms[sym].name, sym, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    lt_emit_value_string(tracer, TSyms[sym].sym, 0, value);
}

void DebugStorageNDS::BeginTracing()
{
    if (!tracer) AllocNew();

    printf("begin tracing\n");
    lt_set_dumpon(tracer);
    tracing = true;
}
void DebugStorageNDS::PauseTracing()
{
    if (!tracer) AllocNew();

    printf("end tracing\n");
    lt_set_dumpoff(tracer);
    tracing = false;
}
void DebugStorageNDS::SetTime(uint64_t t, bool force)
{
    if (!tracer) AllocNew();

    if (force || t > curtime)
        lt_set_time64(tracer, t);

    if (force)
        curtime = t;
}

void DebugStorageNDS::DoSavestate(Savestate* file)
{
    if (!TSyms) AllocNew();

    if (file->Saving)
    {
        uint8_t en = tracing ? 1 : 0;
        file->Var8(&en);
        uint32_t ns = NTSyms;
        file->Var32(&ns);
        for (size_t i = 0; i < ns; ++i)
        {
            uint32_t l = strlen(TSyms[i].name);
            file->Var32(&l);
            file->Var8(&TSyms[i].bits);
            file->Var8(&TSyms[i].typ);
            file->VarArray(TSyms[i].name, l+1);
        }
    }
    else
    {
        uint8_t en = 0;
        file->Var8(&en);
        tracing = en != 0;

        uint32_t ns = 0;
        file->Var32(&ns);

        for (size_t i = 0; i < ns; ++i)
        {
            uint32_t l = 0;
            uint16_t arrlen = 0;
            uint8_t bits = 0, typ = 0;

            file->Var32(&l);
            file->Var8(&bits);
            file->Var8(&typ);

            char* nam = (char*)calloc(1,l+1);
            file->VarArray(nam, l+1);
            AddTraceSym(nam, bits, typ);
            free(nam);
        }
    }
}

}

