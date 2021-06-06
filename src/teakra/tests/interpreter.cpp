#include <memory>
#include <catch.hpp>
#include "../src/core_timing.h"
#include "../src/interpreter.h"
#include "../src/memory_interface.h"
#include "../src/shared_memory.h"

TEST_CASE("Cycle accuracy", "[interpreter]") {
    Teakra::CoreTiming core_timing;
    Teakra::SharedMemory shared_memory;
    Teakra::MemoryInterfaceUnit miu;
    Teakra::MemoryInterface memory_interface{shared_memory, miu};
    Teakra::RegisterState regs;
    Teakra::Interpreter interpreter(core_timing, regs, memory_interface);

    regs.pc = 0;
    regs.a[0] = 0;
    regs.a[1] = 0;
    regs.ie = 1;
    regs.im[0] = 1;

    memory_interface.ProgramWrite(0x0000, 0x4180); // br (RESET vector)

    memory_interface.ProgramWrite(0x0006, 0x4180); // br (INT0 vector)
    memory_interface.ProgramWrite(0x0007, 0x2000); // br 0x2000

    for (u32 i = 0; i < 16; ++i) {
        memory_interface.ProgramWrite(0x1000 + i, 0x67D0); // inc a0
        memory_interface.ProgramWrite(0x2000 + i, 0x77D0); // inc a1
    }

    memory_interface.ProgramWrite(0x1010, 0x57F0); // brr -1

    class InterruptGenerator : public Teakra::CoreTiming::Callbacks {
        Teakra::Interpreter& interpreter;
        u64 counter = 0;

    public:
        InterruptGenerator(Teakra::Interpreter& interpreter) : interpreter(interpreter) {}

        void SetCounter(u64 new_counter) {
            counter = new_counter;
        }

        void Tick() override {
            if (counter != 0) {
                --counter;
                if (counter == 0) {
                    interpreter.SignalInterrupt(0);
                }
            }
        }

        u64 GetMaxSkip() const override {
            if (counter != 0) {
                return counter - 1;
            } else {
                return Infinity;
            }
        }

        void Skip(u64 ticks) override {
            if (counter != 0) {
                counter -= ticks;
            }
        }
    };

    InterruptGenerator interrupt_generator(interpreter);
    interrupt_generator.SetCounter(7);
    core_timing.RegisterCallbacks(&interrupt_generator);

    SECTION("Simple counting") {
        memory_interface.ProgramWrite(0x0001, 0x1000);
        interpreter.Run(1 + 5);
        REQUIRE(regs.a[0] == 5);
        REQUIRE(regs.a[1] == 0);
    }

    SECTION("Counting with interrupt") {
        memory_interface.ProgramWrite(0x0001, 0x1000);
        interpreter.Run(1 + 7 + 1 + 3);
        REQUIRE(regs.a[0] == 7);
        REQUIRE(regs.a[1] == 3);
    }

    SECTION("Counting with idle and interrupt") {
        memory_interface.ProgramWrite(0x0001, 0x100D);
        interpreter.Run(1 + 6);
        REQUIRE(regs.a[0] == 3);
        REQUIRE(regs.a[1] == 0);

        interpreter.Run(3);
        REQUIRE(regs.a[0] == 3);
        REQUIRE(regs.a[1] == 1);
    }

    SECTION("Counting with idle and interrupt 2") {
        memory_interface.ProgramWrite(0x0001, 0x100D);
        interpreter.Run(1 + 7);
        REQUIRE(regs.a[0] == 3);
        REQUIRE(regs.a[1] == 0);

        interpreter.Run(3);
        REQUIRE(regs.a[0] == 3);
        REQUIRE(regs.a[1] == 2);
    }

    SECTION("Counting with idle and interrupt 3") {
        memory_interface.ProgramWrite(0x0001, 0x100D);
        interpreter.Run(1 + 7 + 1 + 2);
        REQUIRE(regs.a[0] == 3);
        REQUIRE(regs.a[1] == 2);
    }

    SECTION("Idle with interrupt 1") {
        memory_interface.ProgramWrite(0x0001, 0x1010);
        interpreter.Run(1 + 7 + 1 + 2);
        REQUIRE(regs.a[0] == 0);
        REQUIRE(regs.a[1] == 2);
    }

    SECTION("Idle with interrupt 2") {
        memory_interface.ProgramWrite(0x0001, 0x1010);
        interpreter.Run(1 + 7);
        REQUIRE(regs.a[0] == 0);
        REQUIRE(regs.a[1] == 0);

        interpreter.Run(1 + 2);
        REQUIRE(regs.a[0] == 0);
        REQUIRE(regs.a[1] == 2);
    }

    SECTION("Idle with interrupt 3") {
        memory_interface.ProgramWrite(0x0001, 0x1010);
        interpreter.Run(1 + 6);
        REQUIRE(regs.a[0] == 0);
        REQUIRE(regs.a[1] == 0);

        SECTION("Single step on the interrupt") {
            interpreter.Run(1);
            REQUIRE(regs.a[0] == 0);
            REQUIRE(regs.a[1] == 0);

            interpreter.Run(1 + 2);
            REQUIRE(regs.a[0] == 0);
            REQUIRE(regs.a[1] == 2);
        }

        SECTION("Run over the interrupt") {
            interpreter.Run(1 + 1 + 2);
            REQUIRE(regs.a[0] == 0);
            REQUIRE(regs.a[1] == 2);
        }
    }
}
