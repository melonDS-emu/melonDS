#pragma once
#include <array>
#include <functional>
#include <utility>
#include <queue>
#include "common_types.h"

namespace Teakra {

class Ahbm {
public:
    enum class UnitSize : u16 {
        U8 = 0,
        U16 = 1,
        U32 = 2,
    };

    enum class BurstSize : u16 {
        X1 = 0,
        X4 = 1,
        X8 = 2,
    };

    enum class Direction : u16 {
        Read = 0,
        Write = 1,
    };

    void Reset();

    u16 GetBusyFlag() const {
        return busy_flag;
    }
    void SetUnitSize(u16 i, u16 value) {
        channels[i].unit_size = static_cast<UnitSize>(value);
    }
    u16 GetUnitSize(u16 i) const {
        return static_cast<u16>(channels[i].unit_size);
    }
    void SetBurstSize(u16 i, u16 value) {
        channels[i].burst_size = static_cast<BurstSize>(value);
    }
    u16 GetBurstSize(u16 i) const {
        return static_cast<u16>(channels[i].burst_size);
    }
    void SetDirection(u16 i, u16 value) {
        channels[i].direction = static_cast<Direction>(value);
    }
    u16 GetDirection(u16 i) const {
        return static_cast<u16>(channels[i].direction);
    }
    void SetDmaChannel(u16 i, u16 value) {
        channels[i].dma_channel = value;
    }
    u16 GetDmaChannel(u16 i) const {
        return channels[i].dma_channel;
    }

    u16 Read16(u16 channel, u32 address);
    u32 Read32(u16 channel, u32 address);
    void Write16(u16 channel, u32 address, u16 value);
    void Write32(u16 channel, u32 address, u32 value);

    u16 GetChannelForDma(u16 dma_channel) const;

    void SetExternalMemoryCallback(
           std::function<u8 (u32)> read8 , std::function<void(u32, u8 )> write8 ,
           std::function<u16(u32)> read16, std::function<void(u32, u16)> write16,
           std::function<u32(u32)> read32, std::function<void(u32, u32)> write32) {

        read_external8 = std::move(read8);
        write_external8 = std::move(write8);
        read_external16 = std::move(read16);
        write_external16 = std::move(write16);
        read_external32 = std::move(read32);
        write_external32 = std::move(write32);
    }

private:
    u16 busy_flag = 0;
    struct Channel {
        UnitSize unit_size = UnitSize::U8;
        BurstSize burst_size = BurstSize::X1;
        Direction direction = Direction::Read;
        u16 dma_channel = 0;

        std::queue<u32> burst_queue;
        u32 write_burst_start = 0;
        unsigned GetBurstSize();
    };
    std::array<Channel, 3> channels;

    std::function<u8(u32)> read_external8;
    std::function<void(u32, u8)> write_external8;
    std::function<u16(u32)> read_external16;
    std::function<void(u32, u16)> write_external16;
    std::function<u32(u32)> read_external32;
    std::function<void(u32, u32)> write_external32;

    void WriteInternal(u16 channel, u32 address, u32 value);
};

} // namespace Teakra
