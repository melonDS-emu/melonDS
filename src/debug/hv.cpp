
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

namespace debug
{

void swi(ARM* cpu, bool thumb, uint32_t scnum)
{
    // of course, only enable this if you trust the ROM...

    /*printf("hypervisor call #%04x on ARM%d in %s mode\n",
        scnum, cpu->Num?7:9, thumb?"THUMB":"ARM");*/

    switch (scnum)
    {
    /* general/misc */
    case 0x80: /* test */
        printf("test hypervisor call #%04x on ARM%d in %s mode\n",
            scnum, cpu->Num?7:9, thumb?"THUMB":"ARM");
        break;
    case 0x81: /* print */
        {
            size_t cursz = 0x100, bufi = 0;
            char* buf = read_sz(cpu, cpu->R[0]);
            printf("[%c%d] %s\n", thumb?'T':'A', cpu->Num?7:9, buf);
            free(buf);
        }
        break;
    case 0x82: /* cycles */
        {
            int  num =  cpu->R[0] & 0x7FFFFFFF      ;
            bool clk = (cpu->R[0] & 0x80000000) != 0;
            uint64_t rv = NDS::GetSysClockCycles(num, clk);
            cpu->R[0] = (rv & 0x00000000FFFFFFFF) >>  0;
            cpu->R[1] = (rv & 0xFFFFFFFF00000000) >> 32;
        }
        break;
    }
}

}

