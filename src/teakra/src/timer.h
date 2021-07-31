#pragma once

#include <functional>
#include <utility>
#include "common_types.h"
#include "core_timing.h"

namespace Teakra {

class Timer : public CoreTiming::Callbacks {
public:
    Timer(CoreTiming& core_timing);

    enum class CountMode : u16 {
        Single = 0,
        AutoRestart = 1,
        FreeRunning = 2,
        EventCount = 3,
    };

    void Reset();

    void Restart();
    void Tick() override;
    void TickEvent();
    u64 GetMaxSkip() const override;
    void Skip(u64 ticks) override;

    u16 update_mmio = 0;
    u16 pause = 0;
    CountMode count_mode = CountMode::Single;
    u16 scale = 0;

    u16 start_high = 0;
    u16 start_low = 0;
    u32 counter = 0;
    u16 counter_high = 0;
    u16 counter_low = 0;

    void SetInterruptHandler(std::function<void()> handler) {
        interrupt_handler = std::move(handler);
    }

private:
    std::function<void()> interrupt_handler;

    void UpdateMMIO();

    class TimerTimingCallbacks;
};

} // namespace Teakra
