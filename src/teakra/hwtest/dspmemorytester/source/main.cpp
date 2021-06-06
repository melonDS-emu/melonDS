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



void FlushCache(void* ptr, u32 size) {
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, ptr, size);
}

void InvalidateCache(void* ptr, u32 size) {
    svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, ptr, size);
}


vu16* dspP = (vu16*)0x1FF00000;
vu16* dspD = (vu16*)0x1FF40000;

u32 playground_size = 0x80;

vu8* dsp_playground = (vu8*)(dspD + 0x1000); // note: address = 0x1000 in dsp space
vu8* fcram_playground;

void PrintAll() {
    consoleSelect(&topScreen);

    MoveCursor(0, 0);
    printf("DSP @ 0x1000:\n");

    InvalidateCache((void*)dsp_playground, playground_size);
    for (u32 i = 0; i < playground_size; ++i) {
        u8 v = dsp_playground[i];
        if (v != i) {
            SetColor(Green, Black);
        }
        printf("%02X", v);
        SetColor(Reset, Reset);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }


    u32 addr = (u32)fcram_playground;
    if (addr < 0x1C000000) {
        addr = addr - 0x14000000 + 0x20000000;
    } else {
        addr = addr - 0x30000000 + 0x20000000;
    }
    printf("\nFCRAM @ 0x%08lX:\n", addr);

    InvalidateCache((void*)fcram_playground, playground_size);
    for (u32 i = 0; i < playground_size; ++i) {
        u8 v = fcram_playground[i];
        if (v != (i | 0x80)) {
            SetColor(Green, Black);
        }
        printf("%02X", v);
        SetColor(Reset, Reset);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }


    consoleSelect(&bottomScreen);
}

void ResetDspPlayground() {
    InvalidateCache((void*)dsp_playground, playground_size);
    for (u32 i = 0; i < playground_size; ++i) {
        dsp_playground[i] = i;
    }
    FlushCache((void*)dsp_playground, playground_size);
}

void ResetFcramPlayground() {
    InvalidateCache((void*)fcram_playground, playground_size);
    for (u32 i = 0; i < playground_size; ++i) {
        fcram_playground[i] = i | 0x80;
    }
    FlushCache((void*)fcram_playground, playground_size);
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
    while(true) {
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
        }
    }
}

int main() {
    fcram_playground = (vu8*)linearAlloc(playground_size);
    ResetDspPlayground();
    ResetFcramPlayground();
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

    // Main loop
    while (aptMainLoop()) {
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown & KEY_START)
            break;

        if (kDown & KEY_A) {
            ResetDspPlayground();
            printf("Reset DSP playground\n");
        }

        if (kDown & KEY_B) {
            ResetFcramPlayground();
            printf("Reset FCRAM playground\n");
        }

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
