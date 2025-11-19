#include <cstdio>
#include "ahbm.h"
#include "../../Savestate.h"

namespace Teakra {

void Ahbm::Reset() {
    busy_flag = 0;
    channels = {};
}

void Ahbm::DoSavestate(melonDS::Savestate *file) {
    file->Section("TKah");

    file->Var16(&busy_flag);
    for (auto& chan : channels) {
        file->Var16((u16*)&chan.unit_size);
        file->Var16((u16*)&chan.burst_size);
        file->Var16((u16*)&chan.direction);
        file->Var16(&chan.dma_channel);

        if (file->Saving) {
            auto tmpqueue = chan.burst_queue;
            u8 qlen = tmpqueue.size();
            file->Var8(&qlen);
            while (!tmpqueue.empty()) {
                u32 val = tmpqueue.front();
                tmpqueue.pop();
                file->Var32(&val);
            }
        } else {
            chan.burst_queue = {};
            u8 qlen;
            file->Var8(&qlen);
            for (u8 i = 0; i < qlen; i++) {
                u32 val;
                file->Var32(&val);
                chan.burst_queue.push(val);
            }
        }

        file->Var32(&chan.write_burst_start);
    }
}

unsigned Ahbm::Channel::GetBurstSize() {
    switch (burst_size) {
    case Ahbm::BurstSize::X1:
        return 1;
    case Ahbm::BurstSize::X4:
        return 4;
    case Ahbm::BurstSize::X8:
        return 8;
    default:
        std::printf("Unknown burst size %04X\n", static_cast<u16>(burst_size));
        return 1;
    }
}

u16 Ahbm::Read16(u16 channel, u32 address) {
    u32 value32 = Read32(channel, address);
    if ((address & 1) == 0) {
        return (u16)(value32 & 0xFFFF);
    } else {
        return (u16)(value32 >> 16);
    }
}
u32 Ahbm::Read32(u16 channel, u32 address) {
    if (channels[channel].direction != Direction::Read) {
        std::printf("Wrong direction!\n");
    }

    if (channels[channel].burst_queue.empty()) {
        u32 current = address;
        unsigned size = channels[channel].GetBurstSize();
        for (unsigned i = 0; i < size; ++i) {
            u32 value = 0;
            switch (channels[channel].unit_size) {
            case UnitSize::U8:
                value = read_external8(current);
                if ((current & 1) == 1) {
                    value <<= 8; // this weird bahiviour is hwtested
                }
                current += 1;
                break;
            case UnitSize::U16: {
                u32 current_masked = current & 0xFFFFFFFE;
                value = read_external16(current_masked);
                current += 2;
                break;
            }
            case UnitSize::U32: {
                u32 current_masked = current & 0xFFFFFFFC;
                value = read_external32(current_masked);
                current += 4;
                break;
            }
            default:
                std::printf("Unknown unit size %04X\n",
                            static_cast<u16>(channels[channel].unit_size));
                break;
            }
            channels[channel].burst_queue.push(value);
        }
    }

    u32 value = channels[channel].burst_queue.front();
    channels[channel].burst_queue.pop();
    return value;
}
void Ahbm::Write16(u16 channel, u32 address, u16 value) {
    WriteInternal(channel, address, value);
}
void Ahbm::Write32(u16 channel, u32 address, u32 value) {
    if ((address & 1) == 1) {
        value >>= 16; // this weird behaviour is hwtested
    }
    WriteInternal(channel, address, value);
}

void Ahbm::WriteInternal(u16 channel, u32 address, u32 value) {
    if (channels[channel].direction != Direction::Write) {
        std::printf("Wrong direction!\n");
    }

    if (channels[channel].burst_queue.empty()) {
        channels[channel].write_burst_start = address;
    }

    channels[channel].burst_queue.push(value);
    if (channels[channel].burst_queue.size() >= channels[channel].GetBurstSize()) {
        u32 current = channels[channel].write_burst_start;
        while (!channels[channel].burst_queue.empty()) {
            u32 value32 = channels[channel].burst_queue.front();
            channels[channel].burst_queue.pop();
            switch (channels[channel].unit_size) {
            case UnitSize::U8: {
                // this weird behaviour is hwtested
                u8 value8 = ((current & 1) == 1) ? (u8)(value32 >> 8) : (u8)value32;
                write_external8(current, value8);
                current += 1;
                break;
            }
            case UnitSize::U16: {
                u32 c0 = current & 0xFFFFFFFE;
                u32 c1 = c0 + 1;
                if (c0 >= current) {
                    write_external16(c0, (u16)value32);
                } else {
                    write_external8(c1, (u8)(value32 >> 8));
                }
                current += 2;
                break;
            }
            case UnitSize::U32: {
                u32 c0 = current & 0xFFFFFFFC;
                u32 c1 = c0 + 1;
                u32 c2 = c0 + 2;
                u32 c3 = c0 + 3;

                if (c0 >= current && c1 >= current && c2 >= current) {
                    write_external32(c0, value32);
                } else if (c2 >= current) {
                    if (c1 >= current) {
                        write_external8(c1, (u8)(value32 >> 8));
                    }
                    write_external16(c2, (u16)(value32 >> 16));
                } else {
                    write_external8(c3, (u8)(value32 >> 24));
                }

                current += 4;
                break;
            }
            default:
                std::printf("Unknown unit size %04X\n",
                            static_cast<u16>(channels[channel].unit_size));
                break;
            }
        }
    }
}

u16 Ahbm::GetChannelForDma(u16 dma_channel) const {
    for (u16 channel = 0; channel < channels.size(); ++channel) {
        if ((channels[channel].dma_channel >> dma_channel) & 1) {
            return channel;
        }
    }
    std::printf("Could not find AHBM channel for DMA channel %04X\n", dma_channel);
    return 0;
}

} // namespace Teakra
