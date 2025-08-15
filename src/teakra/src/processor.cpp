#include "interpreter.h"
#include "processor.h"
#include "register.h"
#include "../../Savestate.h"

namespace Teakra {

struct Processor::Impl {
    Impl(CoreTiming& core_timing, MemoryInterface& memory_interface)
        : core_timing(core_timing), interpreter(core_timing, regs, memory_interface) {}
    CoreTiming& core_timing;
    RegisterState regs;
    Interpreter interpreter;
};

Processor::Processor(CoreTiming& core_timing, MemoryInterface& memory_interface)
    : impl(new Impl(core_timing, memory_interface)) {}
Processor::~Processor() = default;

void Processor::Reset() {
    impl->regs = RegisterState();
    impl->interpreter.Reset();
}

void Processor::DoSavestate(melonDS::Savestate *file) {
    file->Section("TKpr");
    impl->regs.DoSavestate(file);
    impl->interpreter.DoSavestate(file);
}

void Processor::Run(unsigned cycles) {
    impl->interpreter.Run(cycles);
}

void Processor::SignalInterrupt(u32 i) {
    impl->interpreter.SignalInterrupt(i);
}
void Processor::SignalVectoredInterrupt(u32 address, bool context_switch) {
    impl->interpreter.SignalVectoredInterrupt(address, context_switch);
}

} // namespace Teakra
