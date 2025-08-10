#include <string>
#include "btdmp.h"
#include "crash.h"

namespace Teakra {

Btdmp::Btdmp(CoreTiming& core_timing) {
    core_timing.RegisterCallbacks(this);
}

Btdmp::~Btdmp() = default;

void Btdmp::Reset() {
    transmit_clock_config = 0;
    transmit_period = 4096;
    transmit_timer = 0;
    transmit_enable = 0;
    transmit_empty = true;
    transmit_full = false;
    transmit_queue = {};

    receive_clock_config = 0;
    receive_period = 4096;
    receive_timer = 0;
    receive_enable = 0;
    receive_empty = true;
    receive_full = false;
    receive_queue = {};
}

void Btdmp::Tick() {
    // TODO: add support for this?
    // I don't think this is ever used on DSi since it's synced to the I2S sample clock
#if 0
    if (transmit_enable) {
        ++transmit_timer;
        if (transmit_timer >= transmit_period) {
            transmit_timer = 0;
            std::array<std::int16_t, 2> sample;
            for (int i = 0; i < 2; ++i) {
                if (transmit_queue.empty()) {
                    //std::printf("BTDMP: transmit buffer underrun\n");
                    sample[i] = 0;
                } else {
                    sample[i] = static_cast<s16>(transmit_queue.front());
                    transmit_queue.pop();
                    transmit_empty = transmit_queue.empty();
                    transmit_full = false;
                    if (transmit_empty)
                        interrupt_handler();
                }
            }
            if (audio_callback) {
                audio_callback(sample);
            }
        }
    }
#endif
}

u64 Btdmp::GetMaxSkip() const {
#if 0
    if (!transmit_enable || transmit_queue.empty()) {
        return Infinity;
    }

    u64 ticks = 0;
    if (transmit_timer < transmit_period) {
        // number of ticks before the tick of the next transmit
        ticks += transmit_period - transmit_timer - 1;
    }

    // number of ticks from the next transmit to the one just before the transmit that empties
    // the buffer
    ticks += ((transmit_queue.size() + 1) / 2 - 1) * transmit_period;

    return ticks;
#endif
    return Infinity;
}

void Btdmp::Skip(u64 ticks) {
#if 0
    if (!transmit_enable)
        return;

    if (transmit_timer >= transmit_period)
        transmit_timer = 0;

    u64 future_timer = transmit_timer + ticks;
    u64 cycles = future_timer / transmit_period;
    transmit_timer = (u16)(future_timer % transmit_period);

    for (u64 c = 0; c < cycles; ++c) {
        std::array<std::int16_t, 2> sample;
        for (int i = 0; i < 2; ++i) {
            if (transmit_queue.empty()) {
                sample[i] = 0;
            } else {
                sample[i] = static_cast<s16>(transmit_queue.front());
                transmit_queue.pop();
                ASSERT(!transmit_queue.empty());
                transmit_full = false;
            }
        }
        if (audio_callback) {
            audio_callback(sample);
        }
    }
#endif
}

void Btdmp::SampleClock(std::int16_t output[2], std::int16_t input) {
    if (transmit_enable && (!(transmit_queue.empty()))) {
        output[0] = (s16)transmit_queue.front();
        transmit_queue.pop();
        output[1] = (s16)transmit_queue.front();
        transmit_queue.pop();

        transmit_empty = transmit_queue.empty();
        transmit_full = false;
        if (transmit_empty)
            interrupt_handler();
    } else {
        output[0] = 0;
        output[1] = 0;
    }

    if (receive_enable) {
        // due to the way the I2S interface works, each mic sample is duplicated
        if (receive_queue.size() < 16)
            receive_queue.push(input);
        if (receive_queue.size() < 16)
            receive_queue.push(input);

        receive_empty = false;
        receive_full = receive_queue.size() == 16;
        if (receive_full)
            interrupt_handler();
    }
}

} // namespace Teakra
