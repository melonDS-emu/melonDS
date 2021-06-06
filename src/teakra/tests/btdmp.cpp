#include <catch.hpp>
#include <queue>
#include "../src/btdmp.h"

struct BtdmpTestEnvironment {
    Teakra::CoreTiming core_timing;
    Teakra::Btdmp btdmp{core_timing};
    int interrupt_counter = 0;
    std::queue<std::array<std::int16_t, 2>> sample_queue;
    BtdmpTestEnvironment() {
        btdmp.SetInterruptHandler([&]() { interrupt_counter++; });
        btdmp.SetAudioCallback(
            [&](std::array<std::int16_t, 2> samples) { sample_queue.push(samples); });
    }
};

TEST_CASE("Btdmp queueing", "[btdmp]") {
    BtdmpTestEnvironment env;

    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    env.btdmp.SetTransmitEnable(1);
    env.btdmp.SetTransmitPeriod(1000);

    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);

    env.btdmp.Skip(3050);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    REQUIRE(env.sample_queue.size() == 3);
    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());
    for (int i = 0; i < 3; ++i) {
        auto samples = env.sample_queue.front();
        env.sample_queue.pop();
        REQUIRE(samples == std::array<std::int16_t, 2>{{0, 0}});
    }

    env.btdmp.Skip(949);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    REQUIRE(env.sample_queue.size() == 0);
    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Tick();
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    REQUIRE(env.sample_queue.size() == 1);
    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());
    auto samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{0, 0}});

    env.btdmp.Skip(999);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    REQUIRE(env.sample_queue.size() == 0);
    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Skip(1);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    REQUIRE(env.sample_queue.size() == 1);
    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{0, 0}});

    env.btdmp.Skip(100);
    env.btdmp.Send(0x1234);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.btdmp.GetMaxSkip() == 899);
    REQUIRE(env.sample_queue.size() == 0);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Skip(899);
    REQUIRE(env.interrupt_counter == 0);
    REQUIRE(env.btdmp.GetMaxSkip() == 0);
    REQUIRE(env.sample_queue.size() == 0);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Tick();
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    REQUIRE(env.sample_queue.size() == 1);
    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{0x1234, 0}});

    env.btdmp.Skip(100);
    env.btdmp.Send(11);
    env.btdmp.Send(22);
    env.btdmp.Send(33);
    env.btdmp.Send(44);
    env.btdmp.Send(55);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == 900 + 1000 + 1000 - 1);
    REQUIRE(env.sample_queue.size() == 0);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Skip(1500);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == 900 + 1000 + 1000 - 1 - 1500);
    REQUIRE(env.sample_queue.size() == 1);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{11, 22}});

    for (int i = 0; i < 13; ++i) {
        REQUIRE(!env.btdmp.GetTransmitFull());
        env.btdmp.Send(i);
    }

    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == 1000 * 8 - 600 - 1);
    REQUIRE(env.sample_queue.size() == 0);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(env.btdmp.GetTransmitFull());

    for (int i = 0; i < 4567; ++i) {
        env.btdmp.Tick();
    }
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == 1000 * 8 - 600 - 1 - 4567);
    REQUIRE(env.sample_queue.size() == 5);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Skip(1000 * 7 - 600 - 1 - 4567);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == 1000);
    REQUIRE(env.sample_queue.size() == 6);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Tick();
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == 999);
    REQUIRE(env.sample_queue.size() == 7);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{33, 44}});
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{55, 0}});
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{1, 2}});
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{3, 4}});
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{5, 6}});
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{7, 8}});
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{9, 10}});

    env.btdmp.Skip(999);
    REQUIRE(env.interrupt_counter == 1);
    REQUIRE(env.btdmp.GetMaxSkip() == 0);
    REQUIRE(env.sample_queue.size() == 0);
    REQUIRE(!env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());

    env.btdmp.Tick();
    REQUIRE(env.interrupt_counter == 2);
    REQUIRE(env.btdmp.GetMaxSkip() == Teakra::CoreTiming::Callbacks::Infinity);
    REQUIRE(env.sample_queue.size() == 1);
    REQUIRE(env.btdmp.GetTransmitEmpty());
    REQUIRE(!env.btdmp.GetTransmitFull());
    samples = env.sample_queue.front();
    env.sample_queue.pop();
    REQUIRE(samples == std::array<std::int16_t, 2>{{11, 12}});
}
