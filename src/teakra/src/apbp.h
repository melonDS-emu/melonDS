#pragma once
#include <array>
#include <functional>
#include <memory>
#include "common_types.h"

namespace Teakra {
class Apbp {
public:
    Apbp();
    ~Apbp();

    void Reset();

    void SendData(unsigned channel, u16 data);
    u16 RecvData(unsigned channel);
    u16 PeekData(unsigned channel) const;
    bool IsDataReady(unsigned channel) const;
    u16 GetDisableInterrupt(unsigned channel) const;
    void SetDisableInterrupt(unsigned channel, u16 v);
    void SetDataHandler(unsigned channel, std::function<void()> handler);

    void SetSemaphore(u16 bits);
    void ClearSemaphore(u16 bits);
    u16 GetSemaphore() const;
    void MaskSemaphore(u16 bits);
    u16 GetSemaphoreMask() const;
    void SetSemaphoreHandler(std::function<void()> handler);

    bool IsSemaphoreSignaled() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace Teakra
