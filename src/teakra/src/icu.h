#pragma once

#include <array>
#include <bitset>
#include <functional>
#include <mutex>
#include <utility>
#include "common_types.h"

namespace Teakra {

class ICU {
public:
    using IrqBits = std::bitset<16>;
    u16 GetRequest() const {
        std::lock_guard lock(mutex);
        return (u16)request.to_ulong();
    }
    void Acknowledge(u16 irq_bits) {
        std::lock_guard lock(mutex);
        request &= ~IrqBits(irq_bits);
    }
    u16 GetAcknowledge() {
        return 0;
    }
    void Trigger(u16 irq_bits) {
        std::lock_guard lock(mutex);
        IrqBits bits(irq_bits);
        request |= bits;
        for (u32 irq = 0; irq < 16; ++irq) {
            if (bits[irq]) {
                for (u32 interrupt = 0; interrupt < enabled.size(); ++interrupt) {
                    if (enabled[interrupt][irq]) {
                        on_interrupt(interrupt);
                    }
                }
                if (vectored_enabled[irq]) {
                    on_vectored_interrupt(GetVector(irq), vector_context_switch[irq] != 0);
                }
            }
        }
    }
    u16 GetTrigger() {
        return 0;
    }
    void TriggerSingle(u32 irq) {
        Trigger(1 << irq);
    }
    void SetEnable(u32 interrupt_index, u16 irq_bits) {
        std::lock_guard lock(mutex);
        enabled[interrupt_index] = IrqBits(irq_bits);
    }
    void SetEnableVectored(u16 irq_bits) {
        std::lock_guard lock(mutex);
        vectored_enabled = IrqBits(irq_bits);
    }
    u16 GetEnable(u32 interrupt_index) const {
        std::lock_guard lock(mutex);
        return (u16)enabled[interrupt_index].to_ulong();
    }
    u16 GetEnableVectored() const {
        std::lock_guard lock(mutex);
        return (u16)vectored_enabled.to_ulong();
    }

    u32 GetVector(u32 irq) const {
        return vector_low[irq] | ((u32)vector_high[irq] << 16);
    }

    void SetInterruptHandler(std::function<void(u32)> interrupt,
                             std::function<void(u32, bool)> vectored_interrupt) {
        on_interrupt = std::move(interrupt);
        on_vectored_interrupt = std::move(vectored_interrupt);
    }

    std::array<u16, 16> vector_low, vector_high;
    std::array<u16, 16> vector_context_switch;

private:
    std::function<void(u32)> on_interrupt;
    std::function<void(u32, bool)> on_vectored_interrupt;

    IrqBits request;
    std::array<IrqBits, 3> enabled;
    IrqBits vectored_enabled;
    mutable std::mutex mutex;
};

} // namespace Teakra
