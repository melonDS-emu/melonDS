#pragma once

#include <memory>
#include "common_types.h"
#include "core_timing.h"

namespace Teakra {

class MemoryInterface;

class Processor {
public:
    Processor(CoreTiming& core_timing, MemoryInterface& memory_interface);
    ~Processor();
    void Reset();
    void Run(unsigned cycles);
    void SignalInterrupt(u32 i);
    void SignalVectoredInterrupt(u32 address, bool context_switch);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Teakra
