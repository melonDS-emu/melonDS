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

#ifndef AUDIOSTREAMRAMP_H
#define AUDIOSTREAMRAMP_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// conceals the ends of the output stream. a pause or a state jump ends the
// audio wherever the waveform happened to be, and brings it back the same way:
// a step at both ends, heard as a click. keep a copy of the last frames
// played; on End() repeat the period that best matches them under a raised
// cosine, as a packet-loss concealer does, and on Begin() ramp the first
// frames back up. fading the last sample as a constant instead is a decaying
// DC offset rather than a decaying sound, and is heard as a thump.
//
// audio thread only. frames are interleaved stereo s16, kept as normalised
// float inside.
class AudioStreamRamp
{
public:
    static constexpr int kHistFrames = 2048;   // must hold kMaxPeriod + kCorrFrames
    static constexpr int kTailFrames = 512;
    // period the tail repeats, and the window it is matched over. 96..1024 is
    // roughly 47 Hz to 500 Hz at 48 kHz.
    static constexpr int kMinPeriod = 96;
    static constexpr int kMaxPeriod = 1024;
    static constexpr int kCorrFrames = 128;
    // frames over which the tail's join to what was playing is eased in
    static constexpr int kJoinFrames = 128;
    // source frames dropped after the tail, to swallow whatever the pipeline
    // still held when the stream went down
    static constexpr int kMuteFrames = 512;
    static constexpr int kRampInFrames = 512;

    AudioStreamRamp() { Reset(); }

    void Reset()
    {
        std::memset(Hist, 0, sizeof(Hist));
        HistPos = 0;
        HistFill = 0;
        LastOut[0] = LastOut[1] = 0.0f;
        TailFrames = 0;
        TailPeriod = 0;
        TailJoin[0] = TailJoin[1] = 0.0f;
        TailFrom[0] = TailFrom[1] = 0.0f;
        MuteFrames = 0;
        FadeInFrames = 0;
        RampInLatched = false;
    }

    // the stream is down: a tail is playing, or the mute after it is pending
    bool Ended() const { return TailFrames > 0 || MuteFrames > 0; }
    bool TailSpent() const { return TailFrames == 0; }

    // take the stream down from where it is. nothing playing means no step to
    // smooth, so nothing is armed.
    void End()
    {
        if (LastOut[0] == 0.0f && LastOut[1] == 0.0f) return;

        TailPeriod = FindPeriod();

        // the tail reads history as it stood here, so keep the frames it will
        // repeat: ages 1..period+1
        if (TailPeriod)
        {
            for (int k = 0; k <= TailPeriod; k++)
            {
                const float* f = HistAt(k + 1);
                TailSrc[k][0] = f[0];
                TailSrc[k][1] = f[1];
            }
            // however good the match, the repeat starts at its own value rather
            // than the one the stream stopped on. the frame matching the last
            // one played is a whole period before it; age period is its
            // successor, the frame the tail opens on.
            TailJoin[0] = LastOut[0] - TailSrc[TailPeriod][0];
            TailJoin[1] = LastOut[1] - TailSrc[TailPeriod][1];
        }
        else
        {
            TailJoin[0] = TailJoin[1] = 0.0f;
        }

        TailFrom[0] = LastOut[0];
        TailFrom[1] = LastOut[1];
        TailFrames = kTailFrames;
        FadeInFrames = 0;
    }

    // produce numFrames in place of source audio: the tail, then silence.
    // what is written is recorded as played.
    void FillTail(int16_t* samples, int numFrames)
    {
        for (int i = 0; i < numFrames; i++)
        {
            float l = 0.0f, r = 0.0f;
            if (TailFrames > 0) TailFrame(l, r);
            samples[(i*2)+0] = Saturate(l);
            samples[(i*2)+1] = Saturate(r);
            Push(l, r);
        }
    }

    // the stream is coming back: ramp its first frames in. while a tail is
    // still playing the ramp waits for it and the mute after it.
    void Begin()
    {
        if (Ended())
        {
            RampInLatched = true;
            return;
        }
        FadeInFrames = kRampInFrames;
    }

    // source frames about to be played: finish a tail the last buffer could
    // not, drop what the mute still owes, apply the ramp in, and record them
    void Track(int16_t* samples, int numFrames)
    {
        int start = 0;
        // a short read ends the stream mid-buffer and the next buffer is full
        // again: the tail still has to play out, over frames that would
        // otherwise splice in at full level behind it
        if (TailFrames > 0)
        {
            int n = std::min(TailFrames, numFrames);
            for (int j = 0; j < n; j++)
            {
                float l, r;
                TailFrame(l, r);
                samples[(j*2)+0] = Saturate(l);
                samples[(j*2)+1] = Saturate(r);
            }
            start = n;
        }

        if (MuteFrames > 0 && start < numFrames)
        {
            int m = std::min(MuteFrames, numFrames - start);
            std::memset(samples + (start * 2), 0, m * 2 * sizeof(int16_t));
            MuteFrames -= m;
            start += m;
        }

        if (RampInLatched && !Ended())
        {
            RampInLatched = false;
            FadeInFrames = kRampInFrames;
        }

        if (FadeInFrames > 0 && start < numFrames)
        {
            int n = std::min(FadeInFrames, numFrames - start);
            for (int j = 0; j < n; j++)
            {
                // progress against the whole ramp, not what is left of it: the
                // counter falls as the ramp is spent, so measuring from it
                // would restart the gain near zero at the head of every buffer
                int done = kRampInFrames - FadeInFrames + j + 1;
                float g = 0.5f * (1.0f - std::cos((float)M_PI * done / kRampInFrames));
                int16_t* f = samples + ((start + j) * 2);
                f[0] = Saturate((f[0] / 32768.0f) * g);
                f[1] = Saturate((f[1] / 32768.0f) * g);
            }
            FadeInFrames -= n;
        }

        for (int i = 0; i < numFrames; i++)
            Push(samples[(i*2)+0] / 32768.0f, samples[(i*2)+1] / 32768.0f);
    }

private:
    // the next frame of the tail; arms the mute once the tail is spent
    void TailFrame(float& l, float& r)
    {
        int idx = kTailFrames - TailFrames;
        // raised cosine: a linear ramp still corners at both ends
        float g = 0.5f * (1.0f + std::cos((float)M_PI * (idx + 1) / kTailFrames));
        if (TailPeriod)
        {
            int u = idx % TailPeriod;
            // age period continues the waveform from where it stopped;
            // wrapping back to it repeats that cycle under the envelope
            const float* f = TailSrc[TailPeriod - u - 1];
            // the correction must be spent by the time the repeat wraps, or
            // the remainder recurs every cycle as a sawtooth
            int jn = std::min(kJoinFrames, TailPeriod - 1);
            float jw = (u < jn) ? (1.0f - (float)u / jn) : 0.0f;
            l = (f[0] + (TailJoin[0] * jw)) * g;
            r = (f[1] + (TailJoin[1] * jw)) * g;
        }
        else
        {
            l = TailFrom[0] * g;
            r = TailFrom[1] * g;
        }
        TailFrames--;
        if (TailFrames == 0) MuteFrames = kMuteFrames;
    }

    static int16_t Saturate(float v)
    {
        long s = std::lround(v * 32768.0f);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        return (int16_t)s;
    }

    void Push(float l, float r)
    {
        Hist[HistPos][0] = l;
        Hist[HistPos][1] = r;
        HistPos = (HistPos + 1) % kHistFrames;
        if (HistFill < kHistFrames) HistFill++;
        LastOut[0] = l;
        LastOut[1] = r;
    }

    int HistIdx(int age) const
    {
        return (HistPos + kHistFrames - age) % kHistFrames;
    }

    // the frame 'age' frames back from the most recently played one; age 1 is
    // that frame itself. the caller checks age against HistFill.
    const float* HistAt(int age) const
    {
        return Hist[HistIdx(age)];
    }

    // one frame older, wrapping without a modulo
    static int HistPrev(int idx)
    {
        return idx ? (idx - 1) : (kHistFrames - 1);
    }

    // the lag whose copy of the last kCorrFrames frames matches them best.
    // scored by distance rather than correlation: a normalised correlation is
    // blind to level, so the same phrase at half the volume scores a perfect
    // match and repeating it steps the waveform. matched on the channel sum,
    // so a quiet channel's noise cannot choose the period for a loud one.
    // 0 when there is not enough history, leaving the caller on the DC ramp.
    int FindPeriod() const
    {
        if (HistFill < kMinPeriod + kCorrFrames) return 0;
        int maxP = std::min(kMaxPeriod, HistFill - kCorrFrames);

        // the reference window is the same for every candidate, so sum its
        // channels once. the energy only decides whether there is anything
        // here worth matching
        float ref[kCorrFrames];
        float eRef = 0.0f;
        int ia = HistIdx(1);
        for (int k = 0; k < kCorrFrames; k++)
        {
            const float* f = Hist[ia];
            ref[k] = f[0] + f[1];
            eRef += ref[k] * ref[k];
            ia = HistPrev(ia);
        }
        if (eRef <= 0.0f) return 0;

        int best = 0;
        float bestDiff = -1.0f;
        for (int p = kMinPeriod; p <= maxP; p++)
        {
            float diff = 0.0f;
            int ib = HistIdx(1 + p);
            for (int k = 0; k < kCorrFrames; k++)
            {
                const float* b = Hist[ib];
                float d = ref[k] - (b[0] + b[1]);
                diff += d * d;
                ib = HistPrev(ib);
            }
            if (bestDiff < 0.0f || diff < bestDiff)
            {
                bestDiff = diff;
                best = p;
            }
        }
        // no threshold on the score: content with no periodicity has no right
        // answer, and repeating the closest recent stretch of it still hands
        // back the right spectrum at the right level
        return best;
    }

    float Hist[kHistFrames][2];
    int HistPos;
    int HistFill;
    float LastOut[2];

    float TailSrc[kMaxPeriod + 1][2];
    int TailFrames;
    int TailPeriod;
    float TailJoin[2];
    float TailFrom[2];

    int MuteFrames;
    int FadeInFrames;
    bool RampInLatched;
};

#endif // AUDIOSTREAMRAMP_H
