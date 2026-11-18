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

#include "Config.h"
#include "NDS.h"
#include "SPU.h"
#include "Platform.h"
#include "main.h"

#include "AudioSpeed.h"

#include "mic_blow.h"

using namespace melonDS;

// --- AUDIO OUTPUT -----------------------------------------------------------


void EmuInstance::audioInit()
{
    audioVolume = localCfg.GetInt("Audio.Volume");
    audioDSiVolumeSync = localCfg.GetBool("Audio.DSiVolumeSync");

    audioMutedToggle = false;
    audioMutedByFastForward = false;
    audioMutedByWindowFocus = false;
    audioSyncCond = SDL_CreateCond();
    audioSyncLock = SDL_CreateMutex();

    audioFreq = 48000; // TODO: make both of these configurable?
    // 256 rather than 512, for the halved output latency. this used to also cap
    // emulation speed, back when the callback drained the SPU ring.
    audioBufSize = 256;

    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = audioFreq;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = audioBufSize;
    whatIwant.callback = audioCallback;
    whatIwant.userdata = this;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!audioDevice)
    {
        Platform::Log(Platform::LogLevel::Error, "Audio init failed: %s\n", SDL_GetError());
    }
    else
    {
        audioFreq = whatIget.freq;
        audioBufSize = whatIget.samples;
        Platform::Log(Platform::LogLevel::Info, "Audio output frequency: %d Hz\n", audioFreq);
        Platform::Log(Platform::LogLevel::Info, "Audio output buffer size: %d samples\n", audioBufSize);
        SDL_PauseAudioDevice(audioDevice, 1);
    }

    audioRamp.Reset();
    audioTailRequested.store(false, std::memory_order_relaxed);
    audioTailSpent.store(false, std::memory_order_relaxed);
    audioUpdateSpeedUpSettings();
    audioLowPass.Init(audioFreq);
    audioTimeStretch.Reset();
    audioStretchEngaged = false;
    audioLastFrame = 0;
    audioArrivalAvg = 0.0;
    audioSpeedAchieved = 0.0;
    audioLastOffered = 0;
    audioOfferedFrames.store(0, std::memory_order_relaxed);
    audioMeasuredFPS.store(0.0, std::memory_order_relaxed);
    audioSampleFrac = 0.0;
    audioDrainEngaged = false;
    audioFadePrevEngaged = false;
    audioFadeOutFrames = 0;
    audioFadeOutFrom[0] = audioFadeOutFrom[1] = 0.0f;

    micStarted = false;
    micDevice = 0;
    micWavBuffer = nullptr;
    micBuffer = nullptr;

    micLock = SDL_CreateMutex();

    setupMicInputData();
}

void EmuInstance::audioDeInit()
{
    if (audioDevice) SDL_CloseAudioDevice(audioDevice);
    audioDevice = 0;
    micClose();
    micStarted = false;

    if (audioSyncCond) SDL_DestroyCond(audioSyncCond);
    audioSyncCond = nullptr;

    if (audioSyncLock) SDL_DestroyMutex(audioSyncLock);
    audioSyncLock = nullptr;

    if (micWavBuffer) delete[] micWavBuffer;
    micWavBuffer = nullptr;

    if (micLock) SDL_DestroyMutex(micLock);
    micLock = nullptr;
}

void EmuInstance::updateAudioMuteByWindowFocus()
{
    audioMutedByWindowFocus = false;
    if (numEmuInstances() < 2) return;

    switch (mpAudioMode)
    {
        case 1: // only instance 1
            if (instanceID > 0) audioMutedByWindowFocus = true;
            break;

        case 2: // only currently focused instance
            audioMutedByWindowFocus = true;
            for (int i = 0; i < kMaxWindows; i++)
            {
                if (!windowList[i]) continue;
                if (windowList[i]->isFocused())
                {
                    audioMutedByWindowFocus = false;
                    break;
                }
            }
            break;
    }
}

void EmuInstance::toggleAudioMute()
{
    audioMutedToggle = !audioMutedToggle;
}

void EmuInstance::updateFastForwardMute(bool fastForward)
{
    audioMutedByFastForward = fastForward && globalCfg.GetBool("MuteFastForward");
}

bool EmuInstance::audioOffSpeed() const
{
    // both threads decide through here which of them owns the SPU FIFO. they
    // can disagree for one buffer across a speed change; it self-corrects.
    if (!audioTimeStretchEnable.load(std::memory_order_relaxed)) return false;
    return audioIsOffSpeed(curFPS, targetFPS);
}

void EmuInstance::audioDrainSPU()
{
    // draining here rather than from the audio callback is what removes the
    // speed ceiling: the callback could only take one ring's worth per period
    if (!audioDevice || !audioOffSpeed())
    {
        audioDrainEngaged = false;
        return;
    }

    if (!audioDrainEngaged)
    {
        audioDrainEngaged = true;
        audioTimeStretch.BeginSession();
    }

    SDL_LockMutex(audioSyncLock);
    int avail = nds->SPU.GetOutputSize();
    if (avail > kAudioDrainMax) avail = kAudioDrainMax;
    int got = (avail > 0) ? nds->SPU.ReadOutput(audioDrainTemp, avail) : 0;
    SDL_CondSignal(audioSyncCond);
    SDL_UnlockMutex(audioSyncLock);

    if (got > 0)
    {
        audioOfferedFrames.fetch_add(got, std::memory_order_release);

        // frames are already out of the SPU FIFO, so a short accept loses them.
        // mark the discontinuity so it resyncs rather than splicing.
        if (audioTimeStretch.Write(audioDrainTemp, got) < got)
            audioTimeStretch.BeginSession();
    }
}

void EmuInstance::audioMarkDiscontinuity()
{
    audioTimeStretch.BeginSession();
}

bool EmuInstance::audioJumpBegin()
{
    return audioStreamEnd();
}

// emu thread. bounded: the ring is finite and nothing feeds it here.
void EmuInstance::audioDrainRing()
{
    if (nds == nullptr) return;

    SDL_LockMutex(audioSyncLock);
    for (int i = 0; i < 8 && nds->SPU.GetOutputSize() > 0; i++)
        nds->SPU.ReadOutput(audioDrainTemp, kAudioDrainMax);
    SDL_UnlockMutex(audioSyncLock);
}

void EmuInstance::audioJumpEnd(bool ramped)
{
    // the SPU output ring is not part of a savestate, so it still holds
    // audio from before the jump; that would come back spliced behind the
    // tail, or behind the resume ramp if the jump was made while paused.
    audioDrainRing();

    if (!ramped) return;

    // the device kept running through the jump, so the ramp is armed under
    // its lock rather than with it paused
    if (audioDevice) SDL_LockAudioDevice(audioDevice);
    audioTailRequested.store(false, std::memory_order_relaxed);
    audioTailSpent.store(false, std::memory_order_relaxed);
    audioRamp.Begin();
    // a cross-fade still running would re-inject the pre-jump level on top of
    // the frames the ramp is bringing back
    audioFadeOutFrames = 0;
    audioFadeOutFrom[0] = audioFadeOutFrom[1] = 0.0f;
    if (audioDevice) SDL_UnlockAudioDevice(audioDevice);
}

void EmuInstance::audioSetMeasuredFPS(double fps)
{
    audioMeasuredFPS.store(fps, std::memory_order_relaxed);
}

void EmuInstance::audioSync()
{
    // a paused device drains nothing, and neither the callback nor audioDrainSPU
    // can signal from there, so the wait would only spend its timeout
    if (audioDevice && SDL_GetAudioDeviceStatus(audioDevice) == SDL_AUDIO_PLAYING)
    {
        SDL_LockMutex(audioSyncLock);
        while (nds->SPU.GetOutputSize() >= audioBufSize)
        {
            int ret = SDL_CondWaitTimeout(audioSyncCond, audioSyncLock, 500);
            if (ret == SDL_MUTEX_TIMEDOUT) break;
        }
        SDL_UnlockMutex(audioSyncLock);
    }
}

void EmuInstance::audioCallback(void* data, Uint8* stream, int len)
{
    EmuInstance* inst = (EmuInstance*)data;
    len /= (sizeof(s16) * 2);

    if (!inst->audioOffSpeed())
    {
        // bank the speed the hold reached before the average is thrown away
        if (inst->audioStretchEngaged && len > 0)
        {
            double reached = inst->audioArrivalAvg / len;
            if (std::isfinite(reached) && reached > 1.05)
                inst->audioSpeedAchieved = reached;
        }

        // no Reset() here: this is the audio thread, and the emu thread can
        // still be inside Write. re-engaging resyncs via BeginSession anyway.
        inst->audioStretchEngaged = false;
        inst->audioDirectRead((s16*) stream, len);
        return;
    }

    if (!inst->audioStretchEngaged)
    {
        inst->audioStretchEngaged = true;

        // the requested speed is a ceiling the host may not reach, and the
        // average takes most of a short hold to walk down from it. seed from
        // what the last hold actually reached instead, capped by the request
        // only where something enforces it: curFPS is 1000 for uncapped
        // fast-forward, and nothing paces the loop with the limiter off.
        double requested = audioSpeedRatio(inst->curFPS, inst->targetFPS);
        double anchor = requested;
        if (requested > 1.05 && inst->audioSpeedAchieved > 1.05)
        {
            anchor = inst->audioSpeedAchieved;
            if (inst->doLimitFPS && inst->curFPS < 999.0 && anchor > requested)
                anchor = requested;
        }
        inst->audioArrivalAvg = len * anchor;
    }
    inst->audioStretchedRead((s16*) stream, len);
}

// how much input one output buffer is worth at the current speed. without this
// the direct path consumes faster than the SPU fills and gates to silence.
int EmuInstance::audioGetNumSamplesOut(int outlen)
{
    double f_len_in = (outlen * audioSpeedRatio(curFPS, targetFPS)) + audioSampleFrac;

    // capped before the cast: targetFPS goes down to 0.0001, which would
    // otherwise make this far too large for an int
    if (f_len_in > outlen) f_len_in = outlen;
    if (f_len_in < 0.0) f_len_in = 0.0;

    int len_in = (int)f_len_in;
    audioSampleFrac = f_len_in - len_in;
    return len_in;
}

void EmuInstance::audioDirectRead(s16* stream, int len)
{
    int len_in = audioGetNumSamplesOut(len);

    SDL_LockMutex(audioSyncLock);
    int num_in = nds->SPU.ReadOutput(stream, len_in);
    SDL_CondSignal(audioSyncCond);
    SDL_UnlockMutex(audioSyncLock);

    audioFinishBuffer(stream, len, num_in);
}

void EmuInstance::audioStretchedRead(s16* stream, int len)
{
    // the emu thread fills the stretcher; what it has offered gives us the
    // arrival rate, which is lumpy, so smooth it before using it as a rate
    melonDS::s64 offered = audioOfferedFrames.load(std::memory_order_acquire);
    melonDS::s64 delta = offered - audioLastOffered;
    if (delta < 0) delta = 0;
    audioLastOffered = offered;

    audioArrivalAvg += (delta - audioArrivalAvg) * 0.05;

    double ratio = audioComputeStretchRatio(audioArrivalAvg, len,
                                            audioTimeStretch.InputFill(),
                                            AudioTimeStretch::TargetInputFill(audioArrivalAvg));
    int num_in = audioTimeStretch.Read(stream, len, ratio);

    audioFinishBuffer(stream, len, num_in);
}

double EmuInstance::audioLowPassCutoff() const
{
    int ref = audioLowPassEnable.load(std::memory_order_relaxed)
              ? audioSpeedUpLowPass.load(std::memory_order_relaxed)
              : kSpeedUpLowPassOff;

    // the setpoint decides whether we are off-speed at all, so that normal
    // speed stays exactly transparent and measurement jitter can't engage the
    // filter. how far off-speed comes from the achieved rate: curFPS is 1000
    // for an uncapped fast-forward and would over-filter by the shortfall.
    double fps = curFPS;
    if (audioIsOffSpeed(curFPS, targetFPS))
    {
        double measured = audioMeasuredFPS.load(std::memory_order_relaxed);
        if (measured > 0.0) fps = measured;
    }

    return audioComputeLowPassCutoff(fps, targetFPS, ref, audioLowPass.WideOpenCutoff());
}

// switching between the stretcher and the raw FIFO jumps the output by the
// stretcher's reserve in one direction or the other. the two sides cannot be
// joined seamlessly, and a short equal-power cross-fade from the level the
// stream stopped at is less audible than a splice.
void EmuInstance::audioArmHandoverFade()
{
    // not while one is running: the edge can arrive twice in quick succession
    if (audioFadeOutFrames != 0) return;

    audioFadeOutFrames = kAudioHandoverFadeFrames;
    audioFadeOutFrom[0] = (s16)(audioLastFrame & 0xFFFF) / 32768.0f;
    audioFadeOutFrom[1] = (s16)(audioLastFrame >> 16) / 32768.0f;
}

void EmuInstance::audioFinishBuffer(s16* stream, int len, int num_in)
{
    // the stream is being taken down: play the tail in place of the source,
    // which was still read so the ring drains
    bool tailRequested = audioTailRequested.load(std::memory_order_acquire);
    if (tailRequested) num_in = 0;

    if (audioMutedByWindowFocus || audioMutedToggle || audioMutedByFastForward)
    {
        memset(stream, 0, len*sizeof(s16)*2);
        audioLastFrame = 0;
        // keep the cutoff smoother moving while muted, or re-engaging clicks.
        // with MuteFastForward on, this path is the fast-forward case.
        audioLowPass.ProcessMuted(len, audioLowPassCutoff(), len / (double)audioFreq);
        // the ramp's history follows the silence, so a pause while muted has
        // no tail to play
        audioRamp.Track(stream, len);
        if (tailRequested && audioRamp.TailSpent()) audioTailSpent.store(true, std::memory_order_release);
        // follow the handover edge without arming a fade: silence needs none,
        // and an unmute next to one would otherwise fade from a zeroed level
        audioFadePrevEngaged = audioStretchEngaged;
        return;
    }

    // slow-mo with the stretcher off is untreated by design: pad the buffer by
    // holding the last frame, as it always was
    if (num_in >= 1 && num_in < len && !audioStretchEngaged && audioIsOffSpeed(curFPS, targetFPS))
    {
        int last = num_in-1;

        for (int i = num_in; i < len; i++)
            ((u32*)stream)[i] = ((u32*)stream)[last];
        num_in = len;
    }

    // frames came back after the stream ended: bring them in on the ramp
    if (num_in >= 1 && audioRamp.Ended()) audioRamp.Begin();
    audioRamp.Track(stream, num_in);

    if (num_in < len)
    {
        // the source ran out, or the stream is being taken down: end it on a
        // tail from where it stopped rather than a step. the low-pass below
        // runs over the tail too, so its state stays in step with what plays.
        if (!audioRamp.Ended()) audioRamp.End();
        audioRamp.FillTail(stream + (num_in * 2), len - num_in);
        if (tailRequested && audioRamp.TailSpent())
            audioTailSpent.store(true, std::memory_order_release);
    }

    audioLowPass.Process((int16_t*) stream, len, audioLowPassCutoff(),
                         len / (double)audioFreq);

    // either edge of the handover, seen on the buffer that first crosses it
    if (audioStretchEngaged != audioFadePrevEngaged)
    {
        audioFadePrevEngaged = audioStretchEngaged;
        audioArmHandoverFade();
    }

    if (audioFadeOutFrames > 0)
    {
        // smoothstep the progress so the gain leaves and arrives with zero
        // slope; a step in rate of change is heard as a blip at each end
        int n = std::min<int>(audioFadeOutFrames, len);
        for (int j = 0; j < n; j++)
        {
            unsigned done = kAudioHandoverFadeFrames - audioFadeOutFrames + j + 1;
            float u = (float)done / kAudioHandoverFadeFrames;
            float th = 0.5f * (float)M_PI * (u * u * (3.0f - 2.0f * u));
            float g = std::cos(th);
            float gn = std::sin(th);
            float env = 1.0f - ((1.0f - kAudioHandoverDipDepth)
                                * std::sin((float)M_PI * done / kAudioHandoverFadeFrames));
            for (int ch = 0; ch < 2; ch++)
            {
                float v = ((audioFadeOutFrom[ch] * g) + ((stream[(j*2)+ch] / 32768.0f) * gn)) * env;
                stream[(j*2)+ch] = (s16)std::clamp(std::lround(v * 32768.0f), -32768L, 32767L);
            }
        }
        audioFadeOutFrames -= n;
    }

    // pre-volume: what continues from this frame is scaled once, by the
    // volume below
    if (len > 0) audioLastFrame = ((u32*)stream)[len-1];

    if (audioVolume < 256)
    {
        for (int i = 0; i < len * 2; i++)
            stream[i] = ((s32) stream[i] * audioVolume) >> 8;
    }
}


// --- MIC INPUT --------------------------------------------------------------


void EmuInstance::micOpen()
{
    memset(micExtBuffer, 0, sizeof(micExtBuffer));
    micExtBufferWritePos = 0;
    micExtBufferCount = 0;
    micBufferReadPos = 0;

    if (micDevice) return;

    if (micInputType != micInputType_External)
    {
        micDevice = 0;
        return;
    }

    int numMics = SDL_GetNumAudioDevices(1);
    if (numMics == 0)
        return;

    micFreq = 48000;
    micBufSize = 1024;
    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = micFreq;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 1;
    whatIwant.samples = micBufSize;
    whatIwant.callback = micCallback;
    whatIwant.userdata = this;
    const char* mic = NULL;
    if (micDeviceName != "")
    {
        mic = micDeviceName.c_str();
    }
    micDevice = SDL_OpenAudioDevice(mic, 1, &whatIwant, &whatIget, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!micDevice)
    {
        Platform::Log(Platform::LogLevel::Error, "Mic init failed: %s\n", SDL_GetError());
    }
    else
    {
        micFreq = whatIget.freq;
        micBufSize = whatIget.samples;
        Platform::Log(Platform::LogLevel::Info, "Mic output frequency: %d Hz\n", micFreq);
        Platform::Log(Platform::LogLevel::Info, "Mic output buffer size: %d samples\n", micBufSize);
        SDL_PauseAudioDevice(micDevice, 0);
    }

    micSampleFrac = 0;
}

void EmuInstance::micClose()
{
    if (micDevice)
        SDL_CloseAudioDevice(micDevice);

    micDevice = 0;
}

void EmuInstance::micStart()
{
    micStarted = true;
    micOpen();
}

void EmuInstance::micStop()
{
    micClose();
    micStarted = false;
}

void EmuInstance::micLoadWav(const std::string& name)
{
    SDL_AudioSpec format;
    memset(&format, 0, sizeof(SDL_AudioSpec));

    if (micWavBuffer) delete[] micWavBuffer;
    micWavBuffer = nullptr;
    micWavLength = 0;

    u8* buf;
    u32 len;
    if (!SDL_LoadWAV(name.c_str(), &format, &buf, &len))
        return;

    if (len > 0x4000000)
    {
        SDL_FreeWAV(buf);
        return;
    }

    SDL_AudioCVT cvt;
    int cvtres = SDL_BuildAudioCVT(&cvt,
        format.format, format.channels, format.freq,
        AUDIO_S16LSB, 1, 47743);

    if (cvtres < 0)
    {
        // failure
        SDL_FreeWAV(buf);
        return;
    }

    if (cvtres == 0)
    {
        // no conversion needed
        micWavLength = len >> 1;
        micWavBuffer = new s16[micWavLength];
        memcpy(micWavBuffer, buf, len);
    }
    else
    {
        // apply conversion
        cvt.len = len;
        cvt.buf = new u8[cvt.len * cvt.len_mult];
        memcpy(cvt.buf, buf, len);

        if (SDL_ConvertAudio(&cvt) < 0)
        {
            delete[] cvt.buf;
            SDL_FreeWAV(buf);
            return;
        }

        micWavLength = cvt.len_cvt >> 1;
        micWavBuffer = new s16[micWavLength];
        memcpy(micWavBuffer, cvt.buf, cvt.len_cvt);
        delete[] cvt.buf;
    }

    SDL_FreeWAV(buf);
}

void EmuInstance::setupMicInputData()
{
    if (micWavBuffer != nullptr)
    {
        delete[] micWavBuffer;
        micWavBuffer = nullptr;
        micWavLength = 0;
    }

    micInputType = globalCfg.GetInt("Mic.InputType");
    micDeviceName = globalCfg.GetString("Mic.Device");
    micWavPath = globalCfg.GetString("Mic.WavPath");

    switch (micInputType)
    {
        case micInputType_Silence:
            micBuffer = nullptr;
            micBufferLength = 0;
            break;
        case micInputType_External:
            micBuffer = micExtBuffer;
            micBufferLength = sizeof(micExtBuffer) / sizeof(s16);
            break;
        case micInputType_Noise:
            micBuffer = (s16*)&mic_blow[0];
            micBufferLength = sizeof(mic_blow) / sizeof(s16);
            break;
        case micInputType_Wav:
            micLoadWav(micWavPath);
            micBuffer = micWavBuffer;
            micBufferLength = micWavLength;
            break;
    }

    micBufferReadPos = 0;
}

int EmuInstance::micReadInput(s16* data, int maxlength)
{
    int type = micInputType;
    if ((type == micInputType_External) && (micExtBufferCount == 0))
        return 0;

    bool cmd = hotkeyDown(HK_Mic);

    if ((!micBuffer) ||
        ((type != micInputType_External) && (!cmd)))
    {
        type = micInputType_Silence;
    }

    if (type == micInputType_Silence)
    {
        micBufferReadPos = 0;
        memset(data, 0, maxlength * sizeof(s16));
        return maxlength;
    }

    if (type == micInputType_External)
        SDL_LockMutex(micLock);

    int readlength = 0;
    while (readlength < maxlength)
    {
        int thislen = maxlength - readlength;
        if ((micBufferReadPos + thislen) > micBufferLength)
            thislen = micBufferLength - micBufferReadPos;

        if (type == micInputType_External)
        {
            if (thislen > micExtBufferCount)
                thislen = micExtBufferCount;

            micExtBufferCount -= thislen;
        }

        if (!thislen)
            break;

        memcpy(data, &micBuffer[micBufferReadPos], thislen * sizeof(s16));
        data += thislen;
        micBufferReadPos += thislen;
        if (micBufferReadPos >= micBufferLength)
            micBufferReadPos -= micBufferLength;

        readlength += thislen;
    }

    if (type == micInputType_External)
        SDL_UnlockMutex(micLock);

    return readlength;
}

int EmuInstance::micGetNumSamplesIn(int inlen)
{
    float f_len_out = (inlen * 47743.4659091 * (curFPS/60.0)) / (float)micFreq;
    f_len_out += micSampleFrac;
    int len_out = (int)floor(f_len_out);
    micSampleFrac = f_len_out - len_out;

    return len_out;
}

void EmuInstance::micResample(s16* inbuf, int inlen)
{
    int maxlen = sizeof(micExtBuffer) / sizeof(s16);
    int outlen = micGetNumSamplesIn(inlen);

    // alter output length slightly to keep the buffer happy
    if (micExtBufferCount < (maxlen >> 2))
        outlen += 6;
    else if (micExtBufferCount > (3 * (maxlen >> 2)))
        outlen -= 6;

    float res_incr = inlen / (float)outlen;
    float res_timer = -0.5;
    int res_pos = 0;

    for (int i = 0; i < outlen; i++)
    {
        if (micExtBufferCount >= maxlen)
            break;

        s16 s1 = inbuf[res_pos];
        s16 s2 = inbuf[res_pos + 1];

        float s = (float)s1 + ((s2 - s1) * res_timer);

        micExtBuffer[micExtBufferWritePos] = (s16)round(s);
        micExtBufferWritePos++;
        if (micExtBufferWritePos >= maxlen)
            micExtBufferWritePos = 0;

        micExtBufferCount++;

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }
}

void EmuInstance::micCallback(void* data, Uint8* stream, int len)
{
    EmuInstance* inst = (EmuInstance*)data;
    s16* input = (s16*)stream;
    len /= sizeof(s16);

    SDL_LockMutex(inst->micLock);
    inst->micResample(input, len);
    SDL_UnlockMutex(inst->micLock);
}





// atomics, so the audio settings dialog can apply these live with everything
// running - which is what lets the cutoff be set by ear during fast-forward
void EmuInstance::audioUpdateSpeedUpSettings()
{
    audioSpeedUpLowPass.store(globalCfg.GetInt("SpeedUpLowPass"), std::memory_order_relaxed);
    audioTimeStretchEnable.store(globalCfg.GetBool("SpeedUpTimeStretch"), std::memory_order_relaxed);
    audioLowPassEnable.store(globalCfg.GetBool("SpeedUpLowPassEnable"), std::memory_order_relaxed);
}

// from the audio settings dialog, which does not pause the emu thread
void EmuInstance::audioUpdateSettings()
{
    if (micStarted) micClose();

    if (nds != nullptr)
    {
        int audiointerp = globalCfg.GetInt("Audio.Interpolation");
        nds->SPU.SetInterpolation(static_cast<AudioInterpolation>(audiointerp));
    }

    audioUpdateSpeedUpSettings();

    setupMicInputData();
    if (micStarted) micOpen();
}

// from the interface settings dialog, and it has to stay there: SetOutputSkew
// reaches blip_set_rates, which the emu thread reads with no lock, and that
// dialog is the only settings path that pauses.
void EmuInstance::audioUpdateOutputSkew()
{
    if (nds != nullptr) nds->SPU.SetOutputSkew(audioComputeOutputSkew(targetFPS));
}

void EmuInstance::audioEnable()
{
    // whatever the ring still holds was never played: the tail replaced it
    // while the stream went down. a frame step is the loud case - it runs
    // frames with the device paused, and they would all play at the resume.
    audioDrainRing();

    // covers emulator reset, which brackets with audioDisable/audioEnable,
    // else pre-reset audio splices into post. savestate load does not come
    // through here at all - see audioMarkDiscontinuity. Reset clears state the
    // audio thread owns, so hold the device lock.
    if (audioDevice) SDL_LockAudioDevice(audioDevice);
    // the stream is coming back: ramp its first frames in. the ramp's own
    // state is not reset, a tail's mute still applies to what comes back.
    audioTailRequested.store(false, std::memory_order_relaxed);
    audioTailSpent.store(false, std::memory_order_relaxed);
    audioRamp.Begin();
    audioTimeStretch.Reset();
    audioStretchEngaged = false;
    audioLastFrame = 0;
    audioArrivalAvg = 0.0;
    audioSpeedAchieved = 0.0;
    audioLastOffered = 0;
    audioOfferedFrames.store(0, std::memory_order_relaxed);
    audioSampleFrac = 0.0;
    audioDrainEngaged = false;
    audioFadePrevEngaged = false;
    audioFadeOutFrames = 0;
    audioFadeOutFrom[0] = audioFadeOutFrom[1] = 0.0f;
    if (audioDevice) SDL_UnlockAudioDevice(audioDevice);

    // the skew depends only on targetFPS, so it doesn't belong in the audio
    // callback, where it raced SPU::Reset through blip_set_rates. here and in
    // audioUpdateOutputSkew covers every time it can change.
    if (nds != nullptr) nds->SPU.SetOutputSkew(audioComputeOutputSkew(targetFPS));

    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
    if (micStarted) micOpen();
}

// emu thread. end the stream on its tail: the callback plays it in place of
// the source from the next buffer on. the tail is two callbacks long, so wait
// for the callback to report it spent, bounded so a device that has stopped
// calling back cannot hold the emu thread. false if the stream was not up.
bool EmuInstance::audioStreamEnd()
{
    if (!audioDevice || SDL_GetAudioDeviceStatus(audioDevice) != SDL_AUDIO_PLAYING) return false;
    if (audioTailRequested.load(std::memory_order_acquire)) return false;

    audioTailSpent.store(false, std::memory_order_relaxed);
    audioTailRequested.store(true, std::memory_order_release);
    int waitMs = (4 * 1000 * audioBufSize) / audioFreq + 1;
    for (int i = 0; i < waitMs && !audioTailSpent.load(std::memory_order_acquire); i++)
        SDL_Delay(1);
    return true;
}

void EmuInstance::audioDisable()
{
    audioStreamEnd();

    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
    if (micStarted) micClose();
}
