
#ifndef DEBUG_ST_H
#define DEBUG_ST_H

#include <stddef.h>
#include <stdint.h>

#include "lxt_write.h"

namespace debug
{

enum SystemSignal
{
    ARM7_stat = 1<< 0, // pc, cpsr
    ARM9_stat = 1<< 1, // pc, cpsr, cp15
    MemCtl    = 1<< 2, // exmemcnt, wramcnt, vramcnt a-i, mbk1-9(dsi)
    DispCtl   = 1<< 3, // dispcnt, capcnt, hbl, vbl, vcount, capture to VRAM DMA?
    Disp3DCtl = 1<< 4, // disp3dcnt, rdlines_count, gxstat, ram_count
    DmaCtl    = 1<< 5, // DMAxCNT, NDMAxCNT(dsi), NDMAGCNT(dsi)
    IpcFifo   = 1<< 6, // ipcsync, ipcfifocnt
    SoundCtl  = 1<< 7, // SOUNDxCNT, soundcnt, soundbias, sndexcnt(dsi7)
    TimerCtl  = 1<< 8, // TMxCNT, overflow events
    Interrupt = 1<< 9, // ime, ie,if, ie2,if2(dsi7)
    MathsCtl  = 1<<10, // divcnt, sqrtcnt (+start/finish events)
    PowerCtl  = 1<<11, // powcnt1, powcnt2, wifiwaitcnt, haltcnt(?), pwman spi dev reg0?, bptwl(dsi7)
    SConfig   = 1<<12, // SCFG_* (dsi)
    DspCtl    = 1<<13, // dsp_pcfg, psts, psem, pmask, cmd0-2, rep0-2
    SdMmcIoCtl= 1<<14, // cmd, params, datactl, irqstat, irqmask, data32_irq, errdet, portsel, card_opt, card_clkctl (dsi7)
    SdMmcHwCtl= 1<<15, // csr, ?
    WifiNtrCtl= 1<<16, // mode_rst, if, ie, power_us, power_tx, powerstate, powerforce, rxcnt, txbusy, txstat, rf_pins, rf_status
    WifiIIoCtl= 1<<17, // cmd, params, datactl, irqstat, irqmask, data32_irq, errdet, portsel, card_opt, card_clkctl (dsi7)
    WifiISdCtl= 1<<18, // cccr{func en, ie, busctl}, f1{host_int_stat, cpu_int_stat}, mbox stuff?
    WifiIXtCtl= 1<<19, // pc, litbase, epc/intlevel/..., interrupt, ??? // TODO: whenever this is implemented in melonds
    GpioCtl   = 1<<20, // dir, ie, iedge, wifi
    KeypadCtl = 1<<21, // keyinput, keycnt, extkeyin(a7)
    MicNtrCtl = 1<<22, // SNDCAPxCNT
    MicTwlCtl = 1<<23, // mic_cnt(dsi7)
    RtcCtl    = 1<<24, // REG_RTC, FOUT
    SpiCtl    = 1<<25, // spicnt
    AuxSpiCtl = 1<<26, // auxspicnt, romctl
    TscTwlCtl = 1<<27, // ADC, DAC, ovf, int, ... flags
    I2CCtl    = 1<<28, // I2C stuff

    Custom    = 1u<<31
};

class DebugStorageNDS
{
public:
    enum SystemSignal EnabledSignals;

    DebugStorageNDS();
    ~DebugStorageNDS();

    void Reset();
    void AllocNew();

    void DoSavestate(Savestate* file);

    int32_t AddTraceSym(const char* name, int bits, int typ, enum SystemSignal categ);
    int32_t GetTraceSym(const char* name);
    void TraceValue(int32_t sym, int value);
    void TraceValue(int32_t sym, unsigned int value);
    void TraceValue(int32_t sym, double value);
    void TraceValue(int32_t sym, char* value); // bitstring
    void TraceString(int32_t sym, char* value);

    void BeginTracing();
    void PauseTracing();
    void SetTime(uint64_t tim, bool force);

private:
    bool tracing;
    uint64_t curtime;

    struct lt_trace* tracer;

    struct TSymFD
    {
        struct lt_symbol* sym;
        char* name;
        uint16_t l2categ;
        uint8_t bits;
        uint8_t typ;
    };

    size_t NTSyms, CapTSyms;
    struct TSymFD* TSyms;
};

}

#endif

