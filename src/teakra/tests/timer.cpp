#include <catch.hpp>
#include "../src/timer.h"

struct TimerTestEnvironment {
    Teakra::CoreTiming core_timing;
    Teakra::Timer timer{core_timing};
    int interrupt_counter = 0;
    TimerTestEnvironment() {
        timer.SetInterruptHandler([&]() { interrupt_counter++; });
    }
};

TEST_CASE("Single mode", "[timer]") {
    TimerTestEnvironment env;
    env.timer.count_mode = Teakra::Timer::CountMode::Single;
    env.timer.update_mmio = 1;
    env.timer.start_low = 5;
    env.timer.start_high = 0;
    env.timer.Restart();

    REQUIRE(env.timer.counter_low == 5);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 4);

    env.timer.Tick();
    env.timer.Tick();

    REQUIRE(env.timer.counter_low == 3);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 2);

    env.timer.Skip(2);
    REQUIRE(env.timer.counter_low == 1);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0);

    env.timer.pause = 1;
    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 1);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.pause = 0;
    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 0);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 0);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
}

TEST_CASE("Auto restart mode", "[timer]") {
    TimerTestEnvironment env;
    env.timer.count_mode = Teakra::Timer::CountMode::AutoRestart;
    env.timer.update_mmio = 1;
    env.timer.start_low = 5;
    env.timer.start_high = 0x1234;
    env.timer.Restart();

    REQUIRE(env.timer.counter_low == 5);
    REQUIRE(env.timer.counter_high == 0x1234);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0x12340004);

    env.timer.Tick();
    env.timer.Tick();

    REQUIRE(env.timer.counter_low == 3);
    REQUIRE(env.timer.counter_high == 0x1234);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0x12340002);

    env.timer.Skip(0x12340002);
    REQUIRE(env.timer.counter_low == 1);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0);

    env.timer.pause = 1;
    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 1);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.pause = 0;
    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 0);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == 0x12340005);

    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 5);
    REQUIRE(env.timer.counter_high == 0x1234);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == 0x12340004);
}

TEST_CASE("Free running mode", "[timer]") {
    TimerTestEnvironment env;
    // Set to Single most first to reset counter
    env.timer.count_mode = Teakra::Timer::CountMode::Single;
    env.timer.update_mmio = 1;
    env.timer.start_low = 5;
    env.timer.start_high = 0x1234;
    env.timer.Restart();
    env.timer.count_mode = Teakra::Timer::CountMode::FreeRunning;

    REQUIRE(env.timer.counter_low == 5);
    REQUIRE(env.timer.counter_high == 0x1234);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0x12340004);

    env.timer.Restart();

    REQUIRE(env.timer.counter_low == 5);
    REQUIRE(env.timer.counter_high == 0x1234);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0x12340004);

    env.timer.Tick();
    env.timer.Tick();

    REQUIRE(env.timer.counter_low == 3);
    REQUIRE(env.timer.counter_high == 0x1234);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0x12340002);

    env.timer.Skip(0x12340002);
    REQUIRE(env.timer.counter_low == 1);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == 0);

    env.timer.pause = 1;
    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 1);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.pause = 0;
    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 0);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == 0xFFFFFFFF);

    env.timer.Tick();
    REQUIRE(env.timer.counter_low == 0xFFFF);
    REQUIRE(env.timer.counter_high == 0xFFFF);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == 0xFFFFFFFE);
}

TEST_CASE("Event counting restart mode", "[timer]") {
    TimerTestEnvironment env;
    env.timer.count_mode = Teakra::Timer::CountMode::EventCount;
    env.timer.update_mmio = 1;
    env.timer.start_low = 5;
    env.timer.start_high = 0;
    env.timer.Restart();

    REQUIRE(env.timer.counter_low == 5);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.Tick();

    REQUIRE(env.timer.counter_low == 5);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.TickEvent();

    REQUIRE(env.timer.counter_low == 4);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.pause = 1;
    env.timer.TickEvent();

    REQUIRE(env.timer.counter_low == 4);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.pause = 0;
    env.timer.TickEvent();
    env.timer.TickEvent();
    env.timer.TickEvent();

    REQUIRE(env.timer.counter_low == 1);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.TickEvent();

    REQUIRE(env.timer.counter_low == 0);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.timer.TickEvent();

    REQUIRE(env.timer.counter_low == 0);
    REQUIRE(env.timer.counter_high == 0);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.timer.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
}
