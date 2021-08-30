#pragma once

#include <array>
#include <type_traits>

#ifndef COMMON_TYPE_3DS
#include "common_types.h"
#endif

constexpr u16 TestSpaceX = 0x6400;
constexpr u16 TestSpaceY = 0xCC00;
constexpr u16 TestSpaceSize = 0x0200;

struct State {
    std::array<u64, 2> a, b;
    std::array<u32, 2> p;
    std::array<u16, 8> r;
    std::array<u16, 2> x, y;
    u16 stepi0, stepj0, mixp, sv, repc, lc;
    u16 cfgi, cfgj;
    u16 stt0, stt1, stt2;
    u16 mod0, mod1, mod2;
    std::array<u16, 2> ar;
    std::array<u16, 4> arp;

    std::array<u16, TestSpaceSize> test_space_x;
    std::array<u16, TestSpaceSize> test_space_y;
};

static_assert(std::is_trivially_copyable_v<State>);

struct TestCase {
    State before, after;
    u16 opcode, expand;
};

static_assert(sizeof(TestCase) == 4312);
static_assert(std::is_trivially_copyable_v<TestCase>);
