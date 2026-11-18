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

#ifndef AUDIOTIMESTRETCH_H
#define AUDIOTIMESTRETCH_H

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

// WSOLA time-stretcher: changes playback rate while preserving pitch, by
// overlap-adding windowed frames picked for waveform similarity, so consecutive
// frames splice on matching phase.
//
// single producer/single consumer. the emu thread calls Write and BeginSession;
// the audio thread calls Read, InputFill, OutputFill and TotalWritten. Reset is
// neither, and needs both stopped.
class AudioTimeStretch
{
public:
    static constexpr int kFrameSize = 256;                // analysis window, frames
    static constexpr int kSynthesisHop = kFrameSize / 2;  // periodic Hann at 50% sums to unity
    static constexpr int kSearchRadius = 1024;            // frames either side of nominal
    static constexpr int kCoarseStride = 4;
    static constexpr int kFineRadius = 3;
    static constexpr int kInputCapacity = 32768;          // must be a power of two
    static constexpr int kOutputCapacity = 8192;          // must be a power of two

    // floor on how much input the ratio control keeps buffered
    static constexpr int kMinTargetInputFill = 4096;

    // how much input to keep buffered. a fixed figure starves the stretcher at
    // high speed, where one callback consumes more than the target.
    static int TargetInputFill(double arrivalPerCallback)
    {
        // leave half the ring free, or a large hop squeezes Write down to nothing
        const double cap = kInputCapacity / 2;

        double need = (2.0 * arrivalPerCallback) + kSearchRadius + kFrameSize;
        // clamped as a double: a TargetFPS of 0.0001 seeds an arrival estimate
        // around 2.5e9, and casting that to int is UB
        if (!(need > kMinTargetInputFill)) need = kMinTargetInputFill;
        if (need > cap) need = cap;
        return (int)need;
    }

    AudioTimeStretch()
    {
        for (int i = 0; i < kFrameSize; i++)
            Window[i] = (float)(0.5 * (1.0 - std::cos((2.0 * M_PI * i) / kFrameSize)));
        Reset();
    }

    // not thread-safe: call only with both sides stopped
    void Reset()
    {
        WritePos.store(0, std::memory_order_relaxed);
        Generation.store(0, std::memory_order_relaxed);
        ConsumerFloor.store(0, std::memory_order_relaxed);
        SeenWrite = 0;
        SeenGeneration = 0;
        AnalysisPos = 0;
        NaturalPos = 0;
        OutReadPos = 0;
        OutWritePos = 0;
        Primed = false;
        std::memset(AccL, 0, sizeof(AccL));
        std::memset(AccR, 0, sizeof(AccR));

        // the search is already bounded to written frames; this only degrades
        // a future bound slip to silence rather than noise
        std::memset(InL, 0, sizeof(InL));
        std::memset(InR, 0, sizeof(InR));
        std::memset(InMono, 0, sizeof(InMono));
    }

    int InputFill() const
    {
        int64_t pending = WritePos.load(std::memory_order_acquire) - AnalysisPos;
        if (pending < 0) return 0;
        if (pending > kInputCapacity) return kInputCapacity;
        return (int)pending;
    }

    int OutputFill() const { return (int)(OutWritePos - OutReadPos); }

    // append interleaved stereo frames, returning how many were accepted.
    // writing is refused rather than allowed to lap the consumer.
    int Write(const int16_t* samples, int numFrames)
    {
        int64_t w = WritePos.load(std::memory_order_relaxed);

        int64_t floor = ConsumerFloor.load(std::memory_order_acquire);
        int64_t space = kInputCapacity - (w - floor);
        if (space < 0) space = 0;
        if (numFrames > space) numFrames = (int)space;
        if (numFrames <= 0) return 0;

        for (int i = 0; i < numFrames; i++)
        {
            int idx = (int)((w + i) & (kInputCapacity - 1));
            int16_t l = samples[(i*2)+0];
            int16_t r = samples[(i*2)+1];
            InL[idx] = l;
            InR[idx] = r;
            InMono[idx] = 0.5f * ((float)l + (float)r);
        }

        // release: the frames must be visible before the count advertising them
        WritePos.store(w + numFrames, std::memory_order_release);
        return numFrames;
    }

    // marks a discontinuity: the consumer resyncs rather than splicing across it
    void BeginSession()
    {
        Generation.fetch_add(1, std::memory_order_release);
    }

    int64_t TotalWritten() const { return WritePos.load(std::memory_order_acquire); }

    // emit up to numFrames of interleaved stereo, synthesising as needed. works
    // off one snapshot of the producer's count, so the bounds can't shift.
    int Read(int16_t* samples, int numFrames, double ratio)
    {
        // generation first, then WritePos. the other order lets a session that
        // starts between the two loads resync against a stale SeenWrite, and
        // then never resync again.
        uint32_t gen = Generation.load(std::memory_order_acquire);
        SeenWrite = WritePos.load(std::memory_order_acquire);

        if (gen != SeenGeneration)
        {
            SeenGeneration = gen;
            ResyncToLive();
        }

        // the producer outran us: skip forward rather than read what landed
        // on top of the frames we wanted
        int64_t floor = (SeenWrite - kInputCapacity) + kSearchRadius + kFrameSize;
        if (AnalysisPos < floor)
        {
            AnalysisPos = floor;
            NaturalPos = floor;
            Primed = false;
        }

        // overlap-add only reaches full amplitude once the accumulator carries a
        // whole window, so the first hop after a reposition would ramp up from
        // silence. charge the accumulator with one hop and drop its output.
        if (!Primed && CanSynthesise())
            SynthesiseHop(ratio, false);

        while ((OutputFill() < numFrames) && CanSynthesise())
            SynthesiseHop(ratio, true);

        int n = std::min(numFrames, OutputFill());
        for (int i = 0; i < n; i++)
        {
            int idx = (int)(OutReadPos & (kOutputCapacity - 1));
            samples[(i*2)+0] = OutL[idx];
            samples[(i*2)+1] = OutR[idx];
            OutReadPos++;
        }

        PublishFloor();
        return n;
    }

private:
    static int16_t Saturate(float v)
    {
        long s = std::lround(v);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        return (int16_t)s;
    }

    // publish the oldest frame we might still look at, so the producer stops
    // short of it. NaturalPos can sit well below AnalysisPos - kSearchRadius.
    void PublishFloor()
    {
        int64_t floorNow = std::min(AnalysisPos - kSearchRadius, NaturalPos);
        if (floorNow < 0) floorNow = 0;
        ConsumerFloor.store(floorNow, std::memory_order_release);
    }

    // drop whatever is stale and pick up at live data. the published floor can
    // decrease here, but the dip is bounded by kFrameSize + kSearchRadius, far
    // short of kInputCapacity, so the producer can't reach the re-exposed region.
    void ResyncToLive()
    {
        AnalysisPos = std::max<int64_t>(0, SeenWrite - kFrameSize);
        NaturalPos = AnalysisPos;
        Primed = false;
        PublishFloor();
        OutReadPos = 0;
        OutWritePos = 0;
        std::memset(AccL, 0, sizeof(AccL));
        std::memset(AccR, 0, sizeof(AccR));
    }

    bool CanSynthesise() const
    {
        if ((kOutputCapacity - OutputFill()) < kSynthesisHop) return false;

        // only the frame and the natural-continuation reference need to be
        // present; demanding the full search radius would starve a short FIFO
        int64_t frameEnd = AnalysisPos + kFrameSize;
        int64_t naturalEnd = NaturalPos + kSynthesisHop;
        return (SeenWrite >= frameEnd) && (SeenWrite >= naturalEnd);
    }

    // output=false charges the accumulator and advances without writing out.
    // not named emit: Qt defines that away.
    void SynthesiseHop(double ratio, bool output)
    {
        int hop = (int)std::lround(kSynthesisHop * ratio);
        if (hop < 1) hop = 1;

        int64_t chosen = Primed ? FindBestOffset() : AnalysisPos;
        Primed = true;

        for (int i = 0; i < kFrameSize; i++)
        {
            int idx = (int)((chosen + i) & (kInputCapacity - 1));
            float w = Window[i];
            AccL[i] += w * (float)InL[idx];
            AccR[i] += w * (float)InR[idx];
        }

        if (output)
        {
            for (int i = 0; i < kSynthesisHop; i++)
            {
                int idx = (int)(OutWritePos & (kOutputCapacity - 1));
                OutL[idx] = Saturate(AccL[i]);
                OutR[idx] = Saturate(AccR[i]);
                OutWritePos++;
            }
        }

        std::memmove(AccL, AccL + kSynthesisHop, kSynthesisHop * sizeof(float));
        std::memmove(AccR, AccR + kSynthesisHop, kSynthesisHop * sizeof(float));
        std::memset(AccL + kSynthesisHop, 0, kSynthesisHop * sizeof(float));
        std::memset(AccR + kSynthesisHop, 0, kSynthesisHop * sizeof(float));

        // the nominal pointer advances by hop alone; advancing from chosen would
        // make consumption hop + E[bestK], which the ratio control can't see
        NaturalPos = chosen + kSynthesisHop;
        AnalysisPos += hop;
    }

    int64_t FindBestOffset() const
    {
        int64_t oldest = std::max<int64_t>(0, SeenWrite - kInputCapacity);

        // at a high ratio on a near-full ring NaturalPos can fall off the back.
        // the reference would then be overwritten frames, so don't search.
        if (NaturalPos < oldest) return AnalysisPos;

        // NaturalPos is fixed for the whole search: copy the reference window
        // out once rather than paying the masked load per candidate, and take
        // its energy in the same pass
        double ref[kSynthesisHop];
        double refEnergy = 0.0;
        for (int i = 0; i < kSynthesisHop; i++)
        {
            double v = InMono[(int)((NaturalPos + i) & (kInputCapacity - 1))];
            ref[i] = v;
            refEnergy += v * v;
        }

        // full radius regardless of hop: expansion needs the reach
        const int radius = kSearchRadius;

        // masking a negative position wraps it into frames we never wrote
        int lowestK = -radius;
        if ((AnalysisPos + lowestK) < oldest)
            lowestK = (int)(oldest - AnalysisPos);

        // clamp to what has arrived, so a short FIFO narrows the search
        int highestK = radius;
        int64_t latest = SeenWrite - kFrameSize;
        if ((AnalysisPos + highestK) > latest)
            highestK = (int)(latest - AnalysisPos);
        if (highestK < lowestK) highestK = lowestK;

        // seeded from the nominal offset, so ties keep it rather than sliding
        // to the bottom of the window (every score is 0 on digital silence)
        int bestK = std::min(std::max(lowestK, 0), highestK);
        double bestScore = Score(AnalysisPos + bestK, ref, refEnergy);

        for (int k = lowestK; k <= highestK; k += kCoarseStride)
        {
            double s = Score(AnalysisPos + k, ref, refEnergy);
            if (s > bestScore) { bestScore = s; bestK = k; }
        }

        int lo = std::max(lowestK, bestK - kFineRadius);
        int hi = std::min(highestK, bestK + kFineRadius);
        for (int k = lo; k <= hi; k++)
        {
            double s = Score(AnalysisPos + k, ref, refEnergy);
            if (s > bestScore) { bestScore = s; bestK = k; }
        }

        return AnalysisPos + bestK;
    }


    // normalised, so the search doesn't just latch onto the loudest candidate.
    // ref is the natural-continuation window as a linear copy
    double Score(int64_t pos, const double* ref, double refEnergy) const
    {
        double dot = 0.0;
        double energy = 0.0;
        for (int i = 0; i < kSynthesisHop; i++)
        {
            double a = InMono[(int)((pos + i) & (kInputCapacity - 1))];
            dot += a * ref[i];
            energy += a * a;
        }
        return dot / std::sqrt((energy * refEnergy) + 1.0e-9);
    }

    float Window[kFrameSize];

    int16_t InL[kInputCapacity];
    int16_t InR[kInputCapacity];
    float InMono[kInputCapacity];
    std::atomic<int64_t> WritePos{0};      // producer writes, consumer reads
    std::atomic<uint32_t> Generation{0};   // producer writes, consumer reads
    std::atomic<int64_t> ConsumerFloor{0}; // consumer writes, producer reads

    int64_t SeenWrite = 0;                 // consumer's snapshot of WritePos
    uint32_t SeenGeneration = 0;
    int64_t AnalysisPos = 0;
    int64_t NaturalPos = 0;
    bool Primed = false;

    float AccL[kFrameSize];
    float AccR[kFrameSize];

    int16_t OutL[kOutputCapacity];
    int16_t OutR[kOutputCapacity];
    int64_t OutReadPos = 0;
    int64_t OutWritePos = 0;
};

#endif // AUDIOTIMESTRETCH_H
