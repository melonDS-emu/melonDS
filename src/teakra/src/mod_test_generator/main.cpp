#include <cstdio>
#include <cstdlib>
#include <memory>
#include "../test.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "A file path argument must be provided. Exiting...\n");
        return -1;
    }

    std::unique_ptr<std::FILE, decltype(&std::fclose)> f{std::fopen(argv[1], "wb"), std::fclose};
    if (!f) {
        std::fprintf(stderr, "Unable to open file %s. Exiting...\n", argv[1]);
        return -2;
    }

    TestCase test_case{};
    test_case.opcode = 0x4DA0; // mpy  y0, MemR04@3 || mpyus y1, MemR04@3offsZI@2 || sub3  p0, p1,
                               // Ax@4 || R04@3stepII2@2
    test_case.expand = 0;
    test_case.before.mod2 = 1; // enable mod for r0; disable brv
    for (u16 i = 0; i < TestSpaceSize; ++i) {
        test_case.before.test_space_x[i] = TestSpaceX + i;
    }
    for (u16 i = 0; i < 0x20; ++i) {
        test_case.before.r[0] = TestSpaceX + i + 0xF0;
        for (u16 legacy = 0; legacy < 2; ++legacy) {
            test_case.before.mod1 = legacy << 13;
            for (u16 offset_mode = 0; offset_mode < 4; ++offset_mode) {
                for (u16 step_mode = 0; step_mode < 8; ++step_mode) {
                    /*!!!*/ if (step_mode == 3)
                        continue;
                    test_case.before.ar[0] = (step_mode << 5) | (offset_mode << 8);
                    u16 step_min = 0, step_max = 0x20;
                    if (step_mode != 3) {
                        step_min = step_max = std::rand() % 0x20;
                        ++step_max;
                    }
                    for (u16 step = step_min; step < step_max; ++step) {
                        u16 step_true = SignExtend<5>(step) & 0x7F;
                        for (u16 mod = 0; mod < 0x10; ++mod) {
                            test_case.before.cfgi = step_true | (mod << 7);
                            if (std::fwrite(&test_case, sizeof(test_case), 1, f.get()) == 0) {
                                std::fprintf(stderr,
                                             "Unable to completely write test case. Exiting...\n");
                                return -3;
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
