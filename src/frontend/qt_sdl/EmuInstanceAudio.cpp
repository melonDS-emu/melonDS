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


// --- AUDIO OUTPUT -----------------------------------------------------------


void EmuInstance::audioInit()
{
    audioVolume = localCfg.GetInt("Audio.Volume");
    audioDSiVolumeSync = localCfg.GetBool("Audio.DSiVolumeSync");

    audioMuted = false;
    audioSyncCond = SDL_CreateCond();
    audioSyncLock = SDL_CreateMutex();

    audioFreq = 48000; // TODO: make both of these configurable?
    audioBufSize = 1024;
    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = audioFreq;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = audioBufSize;
    whatIwant.callback = audioCallback;
    whatIwant.userdata = this;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
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

    memset(micExtBuffer, 0, sizeof(micExtBuffer));
    micExtBufferWritePos = 0;
    micExtBufferCount = 0;
    micWavBuffer = nullptr;

    micBuffer = nullptr;
    micBufferLength = 0;
    micBufferReadPos = 0;

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
        while (nds->SPU.GetOutputSize() > audioBufSize)
        {
            int ret = SDL_CondWaitTimeout(audioSyncCond, audioSyncLock, 500);
            if (ret == SDL_MUTEX_TIMEDOUT) break;
        }
        SDL_UnlockMutex(audioSyncLock);
    }
}

int EmuInstance::audioGetNumSamplesOut(int outlen)
{
    //float f_len_in = (outlen * 32823.6328125 * (curFPS/60.0)) / (float)audioFreq;
    float f_len_in = (outlen * 47743.4659091 * (curFPS/60.0)) / (float)audioFreq;
    f_len_in += audioSampleFrac;
    int len_in = (int)floor(f_len_in);
    audioSampleFrac = f_len_in - len_in;

    return len_in;
}

void EmuInstance::audioResample(s16* inbuf, int inlen, s16* outbuf, int outlen, int volume)
{
    float res_incr = inlen / (float)outlen;
    float res_timer = -0.5;
    int res_pos = 0;

    for (int i = 0; i < outlen; i++)
    {
        s16 l1 = inbuf[res_pos * 2];
        s16 l2 = inbuf[res_pos * 2 + 2];
        s16 r1 = inbuf[res_pos * 2 + 1];
        s16 r2 = inbuf[res_pos * 2 + 3];

        float l = (float) l1 + ((l2 - l1) * res_timer);
        float r = (float) r1 + ((r2 - r1) * res_timer);

        outbuf[i*2  ] = (s16) (((s32) round(l) * volume) >> 8);
        outbuf[i*2+1] = (s16) (((s32) round(r) * volume) >> 8);

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos++;
        }
    }
}

void EmuInstance::audioCallback(void* data, Uint8* stream, int len)
{
    EmuInstance* inst = (EmuInstance*)data;
    len /= (sizeof(s16) * 2);

    // resample incoming audio to match the output sample rate

    int len_in = inst->audioGetNumSamplesOut(len);
    if (len_in > inst->audioBufSize) len_in = inst->audioBufSize;
    s16 buf_in[inst->audioBufSize*2];
    int num_in;

    SDL_LockMutex(inst->audioSyncLock);
    num_in = inst->nds->SPU.ReadOutput(buf_in, len_in);
    SDL_CondSignal(inst->audioSyncCond);
    SDL_UnlockMutex(inst->audioSyncLock);

    if ((num_in < 1) || inst->audioMuted)
    {
        memset(stream, 0, len*sizeof(s16)*2);
        return;
    }

    int margin = 6;
    if (num_in < len_in-margin)
    {
        int last = num_in-1;

        for (int i = num_in; i < len_in-margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = len_in-margin;
    }

    inst->audioResample(buf_in, num_in, (s16*)stream, len, inst->audioVolume);
}


// --- MIC INPUT --------------------------------------------------------------


void EmuInstance::micOpen()
{
    if (micDevice) return;

    if (micInputType != micInputType_External)
    {
        micDevice = 0;
        return;
    }

    int numMics = SDL_GetNumAudioDevices(1);
    if (numMics == 0)
        return;

    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 48000;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 1;
    whatIwant.samples = 1024;
    whatIwant.callback = micCallback;
    whatIwant.userdata = this;
    const char* mic = NULL;
    if (micDeviceName != "")
    {
        mic = micDeviceName.c_str();
    }
    micDevice = SDL_OpenAudioDevice(mic, 1, &whatIwant, &whatIget, 0);
    if (!micDevice)
    {
        Platform::Log(Platform::LogLevel::Error, "Mic init failed: %s\n", SDL_GetError());
    }
    else
    {
        SDL_PauseAudioDevice(micDevice, 0);
    }
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

void EmuInstance::micProcess()
{
#if 0
    SDL_LockMutex(micLock);

    int type = micInputType;
    bool cmd = hotkeyDown(HK_Mic);

    if (type != micInputType_External && !cmd)
    {
        type = micInputType_Silence;
    }

    const int kFrameLen = 735;

    switch (type)
    {
        case micInputType_Silence: // no mic
            micBufferReadPos = 0;
            nds->MicInputFrame(nullptr, 0);
            break;

        case micInputType_External: // host mic
        case micInputType_Wav: // WAV
            if (micBuffer)
            {
                int len = kFrameLen;
                if (micExtBufferCount < len)
                    len = micExtBufferCount;

                s16 tmp[kFrameLen];

                if ((micBufferReadPos + len) > micBufferLength)
                {
                    u32 part1 = micBufferLength - micBufferReadPos;
                    memcpy(&tmp[0], &micBuffer[micBufferReadPos], part1*sizeof(s16));
                    memcpy(&tmp[part1], &micBuffer[0], (len - part1)*sizeof(s16));

                    micBufferReadPos = len - part1;
                }
                else
                {
                    memcpy(&tmp[0], &micBuffer[micBufferReadPos], len*sizeof(s16));

                    micBufferReadPos += len;
                }

                if (len == 0)
                {
                    memset(tmp, 0, sizeof(tmp));
                }
                else if (len < kFrameLen)
                {
                    for (int i = len; i < kFrameLen; i++)
                        tmp[i] = tmp[len-1];
                }
                nds->MicInputFrame(tmp, 735);

                micExtBufferCount -= len;
            }
            else
            {
                micBufferReadPos = 0;
                nds->MicInputFrame(nullptr, 0);
            }
            break;

        case micInputType_Noise: // blowing noise
            {
                int sample_len = sizeof(mic_blow) / sizeof(u16);
                static int sample_pos = 0;

                s16 tmp[kFrameLen];

                for (int i = 0; i < kFrameLen; i++)
                {
                    tmp[i] = mic_blow[sample_pos] ^ 0x8000;
                    sample_pos++;
                    if (sample_pos >= sample_len) sample_pos = 0;
                }

                nds->MicInputFrame(tmp, kFrameLen);
            }
            break;
    }

    SDL_UnlockMutex(micLock);
#endif
}

int EmuInstance::micReadInput(s16* data, int maxlength)
{
    int type = micInputType;
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

    // TODO adjustments for external mic

    int readlength = 0;
    while (readlength < maxlength)
    {
        int thislen = maxlength - readlength;
        if ((micBufferReadPos + thislen) > micBufferLength)
            thislen = micBufferLength - micBufferReadPos;

        memcpy(data, &micBuffer[micBufferReadPos], thislen * sizeof(s16));
        data += thislen;
        micBufferReadPos += thislen;
        if (micBufferReadPos >= micBufferLength)
            micBufferReadPos -= micBufferLength;

        readlength += thislen;
    }

    return readlength;
}

void EmuInstance::micCallback(void* data, Uint8* stream, int len)
{
    EmuInstance* inst = (EmuInstance*)data;
    s16* input = (s16*)stream;
    len /= sizeof(s16);

    SDL_LockMutex(inst->micLock);
    int maxlen = sizeof(micExtBuffer) / sizeof(s16);

    if ((inst->micExtBufferCount + len) > maxlen)
        len = maxlen - inst->micExtBufferCount;

    if ((inst->micExtBufferWritePos + len) > maxlen)
    {
        u32 len1 = maxlen - inst->micExtBufferWritePos;
        memcpy(&inst->micExtBuffer[inst->micExtBufferWritePos], &input[0], len1*sizeof(s16));
        memcpy(&inst->micExtBuffer[0], &input[len1], (len - len1)*sizeof(s16));
        inst->micExtBufferWritePos = len - len1;
    }
    else
    {
        memcpy(&inst->micExtBuffer[inst->micExtBufferWritePos], input, len*sizeof(s16));
        inst->micExtBufferWritePos += len;
    }

    inst->micExtBufferCount += len;
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
