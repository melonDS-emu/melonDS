
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <bitset>
#include <string>

#include "../NDS.h"

#include "st.h"

#define TRACE_OUT_FILE "melonds-trace.lxt"

#define TRACE_BY_DEFAULT_ON

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
    : tracer(NULL), NTSyms(0), CapTSyms(0), TSyms(NULL)
{
    Reset();
    AllocNew();
}
DebugStorageNDS::~DebugStorageNDS()
{
    Reset();
}


// 33554432 Hz?
// 33513982 Hz?
#define SYS_BUSCLOCK_RATE (33513982)
// A7 clock = busclock
// A9 clock = busclock*2
// dotclock = busclock / 6
// scanlines = dotclock / 355 (256 visible, 99 blank)
// frames = scanlines / 263 (192 visible, 71 blank)
//
// // pre clockdiv settings
// audio masterclock = busclock/2 (SOUNDxTMR = audio masterclock/freq as freq.div)
// timer masterclock = busclock
// SPI masterclock = "4 MHz"
// AUXSPI masterclock = "4 MHz"
// cart/rom xfer masterclock = busclock/2 (/5 or /8 in ROMCTRL)
//
// DSi9 clock = busclock*4
// NDMA masterclock = busclock
// DSP clock = busclock*4 ? busclock*4/1.25 ? 134,060,000 Hz ?? what's the memory bandwidth???
// I2C masterclock = ??? (BPTWL: ???, camera: busclock/2?)
// I2S (mic/nitro+dsp mixer) masterclock = "32.73kHz" or "47.61kHz" (freqdiv in mic_cnt, opt in sndexcnt)
// SDMMC, SDIO masterclock = busclock/2
// Wifi Xtensa masterclock = "26 MHz"? "40 MHz"?
//
//
// GBA:
//  ARM Mode     16.78MHz <---- 16777216 Hz? busclock/2?
//  THUMB Mode   16.78MHz
//  CGB Mode     4.2MHz or 8.4MHz <--- 2<<22 Hz? busclock/4?
//  DMG Mode     4.2MHz <--- 2<<21 Hz? busclock/8?
#define TRACE_TIMESCALE (-12)
// we have to use int128 here because otherwise we'd run out of clocks too quickly. welp.
#define TRACE_SYSCLOCK_TO_TIMESTAMP(c) ((uint64_t)((c*((unsigned __int128)(1e12)))/SYS_BUSCLOCK_RATE))

void DebugStorageNDS::Reset()
{
    if (TSyms && NTSyms) {
        for (size_t i = 0; i < NTSyms; ++i)
            free((void*)TSyms[i].name);

        free(TSyms);
    }

    curtime = 0;

    if (tracer) {
        lt_close(tracer);
    }
    tracer = NULL;
#ifdef TRACE_BY_DEFAULT_ON
    tracing = true;
#else
    tracing = false;
#endif

    CapTSyms = NTSyms = 0;
    TSyms = NULL;

    EnabledSignals = (SystemSignal)(SystemSignal::DispCtl
            | SystemSignal::Interrupt | SystemSignal::Custom);
#ifdef TRACE_BY_DEFAULT_ON
    //EnabledSignals = (SystemSignal)((u32)EnabledSignals | (u32)SystemSignal::DspCtl);
    EnabledSignals = (SystemSignal)~(uint32_t)0;
#endif
}
void DebugStorageNDS::AllocNew()
{
    if (!tracer)
    {
        tracer = lt_init(TRACE_OUT_FILE);
        lt_set_timescale(tracer, TRACE_TIMESCALE);
        // for a correct timescale, ^ could be set to 0, and then v needs to be
        // divided by the system bus clockrate. however, we're working with ints,
        // so it has to be amended a little bit: we use 1e-12 timings, and then
        // prescale with 1e12 and divide by the system clock
        // secondly, currently, we only allow for system clock resolution, but
        // this will probably have to be amended in the future
        lt_set_time64(tracer, TRACE_SYSCLOCK_TO_TIMESTAMP(curtime = NDS::GetSysClockCycles(0, true)));
#ifdef TRACE_BY_DEFAULT_ON
        lt_set_dumpon(tracer);
#else
        lt_set_dumpoff(tracer);
#endif
        lt_set_initial_value(tracer, '0');
    }

    if (!TSyms)
    {
        CapTSyms = 0x40;
        NTSyms = 1;
        TSyms = (struct TSymFD*)calloc(CapTSyms, sizeof(TSymFD));
    }
}

int32_t DebugStorageNDS::AddTraceSym(const char* name, int bits, int typ,
        enum SystemSignal categ)
{
    uint32_t catu = (uint32_t)categ;
    if (bits < 0 || bits > 64) return -1;
    if (!catu || (catu & (catu - 1))) return -1; // definitely a bad categ value

    uint16_t l2categ = __builtin_ctz(catu);
    if (catu != (1u<<l2categ)) return -1; // more than one bit set

    if (!TSyms || !tracer) AllocNew();

    typ = (typ == LT_SYM_F_INTEGER && bits != 32) ? LT_SYM_F_BITS : typ;

    int32_t sym = GetTraceSym(name);
    if (sym > -1)
    {
        uint32_t h = str_hash_djb2(name);
        if (str_hash_djb2(TSyms[sym].name) == h && TSyms[sym].bits == bits
                && TSyms[sym].typ == typ && TSyms[sym].l2categ == l2categ)
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
    TSyms[NTSyms].l2categ  = l2categ;
    TSyms[NTSyms].bits     = bits;
    TSyms[NTSyms].typ      = typ;
    ++NTSyms;

    return sym;
}
int32_t DebugStorageNDS::GetTraceSym(const char* name)
{
    if (!TSyms || !tracer) return -1;

    uint32_t h = str_hash_djb2(name);
    for (size_t i = 1; i < NTSyms; ++i)
        if (str_hash_djb2(TSyms[i].name) == h)
            return i;

    return -1;
}
void DebugStorageNDS::TraceValue(int32_t sym, int value)
{
    if (!tracer || !TSyms || sym < 1 || sym >= NTSyms || !tracing) return;
    if (!((enum SystemSignal)(1u << TSyms[sym].l2categ) & EnabledSignals)) return;

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
void DebugStorageNDS::TraceValue(int32_t sym, unsigned int value)
{
    if (!tracer || !TSyms || sym < 1 || sym >= NTSyms || !tracing) return;
    if (!((enum SystemSignal)(1u << TSyms[sym].l2categ) & EnabledSignals)) return;

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
void DebugStorageNDS::TraceValue(int32_t sym, double value)
{
    if (!tracer || !TSyms || sym < 1 || sym >= NTSyms || !tracing) return;
    if (!((enum SystemSignal)(1u << TSyms[sym].l2categ) & EnabledSignals)) return;

    //printf("trace %s(%d, typ %d) to %f @ time %llu <-> %llu\n", TSyms[sym].name, sym, TSyms[sym].typ, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    lt_emit_value_double(tracer, TSyms[sym].sym, 0, value);
}
void DebugStorageNDS::TraceValue(int32_t sym, const char* value)
{
    if (!tracer || !TSyms || sym < 1 || sym >= NTSyms || !tracing) return;
    if (!((enum SystemSignal)(1u << TSyms[sym].l2categ) & EnabledSignals)) return;

    //printf("trace %s(%d) to %s @ time %llu <-> %llu\n", TSyms[sym].name, sym, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    lt_emit_value_bit_string(tracer, TSyms[sym].sym, 0, (char*)value);
}
void DebugStorageNDS::TraceString(int32_t sym, const char* value)
{
    if (!tracer || !TSyms || sym < 1 || sym >= NTSyms || !tracing) return;
    if (!((enum SystemSignal)(1u << TSyms[sym].l2categ) & EnabledSignals)) return;

    //printf("trace %s(%d) to %s @ time %llu <-> %llu\n", TSyms[sym].name, sym, value,
    //        NDS::SysTimestamp, NDS::GetSysClockCycles(0, true));

    //SetTime(NDS::GetSysClockCycles(0, true));
    lt_emit_value_string(tracer, TSyms[sym].sym, 0, (char*)value);
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
        lt_set_time64(tracer, TRACE_SYSCLOCK_TO_TIMESTAMP(t));

    if (force)
        curtime = t;
}
void DebugStorageNDS::WithTime(uint64_t t, size_t clkshift, std::function<void()> cb) {
    if (cb == nullptr || !tracer) return;

    lt_set_time64(tracer, TRACE_SYSCLOCK_TO_TIMESTAMP(t) >> clkshift);
    cb();
    lt_set_time64(tracer, curtime);
}
void DebugStorageNDS::WithTimeARM7(std::function<void()> cb) {
    WithTime(NDS::ARM7Timestamp, 0, cb);
}
void DebugStorageNDS::WithTimeARM9(std::function<void()> cb) {
    WithTime(NDS::ARM9Timestamp, NDS::ARM9ClockShift, cb);
}

void DebugStorageNDS::DoSavestate(Savestate* file)
{
    if (!TSyms) AllocNew();

    file->Section("DEBN");

    if (file->Saving)
    {
        uint8_t en = tracing ? 1 : 0;
        file->Var8(&en);
        uint32_t ns = NTSyms;
        file->Var32(&ns);
        for (size_t i = 1; i < ns; ++i)
        {
            uint32_t l = strlen(TSyms[i].name);
            file->Var32(&l);
            file->Var16(&TSyms[i].l2categ);
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

        for (size_t i = 1; i < ns; ++i)
        {
            uint32_t l = 0;
            uint16_t l2categ = 0;
            uint8_t bits = 0, typ = 0;

            file->Var32(&l);
            file->Var16(&l2categ);
            file->Var8(&bits);
            file->Var8(&typ);

            char* nam = (char*)calloc(1,l+1);
            file->VarArray(nam, l+1);
            AddTraceSym(nam, bits, typ, (enum SystemSignal)(1u<<l2categ));
            free(nam);
        }
    }
}

}

