#include <array>
#include <string>
#include <vector>
#include <catch.hpp>
#include "../src/ahbm.h"
#include "../src/dma.h"
#include "../src/shared_memory.h"

TEST_CASE("DMA + AHBM test", "[dma]") {
    Teakra::SharedMemory shared_memory;
    Teakra::Ahbm ahbm;
    Teakra::Dma dma(shared_memory, ahbm);
    std::vector<u8> fcram(0x80);
    dma.SetInterruptHandler([] {});
    ahbm.SetDmaChannel(0, 1);
    ahbm.SetExternalMemoryCallback(
        [&fcram](u32 address) -> u8 {
            REQUIRE(address >= 0x20000000);
            return fcram[address - 0x20000000];
        },
        [&fcram](u32 address, u8 v) {
            REQUIRE(address >= 0x20000000);
            fcram[address - 0x20000000] = v;
        },
        [&fcram](u32 address) -> u16 {
            REQUIRE(address >= 0x20000000);
            return fcram[address - 0x20000000] | ((u16)fcram[address - 0x20000000 + 1] << 8);
        },
        [&fcram](u32 address, u16 v) {
            REQUIRE(address >= 0x20000000);
            fcram[address - 0x20000000 + 0] = (u8)v;
            fcram[address - 0x20000000 + 1] = v >> 8;
        },
        [&fcram](u32 address) -> u32 {
            REQUIRE(address >= 0x20000000);
            return fcram[address - 0x20000000] | ((u32)fcram[address - 0x20000000 + 1] << 8)
                | ((u32)fcram[address - 0x20000000 + 2] << 16) | ((u32)fcram[address - 0x20000000 + 3] << 24);
        },
        [&fcram](u32 address, u32 v) {
            REQUIRE(address >= 0x20000000);
            fcram[address - 0x20000000 + 0] = (u8)v;
            fcram[address - 0x20000000 + 1] = (u8)(v >>  8);
            fcram[address - 0x20000000 + 2] = (u8)(v >> 16);
            fcram[address - 0x20000000 + 3] = (u8)(v >> 24);
        });

    for (u8 i = 0; i < 0x80; ++i) {
        shared_memory.raw[0x40000 + i] = i;
        fcram[i] = i + 0x80;
    }

    auto GetDspTestArea = [&shared_memory]() {
        return std::vector<u8>(shared_memory.raw.begin() + 0x40000,
                               shared_memory.raw.begin() + 0x40000 + 0x80);
    };

    auto GenerateExpected = [](const std::string& str, u8 base = 0) {
        auto stri = str.begin();
        std::vector<u8> result;
        for (u8 i = 0; i < 0x80; ++i) {
            if (stri == str.end()) {
                result.push_back(i + base);
            } else {
                if (*stri == '-') {
                    result.push_back(i + base);
                    stri += 2;
                    continue;
                }
                u8 v = (u8)std::stoi(std::string(stri, stri + 2), 0, 16);
                result.push_back(v);
                stri += 2;
            }
        }
        return result;
    };

    // Configurations and results below are from hwtest

    SECTION("Read from AHBM") {
        dma.SetAddrSrcHigh(0x2000);
        dma.SetAddrDstHigh(0x0000);
        dma.SetSize0(4);
        dma.SetSize1(1);
        dma.SetSize2(1);
        dma.SetSrcSpace(7);
        dma.SetDstSpace(0);
        ahbm.SetDirection(0, 0);

        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8000000082000000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081000082830000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828380818283"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(1);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8283808182838485"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(2);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828384858687"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(3);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8283848586878485"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("00810000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8000000000810000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(1);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("0081000082000000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(2);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8200000000830000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081000080810000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(1);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081000082830000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828380818283"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(1);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828380818283"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(2);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828380818283"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(3);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828384858687"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 2);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828384858687"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("88898A8B8C8D8E8F"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("9091929394959697"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("98999A9B9C9D9E9F"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828384858687"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 2);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081868788898E8F"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("9091969798999E9F"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081868788898E8F"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 2);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081000084850000"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("888900008C8D0000"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081000084850000"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 2);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828384858687"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("88898A8B8C8D8E8F"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8081828384858687"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(1);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 2);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("0000828300008687"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("00008A8B00008E8F"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("0000828300008687"));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 2);
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8000000000810000"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8200000000830000"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8400000000850000"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8600000000870000"));
            dma.DoDma(0);
            REQUIRE(GetDspTestArea() == GenerateExpected("8000000000810000"));
        }
    }

    SECTION("Write to AHBM") {
        dma.SetAddrSrcHigh(0x0000);
        dma.SetAddrDstHigh(0x2000);
        dma.SetSize0(4);
        dma.SetSize1(1);
        dma.SetSize2(1);
        dma.SetSrcSpace(0);
        dma.SetDstSpace(7);
        ahbm.SetDirection(0, 1);

        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2021222324252627", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20--22--24--26--", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20232427", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(3);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20----23----24----27", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(4);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20------22------24------26", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x11);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("22252629", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(1);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("--21222526", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20232427", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(1);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("--2122252627", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(1);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("--21--23--25--27", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(3);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2021--23----2425--27", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(3);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20210000----0000--270000", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(4);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20210000222300002425000026270000", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(2);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2021000024250000", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20230000", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(5);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20210000--230000----0000------00", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(1);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(0);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("--21000026270000", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(1);
            dma.SetDstStep0(1);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2023", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(1);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2027", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20--24", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(3);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20----27", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x11);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(3);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20----27", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x12);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(3);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 0);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("24----2B", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(3);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2021--27", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(2);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20212425", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(1);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2027", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(4);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 1);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("2021----2425", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSize0(8);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(4);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("202122232425262728292A2B2C2D2E2F", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSize0(8);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(5);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 0);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("20212223--270000----2A2B------00", 0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSize0(16);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(4);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 1);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("202122232425262728292A2B2C2D2E2F"
                                              "303132333435363738393A3B3C3D3E3F",
                                              0x80));
        }
        SECTION("") {
            dma.SetAddrSrcLow(0x10);
            dma.SetAddrDstLow(0);
            dma.SetSize0(16);
            dma.SetSrcStep0(2);
            dma.SetDstStep0(8);
            dma.SetDwordMode(1);
            ahbm.SetUnitSize(0, 2);
            ahbm.SetBurstSize(0, 1);
            dma.DoDma(0);
            REQUIRE(fcram == GenerateExpected("202122232425262728292A2B2C2D2E2F"
                                              "--------------------------------"
                                              "303132333435363738393A3B3C3D3E3F",
                                              0x80));
        }
    }
}
