#include <cstdio>
#include <cstring>
#include "ahbm.h"
#include "dma.h"
#include "shared_memory.h"
#include "../../Savestate.h"

namespace Teakra {

void Dma::Reset() {
    enable_channel = 0;
    active_channel = 0;
    channels = {};
}

void Dma::DoSavestate(melonDS::Savestate *file) {
    file->Section("TKdm");

    file->Var16(&enable_channel);
    file->Var16(&active_channel);
    for (auto& chan : channels) {
        file->Var16(&chan.addr_src_low);
        file->Var16(&chan.addr_src_high);
        file->Var16(&chan.addr_dst_low);
        file->Var16(&chan.addr_dst_high);
        file->Var16(&chan.size0);
        file->Var16(&chan.size1);
        file->Var16(&chan.size2);
        file->Var16(&chan.src_step0);
        file->Var16(&chan.dst_step0);
        file->Var16(&chan.src_step1);
        file->Var16(&chan.dst_step1);
        file->Var16(&chan.src_step2);
        file->Var16(&chan.dst_step2);
        file->Var16(&chan.src_space);
        file->Var16(&chan.dst_space);
        file->Var16(&chan.dword_mode);
        file->Var16(&chan.y);
        file->Var16(&chan.z);

        file->Var32(&chan.current_src);
        file->Var32(&chan.current_dst);
        file->Var16(&chan.counter0);
        file->Var16(&chan.counter1);
        file->Var16(&chan.counter2);
        file->Var16(&chan.running);
        file->Var16(&chan.ahbm_channel);
    }
}

void Dma::DoDma(u16 channel) {
    channels[channel].Start();

    channels[channel].ahbm_channel = ahbm.GetChannelForDma(channel);

    // TODO: actually Tick this according to global Tick;
    while (channels[channel].running)
        channels[channel].Tick(*this);

    interrupt_handler();
}

void Dma::Channel::Start() {
    running = 1;
    current_src = addr_src_low | ((u32)addr_src_high << 16);
    current_dst = addr_dst_low | ((u32)addr_dst_high << 16);
    counter0 = 0;
    counter1 = 0;
    counter2 = 0;
}

void Dma::Channel::Tick(Dma& parent) {
    static constexpr u32 DataMemoryOffset = 0x20000;
    if (dword_mode) {
        u32 value = 0;
        switch (src_space) {
        case 0: {
            u32 l = current_src & 0xFFFFFFFE;
            u32 h = current_src | 1;
            value = parent.shared_memory.ReadWord(DataMemoryOffset + l) |
                    ((u32)parent.shared_memory.ReadWord(DataMemoryOffset + h) << 16);
            break;
        }
        case 1:
            std::printf("Unimplemented MMIO space");
            value = 0;
            break;
        case 7:
            value = parent.ahbm.Read32(ahbm_channel, current_src);
            break;
        default:
            std::printf("Unknown SrcSpace %04X\n", src_space);
        }

        switch (dst_space) {
        case 0: {
            u32 l = current_dst & 0xFFFFFFFE;
            u32 h = current_dst | 1;
            parent.shared_memory.WriteWord(DataMemoryOffset + l, (u16)value);
            parent.shared_memory.WriteWord(DataMemoryOffset + h, (u16)(value >> 16));
            break;
        }
        case 1:
            std::printf("Unimplemented MMIO space");
            value = 0;
            break;
        case 7:
            parent.ahbm.Write32(ahbm_channel, current_dst, value);
            break;
        default:
            std::printf("Unknown DstSpace %04X\n", dst_space);
        }

        counter0 += 2;
    } else {
        u16 value = 0;
        switch (src_space) {
        case 0:
            value = parent.shared_memory.ReadWord(DataMemoryOffset + current_src);
            break;
        case 1:
            std::printf("Unimplemented MMIO space");
            value = 0;
            break;
        case 7:
            value = parent.ahbm.Read16(ahbm_channel, current_src);
            break;
        default:
            std::printf("Unknown SrcSpace %04X\n", src_space);
        }

        switch (dst_space) {
        case 0:
            parent.shared_memory.WriteWord(DataMemoryOffset + current_dst, value);
            break;
        case 1:
            std::printf("Unimplemented MMIO space");
            value = 0;
            break;
        case 7:
            parent.ahbm.Write16(ahbm_channel, current_dst, value);
            break;
        default:
            std::printf("Unknown DstSpace %04X\n", dst_space);
        }

        counter0 += 1;
    }

    if (counter0 >= size0) {
        counter0 = 0;
        counter1 += 1;
        if (counter1 >= size1) {
            counter1 = 0;
            counter2 += 1;
            if (counter2 >= size2) {
                running = 0;
                return;
            } else {
                current_src += src_step2;
                current_dst += dst_step2;
            }
        } else {
            current_src += src_step1;
            current_dst += dst_step1;
        }
    } else {
        current_src += src_step0;
        current_dst += dst_step0;
    }
}

} // namespace Teakra
