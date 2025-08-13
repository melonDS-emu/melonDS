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
    Btdmp(CoreTiming& core_timing, int num);
    ~Btdmp();

    void Reset();
    void DoSavestate(melonDS::Savestate* file);

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

    void SetReceiveClockConfig(u16 value) {
        receive_clock_config = value;
    }

    u16 GetReceiveClockConfig() const {
        return receive_clock_config;
    }

    void SetReceivePeriod(u16 value) {
        receive_period = value;
    }

    u16 GetReceivePeriod() const {
        return receive_period;
    }

    void SetReceiveEnable(u16 value) {
        receive_enable = value;
        if (mic_enable_callback)
            mic_enable_callback(!!receive_enable);
    }

    u16 GetReceiveEnable() const {
        return receive_enable;
    }

    u16 GetReceiveEmpty() const {
        // this bit seems to be backwards for the receive FIFO
        // after disabling input, the ucode will wait for it to be cleared
        // TODO verify this on hardware
        return !receive_empty;
    }

    u16 GetReceiveFull() const {
        return receive_full;
    }

    u16 Receive() {
        if (receive_queue.empty()) {
            std::printf("BTDMP: receive buffer underrun\n");
            return 0;
        } else {
            u16 ret = receive_queue.front();
            receive_queue.pop();
            receive_empty = receive_queue.empty();
            receive_full = false;
            return ret;
        }
    }

    void SetReceiveFlush(u16 value) {
        receive_queue = {};
        receive_empty = true;
        receive_full = false;
    }

    u16 GetReceiveFlush() const {
        return 0;
    }

    void Tick() override;
    u64 GetMaxSkip() const override;
    void Skip(u64 ticks) override;

    void SampleClock(std::int16_t output[2], std::int16_t input);

    void SetAudioCallback(std::function<void(std::array<std::int16_t, 2>)> callback) {
        audio_callback = std::move(callback);
    }

    void SetInterruptHandler(std::function<void()> handler) {
        interrupt_handler = std::move(handler);
    }

    void SetMicEnableCallback(std::function<void(bool)> cb) {
        mic_enable_callback = std::move(cb);
    }

private:
    int num;

    // TODO: figure out the relation between clock_config and period.
    u16 transmit_clock_config = 0;
    u16 transmit_period = 4096;
    u16 transmit_timer = 0;
    u16 transmit_enable = 0;
    bool transmit_empty = true;
    bool transmit_full = false;
    std::queue<u16> transmit_queue;

    u16 receive_clock_config = 0;
    u16 receive_period = 4096;
    u16 receive_timer = 0;
    u16 receive_enable = 0;
    bool receive_empty = true;
    bool receive_full = false;
    std::queue<u16> receive_queue;

    std::function<void(std::array<std::int16_t, 2>)> audio_callback;
    std::function<void()> interrupt_handler;

    std::function<void(bool)> mic_enable_callback;

    class BtdmpTimingCallbacks;
};

} // namespace Teakra
