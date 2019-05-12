#include <oboe/Oboe.h>
#include "MelonDS.h"
#include "../NDS.h"
#include "../GPU.h"
#include "../SPU.h"
#include "../Platform.h"
#include "OboeCallback.h"

u32* frameBuffer;

u64 startTick = 0;
u64 lastTick = startTick;
u64 lastMeasureTick = lastTick;
u32 fpsLimitCount = 0;
u32 nFrames = 0;
u32 currentFps = 0;
bool limitFps = true;
oboe::AudioStream *audioStream;

namespace MelonDSAndroid
{
    char* configDir;

    void setup(char* configDirPath)
    {
        configDir = configDirPath;

        frameBuffer = new u32[256 * 384 * 4];

        // TODO: Gotta find the correct sound setup
        oboe::AudioStreamBuilder streamBuilder;
        streamBuilder.setChannelCount(2);
        streamBuilder.setFramesPerCallback(1024);
        streamBuilder.setFormat(oboe::AudioFormat::I16);
        streamBuilder.setDirection(oboe::Direction::Output);
        streamBuilder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        streamBuilder.setSharingMode(oboe::SharingMode::Exclusive);
        streamBuilder.setCallback(new OboeCallback());

        oboe::Result result = streamBuilder.openStream(&audioStream);
        if (result != oboe::Result::OK)
        {
            fprintf(stderr, "Failed to init audio stream");
        }

        NDS::Init();
    }

    bool loadRom(char* romPath, char* sramPath, bool loadDirect)
    {
        return NDS::LoadROM(romPath, sramPath, loadDirect);
    }

    void start(u64 initialTicks)
    {
        startTick = initialTicks;
        audioStream->requestStart();
        memset(frameBuffer, 0, 256 * 384 * 4);
    }

    void loop(u64 currentTicks)
    {
        u32 nlines = NDS::RunFrame();
        memcpy(frameBuffer, GPU::Framebuffer, 256 * 384 * 4);

        float framerate;
        if (nlines == 263)
            framerate = 1000.0f / 60.0f;
        else
            framerate = ((1000.0f * nlines) / 263.0f) / 60.0f;

        fpsLimitCount++;
        lastTick = currentTicks;

        u64 wantedtick = startTick + (u32) ((float) fpsLimitCount * framerate);
        if (currentTicks < wantedtick && limitFps)
        {
            // TODO: delay execution if one day this runs fast enough
        }
        else
        {
            fpsLimitCount = 0;
            startTick = currentTicks;
        }

        nFrames++;
        u32 delta = currentTicks - lastMeasureTick;
        lastMeasureTick = currentTicks;
        currentFps = 1000 / delta;
    }

    void pause() {
        audioStream->requestPause();
    }

    void resume() {
        audioStream->requestStart();
    }

    void copyFrameBuffer(void* dstBuffer)
    {
        memcpy(dstBuffer, frameBuffer, 256 * 384 * 4);
    }

    int getFPS()
    {
        return currentFps;
    }

    void cleanup()
    {
        NDS::DeInit();
        audioStream->requestStop();
        audioStream = NULL;

        free(frameBuffer);
        frameBuffer = NULL;
    }
}

void Stop(bool internal)
{
}

bool LocalFileExists(const char* name)
{
    FILE* f = Platform::OpenFile(name, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}