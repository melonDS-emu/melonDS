/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

// measurement tool for the off-speed audio path. not part of the build.
//
//   g++ -std=c++17 -O2 -pthread -I src/frontend/qt_sdl -o build/audiobench tools/audiobench.cpp
//   ./build/audiobench
//
// rebuild with -O1 -g -fsanitize=thread for the race check; the stress section
// is what exercises the producer/consumer boundary.
//
// three sections:
//   pipeline  where audio is lost, and to what, at each speed and drain mode
//   cost      CPU the stretcher and filter actually consume
//   stress    two real threads across the SPSC ring, with a tearing detector
//
// the tearing detector compares L against R, so it catches a torn read of
// InL/InR. it is blind to InMono, which only steers which offset gets picked.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include "AudioTimeStretch.h"
#include "AudioSpeed.h"
#include "AudioLowPass.h"

static constexpr double kFs = 48000.0;

// mirrors of constants owned elsewhere, so this tool stays standalone
static constexpr int kSpuFifo = 2048;    // SPU::InitOutput -> OutputBufferSize
static constexpr int kDrainMax = 2048;   // kAudioDrainMax in EmuInstance.h
static constexpr double kInternalFps = 59.8260982880808;

// ---------------------------------------------------------------- pipeline --

struct PipeResult
{
    long long callbacks, underruns, silent;
    double meanFill, realFrac, survived;
};

// drainPerEmuFrame: the emu thread drains after every emulated frame. otherwise
// only the callback drains, capping speed at roughly kSpuFifo / audioBufSize.
static PipeResult pipeline(double speed, int audioBufSize, bool drainPerEmuFrame,
                           double seconds)
{
    AudioTimeStretch ts;
    ts.Reset();

    const int len = audioBufSize;
    const double callbackPeriod = len / kFs;
    const double emuFramePeriod = 1.0 / (kInternalFps * speed);
    const double framesPerEmuFrame = kFs / kInternalFps;

    std::vector<int16_t> in(kDrainMax * 2), out(len * 2);
    double carry = 0.0;
    int spuPending = 0;
    long long totalMade = 0, totalGot = 0, realFrames = 0;
    double fillSum = 0.0;
    double arrivalAvg = len * speed;
    long long lastWritten = 0;
    double tEmu = 0.0, tCb = 0.0;
    PipeResult r{0, 0, 0, 0.0, 0.0, 0.0};

    auto ingest = [&](int n) {
        if (n <= 0) return;
        for (int i = 0; i < n; i++) { in[(i*2)+0] = (int16_t)i; in[(i*2)+1] = (int16_t)i; }
        ts.Write(in.data(), n);
    };

    auto drain = [&]() {
        int got = spuPending;
        if (got > kDrainMax) got = kDrainMax;
        spuPending -= got;
        totalGot += got;
        ingest(got);
    };

    while (tCb < seconds)
    {
        // advance emulated frames up to the next callback
        while (tEmu + emuFramePeriod <= tCb + callbackPeriod)
        {
            tEmu += emuFramePeriod;
            carry += framesPerEmuFrame;
            int made = (int)carry; carry -= made;
            spuPending += made;
            totalMade += made;
            if (spuPending > kSpuFifo) spuPending = kSpuFifo;  // BufferAudio discards
            if (drainPerEmuFrame) drain();
        }

        tCb += callbackPeriod;
        if (!drainPerEmuFrame) drain();

        long long written = ts.TotalWritten();
        long long delta = written - lastWritten;
        if (delta < 0) delta = 0;
        lastWritten = written;
        arrivalAvg += (delta - arrivalAvg) * 0.05;

        int fill = ts.InputFill();
        double ratio = audioComputeStretchRatio(arrivalAvg, len, fill,
                                                AudioTimeStretch::TargetInputFill(arrivalAvg));
        int n = ts.Read(out.data(), len, ratio);

        r.callbacks++;
        if (n < len) r.underruns++;
        if (n < 1) r.silent++;
        fillSum += fill;
        realFrames += n;
    }

    r.meanFill = fillSum / r.callbacks;
    r.realFrac = realFrames / (double)(r.callbacks * len);
    r.survived = totalMade ? (totalGot / (double)totalMade) : 1.0;
    return r;
}

static void runPipeline()
{
    printf("== pipeline ==\n");
    printf("output%% is how much of each buffer was real audio rather than padding.\n"
           "input%% is how much of what the SPU generated survived its ring.\n\n");

    const double speeds[] = {0.5, 1.5, 2.0, 3.0, 4.0, 8.0, 16.7};
    for (int bufSize : {512, 256})
    {
        for (int mode = 0; mode < 2; mode++)
        {
            bool emuDrain = (mode == 1);
            printf("audioBufSize %3d, drained by %s:\n", bufSize,
                   emuDrain ? "emu thread " : "callback   ");
            for (double s : speeds)
            {
                PipeResult r = pipeline(s, bufSize, emuDrain, 8.0);
                printf("  %5.1fx  underruns %5.1f%%  output %5.1f%%  input %5.1f%%\n",
                       s, 100.0 * r.underruns / r.callbacks,
                       100.0 * r.realFrac, 100.0 * r.survived);
            }
            printf("\n");
        }
    }
}

// -------------------------------------------------------------------- cost --

static volatile long long g_sink = 0;

static void runCost()
{
    printf("== cost ==\n");
    printf("Noise input, the worst case for the similarity search.\n\n");

    const int len = 256;
    const double seconds = 15.0;
    const int callbacks = (int)(seconds * kFs / len);
    const int poolFrames = 1 << 20;

    std::vector<int16_t> pool(poolFrames * 2);
    srand(7);
    double y = 0.0;
    for (int i = 0; i < poolFrames; i++)
    {
        double n = ((rand() / (double)RAND_MAX) * 2.0 - 1.0) * 9000.0;
        y = (0.6 * y) + (0.4 * n);
        int16_t q = (int16_t)std::lround(y);
        pool[(i*2)+0] = q; pool[(i*2)+1] = q;
    }

    for (double ratio : {1.0, 2.0, 3.0, 4.0, 8.0})
    {
        AudioTimeStretch ts; ts.Reset();
        AudioLowPass lp; lp.Init(kFs);
        std::vector<int16_t> out(len * 2);
        double carry = 0.0;
        int pos = 0;

        auto t0 = std::chrono::steady_clock::now();
        for (int c = 0; c < callbacks; c++)
        {
            carry += len * ratio;
            int got = (int)carry; carry -= got;
            if (got > kDrainMax) got = kDrainMax;
            if (pos + got > poolFrames) pos = 0;
            ts.Write(&pool[pos*2], got);
            pos += got;
            int n = ts.Read(out.data(), len, ratio);
            lp.Process(out.data(), len, 6000.0, len / kFs);
            g_sink += n + out[0];
        }
        auto t1 = std::chrono::steady_clock::now();

        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        printf("  ratio %4.1f : %6.1f ms per %.0f s audio = %.2f%% of one core\n",
               ratio, ms, seconds, 100.0 * (ms / 1000.0) / seconds);
    }
    printf("\n");
}

// ------------------------------------------------------------------ stress --

static AudioTimeStretch g_ts;
static std::atomic<bool> g_stop{false};
static std::atomic<long long> g_written{0}, g_read{0}, g_refused{0};
static std::atomic<long long> g_torn{0}, g_outOfRange{0}, g_sessions{0};
static constexpr int kAmp = 9000;

// per-thread state: rand() is neither reentrant nor reproducible when two
// threads share it, and an unreplayable stress test is worth less
static uint32_t nextRand(uint32_t& st)
{
    st = (st * 1664525u) + 1013904223u;
    return st >> 16;
}

static void producer()
{
    std::vector<int16_t> buf(kDrainMax * 2);
    uint32_t rng = 0xA5A5A5A5u;
    long long t = 0;
    int spin = 0;
    while (!g_stop.load(std::memory_order_relaxed))
    {
        int n = 64 + (nextRand(rng) % 1900);
        for (int i = 0; i < n; i++)
        {
            double v = kAmp * std::sin(2.0 * M_PI * 440.0 * ((t + i) / kFs));
            int16_t s = (int16_t)std::lround(v);
            buf[(i*2)+0] = s;
            buf[(i*2)+1] = s;              // L == R, always
        }
        t += n;
        int took = g_ts.Write(buf.data(), n);
        g_written.fetch_add(took, std::memory_order_relaxed);
        if (took < n) g_refused.fetch_add(n - took, std::memory_order_relaxed);
        if (++spin % 500 == 0) { g_ts.BeginSession(); g_sessions.fetch_add(1, std::memory_order_relaxed); }
        if ((spin % 7) == 0) std::this_thread::yield();
    }
}

static void consumer()
{
    const int len = 256;
    uint32_t rng = 0x5A5A5A5Au;
    std::vector<int16_t> out(len * 2);
    while (!g_stop.load(std::memory_order_relaxed))
    {
        double ratio = 0.5 + ((nextRand(rng) % 700) / 100.0);
        int n = g_ts.Read(out.data(), len, ratio);
        g_read.fetch_add(n, std::memory_order_relaxed);
        for (int i = 0; i < n; i++)
        {
            // input frames all have L == R and both channels get the same
            // window and offset, so a difference means a torn read
            if (out[(i*2)+0] != out[(i*2)+1]) g_torn.fetch_add(1, std::memory_order_relaxed);
            if (std::abs((int)out[(i*2)]) > kAmp + 2000) g_outOfRange.fetch_add(1, std::memory_order_relaxed);
        }
        std::this_thread::yield();
    }
}

static int runStress(int seconds)
{
    printf("== stress ==\n");
    srand(12345);
    g_ts.Reset();

    std::thread p(producer), c(consumer);
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    g_stop.store(true, std::memory_order_relaxed);
    p.join(); c.join();

    printf("  %ds: written %lld, read %lld, refused %lld, sessions %lld\n",
           seconds, g_written.load(), g_read.load(), g_refused.load(), g_sessions.load());
    printf("  torn frames (L != R): %lld\n", g_torn.load());
    printf("  out-of-range samples: %lld\n", g_outOfRange.load());

    bool ok = (g_torn.load() == 0) && (g_outOfRange.load() == 0)
              && (g_written.load() > 0) && (g_read.load() > 0);
    printf("  %s\n\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    int stressSeconds = (argc > 1) ? atoi(argv[1]) : 5;
    runPipeline();
    runCost();
    int rc = runStress(stressSeconds);
    if (g_sink == 0x7fffffff) printf("unreachable\n");
    return rc;
}
