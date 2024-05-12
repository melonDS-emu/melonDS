/*
    Copyright 2016-2023 melonDS team

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

#include "AudioInOut.h"

#include <SDL2/SDL.h>

#include "FrontendUtil.h"
#include "Config.h"
#include "NDS.h"
#include "SPU.h"
#include "Platform.h"
#include "Input.h"
#include "main.h"

using namespace melonDS;
namespace AudioInOut
{

SDL_AudioDeviceID audioDevice;
int audioFreq;
bool audioMuted;
SDL_cond* audioSync;
SDL_mutex* audioSyncLock;

SDL_AudioDeviceID micDevice;
s16 micExtBuffer[2048];
u32 micExtBufferWritePos;

u32 micWavLength;
s16* micWavBuffer;

void AudioCallback(void* data, Uint8* stream, int len)
{
    len /= (sizeof(s16) * 2);

    // resample incoming audio to match the output sample rate

    int len_in = Frontend::AudioOut_GetNumSamples(len);
    s16 buf_in[1024*2];
    int num_in;

    EmuThread* emuThread = (EmuThread*)data;
    SDL_LockMutex(audioSyncLock);
    num_in = emuThread->NDS->SPU.ReadOutput(buf_in, len_in);
    SDL_CondSignal(audioSync);
    SDL_UnlockMutex(audioSyncLock);

    if ((num_in < 1) || audioMuted)
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

    Frontend::AudioOut_Resample(buf_in, num_in, (s16*)stream, len, Config::AudioVolume);
}

void MicCallback(void* data, Uint8* stream, int len)
{
    s16* input = (s16*)stream;
    len /= sizeof(s16);

    int maxlen = sizeof(micExtBuffer) / sizeof(s16);

    if ((micExtBufferWritePos + len) > maxlen)
    {
        u32 len1 = maxlen - micExtBufferWritePos;
        memcpy(&micExtBuffer[micExtBufferWritePos], &input[0], len1*sizeof(s16));
        memcpy(&micExtBuffer[0], &input[len1], (len - len1)*sizeof(s16));
        micExtBufferWritePos = len - len1;
    }
    else
    {
        memcpy(&micExtBuffer[micExtBufferWritePos], input, len*sizeof(s16));
        micExtBufferWritePos += len;
    }
}

void AudioMute(QMainWindow* mainWindow)
{
    int inst = Platform::InstanceID();
    audioMuted = false;

    switch (Config::MPAudioMode)
    {
    case 1: // only instance 1
        if (inst > 0) audioMuted = true;
        break;

    case 2: // only currently focused instance
        if (mainWindow != nullptr)
            audioMuted = !mainWindow->isActiveWindow();
        break;
    }
}


void MicOpen()
{
    if (Config::MicInputType != micInputType_External)
    {
        micDevice = 0;
        return;
    }

    int numMics = SDL_GetNumAudioDevices(1);
    if (numMics == 0)
        return;

    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 44100;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 1;
    whatIwant.samples = 1024;
    whatIwant.callback = MicCallback;
    const char* mic = NULL;
    if (Config::MicDevice != "")
    {
        mic = Config::MicDevice.c_str();
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

void MicClose()
{
    if (micDevice)
        SDL_CloseAudioDevice(micDevice);

    micDevice = 0;
}

void MicLoadWav(const std::string& name)
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

    const u64 dstfreq = 44100;

    int srcinc = format.channels;
    len /= ((SDL_AUDIO_BITSIZE(format.format) / 8) * srcinc);

    micWavLength = (len * dstfreq) / format.freq;
    if (micWavLength < 735) micWavLength = 735;
    micWavBuffer = new s16[micWavLength];

    float res_incr = len / (float)micWavLength;
    float res_timer = 0;
    int res_pos = 0;

    for (int i = 0; i < micWavLength; i++)
    {
        u16 val = 0;

        switch (SDL_AUDIO_BITSIZE(format.format))
        {
        case 8:
            val = buf[res_pos] << 8;
            break;

        case 16:
            if (SDL_AUDIO_ISBIGENDIAN(format.format))
                val = (buf[res_pos*2] << 8) | buf[res_pos*2 + 1];
            else
                val = (buf[res_pos*2 + 1] << 8) | buf[res_pos*2];
            break;

        case 32:
            if (SDL_AUDIO_ISFLOAT(format.format))
            {
                u32 rawval;
                if (SDL_AUDIO_ISBIGENDIAN(format.format))
                    rawval = (buf[res_pos*4] << 24) | (buf[res_pos*4 + 1] << 16) | (buf[res_pos*4 + 2] << 8) | buf[res_pos*4 + 3];
                else
                    rawval = (buf[res_pos*4 + 3] << 24) | (buf[res_pos*4 + 2] << 16) | (buf[res_pos*4 + 1] << 8) | buf[res_pos*4];

                float fval = *(float*)&rawval;
                s32 ival = (s32)(fval * 0x8000);
                ival = std::clamp(ival, -0x8000, 0x7FFF);
                val = (s16)ival;
            }
            else if (SDL_AUDIO_ISBIGENDIAN(format.format))
                val = (buf[res_pos*4] << 8) | buf[res_pos*4 + 1];
            else
                val = (buf[res_pos*4 + 3] << 8) | buf[res_pos*4 + 2];
            break;
        }

        if (SDL_AUDIO_ISUNSIGNED(format.format))
            val ^= 0x8000;

        micWavBuffer[i] = val;

        res_timer += res_incr;
        while (res_timer >= 1.0)
        {
            res_timer -= 1.0;
            res_pos += srcinc;
        }
    }

    SDL_FreeWAV(buf);
}

void MicProcess(melonDS::NDS& nds)
{
    int type = Config::MicInputType;
    bool cmd = Input::HotkeyDown(HK_Mic);

    if (type != micInputType_External && !cmd)
    {
        type = micInputType_Silence;
    }

    switch (type)
    {
    case micInputType_Silence: // no mic
        Frontend::Mic_FeedSilence(nds);
        break;

    case micInputType_External: // host mic
    case micInputType_Wav: // WAV
        Frontend::Mic_FeedExternalBuffer(nds);
        break;

    case micInputType_Noise: // blowing noise
        Frontend::Mic_FeedNoise(nds);
        break;
    }
}

void SetupMicInputData()
{
    if (micWavBuffer != nullptr)
    {
        delete[] micWavBuffer;
        micWavBuffer = nullptr;
        micWavLength = 0;
    }

    switch (Config::MicInputType)
    {
    case micInputType_Silence:
    case micInputType_Noise:
        Frontend::Mic_SetExternalBuffer(NULL, 0);
        break;
    case micInputType_External:
        Frontend::Mic_SetExternalBuffer(micExtBuffer, sizeof(micExtBuffer)/sizeof(s16));
        break;
    case micInputType_Wav:
        MicLoadWav(Config::MicWavPath);
        Frontend::Mic_SetExternalBuffer(micWavBuffer, micWavLength);
        break;
    }
}

void Init(EmuThread* thread)
{
    audioMuted = false;
    audioSync = SDL_CreateCond();
    audioSyncLock = SDL_CreateMutex();

    audioFreq = 48000; // TODO: make configurable?
    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = audioFreq;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = 1024;
    whatIwant.callback = AudioCallback;
    whatIwant.userdata = thread;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!audioDevice)
    {
        Platform::Log(Platform::LogLevel::Error, "Audio init failed: %s\n", SDL_GetError());
    }
    else
    {
        audioFreq = whatIget.freq;
        Platform::Log(Platform::LogLevel::Info, "Audio output frequency: %d Hz\n", audioFreq);
        SDL_PauseAudioDevice(audioDevice, 1);
    }

    micDevice = 0;

    memset(micExtBuffer, 0, sizeof(micExtBuffer));
    micExtBufferWritePos = 0;
    micWavBuffer = nullptr;

    Frontend::Init_Audio(audioFreq);

    SetupMicInputData();
}

void DeInit()
{
    if (audioDevice) SDL_CloseAudioDevice(audioDevice);
    audioDevice = 0;
    MicClose();

    if (audioSync) SDL_DestroyCond(audioSync);
    audioSync = nullptr;

    if (audioSyncLock) SDL_DestroyMutex(audioSyncLock);
    audioSyncLock = nullptr;

    if (micWavBuffer) delete[] micWavBuffer;
    micWavBuffer = nullptr;
}

void AudioSync(NDS& nds)
{
    if (audioDevice)
    {
        SDL_LockMutex(audioSyncLock);
        while (nds.SPU.GetOutputSize() > 1024)
        {
            int ret = SDL_CondWaitTimeout(audioSync, audioSyncLock, 500);
            if (ret == SDL_MUTEX_TIMEDOUT) break;
        }
        SDL_UnlockMutex(audioSyncLock);
    }
}

void UpdateSettings(NDS& nds)
{
    MicClose();

    nds.SPU.SetInterpolation(static_cast<AudioInterpolation>(Config::AudioInterp));
    SetupMicInputData();

    MicOpen();
}

void Enable()
{
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
    MicOpen();
}

void Disable()
{
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
    MicClose();
}

}
