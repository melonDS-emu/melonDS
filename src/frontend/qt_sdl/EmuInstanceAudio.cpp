/*
    Copyright 2016-2025 melonDS team

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

#include "mic_blow.h"

using namespace melonDS;

#define INTERNAL_FRAME_RATE 59.8260982880808f

// --- AUDIO OUTPUT -----------------------------------------------------------


void EmuInstance::audioInit()
{
    audioVolume = localCfg.GetInt("Audio.Volume");
    audioDSiVolumeSync = localCfg.GetBool("Audio.DSiVolumeSync");

    audioMuted = false;
    audioSyncCond = SDL_CreateCond();
    audioSyncLock = SDL_CreateMutex();

    audioFreq = 48000; // TODO: make both of these configurable?
    audioBufSize = 512;

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

    audioSampleFrac = 0;

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

void EmuInstance::audioMute()
{
    audioMuted = false;
    if (numEmuInstances() < 2) return;

    switch (mpAudioMode)
    {
        case 1: // only instance 1
            if (instanceID > 0) audioMuted = true;
            break;

        case 2: // only currently focused instance
            audioMuted = true;
            for (int i = 0; i < kMaxWindows; i++)
            {
                if (!windowList[i]) continue;
                if (windowList[i]->isFocused())
                {
                    audioMuted = false;
                    break;
                }
            }
            break;
    }
}

void EmuInstance::audioSync()
{
    if (audioDevice)
    {
        SDL_LockMutex(audioSyncLock);
        while (nds->SPU.GetOutputSize() >= audioFreq / INTERNAL_FRAME_RATE)
        {
            int ret = SDL_CondWaitTimeout(audioSyncCond, audioSyncLock, 500);
            if (ret == SDL_MUTEX_TIMEDOUT) break;
        }
        SDL_UnlockMutex(audioSyncLock);
    }
}

int EmuInstance::audioGetNumSamplesOut(int outlen)
{
    float f_len_in = outlen * (curFPS/targetFPS);
    f_len_in += audioSampleFrac;
    int len_in = (int)floor(f_len_in);
    audioSampleFrac = f_len_in - len_in;

    return len_in;
}

void EmuInstance::audioCallback(void* data, Uint8* stream, int len)
{
    EmuInstance* inst = (EmuInstance*)data;
    len /= (sizeof(s16) * 2);

    double skew = std::clamp(inst->targetFPS / INTERNAL_FRAME_RATE, 0.995, 1.005);
    inst->nds->SPU.SetOutputSkew(skew);

    int len_in = inst->audioGetNumSamplesOut(len);
    if (len_in > inst->audioBufSize) len_in = inst->audioBufSize;
    s16 buf_in[inst->audioBufSize*2];

    SDL_LockMutex(inst->audioSyncLock);
    int num_in = inst->nds->SPU.ReadOutput((s16*) stream, len_in);
    SDL_CondSignal(inst->audioSyncCond);
    SDL_UnlockMutex(inst->audioSyncLock);

    if ((num_in < 1) || inst->audioMuted)
    {
        memset(stream, 0, len*sizeof(s16)*2);
        return;
    }

    if (inst->audioVolume < 256)
    {
        s16* samples = (s16*) stream;
        for (int i = 0; i < num_in * 2; i++)
            samples[i] = ((s32) samples[i] * inst->audioVolume) >> 8;
    }

    int margin = 6;
    if (num_in < len_in-margin)
    {
        int last = num_in-1;

        for (int i = num_in; i < len_in-margin; i++)
            ((u32*)stream)[i] = ((u32*)stream)[last];
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





void EmuInstance::audioUpdateSettings()
{
    if (micStarted) micClose();

    if (nds != nullptr)
    {
        int audiointerp = globalCfg.GetInt("Audio.Interpolation");
        nds->SPU.SetInterpolation(static_cast<AudioInterpolation>(audiointerp));
    }

    setupMicInputData();
    if (micStarted) micOpen();
}

void EmuInstance::audioEnable()
{
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
    if (micStarted) micOpen();
}

void EmuInstance::audioDisable()
{
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
    if (micStarted) micClose();
}
