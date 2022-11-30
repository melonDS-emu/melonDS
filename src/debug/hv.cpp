
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef WIN32
#error "TODO" /* sorry */
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "../ARM.h"
#include "../NDS.h"
#include "../Platform.h"

#include "hv.h"

static char* read_sz(ARM* cpu, uint32_t addr)
{
    size_t cursz = 0x100, bufi = 0;
    char* buf = (char*)calloc(1, cursz);

    int32_t cycd = cpu->DataCycles;
    uint32_t v = 0;
    do
    {
        cpu->DataRead8(addr+bufi, &v);
        buf[bufi] = v & 0xFF;
        ++bufi;

        if (bufi >= cursz && v)
        {
            cursz <<= 1;
            buf = (char*)realloc(buf, cursz);
        }
    } while (v);
    cpu->DataCycles = cycd;

    return buf;
}
static uint8_t* read_buf(ARM* cpu, uint32_t addr, uint32_t len)
{
    uint8_t* buf = (uint8_t*)calloc(len, 1);

    int32_t cycd = cpu->DataCycles;
    for (size_t i = 0; i < len; ++i)
    {
        uint32_t vvv = 0;
        cpu->DataRead8(addr+i, &vvv);
        buf[i] = vvv & 0xFF;
    }
    cpu->DataCycles = cycd;

    return buf;
}
static void write_buf(ARM* cpu, uint32_t addr, uint8_t* buf, uint32_t len)
{
    int32_t cycd = cpu->DataCycles;
    for (size_t i = 0; i < len; ++i)
        cpu->DataWrite8(addr+i, buf[i]);
    cpu->DataCycles = cycd;
}

static void dump_cpu_state(ARM* cpu, bool thumb)
{
    static const char* regnames[16] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10","r11","ip", "sp", "lr", "pc"
    };

    printf("ARM%d regs (%c): cpsr=%08x pipeline=%08x %08x %08x\n",
            cpu->Num?7:9, thumb?'T':'A', cpu->CPSR, cpu->CurInstr,
            cpu->NextInstr[0], cpu->NextInstr[1]);
    for (int i = 0; i < 16; i += 4) {
        printf("    %s=%08x %s=%08x %s=%08x %s=%08x\n",
                regnames[i+0], cpu->R[i+0], regnames[i+1], cpu->R[i+1],
                regnames[i+2], cpu->R[i+2], regnames[i+3], cpu->R[i+3]);
    }
}

namespace debug
{

void swi(ARM* cpu, bool thumb, uint32_t scnum)
{
    if (!Platform::GetConfigBool(Platform::ConfigEntry::DBG_EnableHypercalls))
        return;

    const bool allow_misc = Platform::GetConfigBool(Platform::ConfigEntry::DBG_HVMisc);
    const bool allow_sgtr = Platform::GetConfigBool(Platform::ConfigEntry::DBG_HVSignalTracing);

    // of course, only enable this if you trust the ROM...

    printf("hypervisor call #%02x on ARM%d in %s mode\n",
        scnum, cpu->Num?7:9, thumb?"THUMB":"ARM");

    switch (scnum)
    {
    /* general/misc */
    case 0x80: /* test */
        if (!allow_misc) break;
        printf("test hypervisor call #%02x on ARM%d in %s mode\n",
            scnum, cpu->Num?7:9, thumb?"THUMB":"ARM");
        break;
    case 0x81: /* print */
        if (!allow_misc) break;
        {
            size_t cursz = 0x100, bufi = 0;
            char* buf = read_sz(cpu, cpu->R[0]);
            printf("[%c%d] %s\n", thumb?'T':'A', cpu->Num?7:9, buf);
            free(buf);
        }
        break;
    case 0x82: /* cycles */
        if (!allow_misc) break;
        {
            int  num =  cpu->R[0] & 0x7FFFFFFF      ;
            bool clk = (cpu->R[0] & 0x80000000) != 0;
            uint64_t rv = NDS::GetSysClockCycles(num, clk);
            cpu->R[0] = (rv & 0x00000000FFFFFFFF) >>  0;
            cpu->R[1] = (rv & 0xFFFFFFFF00000000) >> 32;
        }
        break;
    case 0x83: /* dump registers */
        if (!allow_misc) break;
        dump_cpu_state(cpu, thumb);
        break;
    case 0x84: /* stop emulation */
        if (!allow_misc) break;
        NDS::ExitEmulator = true;
        break;

    case 0x88: /* add trace signal (const char* name, int arrlen, int bits, int typ) */
        if (!allow_sgtr) break;
        {
            uint32_t namaddr = cpu->R[0], bits = cpu->R[1], typ = cpu->R[2];

            if (bits > 64 || bits == 0) cpu->R[0] = -1;
            else
            {
                char* buf = read_sz(cpu, namaddr);
                cpu->R[0] = NDS::DebugStuff.AddTraceSym(buf, bits, (int)typ, SystemSignal::Custom);
                free(buf);
            }
        }
        break;
    case 0x89: /* get trace signal */
        if (!allow_sgtr) break;
        {
            uint32_t namaddr = cpu->R[0];
            char* buf = read_sz(cpu, namaddr);
            cpu->R[0] = NDS::DebugStuff.GetTraceSym(buf);
            free(buf);
        }
        break;
    case 0x8a: /* trace int value (int sig, int ind, int val) */
        if (!allow_sgtr) break;
        {
            int32_t sig = cpu->R[0],
                    val = cpu->R[1];

            NDS::DebugStuff.TraceValue(sig, val);
        }
        break;
    case 0x8b: /* trace float value (int sig, int ind, float val) */
        if (!allow_sgtr) break;
        {
            int32_t sig = cpu->R[0];
            union { uint32_t u; float f; } uf;
            uf.u = cpu->R[1];

            //printf("trace sym %d value %f\n", sig, uf.f);
            NDS::DebugStuff.TraceValue(sig, (double)uf.f);
        }
        break;
    case 0x8c: /* trace bit string (int sig, int ind, const char* val) */
        if (!allow_sgtr) break;
        {
            int32_t sig = cpu->R[0];
            uint32_t bufaddr = cpu->R[1];

            char* buf = read_sz(cpu, bufaddr);
            NDS::DebugStuff.TraceValue(sig, buf);
            free(buf);
        }
        break;
    case 0x8d: /* trace string (int sig, int ind, const char* val) */
        if (!allow_sgtr) break;
        {
            int32_t sig = cpu->R[0];
            uint32_t bufaddr = cpu->R[1];

            char* buf = read_sz(cpu, bufaddr);
            NDS::DebugStuff.TraceString(sig, buf);
            free(buf);
        }
        break;
    case 0x8e: /* pause or start/resume tracing */
        if (!allow_sgtr) break;
        {
            uint32_t en = cpu->R[0];
            if (en == 0) NDS::DebugStuff.PauseTracing();
            else NDS::DebugStuff.BeginTracing();
        }
        break;
    case 0x8f: /* get/set/mask tracing flags */
        if (!allow_sgtr) break;
        {
            // oldv = flags
            // flags = (flags & mask) | add
            // return oldv
            uint64_t mask = (uint64_t)cpu->R[0] | ((uint64_t)cpu->R[1]<<32),
                     add  = (uint64_t)cpu->R[2] | ((uint64_t)cpu->R[3]<<32);
            uint64_t curen = (uint64_t)NDS::DebugStuff.EnabledSignals;
            cpu->R[0] = (uint32_t)curen;
            cpu->R[1] = (uint32_t)(curen >> 32);
            NDS::DebugStuff.EnabledSignals = (SystemSignal)((curen & mask) | add);
            printf("set tracing mask 0x%16x -> 0x%16x\n", curen,
                    (uint64_t)NDS::DebugStuff.EnabledSignals);
        }
        break;
    }
}

void snoop_insn(ARM* cpu, bool thumb, u32 insn)
{
    // actual address of the currently-executing instruction
    u32 ip = cpu->R[15] - (thumb ? 2 : 4);

    //dump_cpu_state(cpu, thumb);
}

}

