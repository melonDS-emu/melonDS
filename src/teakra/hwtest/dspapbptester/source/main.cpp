#include <atomic>
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

void FlushCache(volatile void* ptr, u32 size) {
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (void*)ptr, size);
}

void InvalidateCache(volatile void* ptr, u32 size) {
    svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, (void*)ptr, size);
}

vu16* dspP = (vu16*)0x1FF00000;
vu16* dspD = (vu16*)0x1FF40000;

std::atomic<int> interrupt_counter;

void PrintAll() {
    consoleSelect(&topScreen);

    MoveCursor(0, 0);
    printf("DSP registers:\n");

    InvalidateCache(dspD + 4, 2 * 8);

    printf("SetSem = %04X\n", dspD[5]);
    printf("MskSem = %04X\n", dspD[6]);
    printf("AckSem = %04X\n", dspD[7]);
    printf("GetSem = %04X\n", dspD[8]);
    printf("IrqMsk = %04X\n", dspD[9]);
    printf("Status = %04X\n", dspD[10]);
    printf("?????? = %04X\n", dspD[11]);
    printf("interrupt = %d\n", dspD[4]);

    printf("\nCPU registers:\n");
    printf("PCFG   = %04X\n", *(vu16*)0x1ed03008);
    printf("PSTS   = %04X\n", *(vu16*)0x1ed0300C);
    printf("PSEM   = %04X\n", *(vu16*)0x1ed03010);
    printf("PMASK  = %04X\n", *(vu16*)0x1ed03014);
    printf("PCLEAR = %04X\n", *(vu16*)0x1ed03018);
    printf("SEM    = %04X\n", *(vu16*)0x1ed0301C);
    printf("interrupt = %d\n", (int)interrupt_counter);

    consoleSelect(&bottomScreen);
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

constexpr unsigned BUFLEN = 512;
char buf[BUFLEN];

void Fire() {
    dspD[0] = 1;
    FlushCache((void*)dspD, 8);
    while (true) {
        InvalidateCache((void*)dspD, 8);
        if (dspD[0] == 0)
            break;
    }
}

void CheckPackage() {
    sockaddr_in si_other;
    socklen_t slen = sizeof(si_other);
    int recv_len;
    if ((recv_len = recvfrom(udp_s, buf, BUFLEN, MSG_DONTWAIT, (sockaddr*)&si_other, &slen)) < 4)
        return;
    u16 magic;
    memcpy(&magic, buf, 2);
    if (magic == 0xD592) {
        std::vector<u16> command_package((recv_len - 2) / 2);
        printf("Command received\n");
        memcpy(command_package.data(), buf + 2, command_package.size() * 2);
        switch (command_package[0]) {
        case 0: {
            if (command_package.size() != 2) {
                printf("Wrong length for Read\n");
                break;
            }
            u16 addr = command_package[1];
            printf("Read  [%04X] -> ", addr);
            dspD[1] = 0;
            dspD[2] = addr;
            dspD[3] = 0xCCCC;

            Fire();

            printf("%04X\n", dspD[3]);
            break;
        }
        case 1: {
            if (command_package.size() != 3) {
                printf("Wrong length for Write\n");
                break;
            }
            u16 addr = command_package[1];
            u16 value = command_package[2];
            printf("Write [%04X] <- %04X", addr, value);
            dspD[1] = 1;
            dspD[2] = addr;
            dspD[3] = value;

            Fire();

            printf(" OK\n");
            break;
        }
        case 2: {
            if (command_package.size() != 3) {
                printf("Wrong length for CPU Read\n");
                break;
            }
            u32 addr = command_package[1] | ((u32)command_package[2] << 16);
            printf("Read CPU  [%08lX] -> ", addr);
            // FlushCache((vu16*)addr, 2);
            printf("%04X\n", *(vu16*)(addr));
            break;
        }
        case 3: {
            if (command_package.size() != 4) {
                printf("Wrong length for CPU Write\n");
                break;
            }
            u32 addr = command_package[1] | ((u32)command_package[2] << 16);
            u16 value = command_package[3];
            printf("Write CPU  [%08lX] <- %04X", addr, value);
            // InvalidateCache((vu16*)addr, 2);
            *(vu16*)(addr) = value;
            printf(" OK\n");
            break;
        }
        }
    }
}

Handle pmHandle;
Result pmInit_(void) {
    Result res = srvGetServiceHandle(&pmHandle, "pm:app");
    return res;
}
void pmExit_(void) {
    svcCloseHandle(pmHandle);
}

Result PM_TerminateTitle(u64 tid, u64 timeout) {
    Result ret = 0;
    u32* cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x4, 4, 0);
    cmdbuf[1] = tid & 0xFFFFFFFF;
    cmdbuf[2] = tid >> 32;
    cmdbuf[3] = timeout & 0xffffffff;
    cmdbuf[4] = (timeout >> 32) & 0xffffffff;

    if (R_FAILED(ret = svcSendSyncRequest(pmHandle)))
        return ret;

    return (Result)cmdbuf[1];
}

Handle dsp_interrupt;
Handle threadA;
u32 threadA_stack[0x400];

void threadA_entry(void*) {
    while (true) {
        svcWaitSynchronization(dsp_interrupt, INT64_MAX);
        ++interrupt_counter;
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

    svcSleepThread(1000000000);
    char hostname[100];
    gethostname(hostname, 100);
    printf("IP: %s port: 8888\n", hostname);

    pmInit_();
    printf("PM_TerminateTitle(DSP): %08lX\n", PM_TerminateTitle(0x0004013000001a02, 0));
    pmExit_();

    svcCreateEvent(&dsp_interrupt, ResetType::RESET_ONESHOT);
    interrupt_counter = 0;
    svcCreateThread(&threadA, threadA_entry, 0x0, threadA_stack + 0x400, 4, 0xFFFFFFFE);
    printf("BindInterrupt: %08lX\n", svcBindInterrupt(0x4A, dsp_interrupt, 4, 0));

    // Main loop
    while (aptMainLoop()) {
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown & KEY_START)
            break;

        if (kDown & KEY_A)
            printf("hello\n");

        CheckPackage();

        PrintAll();

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
