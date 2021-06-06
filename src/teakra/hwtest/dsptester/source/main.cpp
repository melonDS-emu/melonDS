#include <string>
#include <unordered_map>
#include <vector>
#include <3ds.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "cdc_bin.h"

#define COMMON_TYPE_3DS
#include "../../src/test.h"

PrintConsole topScreen, bottomScreen;

void MoveCursor(unsigned row, unsigned col) {
    printf("\x1b[%u;%uH", row + 1, col + 1);
}

enum Color {
    Reset = 0,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magnenta = 35,
    Cyan = 36,
    White = 37,
};
void SetColor(Color color, Color background) {
    printf("\x1b[%dm\x1b[%dm", (int)color, (int)background + 10);
}

class IReg {
public:
    IReg(const std::string& name, const std::string& flags) : name(name), flags(flags) {}
    virtual ~IReg() = default;
    virtual unsigned GetLength() = 0;
    virtual unsigned GetSrcDigit(unsigned pos) = 0;
    virtual unsigned GetDstDigit(unsigned pos) = 0;
    virtual void SetSrcDigit(unsigned pos, unsigned value) = 0;
    virtual unsigned GetDigitRange() = 0;
    void Print(unsigned row, unsigned col, unsigned selected) {
        SetColor(Cyan, Black);
        MoveCursor(row, col - name.size());
        printf("%s", name.c_str());
        unsigned len = GetLength();
        for (unsigned i = 0; i < len; ++i) {
            unsigned src = GetSrcDigit(len - i - 1);
            unsigned dst = GetDstDigit(len - i - 1);
            MoveCursor(row, col + i);
            if (len - i - 1 == selected)
                SetColor(Black, White);
            else
                SetColor(White, Black);
            if (flags.empty())
                printf("%X", src);
            else
                printf("%c", src ? flags[i] : '-');
            MoveCursor(row + 1, col + i);
            SetColor(src == dst ? Magnenta : Green, Black);
            if (flags.empty())
                printf("%X", dst);
            else
                printf("%c", dst ? flags[i] : '-');
        }
    }

private:
    std::string name, flags;
};

class HexReg : public IReg {
public:
    HexReg(const std::string& name, u16& src, u16& dst) : IReg(name, ""), src(src), dst(dst) {
        src = dst;
    }
    unsigned GetLength() override {
        return 4;
    }
    unsigned GetSrcDigit(unsigned pos) override {
        return (src >> (pos * 4)) & 0xF;
    }
    unsigned GetDstDigit(unsigned pos) override {
        return (dst >> (pos * 4)) & 0xF;
    }
    void SetSrcDigit(unsigned pos, unsigned value) override {
        src &= ~(0xF << (pos * 4));
        src |= value << (pos * 4);
    }
    unsigned GetDigitRange() override {
        return 16;
    }

private:
    u16& src;
    u16& dst;
};

class BinReg : public IReg {
public:
    BinReg(const std::string& name, u16& src, u16& dst, const std::string flags)
        : IReg(name, flags), src(src), dst(dst) {
        src = dst;
    }
    unsigned GetLength() override {
        return 16;
    }
    unsigned GetSrcDigit(unsigned pos) override {
        return (src >> pos) & 1;
    }
    unsigned GetDstDigit(unsigned pos) override {
        return (dst >> pos) & 1;
    }
    void SetSrcDigit(unsigned pos, unsigned value) override {
        src &= ~(1 << pos);
        src |= value << pos;
    }
    unsigned GetDigitRange() override {
        return 2;
    }

private:
    u16& src;
    u16& dst;
};

u16* dspP = (u16*)0x1FF00000;
u16* dspD = (u16*)0x1FF40000;

u16* srcBase = &dspD[0x2000];
u16* dstBase = &dspD[0x2FD0];

constexpr u32 reg_region_size = 0x60;

std::unordered_map<std::string, unsigned> reg_map;

IReg* MakeHexReg(const std::string& name, unsigned offset) {
    reg_map[name] = offset;
    return new HexReg(name, srcBase[offset], dstBase[offset]);
}

IReg* MakeBinReg(const std::string& name, unsigned offset, const std::string flags = "") {
    reg_map[name] = offset;
    return new BinReg(name, srcBase[offset], dstBase[offset], flags);
}

constexpr unsigned t_row = 15;
constexpr unsigned t_col = 4;

IReg* grid[t_row][t_col] = {};

unsigned c_row = 0, c_col = 0, c_pos = 0;

void PrintGrid(unsigned row, unsigned col) {
    IReg* r = grid[row][col];
    if (!r)
        return;
    r->Print(row * 2, 4 + col * 9, row == c_row && col == c_col ? c_pos : 0xFFFFFFFF);
}

void PrintAll() {
    for (unsigned row = 0; row < t_row; ++row)
        for (unsigned col = 0; col < t_col; ++col)
            PrintGrid(row, col);
}

void FlushCache(void* ptr, u32 size) {
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, ptr, size);
}

void InvalidateCache(void* ptr, u32 size) {
    svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, ptr, size);
}

void StartDspProgram() {
    dspD[1] = 1;
    FlushCache(&dspD[1], 2);
    while (dspD[1])
        InvalidateCache(&dspD[1], 2);
}

void StopDspProgram() {
    dspD[2] = 1;
    FlushCache(&dspD[2], 2);
    while (dspD[2])
        InvalidateCache(&dspD[2], 2);
}

void SetOnshotDspProgram() {
    dspD[3] = 1;
    FlushCache(&dspD[3], 2);
}

void PulseDspProgram() {
    SetOnshotDspProgram();
    StartDspProgram();
    StopDspProgram();
}

void ExecuteTestCases() {
    consoleSelect(&bottomScreen);
    printf("--------\nExecuting test cases...!\n");

    FILE* fi = fopen("teaklite2_tests", "rb");
    if (!fi)
        printf("failed to open input file\n");
    FILE* fo = fopen("teaklite2_tests_result", "wb");
    if (!fo)
        printf("failed to open output file\n");

    StopDspProgram();
    int i = 0;
    while (true) {
        TestCase test_case;
        if (!fread(&test_case, sizeof(test_case), 1, fi))
            break;

        srcBase[reg_map.at("a0l")] = test_case.before.a[0] & 0xFFFF;
        srcBase[reg_map.at("a0h")] = (test_case.before.a[0] >> 16) & 0xFFFF;
        srcBase[reg_map.at("a0e")] = (test_case.before.a[0] >> 32) & 0xFFFF;
        srcBase[reg_map.at("a1l")] = test_case.before.a[1] & 0xFFFF;
        srcBase[reg_map.at("a1h")] = (test_case.before.a[1] >> 16) & 0xFFFF;
        srcBase[reg_map.at("a1e")] = (test_case.before.a[1] >> 32) & 0xFFFF;
        srcBase[reg_map.at("b0l")] = test_case.before.b[0] & 0xFFFF;
        srcBase[reg_map.at("b0h")] = (test_case.before.b[0] >> 16) & 0xFFFF;
        srcBase[reg_map.at("b0e")] = (test_case.before.b[0] >> 32) & 0xFFFF;
        srcBase[reg_map.at("b1l")] = test_case.before.b[1] & 0xFFFF;
        srcBase[reg_map.at("b1h")] = (test_case.before.b[1] >> 16) & 0xFFFF;
        srcBase[reg_map.at("b1e")] = (test_case.before.b[1] >> 32) & 0xFFFF;
        srcBase[reg_map.at("p0l")] = test_case.before.p[0] & 0xFFFF;
        srcBase[reg_map.at("p0h")] = (test_case.before.p[0] >> 16) & 0xFFFF;
        srcBase[reg_map.at("p1l")] = test_case.before.p[1] & 0xFFFF;
        srcBase[reg_map.at("p1h")] = (test_case.before.p[1] >> 16) & 0xFFFF;
        srcBase[reg_map.at("r0")] = test_case.before.r[0];
        srcBase[reg_map.at("r1")] = test_case.before.r[1];
        srcBase[reg_map.at("r2")] = test_case.before.r[2];
        srcBase[reg_map.at("r3")] = test_case.before.r[3];
        srcBase[reg_map.at("r4")] = test_case.before.r[4];
        srcBase[reg_map.at("r5")] = test_case.before.r[5];
        srcBase[reg_map.at("r6")] = test_case.before.r[6];
        srcBase[reg_map.at("r7")] = test_case.before.r[7];
        srcBase[reg_map.at("x0")] = test_case.before.x[0];
        srcBase[reg_map.at("y0")] = test_case.before.y[0];
        srcBase[reg_map.at("x1")] = test_case.before.x[1];
        srcBase[reg_map.at("y1")] = test_case.before.y[1];
        srcBase[reg_map.at("sti0")] = test_case.before.stepi0;
        srcBase[reg_map.at("stj0")] = test_case.before.stepj0;
        srcBase[reg_map.at("mixp")] = test_case.before.mixp;
        srcBase[reg_map.at("sv")] = test_case.before.sv;
        srcBase[reg_map.at("repc")] = test_case.before.repc;
        srcBase[reg_map.at("lc")] = test_case.before.lc;
        srcBase[reg_map.at("cfgi")] = test_case.before.cfgi;
        srcBase[reg_map.at("cfgj")] = test_case.before.cfgj;
        srcBase[reg_map.at("stt0")] = test_case.before.stt0;
        srcBase[reg_map.at("stt1")] = test_case.before.stt1;
        srcBase[reg_map.at("stt2")] = test_case.before.stt2;
        srcBase[reg_map.at("mod0")] = test_case.before.mod0;
        srcBase[reg_map.at("mod1")] = test_case.before.mod1;
        srcBase[reg_map.at("mod2")] = test_case.before.mod2;
        srcBase[reg_map.at("ar0")] = test_case.before.ar[0];
        srcBase[reg_map.at("ar1")] = test_case.before.ar[1];
        srcBase[reg_map.at("arp0")] = test_case.before.arp[0];
        srcBase[reg_map.at("arp1")] = test_case.before.arp[1];
        srcBase[reg_map.at("arp2")] = test_case.before.arp[2];
        srcBase[reg_map.at("arp3")] = test_case.before.arp[3];

        for (u16 i = 0; i < TestSpaceSize; ++i) {
            dspD[TestSpaceX + i] = test_case.before.test_space_x[i];
            dspD[TestSpaceY + i] = test_case.before.test_space_y[i];
        }

        InvalidateCache(&dspP[0x2000], 0x2000);

        dspP[0x2000] = test_case.opcode;
        dspP[0x2001] = test_case.expand;
        dspP[0x2002] = 0x0000; // nop
        dspP[0x2003] = 0x0000; // nop
        dspP[0x2004] = 0x0000; // nop
        dspP[0x2005] = 0x4180; // br 0x1800
        dspP[0x2006] = 0x1800;

        FlushCache(&dspP[0x2000], 0x2000);

        FlushCache(&dspD[0], 0x20000);
        InvalidateCache(&dspD[0], 0x20000);

        PulseDspProgram();

        FlushCache(&dspD[0], 0x20000);
        InvalidateCache(&dspD[0], 0x20000);

        test_case.after.a[0] = dstBase[reg_map.at("a0l")] |
                               ((u64)dstBase[reg_map.at("a0h")] << 16) |
                               ((u64)dstBase[reg_map.at("a0e")] << 32);
        test_case.after.a[1] = dstBase[reg_map.at("a1l")] |
                               ((u64)dstBase[reg_map.at("a1h")] << 16) |
                               ((u64)dstBase[reg_map.at("a1e")] << 32);
        test_case.after.b[0] = dstBase[reg_map.at("b0l")] |
                               ((u64)dstBase[reg_map.at("b0h")] << 16) |
                               ((u64)dstBase[reg_map.at("b0e")] << 32);
        test_case.after.b[1] = dstBase[reg_map.at("b1l")] |
                               ((u64)dstBase[reg_map.at("b1h")] << 16) |
                               ((u64)dstBase[reg_map.at("b1e")] << 32);
        test_case.after.p[0] = dstBase[reg_map.at("p0l")] | ((u64)dstBase[reg_map.at("p0h")] << 16);
        test_case.after.p[1] = dstBase[reg_map.at("p1l")] | ((u64)dstBase[reg_map.at("p1h")] << 16);
        test_case.after.r[0] = dstBase[reg_map.at("r0")];
        test_case.after.r[1] = dstBase[reg_map.at("r1")];
        test_case.after.r[2] = dstBase[reg_map.at("r2")];
        test_case.after.r[3] = dstBase[reg_map.at("r3")];
        test_case.after.r[4] = dstBase[reg_map.at("r4")];
        test_case.after.r[5] = dstBase[reg_map.at("r5")];
        test_case.after.r[6] = dstBase[reg_map.at("r6")];
        test_case.after.r[7] = dstBase[reg_map.at("r7")];
        test_case.after.x[0] = dstBase[reg_map.at("x0")];
        test_case.after.y[0] = dstBase[reg_map.at("y0")];
        test_case.after.x[1] = dstBase[reg_map.at("x1")];
        test_case.after.y[1] = dstBase[reg_map.at("y1")];
        test_case.after.stepi0 = dstBase[reg_map.at("sti0")];
        test_case.after.stepj0 = dstBase[reg_map.at("stj0")];
        test_case.after.mixp = dstBase[reg_map.at("mixp")];
        test_case.after.sv = dstBase[reg_map.at("sv")];
        test_case.after.repc = dstBase[reg_map.at("repc")];
        test_case.after.lc = dstBase[reg_map.at("lc")];
        test_case.after.cfgi = dstBase[reg_map.at("cfgi")];
        test_case.after.cfgj = dstBase[reg_map.at("cfgj")];
        test_case.after.stt0 = dstBase[reg_map.at("stt0")];
        test_case.after.stt1 = dstBase[reg_map.at("stt1")];
        test_case.after.stt2 = dstBase[reg_map.at("stt2")];
        test_case.after.mod0 = dstBase[reg_map.at("mod0")];
        test_case.after.mod1 = dstBase[reg_map.at("mod1")];
        test_case.after.mod2 = dstBase[reg_map.at("mod2")];
        test_case.after.ar[0] = dstBase[reg_map.at("ar0")];
        test_case.after.ar[1] = dstBase[reg_map.at("ar1")];
        test_case.after.arp[0] = dstBase[reg_map.at("arp0")];
        test_case.after.arp[1] = dstBase[reg_map.at("arp1")];
        test_case.after.arp[2] = dstBase[reg_map.at("arp2")];
        test_case.after.arp[3] = dstBase[reg_map.at("arp3")];

        for (u16 i = 0; i < TestSpaceSize; ++i) {
            test_case.after.test_space_x[i] = dspD[TestSpaceX + i];
            test_case.after.test_space_y[i] = dspD[TestSpaceY + i];
        }

        fwrite(&test_case, sizeof(test_case), 1, fo);
        ++i;
        if (i % 100 == 0)
            printf("case %d\n", i);
    }

    fclose(fi);
    fclose(fo);

    printf("Finished!\n");
    consoleSelect(&topScreen);
}

void UploadDspProgram(const std::vector<u16>& code, bool oneshot) {
    StopDspProgram();
    InvalidateCache(&dspP[0x2000], 0x2000);

    memcpy(&dspP[0x2000], code.data(), code.size() * 2);
    unsigned end = 0x2000 + code.size();
    dspP[end + 0] = 0x0000; // nop
    dspP[end + 1] = 0x0000; // nop
    dspP[end + 2] = 0x0000; // nop
    dspP[end + 3] = 0x4180; // br 0x1800
    dspP[end + 4] = 0x1800;

    FlushCache(&dspP[0x2000], 0x2000);

    if (oneshot) {
        PulseDspProgram();
    } else {
        StartDspProgram();
    }
}

int udp_s;
int udp_s_broadcast;

void UdpInit() {
#define SOC_ALIGN 0x1000
#define SOC_BUFFERSIZE 0x100000
    static u32* SOC_buffer;
    // allocate buffer for SOC service
    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if (SOC_buffer == NULL) {
        printf("memalign: failed to allocate\n");
        return;
    }

    Result ret;
    if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
        printf("socInit: 0x%08lX\n", ret);
        return;
    }

    sockaddr_in si_me;

    // create a UDP socket
    if ((udp_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("socket() failed\n");
        return;
    }

    // zero out the structure
    memset(&si_me, 0, sizeof(si_me));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(8888);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket to port
    if (bind(udp_s, (sockaddr*)&si_me, sizeof(si_me)) == -1) {
        printf("bind() failed\n");
        return;
    }

    // create a UDP broadcast socket
    if ((udp_s_broadcast = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("socket()(broadcast) failed\n");
        return;
    }
}

void CheckPackage() {
    constexpr unsigned BUFLEN = 512;
    char buf[BUFLEN];
    sockaddr_in si_other;
    socklen_t slen = sizeof(si_other);
    int recv_len;
    if ((recv_len = recvfrom(udp_s, buf, BUFLEN, MSG_DONTWAIT, (sockaddr*)&si_other, &slen)) < 4)
        return;
    u16 magic;
    memcpy(&magic, buf, 2);
    if (magic == 0xD590 || magic == 0xD591) {
        std::vector<u16> program_package((recv_len - 2) / 2);
        memcpy(program_package.data(), buf + 2, program_package.size() * 2);
        consoleSelect(&bottomScreen);
        printf("--------\nNew program received!\n");
        for (u16 code : program_package) {
            printf("%04X ", code);
        }
        UploadDspProgram(program_package, magic == 0xD591);
        printf("\nUploaded!\n");

        consoleSelect(&topScreen);
    } else if (magic == 0x4352) {
        recv_len -= 2;
        if ((unsigned)recv_len > reg_region_size)
            recv_len = reg_region_size;
        consoleSelect(&bottomScreen);
        printf("--------\nRegister sync received!\n");
        memcpy(srcBase, buf + 2, recv_len);
        printf("\nSynced!\n");
        consoleSelect(&topScreen);
    }
}

int main() {
    aptInit();
    gfxInitDefault();

    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    consoleSelect(&bottomScreen);
    printf("Hello!\n");

    UdpInit();

    printf("dspInit: %08lX\n", dspInit());
    bool loaded = false;
    printf("DSP_LoadComponent: %08lX\n",
           DSP_LoadComponent(cdc_bin, cdc_bin_size, 0xFF, 0xFF, &loaded));
    printf("loaded = %d\n", loaded);
    consoleSelect(&topScreen);

    svcSleepThread(1000000000);

    grid[7][0] = MakeHexReg("r7", 0);
    grid[6][0] = MakeHexReg("r6", 1);
    grid[5][0] = MakeHexReg("r5", 2);
    grid[4][0] = MakeHexReg("r4", 3);
    grid[3][0] = MakeHexReg("r3", 4);
    grid[2][0] = MakeHexReg("r2", 5);
    grid[1][0] = MakeHexReg("r1", 6);
    grid[0][0] = MakeHexReg("r0", 7);

    grid[6][1] = MakeHexReg("mixp", 8);
    grid[7][1] = MakeHexReg("repc", 9);
    grid[5][1] = MakeHexReg("stj0", 0xA);
    grid[5][2] = MakeHexReg("sti0", 0xB);
    grid[7][2] = MakeHexReg("lc", 0xC);

    grid[3][1] = MakeHexReg("p1h", 0xD);
    grid[3][2] = MakeHexReg("p1l", 0xE);
    grid[2][2] = MakeHexReg("y1", 0xF);
    grid[2][1] = MakeHexReg("x1", 0x10);
    grid[1][1] = MakeHexReg("p0h", 0x11);
    grid[1][2] = MakeHexReg("p0l", 0x12);
    grid[0][2] = MakeHexReg("y0", 0x13);
    grid[0][1] = MakeHexReg("x0", 0x14);

    grid[6][2] = MakeHexReg("sv", 0x15);

    grid[12][1] = MakeHexReg("b1h", 0x16);
    grid[12][2] = MakeHexReg("b1l", 0x17);
    grid[12][0] = MakeHexReg("b1e", 0x18);
    grid[11][1] = MakeHexReg("b0h", 0x19);
    grid[11][2] = MakeHexReg("b0l", 0x1A);
    grid[11][0] = MakeHexReg("b0e", 0x1B);
    grid[10][1] = MakeHexReg("a1h", 0x1C);
    grid[10][2] = MakeHexReg("a1l", 0x1D);
    grid[10][0] = MakeHexReg("a1e", 0x1E);
    grid[9][1] = MakeHexReg("a0h", 0x1F);
    grid[9][2] = MakeHexReg("a0l", 0x20);
    grid[9][0] = MakeHexReg("a0e", 0x21);

    grid[13][3] = MakeBinReg("arp3", 0x22, "#RR#RRiiiiijjjjj");
    grid[12][3] = MakeBinReg("arp2", 0x23, "#RR#RRiiiiijjjjj");
    grid[11][3] = MakeBinReg("arp1", 0x24, "#RR#RRiiiiijjjjj");
    grid[10][3] = MakeBinReg("arp0", 0x25, "#RR#RRiiiiijjjjj");
    grid[9][3] = MakeBinReg("ar1", 0x26, "RRRRRRoosssoosss");
    grid[8][3] = MakeBinReg("ar0", 0x27, "RRRRRRoosssoosss");
    grid[7][3] = MakeBinReg("mod2", 0x28, "7654321m7654321M");
    grid[6][3] = MakeBinReg("mod1", 0x29, "jicB####pppppppp");
    grid[5][3] = MakeBinReg("mod0", 0x2A, "#QQ#PPooSYY###SS");
    grid[4][3] = MakeBinReg("stt2", 0x2B, "LBBB####mm##V21I");
    grid[3][3] = MakeBinReg("stt1", 0x2C, "QP#########R####");
    grid[2][3] = MakeBinReg("stt0", 0x2D, "####C###ZMNVCELL");
    grid[1][3] = MakeBinReg("cfgj", 0x2E, "mmmmmmmmmsssssss");
    grid[0][3] = MakeBinReg("cfgi", 0x2F, "mmmmmmmmmsssssss");

    MoveCursor(28, 0);
    char hostname[100];
    gethostname(hostname, 100);
    printf("IP: %s port: 8888\n", hostname);
    printf("X: use sample program    Y: Fill memory");

    // Main loop
    while (aptMainLoop()) {
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown & KEY_START)
            break;

        if (kDown & KEY_SELECT)
            ExecuteTestCases();

        if (kDown & KEY_DOWN) {
            for (int next = (int)c_row + 1; next < (int)t_row; ++next) {
                if (grid[next][c_col]) {
                    c_row = next;
                    break;
                }
            }
        }

        if (kDown & KEY_UP) {
            for (int next = (int)c_row - 1; next >= 0; --next) {
                if (grid[next][c_col]) {
                    c_row = next;
                    break;
                }
            }
        }

        if (kDown & KEY_LEFT) {
            if (c_pos == grid[c_row][c_col]->GetLength() - 1) {
                for (int next = (int)c_col - 1; next >= 0; --next) {
                    if (grid[c_row][next]) {
                        c_col = next;
                        c_pos = 0;
                        break;
                    }
                }
            } else {
                ++c_pos;
            }
        }

        if (kDown & KEY_RIGHT) {
            if (c_pos == 0) {
                for (int next = (int)c_col + 1; next < (int)t_col; ++next) {
                    if (grid[c_row][next]) {
                        c_col = next;
                        c_pos = grid[c_row][c_col]->GetLength() - 1;
                        break;
                    }
                }
            } else {
                --c_pos;
            }
        }

        if (kDown & KEY_A) {
            unsigned v = grid[c_row][c_col]->GetSrcDigit(c_pos);
            ++v;
            if (v == grid[c_row][c_col]->GetDigitRange())
                v = 0;
            grid[c_row][c_col]->SetSrcDigit(c_pos, v);
        }

        if (kDown & KEY_B) {
            unsigned v = grid[c_row][c_col]->GetSrcDigit(c_pos);
            if (v == 0)
                v = grid[c_row][c_col]->GetDigitRange();
            --v;
            grid[c_row][c_col]->SetSrcDigit(c_pos, v);
        }

        if (kDown & KEY_X) {
            UploadDspProgram({0x86A0}, false); // add r0, a0
        }

        if (kDown & KEY_Y) {
            FlushCache(&dspD[0x3000], 0x40000 - 0x6000);
            for (u32 i = 0x3000; i < 0x20000; ++i) {
                dspD[i] = i; // i >> 4;
            }
            InvalidateCache(&dspD[0x3000], 0x40000 - 0x6000);
        }

        if (kDown & KEY_TOUCH) {
            consoleSelect(&bottomScreen);
            printf("--------\nSending register sync!\n");
            u8 buf[reg_region_size + 2];
            u16 magic = 0x4352;
            memcpy(buf, &magic, 2);
            memcpy(buf + 2, srcBase, reg_region_size);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(8888);
            addr.sin_addr.s_addr = 0xFFFFFFFF;
            sendto(udp_s_broadcast, buf, reg_region_size + 2, 0, (sockaddr*)&addr, sizeof(addr));
            printf("\nSynced!\n");
            consoleSelect(&topScreen);
        }

        FlushCache(&dspD[0x2000], 0x1000);
        InvalidateCache(&dspD[0x2000], 0x2000);
        PrintAll();

        CheckPackage();

        // Flush and swap framebuffers
        gfxFlushBuffers();
        gfxSwapBuffers();

        // Wait for VBlank
        gspWaitForVBlank();
    }
    socExit();
    dspExit();
    gfxExit();
    aptExit();
    return 0;
}
