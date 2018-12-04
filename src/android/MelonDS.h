#ifndef MELONDS_MELONDS_H
#define MELONDS_MELONDS_H

#include "../types.h"

namespace MelonDSAndroid {
    extern char* configDir;

    extern void setup(char* configDirPath);
    extern bool loadRom(char* romPath, char* sramPath, bool loadDirect);
    extern void start(u64 initialTicks);
    extern void loop(u64 currentTicks);
    extern void pause();
    extern void resume();
    extern void copyFrameBuffer(void* dstBuffer);
    extern int getFPS();
    extern float getTargetFPS();
    extern void cleanup();
}

#endif //MELONDS_MELONDS_H
