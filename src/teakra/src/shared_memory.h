#pragma once
#include <array>
#include <cstdio>
#include "common_types.h"

namespace Teakra {
struct SharedMemory {
    u16 ReadWord(u32 word_address) const {
        return read_external16(word_address << 1);
    }
    void WriteWord(u32 word_address, u16 value) {
        write_external16(word_address << 1, value);
    }

    void SetExternalMemoryCallback(
           std::function<u16(u32)> read16, std::function<void(u32, u16)> write16) {

        read_external16 = std::move(read16);
        write_external16 = std::move(write16);
    }

    std::function<u16(u32)> read_external16;
    std::function<void(u32, u16)> write_external16;
};
} // namespace Teakra
