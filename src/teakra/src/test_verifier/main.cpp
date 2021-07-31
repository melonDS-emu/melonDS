#include <cinttypes>
#include <cstdio>
#include <iomanip>
#include <memory>
#include <teakra/disassembler.h>
#include "../core_timing.h"
#include "../interpreter.h"
#include "../memory_interface.h"
#include "../shared_memory.h"
#include "../test.h"

std::string Flag16ToString(u16 value, const char* symbols) {
    std::string result = symbols;
    for (int i = 0; i < 16; ++i) {
        if ((value >> i & 1) == 0)
            result[15 - i] = '-';
    }
    return result;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "A filename argument must be provided. Exiting...\n");
        return -1;
    }

    std::unique_ptr<std::FILE, decltype(&std::fclose)> file{std::fopen(argv[1], "rb"), std::fclose};
    if (!file) {
        std::fprintf(stderr, "Unable to open file %s. Exiting...\n", argv[1]);
        return -2;
    }

    Teakra::CoreTiming core_timing;
    Teakra::SharedMemory shared_memory;
    Teakra::MemoryInterfaceUnit miu;
    Teakra::MemoryInterface memory_interface{shared_memory, miu};
    Teakra::RegisterState regs;
    Teakra::Interpreter interpreter(core_timing, regs, memory_interface);

    int i = 0;
    int passed = 0;
    int total = 0;
    int skipped = 0;
    while (true) {
        TestCase test_case;
        if (std::fread(&test_case, sizeof(test_case), 1, file.get()) == 0) {
            break;
        }
        regs.Reset();
        regs.a = test_case.before.a;
        regs.b = test_case.before.b;
        regs.p = test_case.before.p;
        regs.r = test_case.before.r;
        regs.x = test_case.before.x;
        regs.y = test_case.before.y;
        regs.stepi0 = test_case.before.stepi0;
        regs.stepj0 = test_case.before.stepj0;
        regs.mixp = test_case.before.mixp;
        regs.sv = test_case.before.sv;
        regs.repc = test_case.before.repc;
        regs.Lc() = test_case.before.lc;
        regs.Set<Teakra::cfgi>(test_case.before.cfgi);
        regs.Set<Teakra::cfgj>(test_case.before.cfgj);
        regs.Set<Teakra::stt0>(test_case.before.stt0);
        regs.Set<Teakra::stt1>(test_case.before.stt1);
        regs.Set<Teakra::stt2>(test_case.before.stt2);
        regs.Set<Teakra::mod0>(test_case.before.mod0);
        regs.Set<Teakra::mod1>(test_case.before.mod1);
        regs.Set<Teakra::mod2>(test_case.before.mod2);
        regs.Set<Teakra::ar0>(test_case.before.ar[0]);
        regs.Set<Teakra::ar1>(test_case.before.ar[1]);
        regs.Set<Teakra::arp0>(test_case.before.arp[0]);
        regs.Set<Teakra::arp1>(test_case.before.arp[1]);
        regs.Set<Teakra::arp2>(test_case.before.arp[2]);
        regs.Set<Teakra::arp3>(test_case.before.arp[3]);

        for (u16 offset = 0; offset < TestSpaceSize; ++offset) {
            memory_interface.DataWrite(TestSpaceX + offset, test_case.before.test_space_x[offset]);
            memory_interface.DataWrite(TestSpaceY + offset, test_case.before.test_space_y[offset]);
        }

        memory_interface.ProgramWrite(0, test_case.opcode);
        memory_interface.ProgramWrite(1, test_case.expand);

        bool pass = true;
        bool skip = false;
        try {
            interpreter.Run(1);
            auto Check40 = [&](const char* name, u64 expected, u64 actual) {
                if (expected != actual) {
                    std::printf("Mismatch: %s: %010" PRIx64 " != %010" PRIx64 "\n", name,
                                expected & 0xFF'FFFF'FFFF, actual & 0xFF'FFFF'FFFF);
                    pass = false;
                }
            };

            auto Check32 = [&](const char* name, u32 expected, u32 actual) {
                if (expected != actual) {
                    std::printf("Mismatch: %s: %08X != %08X\n", name, expected, actual);
                    pass = false;
                }
            };

            auto Check = [&](const char* name, u16 expected, u16 actual) {
                if (expected != actual) {
                    std::printf("Mismatch: %s: %04X != %04X\n", name, expected, actual);
                    pass = false;
                }
            };

            auto CheckAddress = [&](const char* name, u16 address, u16 expected, u16 actual) {
                if (expected != actual) {
                    std::printf("Mismatch: %s%04X: %04X != %04X\n", name, address, expected,
                                actual);
                    pass = false;
                }
            };

            auto CheckFlag = [&](const char* name, u16 expected, u16 actual, const char* symbols) {
                if (expected != actual) {
                    std::printf("Mismatch: %s: %s != %s\n", name,
                                Flag16ToString(expected, symbols).c_str(),
                                Flag16ToString(actual, symbols).c_str());
                    pass = false;
                }
            };

            Check40("a0", SignExtend<40>(test_case.after.a[0]), regs.a[0]);
            Check40("a1", SignExtend<40>(test_case.after.a[1]), regs.a[1]);
            Check40("b0", SignExtend<40>(test_case.after.b[0]), regs.b[0]);
            Check40("b1", SignExtend<40>(test_case.after.b[1]), regs.b[1]);
            Check32("p0", test_case.after.p[0], regs.p[0]);
            Check32("p1", test_case.after.p[1], regs.p[1]);
            Check("r0", test_case.after.r[0], regs.r[0]);
            Check("r1", test_case.after.r[1], regs.r[1]);
            Check("r2", test_case.after.r[2], regs.r[2]);
            Check("r3", test_case.after.r[3], regs.r[3]);
            Check("r4", test_case.after.r[4], regs.r[4]);
            Check("r5", test_case.after.r[5], regs.r[5]);
            Check("r6", test_case.after.r[6], regs.r[6]);
            Check("r7", test_case.after.r[7], regs.r[7]);
            Check("x0", test_case.after.x[0], regs.x[0]);
            Check("x1", test_case.after.x[1], regs.x[1]);
            Check("y0", test_case.after.y[0], regs.y[0]);
            Check("y1", test_case.after.y[1], regs.y[1]);
            Check("stepi0", test_case.after.stepi0, regs.stepi0);
            Check("stepj0", test_case.after.stepj0, regs.stepj0);
            Check("mixp", test_case.after.mixp, regs.mixp);
            Check("sv", test_case.after.sv, regs.sv);
            Check("repc", test_case.after.repc, regs.repc);
            Check("lc", test_case.after.lc, regs.Lc());
            CheckFlag("cfgi", test_case.after.cfgi, regs.Get<Teakra::cfgi>(), "mmmmmmmmmsssssss");
            CheckFlag("cfgj", test_case.after.cfgj, regs.Get<Teakra::cfgj>(), "mmmmmmmmmsssssss");
            CheckFlag("stt0", test_case.after.stt0, regs.Get<Teakra::stt0>(), "####C###ZMNVCELL");
            CheckFlag("stt1", test_case.after.stt1, regs.Get<Teakra::stt1>(), "QP#########R####");
            CheckFlag("stt2", test_case.after.stt2, regs.Get<Teakra::stt2>(), "LBBB####mm##V21I");
            CheckFlag("mod0", test_case.after.mod0, regs.Get<Teakra::mod0>(), "#QQ#PPooSYY###SS");
            CheckFlag("mod1", test_case.after.mod1, regs.Get<Teakra::mod1>(), "???B####pppppppp");
            CheckFlag("mod2", test_case.after.mod2, regs.Get<Teakra::mod2>(), "7654321m7654321M");
            CheckFlag("ar0", test_case.after.ar[0], regs.Get<Teakra::ar0>(), "RRRRRRoosssoosss");
            CheckFlag("ar1", test_case.after.ar[1], regs.Get<Teakra::ar1>(), "RRRRRRoosssoosss");
            CheckFlag("arp0", test_case.after.arp[0], regs.Get<Teakra::arp0>(), "#RR#RRjjjjjiiiii");
            CheckFlag("arp1", test_case.after.arp[1], regs.Get<Teakra::arp1>(), "#RR#RRjjjjjiiiii");
            CheckFlag("arp2", test_case.after.arp[2], regs.Get<Teakra::arp2>(), "#RR#RRjjjjjiiiii");
            CheckFlag("arp3", test_case.after.arp[3], regs.Get<Teakra::arp3>(), "#RR#RRjjjjjiiiii");

            for (u16 offset = 0; offset < TestSpaceSize; ++offset) {
                CheckAddress("memory_", (TestSpaceX + offset), test_case.after.test_space_x[offset],
                             memory_interface.DataRead(TestSpaceX + offset));
                CheckAddress("memory_", (TestSpaceY + offset), test_case.after.test_space_y[offset],
                             memory_interface.DataRead(TestSpaceY + offset));
            }
            ++total;
        } catch (const Teakra::UnimplementedException&) {
            std::printf("Skipped one unimplemented case\n");
            pass = false;
            skip = true;
            ++skipped;
        }

        if (pass) {
            ++passed;
        } else {
            Teakra::Disassembler::ArArpSettings ar_arp;
            ar_arp.ar = test_case.before.ar;
            ar_arp.arp = test_case.before.arp;
            std::printf(
                "Test case %d: %04X %04X %s\n", i, test_case.opcode, test_case.expand,
                Teakra::Disassembler::Do(test_case.opcode, test_case.expand, ar_arp).c_str());
            if (!skip) {
                std::printf("before:\n");
                std::printf("a0 = %010" PRIx64 "; a1 = %010" PRIx64 "\n",
                            test_case.before.a[0] & 0xFF'FFFF'FFFF,
                            test_case.before.a[1] & 0xFF'FFFF'FFFF);
                std::printf("b0 = %010" PRIx64 "; b1 = %010" PRIx64 "\n",
                            test_case.before.b[0] & 0xFF'FFFF'FFFF,
                            test_case.before.b[1] & 0xFF'FFFF'FFFF);
                std::printf("p0 = %08X; p1 = %08X\n", test_case.before.p[0], test_case.before.p[1]);
                std::printf("x0 = %04X; x1 = %04X\n", test_case.before.x[0], test_case.before.x[1]);
                std::printf("y0 = %04X; y1 = %04X\n", test_case.before.y[0], test_case.before.y[1]);
                std::printf("r0 = %04X; r1 = %04X; r2 = %04X; r3 = %04X\n", test_case.before.r[0],
                            test_case.before.r[1], test_case.before.r[2], test_case.before.r[3]);
                std::printf("r4 = %04X; r5 = %04X; r6 = %04X; r7 = %04X\n", test_case.before.r[4],
                            test_case.before.r[5], test_case.before.r[6], test_case.before.r[7]);
                std::printf("stepi0 = %04X\n", test_case.before.stepi0);
                std::printf("stepj0 = %04X\n", test_case.before.stepj0);
                std::printf("mixp = %04X\n", test_case.before.mixp);
                std::printf("sv = %04X\n", test_case.before.sv);
                std::printf("repc = %04X\n", test_case.before.repc);
                std::printf("lc = %04X\n", test_case.before.lc);
                std::printf("cfgi = %s\n",
                            Flag16ToString(test_case.before.cfgi, "mmmmmmmmmsssssss").c_str());
                std::printf("cfgj = %s\n",
                            Flag16ToString(test_case.before.cfgj, "mmmmmmmmmsssssss").c_str());
                std::printf("stt0 = %s\n",
                            Flag16ToString(test_case.before.stt0, "####C###ZMNVCELL").c_str());
                std::printf("stt1 = %s\n",
                            Flag16ToString(test_case.before.stt1, "QP#########R####").c_str());
                std::printf("stt2 = %s\n",
                            Flag16ToString(test_case.before.stt2, "LBBB####mm##V21I").c_str());
                std::printf("mod0 = %s\n",
                            Flag16ToString(test_case.before.mod0, "#QQ#PPooSYY###SS").c_str());
                std::printf("mod1 = %s\n",
                            Flag16ToString(test_case.before.mod1, "jicB####pppppppp").c_str());
                std::printf("mod2 = %s\n",
                            Flag16ToString(test_case.before.mod2, "7654321m7654321M").c_str());
                std::printf("ar0 = %s\n",
                            Flag16ToString(test_case.before.ar[0], "RRRRRRoosssoosss").c_str());
                std::printf("ar1 = %s\n",
                            Flag16ToString(test_case.before.ar[1], "RRRRRRoosssoosss").c_str());
                std::printf("arp0 = %s\n",
                            Flag16ToString(test_case.before.arp[0], "#RR#RRiiiiijjjjj").c_str());
                std::printf("arp1 = %s\n",
                            Flag16ToString(test_case.before.arp[1], "#RR#RRiiiiijjjjj").c_str());
                std::printf("arp2 = %s\n",
                            Flag16ToString(test_case.before.arp[2], "#RR#RRiiiiijjjjj").c_str());
                std::printf("arp3 = %s\n",
                            Flag16ToString(test_case.before.arp[3], "#RR#RRiiiiijjjjj").c_str());
                std::printf("FAILED\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
            }
        }

        ++i;
    }

    std::printf("%d / %d passed, %d skipped\n", passed, total, skipped);

    if (passed < total) {
        return 1;
    }

    return 0;
}
