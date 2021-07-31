#pragma once
#include <array>
#include <cstdio>
#include <functional>
#include <utility>
#include <queue>
#include "common_types.h"
#include "core_timing.h"

namespace Teakra {

class Btdmp : public CoreTiming::Callbacks {
public:
    Btdmp(CoreTiming& core_timing);
    ~Btdmp();

    void Reset();

    void SetTransmitClockConfig(u16 value) {
        transmit_clock_config = value;
    }

    u16 GetTransmitClockConfig() const {
        return transmit_clock_config;
    }

    void SetTransmitPeriod(u16 value) {
        transmit_period = value;
    }

    u16 GetTransmitPeriod() const {
        return transmit_period;
    }

    void SetTransmitEnable(u16 value) {
        transmit_enable = value;
    }

    u16 GetTransmitEnable() const {
        return transmit_enable;
    }

    u16 GetTransmitEmpty() const {
        return transmit_empty;
    }

    u16 GetTransmitFull() const {
        return transmit_full;
    }

    void Send(u16 value) {
        if (transmit_queue.size() == 16) {
            std::printf("BTDMP: transmit buffer overrun\n");
        } else {
            transmit_queue.push(value);
            transmit_empty = false;
            transmit_full = transmit_queue.size() == 16;
        }
    }

    void SetTransmitFlush(u16 value) {
        transmit_queue = {};
        transmit_empty = true;
        transmit_full = false;
    }

    u16 GetTransmitFlush() const {
        return 0;
    }

    void Tick() override;
    u64 GetMaxSkip() const override;
    void Skip(u64 ticks) override;

    void SetAudioCallback(std::function<void(std::array<std::int16_t, 2>)> callback) {
        audio_callback = std::move(callback);
    }

    void SetInterruptHandler(std::function<void()> handler) {
        interrupt_handler = std::move(handler);
    }

private:
    // TODO: figure out the relation between clock_config and period.
    // Default to period = 4096 for now which every game uses
    u16 transmit_clock_config = 0;
    u16 transmit_period = 4096;
    u16 transmit_timer = 0;
    u16 transmit_enable = 0;
    bool transmit_empty = true;
    bool transmit_full = false;
    std::queue<u16> transmit_queue;
    std::function<void(std::array<std::int16_t, 2>)> audio_callback;
    std::function<void()> interrupt_handler;

    class BtdmpTimingCallbacks;
};

} // namespace Teakra
