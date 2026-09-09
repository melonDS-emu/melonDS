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

#ifndef AUDIOSPEED_H
#define AUDIOSPEED_H

#include <algorithm>
#include <cmath>

// the DS's true framerate; a skew of 1.0 emits at the hardware's rate
constexpr double INTERNAL_FRAME_RATE = 59.8260982880808;

// sentinel value: don't filter at all. the reference is divided by speed, so
// the top of the range bounds how gentle the filter can be; well past any
// device's Nyquist on purpose.
constexpr int kSpeedUpLowPassOff = 48000;

constexpr double kStretchTrimGain = 0.25;

// below 1 the stretcher expands (slow-mo), above it compresses (fast-forward)
constexpr double kMinStretchRatio = 1.0 / 32.0;
constexpr double kMaxStretchRatio = 32.0;

// SPU::BufferAudio flushes every 512*128 SPU cycles, accumulating
// 65536*48000 / (16756991*skew) ~= 188/skew samples, and blip_new(512) asserts
// avail <= 512. below ~0.37 that overflows the allocation.
constexpr double kMinOutputSkew = 0.4;

// emulated speed relative to normal
inline double audioSpeedRatio(double curFPS, double targetFPS)
{
    if (targetFPS <= 0.0) return 1.0;
    return curFPS / targetFPS;
}

inline bool audioIsOffSpeed(double curFPS, double targetFPS)
{
    return std::fabs(curFPS - targetFPS) > 0.01;
}

// independent of emulation speed: deriving this from curFPS would resample,
// shifting pitch. fitting off-speed audio to the output is the stretcher's job.
inline double audioComputeOutputSkew(double targetFPS)
{
    return std::max(targetFPS / INTERNAL_FRAME_RATE, kMinOutputSkew);
}

// how much input each output frame consumes. driven by measured arrival rather
// than the requested speed, which the host may not achieve. the FIFO-fill term
// only steers the level: a larger ratio drains faster.
inline double audioComputeStretchRatio(double arrivalPerCallback, int outputPerCallback,
                                       int inputFill, int targetFill)
{
    if (outputPerCallback <= 0) return 1.0;

    double ratio = arrivalPerCallback / (double)outputPerCallback;

    if (targetFill > 0)
    {
        double err = (inputFill - (double)targetFill) / (double)targetFill;
        err = std::clamp(err, -1.0, 1.0);
        ratio *= 1.0 + (kStretchTrimGain * err);
    }

    return std::clamp(ratio, kMinStretchRatio, kMaxStretchRatio);
}

// low-pass cutoff in Hz. wideOpen means transparent; the filter only engages
// above normal speed, and gets duller the faster the emulator runs.
inline double audioComputeLowPassCutoff(double curFPS, double targetFPS,
                                        int reference, double wideOpen)
{
    if (wideOpen <= 200.0) return wideOpen;

    double speed = audioSpeedRatio(curFPS, targetFPS);
    if (speed <= 1.0) return wideOpen;
    // zero is off too, rather than clamping to the strongest filter
    if (reference <= 0 || reference >= kSpeedUpLowPassOff) return wideOpen;

    return std::clamp(reference / speed, 200.0, wideOpen);
}

#endif // AUDIOSPEED_H
